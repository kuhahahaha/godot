# 独立粒子节点审查 — 2026-11-01

结论：冻结源有两项 CPU must-fix，均已由真实 pinned engine RED 复现。最小候选已准备，但没有构建候选 engine，因此没有 native GREEN 声明。其余所列节点增量没有发现明确功能阻塞。

## 1. P1：停止后的手动发射没有恢复活动标志与处理调度

两维 `_update_internal()` 新增 request 绕过 idle-return 后，manual emit 分支只写 `emitting = true`；没有执行 `_set_emitting()` 中的 `active = true` 与 `set_process_internal(true)`。真实 `_particles_process()` 只会清除节点 active，创建单个粒子不会设置节点 active。因此从完全停止状态请求 emit 0.5 秒、residual 0.25 秒（lifetime 2）后，即便粒子仍应存活，内部处理仍关闭，后续帧不推进；再进入 idle 检查还会隐藏并重置系统。仅 emit 情形也不恢复调度，且 emitting 已为 true 会使后续同值 setter 提前返回。

原 manual fixture 的模拟器额外执行 `active = true`，真实函数没有这一步，掩盖了错误。候选删除该假行为，明确 fixture 仅验证 update 的控制流和调度，并以 native RED 补充真实入口证据。

最小修复仅在实际正数 manual emission 阶段设置 active 和 internal processing。不调用 `_set_emitting()`，避免 time == 0 时递归进入 `_update_internal()`。residual-only 不复活空系统。lifetime/one_shot 属性不变；speed_scale 仍临时置 1 并恢复；cycle、单粒子寿命、one-shot 终止仍由真实 `_particles_process()` 处理。实际跨寿命 finished/one-shot 行为还须由新 engine 验证。

## 2. P2：不可推进的手动请求先启用绘制，再拒绝

两维 `_set_do_redraw(true)` / `_set_redraw(true)` 位于 todo 有限值及浮点减法进度校验之前。fixed_fps 30、请求 16777216 秒无法按 float frame_step 递减，会拒绝，但 frame_pre_draw 已连接、可见实例已开启。2D `_refresh_interpolation_state()` 更早，还可能更改内部物理处理模式。

候选把 admission 放到这些副作用之前；原 inactive 系统仍跳过未使用的预处理校验，2D 成功/idle 路径仍在原 idle 判定动作前刷新插值。帧步长的 real_t 精度、fixed_fps 选择和正常帧累加未变。

精确状态合同：失败后不修改模拟进度、active/emitting、speed_scale、绘制连接/可见性、内部处理或插值状态。**保留现有 pending request 字段语义**：request_particles_process 已入队的两个字段在 update 拒绝后仍保留，不宣称“全部状态不变”。非有限参数由公开 setter 提前拒绝、不入队，是另一条路径。

## 证据与候选

- `astra-review/CPU_MANUAL_FIX.patch`：仅两维 CPU cpp 和 manual fixture；对冻结主树 `git apply --check` 成功。
- `astra-review/candidate/`：候选完整文件，生产与 tests 原文件未改。
- `astra-review/native-red.json` / `.log`：使用独立 EDF pin，SHA-256 `edf3b010e2391120af96df148a81d87fc5a4c60801fa2de7ebffc7e9696eff58`，执行前重算吻合。headless、固定 60 FPS，未启用 GPU 测试。两维调用前 processing=false / draw connections=0；emit+residual 后 processing=false，5 帧后仍 false；非法大请求使 draw connections 0→1。rc=42，四项精确失败。
- `astra-review/probe/probe.gd`：真实 SceneTree 和 CPU 节点，没有伪造 active、调度或 RenderingServer 连接。使用普通 runtime --script，不经过原 editor 双 SceneTree 问题。观察调度/信号连接，不声称像素或私有粒子数组 readback。
- `astra-review/fixture-results.json`：删除假 active 并补负例后，冻结源 2D 15/22、3D 16/22；候选 2D/3D × float/double 均 22/22。提取的是生产 `_update_internal`，模拟器只记录时间，不是完整 `_particles_process`；结果仅属源码提取层。
- `astra-review/source-manifest.json`：本次十个范围文件的源 hash。

## 其余范围审查

- CPU/GPU finite、enum、fixed_fps guards 在各自属性和 renderer 赋值前；CPU min/max 相互约束也先拒绝非法参数。没有发现新增负例状态破坏。
- CPU animation offset 改用 PARAM_ANIM_OFFSET curve，匹配出生偏移语义；fixture 从生产片段提取，并以无曲线、仅 angle、仅 offset、两者和零值作区分。
- 3D CPU/GPU draw-order 枚举不能数值直转，显式映射正确；reverse lifetime 降级为 CPU lifetime，view depth 不错映射。ring axis 与 visibility AABB 拷贝符合已有资源语义。
- GPU3D mesh changed 连接按每个 draw pass 引用计数；set_draw_passes 删除槽位通过 setter 解除对应计数，Resource::connect_changed 对 CONNECT_REFERENCE_COUNTED 确实继续 connect；共享 mesh 不会提前丢连接。Skin changed 的替换/断开和延迟回调读取当前资源匹配。
- GPU3D emitter position history 只在 physics callback 消费；render callback 保留 one-shot 时间处理。两维非正 delta 写零 velocity；fixtures 从通知分支提取，3D 用单轴模拟揭示 render/physics 重复消费，不等同完整引擎物理验证。
- 两个 C++ 测试有 TEST_FORCE_LINK，tests/SCsub 递归收集；四个 Python fixtures 是独立 source tests，并未注册为 engine doctest，证据应继续分层。
- `test_particles_process_retry.cpp` 的 RetryCleanup 覆盖正常返回与异常展开。先前关于剩余 REQUIRE 会跨 render callback 的疑点已撤回：当前 TEST_CASE 直接调用两个 helper，没有该回调包装；候选不含无必要测试修改。本次未运行实际 GPU retry。

## 工程检查和边界

冻结范围六个 C++ 文件通过 clang-format dry-run，git diff --check 无输出。原四个新 Python fixture 均不通过仓库固定 Ruff 0.15.8 formatting；lint 共六项（I001，2D emitter 另有 E401/E701），详见 `astra-review/original-ruff-results.json`。候选 manual fixture 已按仓库 Ruff 修复；其余三份需要集成时纯格式整理。

没有运行 scons、没有修改 production/tests、没有重复既有 GPU smoke。原 RECEIPT 的八文件 source drift 已明确记载，不能把该历史 smoke 当成本次组合源的最终 receipt。最终仍需 parent 应用候选、唯一构建器构建后跑真实 CPU probe GREEN，并补跨寿命自然停止、one-shot、残留-only 空系统、开启状态失败请求和 2D 插值场景；本审查未把这些未跑边界标作已通过。
