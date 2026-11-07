# Vertex range independent source review

2026-11-07；reviewer `/root/review_vertex`。范围仅为 `RenderingDevice::vertex_array_create` 未提交范围校验块、`rendering_device.h` 的 `TestVertexArrayRangeAccess` friend、`tests/servers/rendering/test_vertex_array_range.cpp`。没有编辑生产源码、测试源码、构建、测试执行或 git 提交。

已读仓库 `AGENTS.md`、godot-fr 及 Godot 验收适配；按 Karpathy 技能保持审查边界。工作树 HEAD 为 `e433d4ef40e1e6e3700412b82d76b10fc6206a91`。其它 dirty 修改不在本结论范围。

结论：限定范围内没有发现阻断问题。此结论为独立源码审查；运行结果来自根代理提供的已有工件。

## 算术与契约

- `rendering_device.cpp:4808` 先判断 `binding_offset > buffer->size`，以 `||` 短路保护 `buffer->size - binding_offset`；随后确认 attribute offset 可减。两次 subtraction 均不下溢。未计算 binding+attribute 的潜在溢出和。
- 完整元素判断保证 `available >= element_size > 0`，后续 `available - element_size` 安全。attribute offset 为 uint32，binding offset 为 uint64。
- per-vertex product 在首项转为 uint64 后计算。两个因子均至多 UINT32_MAX（count0 的既有 uint32 `count - 1` 结果同样为 UINT32_MAX），故乘积至多 `(2^32-1)^2 < 2^64`。与剩余容量直接比较，不再把大 offset 加入乘积。
- `Buffer::size` 为 uint32（`rendering_device.h:374`），public `vertex_buffer_create` 的 size 也为 uint32（`:1316`）。非零 instance stride 的结果 `(available-element_size)/stride+1` 至多 UINT32_MAX，因此转回 uint32 不截断。此推论依赖当前 buffer size 契约；若未来扩大 buffer size，需要再审查。
- stride0 走条件表达式独立分支，不除0；一个元素完整时允许 UINT32_MAX 次重复；不完整则在分支之前拒绝。多 attribute 用 MIN 约束实例上限；先通过元素检查后，无副作用失败。
- 未新增全局 `p_vertex_count == 0` 拒绝。`mesh_storage.cpp:2146` 的 point-size emulation 将频率设为 INSTANCE，`:2267` 会传 count0；instance 分支不计算 `count-1`，测试覆盖这种合法调用。
- Metal `rendering_device_driver_metal.cpp:1125-1146` 对 stride0 设置 `VertexStepFunctionConstant`、stepRate0，再为 descriptor 填充元素所需 stride；不按 instance 推进。该实现支持本次常量元素 admission 契约。这里只查源码，没有执行 Metal/GPU。
- friend 声明仅允许专用 fixture 观察 owners、transfer metadata 和实例上限。fixture 经 public factories 构造资源；不以手工构造 VertexArray 冒充 production admission。新 TU 带 `TEST_FORCE_LINK`。

## 测试范围与限制

覆盖 binding+attribute exact-end、tail+1、超范围及极大 binding offset、stride0 per-vertex、count0 all-instance、重叠元素 stride<element、部分末尾 stride、常量 instance 元素完整/不完整；失败同时核对 owner 和 dependency 数量无增长，成功核对 tracker 与 transfer metadata，所有分支检查 pending transfer 未被消费。

非阻断覆盖限制：`large stride and count cannot wrap the range` 同时给出超范围 binding offset，所以会在 offset guard 早退，不能独立证明 stride/count 算术分支。该分支的安全性来自上面的类型上界证明。本次不扩大测试或生产修复。测试仅一个 attribute/buffer，未运行多绑定集成。per-vertex count0 的既有 unsigned count-1 语义不在本次修复范围，也不把它当作 all-instance 的证据。

## 红证据和身份

读取 `work/native-preparation-red/RUNS.json` 及 `SOURCE_IDENTITY.json`，并重新计算 red snapshot 的三个 scoped 文件 hash，均与 manifest 一致。red base 为上述 HEAD；red production 保留原 vertex range 行为；test 和 friend 与当前一致。

- Red production cpp SHA256 `6e5ec272227a6b118cdc6207958b54d34344b7e3365636d40b5650b728060d17`。
- Header SHA256 `65facb3aa564583514d85749e726076bbe3418907705d1d3f4051d4841f41216`；test SHA256 `f9c8d02924221bf74832ea12be73cc93b8d295f651e3a7b079b70172f3e4fff1`。
- Red binary 156746600 bytes，SHA256 `c0f6ac8b49730ace0fa8925483cec50894fee2bc7131cb6a8754697175c21481`。
- `work/native-preparation-vertex-red.log`：exit1，2 cases failed，369 assertions，35 failed；有效反证包括 invalid range 接纳和错误实例上限。log SHA256 `cad002614dda751bb06863ec8488547cd05470752b45c89cb50f030325c7a911`。
- `work/native-preparation-constant-red.log`：exit1，1 case failed，47 assertions，5 failed。log SHA256 `0a2ca840995e91538c4081aa3315723f995cd8cd9520a87f73380c12a3bbe36a`。旧 stride0 实例路径包含除0，红日志中得到1只是该次二进制观测，不宣称这是可移植的旧行为。

当前完整 cpp SHA256 为 `c0b95e0c7b6cb2138b5e0e99fc4e99a45fdb046b1b35f66bdc73f30409a03936`，其中另有非本审查范围变更。单独提取 `vertex_array_create` 函数文本 SHA256 为 `f4cd173c471c9ea6a0669dc4198288ec657201d1a6c51bf4e19492f173b1190a`。

## 绿证据

根代理已运行；本审查者仅读取工件，不重复执行。`work/native-preparation-green-runs.json` 记录 focused 和 combined 命令均 exit0，同一 binary 156746616 bytes，SHA256 `1a9d51daf742439f4531e23053df4e1678caf680f9497e330429d7e125b3202d`。`work/native-preparation-focused-green.log` 为19 cases /7909 assertions通过；`work/native-preparation-combined-green.log` 为198 cases /88268 assertions通过。两个筛选都包含 `[VertexArrayRange]` 与 `[VertexArrayRangeConstant]`。

这是 public factory + controlled driver 的 CPU/headless admission 回归，不证明真实 GPU、上传、draw、平台设备兼容性或 Metal execution。
