# Checked pipeline tuple / ShaderRD mutex 独立源码审查

审查日期：2026-11-07。范围：当前工作树 `RenderingDevice::_draw_list_bind_render_pipeline_recorded` 的 `p_capture` tuple guard、`[CheckedPipelineCompatibility]` fixture、`ShaderRD::version_free` 保存 mutex 指针修复。结论：此范围未发现阻断问题；源码满足 checked bind 在新增 effect 前同时验证 framebuffer format 与 subpass 的要求。绿测试由主代理执行，下面区分其记录与本次源码审查。本审查未运行构建、测试或 Git 命令，未修改生产源码。

## Checked pipeline

- `servers/rendering/rendering_device.cpp:6873` public wrapper 显式传 `false`。`:6890-6892` 新增 guard 仅 `p_capture` 生效，使用 mismatch OR，因此任意一项不符即 `ERR_INVALID_PARAMETER`，且不受 `DEBUG_ENABLED` 条件控制。`:6893-6895` 原有 debug mismatch AND 保留。因此本次新增 guard 不改变 public false 的现有 debug/release 语义；不能将其表述为 public API 也获得了 exact tuple 检查。
- 验证在 `:6897` cached pipeline 提前返回前，故缓存同一不兼容 RID 不能绕过检查；也在 `:6901-6902` pipeline/push state 修改和 `:6904` `add_draw_list_bind_pipeline` 前。后续 shader/set state 与 debug validation 更新都更晚。失败时返回初始 `DrawAppendResult` 的复制，仅改 error；`servers/rendering/rendering_device.h:482-499` 默认 NoNewEffect、零 prefix、capture false、has_transfer false。此位置足以给出 before-effect 源码证据。
- 原 factory 在 `servers/rendering/rendering_device.cpp:6052-6053` 无条件写入 framebuffer/pass tuple。fixture `tests/servers/rendering/test_native_disposal.cpp:849-868` 通过真实 texture、multipass framebuffer 和 render_pipeline 工厂构造四种 tuple，并读取而不改写 validation 数据。并非直接向 owner 填造 tuple。
- fixture `:879-903` 覆盖 format-only、pass-only、both mismatch；检查 error/effect、无 graph handle/prefix/transfer/capture、graph command count 不变，以及 pipeline/shader/set_count/push/draw_count。`servers/rendering/rendering_device_graph.h:79` 的 command count getter 返回实际 `command_count`。`:904-908` 验证兼容 bind 确有 Recorded handle。`:909-913` 经 public false 缓存 wrong_format 后再次 checked 拒绝，直接防止把 guard 移到冗余状态早退之后的回归。
- 测试没有逐字段快照所有 descriptor/debug state；这里只能将其作为选定状态与 graph count 的运行断言。完整 before-effect 结论还依赖上述 guard 位置的源码审查。它也不执行真实 GPU draw。

## ShaderRD mutex lifetime

- `servers/rendering/renderer_rd/shader_rd.cpp:237-241` 使用 `memnew(Mutex)` 独立分配 mutex，Version 仅保存指针并登记；`servers/rendering/renderer_rd/shader_rd.h:82-107` 无自定义删除 mutex 的析构器。
- `core/templates/rid_owner.h:360-365` 在 `free` 中先析构 T、使 validator 无效并把 slot 放回 free list。因此 `version_owner.free(p_version)` 后读取 `version->mutex` 会访问已结束生命周期且可复用的 Version slot；mutex 自身则尚未释放。
- 当前 `servers/rendering/renderer_rd/shader_rd.cpp:948-953` 在 free 前把原指针保存为 `Mutex *const`，通过同一指针 lock、clear、free、unlock、memdelete；free 后不再解引用 Version。不会误 unlock/delete 随后复用该 slot 的新 Version 所拥有的 mutex。
- `_clear_version` (`:263-279`) 先 `_compile_ensure_finished`；该函数 (`:796-801`) 调用 `_compile_version_end`，后者 (`:751-757`) 等待 compilation group 完成。清理与 worker task 的现有顺序未改变。
- 这是释放后读取 Version 成员的局部生命周期修复，不建立任意线程可同时 `version_free`/访问同一 RID 的保证。`owns/get_or_null/lock` 序列并非跨调用的生命周期 pin；本审查不声称 TSan、并发释放或 GPU 验收通过。

## 已存在的红证据与待验收项

`work/native-preparation-pipeline-red.log`：1 test case failed，105 assertions / 93 passed / 12 failed。日志显示旧行为对单项 mismatch 产生 OK/Recorded/有效 handle 或更改绑定状态，cached mismatch 也返回 OK。`work/native-preparation-red/RUNS.json` 记录命令、退出码 1 和二进制哈希；本审查只读取该记录，未重跑。`work/native-preparation-red/SOURCE_IDENTITY.json` 明示红阶段已经包含 ShaderRD mutex 修复，因此该红日志不是 mutex 修复的 red/green 证明。

主代理后续提供的绿记录已读取：`work/native-preparation-green-runs.json` 记录 focused 与 combined 均退出 0；focused 日志为 19 cases / 7909 assertions，combined 为 198 cases / 88268 assertions，均通过。命令均包含 `[CheckedPipelineCompatibility]`；二进制 156746616 bytes，SHA-256 `1a9d51daf742439f4531e23053df4e1678caf680f9497e330429d7e125b3202d`。日志分别为 `work/native-preparation-focused-green.log` 与 `work/native-preparation-combined-green.log`。此为主代理执行的 headless/doctest 证据，未提供真实 GPU 或 TSan 通过证据。

审查读取时 SHA-256：

- `servers/rendering/rendering_device.cpp`: `c0b95e0c7b6cb2138b5e0e99fc4e99a45fdb046b1b35f66bdc73f30409a03936`
- `servers/rendering/renderer_rd/shader_rd.cpp`: `00a5b7ecb9640a520f2a05263a0d2805afa5bd4c0a92267288260c4ea24f5a82`
- `tests/servers/rendering/test_native_disposal.cpp`: `e6eb461c4de7049ff1b9ec5562f54ea69e445550cf05169f98c22091c318af4e`
- `work/native-preparation-pipeline-red.log`: `1554af9b15c29360a80f1721fa3a28698108275c45fff0938a6d9897e6452757`
