# NATIVE_OUTPUT / OUTPUT_ALIAS admission and freeze audit

2026-11-07；只读源码审查，未构建、未运行 Git、未修改生产代码。依据 `work/D_NATIVE_OUTPUT_ABI_FIELDS.md`。下列行号对应读取时工作树，实施时以函数和条件定位。当前头文件已有 alias/deferred/output action 私有字段（`gdextension_production_request.h:210-219,258-275`），但字段本身不能构成 accepted/prepared/physical 事实。

## 必须修改的接纳分支

| 位置 | 现有行为 | 必须落实的边界 |
| --- | --- | --- |
| `core/extension/gdextension_production_request.cpp:554-561` resource declaration loop | ownership 上界 `RETAINED_PREPARED`，新 enum 会被拒绝 | 显式白名单新增 `OUTPUT_ALIAS`；仅允许 resource_kind STORAGE、非零 physical_bytes、原 generation 非零。UNIT 仍必须 RETAINED_PREPARED；OUTPUT_METADATA 仍 CREATED_BY_OWNER；SET 不接受 OUTPUT_ALIAS。不能只扩大上界而让新 ownership 进入其它种类。 |
| 同文件 `:579-590` action declaration loop | action 上界 READBACK | 显式接纳 NATIVE_OUTPUT；保留 ordinal==index、canonical range/单调、source_recipe_reserved、first_use/use_count、list_boundary 校验。新 action 不走 DISPATCH 的 UNIT requirement，也不能把它当 READBACK slot；原 OUTPUT_METADATA physical_unit 禁用规则不能丢。若新 action 的 physical_unit 无业务含义，固定 sentinel，别隐式解释为任意资源成功。 |
| 同文件 `:601-614` contract2 readback writer declaration coverage | 任意非 READBACK action + writable use 即可 | 新 NATIVE_OUTPUT 可作为 producer，但接纳阶段 writable use 仅是声明覆盖，不证明该 alias 是该 action 的 role。保留区间与访问校验；最终 role/action/generation 精确关联必须在 prepare/freeze 补齐。对 OUTPUT_ALIAS 的 readback 应要求 producer 是它真正所属的 NATIVE_OUTPUT，不能由普通 UPLOAD/COPY/DISPATCH 声明覆盖替代。 |
| 同文件 `:620-642` worker bound | 逐 action distinct STORAGE uses 计数 | alias 仍为 STORAGE，应纳入现有有限 worker 上界；不能通过跳过新 action 丢失其 source/role storage 预算。额外 read continuation 的容量必须另外按 immutable worst-case 预算约束。 |

V1/V2 是独立契约；新枚举适用于哪个入口必须显式决定，不能无意让旧 V1 同样接收。`production_reserve_metadata_rows_locked` 被 V1/V2 共用，保留 contract-specific ordinal/canonical 规则。

## 保持严格的普通入口，防止 alias 绕过

1. `gdextension_production_resources.cpp:69` `production_import_storage` 目前严格要求 RETAINED_PREPARED。`gdextension_production_owned.cpp:140` `production_storage_create` 严格要求 CREATED_BY_OWNER。两处都应保持该等值门禁，不能为了 alias 使用 `ownership != RETAINED`、`<= OUTPUT_ALIAS` 或“凡 STORAGE 均可”之类放宽。`storage_retire` (`owned.cpp:202`) 同样只处理普通 owner，alias 实际 free owner 是 output disposal group。
2. `production_resource_slot_locked` (`request.cpp:492`) 只是找到原 slot，不提供 ownership 认证。alias binding 必须只从已 accepted/prepared NATIVE_OUTPUT action 的私有 receiver 路径进入，验证 request/request_serial、role/resource_slot/use_index、allocation generation/bytes，以及实际 native factory witness/group/member/root；不能转调普通 import/create 来伪造身份。
3. `production_import_set` (`gdextension_production_set.cpp:74`) 严格 RETAINED_PREPARED，仍拒绝 CREATED_BY_OWNER deferred SET。不能将 defer 当成 imported set 成功。普通 SET 创建入口可以接受“声明已保存、等待 alias”的状态，但实际 RID/imported/physical witness 只能在 factory 真正成功后出现。
4. Alias 绑定不得填普通 owned_physical/owned_serial 冒充独立 owner；`request.h:210-219` 已说明它是 output DISPOSAL_MEMBER 的非 owning alias。防止双重销毁及绕过 group custody。

## Freeze 必须改的三个准确分支

### A. `production_freeze_locked :858-867` prepared actions / imported resources

保留所有 action.prepared 检查，NATIVE_OUTPUT 必须由 `production_prepare_output` 完成不可变 prepare，而非 reserve 自动置 true。

现有 resource loop 仅为 OUTPUT_METADATA 豁免 imported。新增两个精确例外，不能写成 `!imported && !output_alias_prepared && !output_set_deferred` 这种只看布尔位的普遍豁免：

- 未物理绑定 alias：declaration 恰为 STORAGE/OUTPUT_ALIAS；request/request_serial 一致；output_alias_prepared；所属 output_action_ordinal 在范围内、action.request==&r、kind==NATIVE_OUTPUT 且 prepared；原 action 的一个非 absent role 的 resource_slot、use_index、generation、bytes 与该 declaration/use row 精确一致。没有 role 归属的 alias 必须拒绝。prepare 不能创建额外 source table 或重写 accepted declaration。
- 未创建 SET：恰为 SET/CREATED_BY_OWNER；output_set_deferred；已在原 finite uses arena 复制并固定 unit/shader_set/first_binding/count 的完整 binding origin；每个缺失 backing 的 binding 都指向上述精确 prepared alias；其余 binding 必须仍满足原 live import 验证。至少一个真实 alias 依赖才允许 defer。任意漏 import 的普通资源/普通 SET 仍拒绝。

已物理绑定 alias 也必须维持 output witness 与 group/root/actual backing 验证，不能因为 imported=true 跳过 ownership 关联。

### B. `production_freeze_locked :874-881` readback resource physical readiness

当前条件强制 imported + valid RID + backing_serial + use_count==1 + linked + same_physical；尚未 acquire 的合法 output alias 会在这里被拒绝。拆成两种：

- 普通 storage 保持当前完整 physical 校验。
- OUTPUT_ALIAS 可以在 freeze 时处于精确 prepared 的逻辑状态（A 所述），但这只是 freeze eligibility。实际 readback append 前必须重新要求真实 native bound witness、RID/backing、group/root/member 存活与原 generation；绝不可把逻辑 prepared 当作 `same_physical` 成功。

alias 已绑定时也检查其 actual physical provenance。retain 旧 readback ordinal/resource slot 区间检查。

### C. `production_freeze_locked :883-906` writer coverage

新增 NATIVE_OUTPUT writer 分支：writer 必须是 resource.output_action_ordinal 指向的同一 accepted prepared action；alias 必须出现在它的非 absent role 中，resource_slot/use_index/generation/bytes 完全相等；role writable use 覆盖 readback 区间；bytes 非零。不能仅把 NATIVE_OUTPUT 塞进 DISPATCH 的 generic writable-use loop。

对 OUTPUT_ALIAS，即使普通 UPLOAD/COPY/DISPATCH 具有匹配 destination/use，也不能冒充该 alias 的 native producer。先按 ownership 分流，明确 OUTPUT_ALIAS producer 必须走上述 NATIVE_OUTPUT 关联。普通 storage 继续原 UPLOAD/COPY/DISPATCH coverage；普通资源不应被 NATIVE_OUTPUT writer 仅凭 writable use 接纳。

## Deferred SET 的相邻必要修改（否则 freeze 放行仍不可执行）

- `production_set_create` (`owned.cpp:305-373`) 当前 binding loop `:348-351` 要求每个 storage 已 imported/live/linked；需要分出精确 alias 依赖的“只保存不可变 binding origin”分支。应在完整校验后占用 finite binding rows；重试仍逐字段一致。未绑定 alias 时不要发布 SET_CREATE control、不要标 imported，也不要把 deferred preparation报成物理创建成功。已有 owned_serial 重试分支仅适合已创建/已排队状态，deferred 的重复调用也需独立 identity 检查。
- `production_create_step_locked` (`owned.cpp:650-739`) 在实际 SET factory 前再次验证所有 alias actual witness 与 native physical closure，完整检查后才生成真实 uniform set。延后创建是明确的 receiver 内路径，不能通过冻后放开任意普通 set_create 修改声明。
- `production_prepare_dispatch_v2` (`request.cpp:944`) 立即调用 `production_dispatch_closure_locked`；该 closure (`gdextension_production_action.cpp:43-79`) 强制 SET 和 storage 都 imported，且要求实际 uniform_set 的 dependencies。若 output-dependent dispatch 也要在 acquire 前 prepare，则须有独立的严格声明/reflection闭包校验路径，保存 immutable dispatch；真实 append 前仍调用完整现有 physical closure。不能单纯在 closure 中看到 deferred 就 `continue`，否则 writable shader/binding/use 双向覆盖会被跳过。
- 即便当前 native output read continuation 不通过普通 DISPATCH action，这些 SET deferred rows 仍须有明确创建时点与 custody，不能仅以 flag 消除 freeze 错误。

## Prepare / source / continuation 的验收边界

ABI 文档要求 prepare 精确保存原 source resource/use row 指针并验证 source 是 live imported storage、原 generation、READ/RW access、范围；必须由 B Pending/finalized 原路径建立 accepted source admission。`output_source_admitted` 不得从 DTO scalar 相等直接置真。capacity==0 无 writable role alias，但 source/pin/action admission 仍执行；absent role 必须 sentinel + bytes0，不允许 sentinel role 指向有效 use。

read_capacity/read_graph_capacity 是不可变预算，整个 worst-case command budget 在首 append 前检查；每次 read 仍需真实 checked command closure，并复用原 native action ordinal/canonical/substep/source generation。read cursor 不能再次推进 producer next_action。以上字段存在不等于已获实际 read binding/GPU 成功。

建议根实施后至少负例覆盖：alias 经 import/storage_create 拒绝；OUTPUT_ALIAS 用于非 STORAGE 拒绝；未 prepare alias/deferred flag 单独放行拒绝；readback producer 与 alias role action 不一致拒绝；普通 writer 冒充 native alias producer 拒绝；错误 generation/use_index/bytes 拒绝；合法 deferred SET 冻结通过但实际绑定缺失仍无 append effect；capacity0仍拒绝缺 source admission。
