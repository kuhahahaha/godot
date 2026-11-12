# Native output acquire/poll ABI test audit

2026-11-07；只读审查 `/root/review_vertex`。本轮不改生产或测试源码、不生成 header、不构建、不执行测试、不提交。新 Acquire/Poll 字段由根代理冻结；本文件不是字段或成功行为的批准。

## 现有测试入口

最适合独立 wire/boundary TU 的参考是 `tests/servers/rendering/test_external_output_create_interface.cpp`。它包含 `gdextension_external_output_interface.h`、`TEST_FORCE_LINK(test_external_output_create_interface)`，用本地 `input()`、`sentinel()`、`contract()` helpers，无 friend Access、无需 RD/Registry 初始化。现有 `[B4][OutputCreateWire]` 覆盖 V2 header/version/size/reserved、span 与 outer header/selector/each other 重叠、空指针/不对齐/地址加法溢出、result alias 保留字节。它调用原公开 C 函数实现，尚未经 guarded lookup 检查名字注册。建议新独立 `test_external_output_acquire_poll_interface.cpp` 采用此隔离方式，仅做 wire/registration；不要复制整个 ProductionRequest fixture。

需要证明真实 RECORD/custody 路由时，最适合的现有 TU 是 `tests/core/extension/test_production_request.cpp`：

- namespace `TestProductionRequest` 中的 `Access`（约66行）有原 RD/Registry friend 访问权限；`SingletonEnrollment`（约1144行）实现原 device/owner/request 初始化。
- `Access::OutputLeafProbe::load`（约7932行）逐个名字检查 legacy `gdextension_get_proc_address` 返回 nullptr、guarded `gdextension_get_guarded_proc_address` 返回非空，然后转成 generated typedef。它也验证旧 type aliases 的可赋值性。
- `OutputLeafProbe::invalid_inputs`（约7994-8106行）先将两个结果置0x5a，记录完整 byte snapshot；非法 DTO pointer、misalignment、UINTPTR_MAX extent、nullptr result、misaligned result、input/result alias 均用 memcmp 验证未写。输入 span 与 result alias 也应不写；input/span 或两个 input span 重叠则写 CONTRACT。这两类行为不能混成同一个期望。
- 同 helper 对 abi_version、struct_size、abi.reserved，以及 span.reserved 做 checked contract 反例；末尾检查 request revision、binding_used、output_owner_head/cursor 与原 native slot/withdraw 状态未变化。
- `output_registered_leaves`（约8189行）由 `[SceneTree][ProductionRequest][OutputRegisteredLeaves]` 测试调用。`ordinal_server()` 检查主线程、无既有 RD 并 sync；`ordinal_initialize()` 调 `fixture.prepare_device(server)` 与 `fixture.initialize_owner(...)`。`OrdinalPlan` 先固定 capacity；fixture.public_port=true；安装真实 RECORD receiver。经 schedule/server sync/record_consume 执行，而不是造 physical context。`OutputLeafProbe::receive` 保存原 context，安装原 pin，测试错误 context/不同 request；RECORD 结束后再调用保存值应拒绝。cleanup 经原 release/OBSERVE。

独立 wire TU 不应为了指针测试复制这些 owner 授权步骤。相反，真实 owner/custody 成功和无副作用拒绝应留在该现有集成 TU 或根代理指定的新共享 fixture，不用伪造 Access/private owner 来声称 acquisition 成功。

## 生成与 registration 的实际职责

`core/extension/SCsub:15` 从 JSON 与 `make_interface_header.py` 生成 `gdextension_interface.gen.h`。generator 的 `check_type()` 验证类型引用，`check_formatting()` 校验格式，`write_struct_type()` 按 JSON 成员顺序输出 C struct，`write_interface()` 输出函数 typedef 和参数文档。生成器不运行 pointer alignment/overlap 校验，也不注册 exported procedure；生成通过不证明 runtime ABI 边界。

现有 runtime extent helpers 位于 `servers/rendering/rendering_device.cpp` 的 `OutputMemoryRange/output_range/output_object/output_overlaps`（本轮约14979-15004行）。它们先验证 null、alignment 和 `(UINTPTR_MAX-begin)/stride` 容量，再形成半开区间。create/seal 在任何 result write 前验证 result/input 区间不重叠。runtime source 由根代理负责复用/扩展，不归本审查修改。

注册位于 `core/extension/gdextension_production_interface.cpp:249-258`，每个现有 leaf 先 static_assert `decltype(&implementation)` 等于 generated typedef，再调用 `GDExtension::register_interface_function`。`gdextension_interface.cpp:351` 明确 legacy 查找隐藏全部 `device_session_` 名字；guarded 查找才走 `get_interface_function`。因此 public exported proc 的验收应是 guarded callable、legacy hidden，不能要求 legacy 非空。

GDExtension 全局 interface 初始化由 `core/register_core_types.cpp` 的 initialize_gdextensions 到 `gdextension.cpp:1329` 的 gdextension_setup_interface 完成；正常 doctest 核心启动已执行。独立 TU 应直接查 registry，不再手动重复 setup 或 register，避免重复初始化掩盖缺失生产注册。

## 字段冻结后的建议矩阵

| 触发 | 独立预期 | 推荐层 |
| --- | --- | --- |
| exact acquire/poll names lookup | guarded非空；legacy nullptr；用生成 typedef 取函数指针并实际调用 | 新独立 wire TU |
| 实现函数签名 | generated typedef 与 implementation `decltype` 同型；存在兼容alias时只校验冻结后的alias | production registration static_assert + test编译 |
| result nullptr/misaligned/UINTPTR_MAX extent | 不解引用、不调用 native receiver；提供真实 backing 时全部 sentinel bytes 不变 | 新 wire TU |
| input nullptr/misaligned/UINTPTR_MAX extent | 已传 result 保持原 bytes；不允许先写默认 result 再检查input | 新 wire TU |
| input/result exact alias、aligned partial overlap（两个方向） | 整块 borrowed backing保持不变；必须用足够大的 alignas byte storage，不能依赖小input对象可容纳大result | 新 wire TU |
| 冻结ABI version、size±1、reserved非0 | 有效互不重叠 pointer下返回确定 CONTRACT，失败 DTO中的token/writable/count/state保持契约定义的零值 | wire或有storage的集成TU，取决于header校验所属层 |
| 有效input、无renderer | 仅在该 TU环境保证无ParticlesStorage时断言 renderer_unavailable，不以任意错误算通过 | 新 wire TU，可选 |
| 嵌套pointer/span（仅冻结字段确实含有时）null/misaligned/overflow/result overlap | 先保证所有extent安全才写；result overlap应保持bytes；input-input overlap按冻结契约返回CONTRACT | 新 wire TU |
| 零长度span、刚好相邻的半开区间 | 确认空span/相邻区间不被错误判overlap；不声称可读取非零无效地址 | 新 wire TU |
| 已关闭或过期physical context、不同request/attachment | 不能获得lease或推进原owner；检查revision/pin/slot/计数不变 | 原ProductionRequest集成fixture |
| acquire NO_CUSTODY 与 retained/unknown前缀（若冻结包含disposition） | 错误码不能代替disposition；no-custody没有lease/writables，retained/unknown仍保留原义务 | 真实owner集成测试，不能wire模拟成功 |
| poll重复调用、错误generation/lease | 原request/owner完全不变；不schedule、不append、不advance action，不从时间推断完成 | 真实owner集成测试 |

Compile-time layout 应在 JSON 冻结后为跨DSO agreed types固定 sizeof、alignof 和每个 public field offsetof；仅 `sizeof(field)==sizeof(type)` 或从同一struct推导expected不能发现布局漂移。64-bit平台固定数字只在该目标断言；如需32-bit或float/double配置，分别使用明确目标合同。不要手写另一套ABI struct作为“expected”然后让两份一起漂移。generated header更新与runtime注册检查是互补门槛。

## 本轮状态

读取时 JSON 已出现 AcquireResult/PollResult 与 exported names，但仍视作根代理正在编辑中的候选，不在这里冻结数值布局或新测试签名。等待根明确字段冻结与生产forwarder职责后，才可落实新独立 ABI TU。届时只编辑分派的测试文件，构建和红绿由根统一执行。现有测试通过情况未在本轮重新验证。

## 字段冻结后的测试交付（未运行）

根代理已明确字段冻结并授权仅扩展 `tests/servers/rendering/test_external_output_create_interface.cpp`。新增两个 `[B4][OutputAcquirePollWire]` case：generated ABI/implementation显式签名和standard-layout编译断言、guarded registration/legacy隐藏、两endpoint的null/misalignment/uintptr extent overflow及exact/双向partial alias保字节、合法DTO/空原context拒绝并检查不产生lease/writables/完成事实。shared alias backing足够容纳两种DTO；snapshot用memcpy避免copy padding差异，writable的失败值按字段检查。

未构建、未运行。旧binary不包含新增TU case，不能以旧筛选0匹配伪造RED；根代理统一保留未注册导出/旧源码状态并安排有效红证据。预期旧生产注册缺失不能通过guarded非空门槛；若编译旧ABI则类型缺失属于构建层失败，不能当runtime边界反证。绿需生成当前JSON、编译该TU并确认两个新case实际被执行后才可宣告。
