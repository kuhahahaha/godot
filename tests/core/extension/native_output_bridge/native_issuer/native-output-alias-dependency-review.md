# Native output alias dependency independent review

2026-11-07；reviewer `/root/review_vertex`。只读范围：NativeDisposalWitness::Cell.production_dependent_sets，RD uniform-set 创建/物理free、native disposal free/enqueue门禁、production_owned 的 deferred set 创建路径。为核查相邻调用仅延伸到 immutable_hold 的原同组factory、production_output alias matcher、action/冻结校验和原report/service顺序。未编辑源码、未构建、未执行测试、未提交。

当前结论：初审发现的同组factory回归已在下方复审读点由源码修复；修复限定范围内无新阻断发现。初审证据保留用于追溯。计数加减主体未发现独立失衡问题。源码仍在变化，本结论仅针对列出的读点，不是对尚未完成的 Acquire/创建join 的通过声明。

## P1 — 新 alias admission 封死原同组 owned descriptor factory

触发路径：已有合法 protected RECORD，通过 `_production_output_owned_storage_create` 创建同组buffer，再调用 `_production_output_owned_uniform_set_create` 创建原组自己的descriptor。

证据链：

1. `rendering_device_immutable_hold.cpp:1583-1630` 的原factory验证每个buffer的production_request、request_serial、group、pin完全相同，然后在1618调用 `_native_disposal_uniform_set_create`。
2. `rendering_device.cpp:9998-10042` 的 helper 验证live Core-created buffer后，在10035以默认最后参数调用 `_uniform_set_create_qualification(..., &reserved)`，因此 `p_production == nullptr`。
3. 新 `rendering_device.cpp:5346-5349` 对任何buffer native witness带production_request的情况，执行 `!p_production` 拒绝。原same-group factory中的buffer必满足这个条件，因此在driver native set创建之前返回空RID；上层把它变成ERR_CANT_CREATE。
4. 已有positive场景 `tests/core/extension/test_production_request.cpp:2234` 明确要求该原owned-group set创建返回OK；2230是错误uniform类型的独立negative，不能把合法场景改为Unavailable以吞掉回归。

这是原实现路径被新guard阻断，非“未来join尚未实现”。应保留原group-owning set的准确admission与dependency语义，同时保持public普通factory无法用裸RID引用output-owned buffer。不能简单删除nullptr拒绝或仅凭RID放行。根/实现代理负责修复，本审查未改任何源。

## 已核对的计数与失败路径

- dependency vector以witness cell去重，同一buffer在一个set中重复绑定只加一次；不同set分别加一。进入集合前检查UINT32_MAX；RD mutex覆盖factory校验和递增，避免检查与加法之间另一个factory交错。
- 反射/format/资源检查失败均发生在计数递增之前。driver uniform_set_create失败释放预留RID，并且尚未增加production_dependent_sets。成功后才复制vector并加计数；之后没有可恢复的ERR_FAIL早退，set owner承接其物理free责任。
- witness为Ref持有，RID slot逻辑销毁后pending UniformSet副本保留所有dependency cells。逻辑free、RID owner释放、invalidation callback、frame等待均不减计数。
- bounded `_free_pending_resources_step` 的uniform set case4，和旧 `_free_pending_resources` 的uniform-set循环，都在 `driver->uniform_set_free` 返回后逐cell减一次、再pop队列。case4在free前的预算admit失败时不减也不pop；成功后这段没有可返回Busy的中途点。两条drain路径消费同一个pending queue，不是各自重复处理同一set。
- 两路径都检查cell非空和count非0，避免默默下溢。物理buffer free在set阶段之后处理；新counter使buffer即使跨frame队列也不能先被enqueue。
- `_native_disposal_free` 在owner/witness identity核对后、调用free_rid及其依赖递归之前，遇到非零count返回Busy；不会把live转为Enqueued/Returned。已Enqueued/Returned的幂等分支不会引入新依赖，因为set创建guard只接纳LIVE witness。
- `_native_disposal_enqueue` 还有防绕过门禁；直接普通free_rid走到_internal也不能把有physical set dependency的buffer入队。普通free可能已做逻辑dependent invalidation/边关系处理，但pending set持有的cell及counter保留物理buffer；不会因此提前driver-free。
- 原 `_production_disposal_returned` 只发布原use状态，不执行DSO callback。当前free顺序不会在driver free之前调用dependency decrement。

## deferred set 的已写路径与进行中门槛

production_set_create在prefreeze阶段接受尚未import但已prepared的output aliases，完整复制有限binding；分配owned/retire/physical serial后把set标记output_set_deferred并返回，尚未发布原create control。重复相同binding的准备调用返回OK；不应将它解释为物理set已创建。

production_create_execute_locked只在真实executing RECORD、frozen request、当前action source已admitted、state RESERVING且root非0的条件下给deferred set放开既有frozen拒绝。其后仍核查owner close/cancel/release/device/RT/RD锁、shader reflection及每个alias的exact matcher。exact alias不伪装成QualificationPhysicalReturn；最终交给RD新witness依赖。

成功创建后即使use attach失败，slot.primary仍保存真实RID，returns.pending应由调用join先建立，原sets cleanup可按primary处理前缀；不能因错误返回抹去native set。driver失败路径释放预留free-event。RD锁暂不可得返回Busy时保留creating，为后续同一原路径重试。

读取点的production_output.cpp已有alias bind/matches与prepare，但尚未出现production_create_execute_locked/deferred set execute调用。以下是待根/实现代理完成后的复审门槛，不是对缺失代码的现成bug报告：调用前正确设置creating及returns.pending/serial；创建多个set中途失败保留所有成功前缀；Busy重试不重复计数/创建；alias绑定于正确原action/root；cancel/release仍能通过原sets service先物理释放sets，再允许buffer退休。

service顺序也已核查：OBSERVE即使output owner service返回Busy仍继续原maintenance/sets cleanup；production_sets_step检查native issue完成，但不把physical_returns_pending当作拒绝条件，因此目前没有发现“buffer等set、set等buffer物理返回”的计数循环。

## 建议验证（本审查未运行）

先用现有2234同组positive保留邻居语义；新alias用例分别验证driver set创建失败count不变、同buffer重复binding只计一次、两个set计两次、逻辑free后计数仍在、bounded drain预算不够不减、第一/第二set物理free后count2→1→0、buffer在count非0时Busy且owner仍存在、最后set物理free返回后buffer可enqueue/return。别只看最终live=0：检查driver free callback顺序及原owner/rid不变，才能抓提前退还。

## 读取身份

读取时间：2026-11-07T00:50:07.219661+00:00。行号按该轮读点。完整文件SHA256如下；文件含其它并行修改，hash用于识别读点而非批准整文件：

- `servers/rendering/native_disposal_witness.h`: `e98592a01b167d4d1fd16c4dda880a5af8ca16053d50e94c3c99ccd9fc2a77fb`
- `servers/rendering/rendering_device.cpp`: `2cbf2bd7acaf97e6659abf4f9472d4626179bdb98db4ff66ee24a2d884d32a07`
- `servers/rendering/rendering_device.h`: `a5a3a75c53baa9fef00d267775216e39d5f34b14deb052be9ecac330ce43d880`
- `core/extension/gdextension_production_owned.cpp`: `a4ab585abbc7cbe7850dff299cb31956cd6ab05c6d6f01cc69d1471addf83062`
- `core/extension/gdextension_production_output.cpp`: `e17fd57affb39eba5ee75b4fc1e0279b50b7eb07f94e959990d3d8be009cb551`
- `servers/rendering/rendering_device_immutable_hold.cpp`: `f02fd685b1750b1388f046070d0ab4ddcd76bd354fac18bcc6147edbfbc7a37a`

## 修复复审 — 原同组factory回归已源码解决

复审读取时间：2026-11-07T00:52:42.818824+00:00。只读；未构建或执行现有positive测试，运行验收仍由根代理完成。

- 原 `_production_output_owned_uniform_set_create` 保留原 Registry physical context / pin匹配、实际device、原group存在、未release和确切ordinal仍为DISPOSAL_RESERVED的preflight。它在RD锁下逐buffer核对原request、request_serial、group、pin，随后把真实 `request, group, pin.serial, ordinal` 传入私有 `_native_disposal_uniform_set_create`。
- 该 helper 在创建native descriptor之前，先为reserved witness填写真实production_request/serial/group/root/ordinal；owner存在时要求serial/group/root非0、ordinal不是UINT32_MAX。这些参数是private C++ factory输入，不是新增公开RID解锁口。创建后原owned factory再次保留相同association，再用原 `_production_output_owned_bind_locked` 核对实际RID/native backing及确切group ordinal reserved row后发布member。
- RD admission仍先执行非空p_production、相同原request/serial、LIVE、count不饱和的共同门禁。之后两个接纳分支：reserved set witness与buffer同request/group/root，接纳原同组factory；或者原request中已有精确witness/RID/group/root的imported alias，接纳nonowning alias set。前者的ordinal来自原reservation preflight，并在后续bind再次校验，不把buffer自己的ordinal错误要求等于descriptor ordinal。
- public `uniform_set_create` 及旧无owner的native helper仍以nullptr p_production进入共同门禁，因此对production-owned buffer不能借“同group分支”绕过；其p_native_disposal即使存在，也没有production owner association。普通无production_request的native disposal buffer仍按旧路径，不被新guard拒绝。
- 原same-group descriptor现在满足第一个分支，修复初审必然ERR_CANT_CREATE路径；同时它也保留一个去重witness dependency，逻辑free不减，driver物理free后减。因此已有同组buffer/set场景不会因group拥有者同一而绕过物理依赖保护。
- 所有新增reserved witness字段填写只修改尚未发布的local cell。common admission失败、shader/reflection失败、driver创建失败均在递增之前；创建失败不写r_rid/r_witness。driver失败仍释放预留set RID。新参数本身没有提前递增counter。
- 原group drop选择dependency leaf，先enqueue descriptor，buffer随后即使无RID dependency edge仍受physical counter阻挡；descriptor实际free后count归零，原group member可继续buffer退休。本项没有发现新增永久等待循环。

源码结论：初审P1在当前读点标记 SOURCE-RESOLVED / RUNTIME-PENDING。没有把未运行测试标成通过。应由根继续运行已有 `test_production_request.cpp:2234` positive及同组/alias失败路径组合验收。

复审完整文件SHA256（不替代原初审读点）：

- `servers/rendering/rendering_device.cpp`: `2cbf2bd7acaf97e6659abf4f9472d4626179bdb98db4ff66ee24a2d884d32a07`
- `servers/rendering/rendering_device.h`: `a5a3a75c53baa9fef00d267775216e39d5f34b14deb052be9ecac330ce43d880`
- `servers/rendering/rendering_device_immutable_hold.cpp`: `02e091ec93c533e3d09e301ebe62893223ab1a713745d68a723a3bb6a73ec143`
- `servers/rendering/native_disposal_witness.h`: `e98592a01b167d4d1fd16c4dda880a5af8ca16053d50e94c3c99ccd9fc2a77fb`


## 紧急复审 — 同组物理依赖触发单pass预算饥饿

前次“未发现永久等待循环”的结论遗漏原同组descriptor的单pass调度；该结论由本节修正。计数本身无需失衡即可导致停滞。

1. group drop先丢各member immutable counter，再选择RID dependency leaf，把descriptor逻辑enqueue。enqueue移除普通dependency_map边，但同组descriptor新增的production_dependent_sets仍为1，直到真实driver uniform_set_free。
2. 下一次drop扫描先遇buffer。没有普通dependent边时，旧代码先调用_native_disposal_admit(WORK)，再调用_native_disposal_free；后者因physical count非零返回ERR_BUSY。drop循环不会立即因该Busy返回，但这一尝试已经消耗一个pass。
3. _native_disposal_admit对WORK同样递增usage.passes（rendering_device.cpp:11394）。production_maintenance_locked在drop结束后发现单pass用尽，返回drop_budget_exhausted（gdextension_production_owned.cpp:940），尚未启动native cursor。下次重复buffer重试，因此descriptor无法物理free以清零buffer count。已有active cursor优先完成的防护不能帮助尚未启动的cursor。
4. 当前共享源码rendering_device_immutable_hold.cpp:1147-1153已在WORK admit之前检查实际storage-buffer native witness的production_dependent_sets；非零直接continue。这样已enqueue的descriptor无需再次占drop pass，maintenance可以启动/续行原native drain，实际descriptor free使count归零，后续drop才enqueue buffer。此guard只改变等待调度，不提前减count，也不扩大admission。
5. 再核对两条物理free路径：bounded case4在driver free之前admit FREE，失败不修改counter/queue；成功后driver free、return发布、逐dependency减一次、pop，中途无Busy出口。旧unbounded队列路径同样在driver free后减计数并pop。成功set创建在driver success之后去重逐cell加一次。未发现额外计数失衡。

本次仅源码复审与文档更新；未构建、未测试、未提交。当前guard标记SOURCE-RESOLVED / RUNTIME-PENDING，由根代理唯一build/test lane验证同组positive与原关闭路径。

读取时间：2026-11-07T01:16:29.458493+00:00

- `servers/rendering/rendering_device.cpp`: `560f1c32c83eac6f890ac243338256c2dcf434922bf1489f94fb538b6ba495fd`
- `servers/rendering/rendering_device_immutable_hold.cpp`: `6e2c9bdcb0eb70e9da2288c6ee963ee2f20f7d6eabd98332c2c5552ab0957cbf`
- `core/extension/gdextension_production_owned.cpp`: `a4ab585abbc7cbe7850dff299cb31956cd6ab05c6d6f01cc69d1471addf83062`
