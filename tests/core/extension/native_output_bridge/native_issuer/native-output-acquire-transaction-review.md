# Actual acquire transaction — 进行中独立审查

2026-11-07。读取 `core/extension/gdextension_production_output.cpp` 新 `production_output_acquire`，及原owned factory/pin/slot cleanup。未修改生产代码、未构建/运行。生产仍在变动，本记录不是完成收据。公共Create仍SceneOnly gate是已知未完成项，本审查不将其重复列为新缺陷，也不声称机械链为E2E。

## 已即时报告给根与Astra的事务问题

1. **BOUND duplicate可跳过live/source/retiring复验。** 读取时，`production_output_acquire` 的 `action.output_state != UNBOUND` 分支仅比particles/generation/slot/request/lease，随后project并对BOUND返回OK；该分支在 `production_output_source_matches_locked`、lists_closed、effects_pending/source_device_lost检查之前。project对BOUND直接输出alias writables，不检slot.state、original pin live或alias witness。`ExternalOutputOwner::cancel_slot` (`external_output_owner.cpp:417...`) 只改slot RETIRING；检索cleanup没有看到同步action.output_state到RETIRING/RETURNED。因此同一action在slot已retiring、source use_closed或actual witness returned时，可能重复获得陈旧writable投影/OK。应对重复BOUND做Registry→RD下的source/alias/slot/pin复验；已retiring/returned只能报告custody，不能发writable grant。修复状态待Astra最终源码复读。
2. **Deferred SET的无效应ERR_BUSY可能遗留creating/pending。** acquire先置 `set.creating=true`、`returns[i].pending=true`，调用 `production_create_execute_locked`。该函数 (`gdextension_production_owned.cpp`，render thread/`rd._thread_safe_.try_lock()` guard) 失败直接返回ERR_BUSY，不走本函数的finish；旧queued service可以重试，但新acquire随后转slot/action RETIRING并返回，没安排继续此SET创建。SET无primary时 `production_sets_step_locked` 跳过；creating/pending可能卡住terminal/release。需要在该路径走finish，或由acquire恢复无工厂效应的creating/pending；实际已有RID前缀必须照常留custody。修复状态待复读。
3. **Zero capacity的role约定需收口。** `plan_instance192(0,surfaces)` 仍有OUTPUT_META32等protected extents；acquire当前按roles[i].bytes直接等layout.bytes[i]，且对i<3绑定alias，故容量0要求meta writable role。固定ABI协议写明zero capacity无writable aliases。应将zero capacity对外role投影与内部必要控制分配分开，不能用有control buffer证明可给producer writable。此是协议与当前机械路径不一致，不是GPU验收发现。

## 已核查的正确结构

- 入口真实typed decode后认证原physical RECORD context/request，要求frozen、EXECUTING RECORD、prepared NATIVE_OUTPUT、原ordinal与精确frozen input。新事务只有UNBOUND且next_action==ordinal可开始。
- 初始physical preflight按Registry锁→RD try_lock，不在持RD锁时等待另一个阻塞Registry。storage factory前显式释放Registry，`allocate_instance192_owned` 每个原factory自行按Registry/RD规则验证；返回后恢复Registry。request处于EXECUTING RECORD阻止公开cancel/release直接回收；slot original pin把request arena与owner slot关联保留。
- 原pin直接安装在稳定的 `owner.slots[slot_index].original_pin`；row.output_consumer指向此地址。没有先在栈pin上捕获再移动到slot的地址失配。
- source/closed-list、slot、surface/layout、roles extent、spare cells/read预算、metadata bytes等检查均在pin/slot状态提交前。接收者先用accepted slab placement建立storage/surface rows，之后实际owned factory成功资源保存在原slot；不能把声明prepared视为factory成功。
- owner_bind之前失败，pin回滚；storage first-call无效应可回滚时，析构placement对象、unpin、回UNBOUND。lease serial消耗不回退，不复用曾发出的serial。实际group或RID前缀存在时保持pin/storage并转RETIRING或UNKNOWN，调用cancel_slot封闭publication/pass入口；不在栈上free真实prefix。
- deferred SET factory保留在原request resource/return cell及owned_physical；后续SET失败不把此前成功SET伪装未创建。原清理是否立即推进还依赖request cancel/exhaustion，不能把返回错误本身当成native return。
- `++request.next_action`仅在最终成功点；重复BOUND分支没有再次推进；prefix失败没有推进。read continuation另需保证不重复推进producer，这不由acquire本身证明。

## 待最终复读/运行验证

上述duplicate/live状态、无效应SET BUSY恢复、zero-capacity规则修复；真实partial factory prefix及SET失败清理；same request重复输入变化拒绝、source retired重试拒绝；root/pin/group返回后无writable投影。当前未运行这些负例。最终source identity与fixture/targeted测试记录由根统一收口。

## 后续静态复读（2026-11-07，同日进行中快照）

已复读Astra修订：BOUND重复分支现在先检owner admits_work、slot publication/pass关闭及RESERVED/WRITING状态、original pin、effects/source loss，再在RD锁内复验完整source span和所有存在的alias；失败只project(false)。slot RETIRING同步action后不再输出writables。同步deferred SET失败现在恢复creating=false，且仅无primary时清pending，保留真实RID前缀。zero capacity对外roles严格要求0，而内部protected storage仍按原layout处理。`return_slot_pin`保留冷request/ordinal/lease身份并通过原unpin推进action RETURNED。上述三个已报告问题在此源码快照已看到对应修订，不代表运行验收。

已在唯一授权测试TU添加OutputNativeSceneAcquire实际链：同attachment第一请求经安装CreateV2获得真实SceneOnly owner；第二请求经原storage_create与prepare_copy，安装RECORD执行物理COPY ordinal0后output_acquire ordinal1。覆盖不完整source span、跨action anchor、错误ordinal、精确重复不推进revision/frontier、输入变化拒绝、cancel后无writables。容量0SceneOnly所有roles缺省，不伪造raster readiness。清理使用原slot OBSERVE、generation完整原control消费、particles_free、storage retire/release与V2 request release。QuoteDriver仅执行受控CPU同buffer非重叠copy，另检实际复制字节；不能作GPU结论。此测试尚需根统一编译与运行；子代理未构建、未运行、未使用Git。
