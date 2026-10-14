# Godot 构建与验收适配

以下定位核于 fork `5e557f10bcf9542e78dd3d9376c237bd70b68bb9` / 4.7.2；升级时按符号重查，不把本文件当永不变的命令手册。

## 本地入口

- `SConstruct` 的 `tests=yes` 启用测试编译，构建不是运行。`tests/SCsub` 当前递归发现 `.cpp` 并生成force-link；仍有通过`modules_tests.gen.h`接入的module header，按邻居核查。
- `tests/test_main.cpp::test_main` 使用 **doctest**，非ANGE Catch2。新测试TU还须按邻居提供`TEST_FORCE_LINK`，不能只增加文件。`main/main.cpp` 的 `--test` 初始化dummy rasterizer，不能通过补渲染flag使它变成真实GPU套件。
- `.github/workflows/macos_builds.yml` 给出本仓库实际test命令。运行前从拟用二进制的 `--help`/测试清单核实筛选参数，核对匹配名称和数量；0匹配不算通过。不从记忆复制Catch2标签语法。
- 每台机器重新核查binary清单和SCons解释器，正式单元计划须建设/定位 `tests=yes` 的binary，不假设现有release带测试。2026-10-14本机试用仅查到 `bin/godot.macos.template_release.arm64`，PATH无SCons；这是环境快照，不是仓库固有约束。工具未安装须写清前置，不能把`command not found`当引擎失败。
- Godot内核不套用ANGE的dylib/object清理规则。记录实际SCons参数、源码/二进制身份及改动是否进入产物；修改release-only边界须验相应release目标，debug断言不能证明release安全。

## 三类验证分别判定

| 层 | 用途 | 不能证明 |
| --- | --- | --- |
| CPU单元/模型 | 布局、状态、算术、接纳/拒绝、独立参考 | 真实shader、GPU同步和画面 |
| 独立可渲染Godot宿主 | main RD、真实scene producer/consumer、资源退休、数值/图像/性能 | ANGE作者编译器/ECS全链路 |
| 真实ANGE生产者 | 程序生成、打包/桥接、world和CPU程序接入 | 未实际执行的其它平台/提供者 |

真实GPU入口必须记录renderer、driver、device、shader/pipeline结果和输出。`--headless`/dummy不证明GPU通过；无设备明确UNAVAILABLE并让必需门槛失败。Compatibility是否支持按任务范围，不能悄悄替代RD。

`RenderingDevice::submit`/`sync`明确只给local device（`servers/rendering/rendering_device.cpp`）；main RD由render loop驱动，操作按`RenderingServer.call_on_render_thread`及内核阶段处理。官方compute教程的local-RD写法不能直接搬到main-RD场景共享缓冲。

`RenderingServerDefault::sync`（`servers/rendering/rendering_server_default.cpp`）同步的是CPU命令队列/线程，不能拿它替代GPU完成观测。与RD帧fence/读回完成分开记录。

读回/截图会引入同步。逐步数值验证、连续无逐步读回验证、性能测量各有明确模式；不拿同步读回的成功证明无stall调度。GPU任务完成、RT已录制和CPU请求接纳使用不同观测值。

## 证据和反证

证据包应固定源码/二进制/程序/输入身份、环境、完整命令、stdout/stderr、实际退出/超时、期望和实测、断言结果、图像/数据引用、已知限制。进程结束后再判定日志与泄漏，内部PASS不能覆盖崩溃或退出阶段错误。

通过要求positive全满足；negative只有目标断言被触发才算检测有效，不能以任意崩溃/工具失败“证明反证”。已知缺陷保留失败并单独归因，不能基线白名单吞掉引擎错误。允许精确声明预期诊断，但须匹配对应负例和次数，不能全局忽略ERROR。

检查器只做Markdown静态结构核验，不认证命令可运行或语义完整。独立审查必须实际读被引用的源与测试矩阵。

在线背景：[Godot compute教程](https://docs.godotengine.org/en/latest/tutorials/shaders/compute_shaders.html)。latest可能先于本fork；具体行为以源码为准。
