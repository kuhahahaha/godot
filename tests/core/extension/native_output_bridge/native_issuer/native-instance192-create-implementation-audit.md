# 公共 Instance192 Create 最小完整实施路径审查

2026-11-07。只读审查；不构建、不改生产代码、不以内部 fixture 推导公共成功。范围为 accepted native typed Create 到 generation/selection/material/owned pipeline custody，供 native slot/alias 实施合并使用。

结论：不能只删除 SceneOnly 两个 gate。现有 native Instance192 storage 工厂和两个 forward renderer 的真实 pipeline preparation 已存在，但公共 Create 尚缺 slab 内 generation selection/config/preparation 的持久目录与明确容量来源；公共 selection/view/resolve/list closure 入口仍为 unavailable。生成一个成功 particles RID 不等于可渲染能力已实现。

## 当前准确阻断点

- `servers/rendering/renderer_rd/storage_rd/particles_storage_external.cpp:250-260`：旧 typed Create 传 metadata_slot sentinel；typed_v2 虽有 metadata slot，仍传 view/read/surface capacities 全 0。
- `_external_create_typed :336-350`：`validate_native_selection` 只读预检后，metadata sentinel 直接拒绝；所有非 SCENE_ONLY 拒绝 `native_profile_preparation_required`。
- `_external_create_received :382`：第二个 SCENE_ONLY gate；`:399` placement 构造 `ExternalOutputOwner(...,true)`；`external_output_owner.h:198-213` inline constructor 明确 CRASH 非 SCENE_ONLY 或非空 mesh/material Vector。去 gate 而不改构造会直接 crash。
- `_external_create_received :434`：capabilities 永远 0。`particles_external_surface_count :546-554` 永远0；surface_acquire/view_acquire/resolve/end_draw_list `:556-570` 全部 ERR_UNAVAILABLE。
- `external_output_owner.cpp:236-325` 的 MetadataLayout 仅覆盖 Owner、ViewCell、ReadCell、SurfaceCell（借用/consumer 目录）；它没有 generation-wide retained flattened geometry/material selection rows、每 slot 的 ExternalOutputStorage/SurfaceBinding、ExternalPipelinePreparation placement 空间。不能将现有 SurfaceCell 数量当成已保留 selection 的表。

## 最小完整的 Create 接入顺序

1. 保持 `_external_typed_context` 原 accepted context/request/device admission，原 config ABI/size/range/tag 校验；只开放实际实现的 INSTANCE_192。INDEXED_144 继续明确 unavailable。`validate_native_selection` (`owner.cpp:39-89`) 已验证真实 mesh ownership、逐 mesh surface 数展开、flattened material 数精确相同，以及原 MaterialStorage preparation data。该预检不保留所有者，实际 commit 前必须再次核查并 acquire 原 hold。
2. 从 accepted request 的显式预声明/有限容量导出 generation 元数据总布局。当前 v2只带 metadata_slot而内部capacities=0；需给 native receiver 一个经原 request 认证的 capacity来源，不能自行默认 8 views/若干 reads 或从任意 DTO声称有 charge。按 max_views/max_inflight、flattened surfaces、允许 preparation/pass opportunities、read capacity计算最坏 cell数和bytes；所有乘加/对齐先验算完，原 OUTPUT_METADATA declaration 的 physical_bytes必须精确匹配。扩展 planner、metadata bytes quote和接收校验使用同一布局。
3. 扩展 slab placement：内联 five 32-byte tags（已有），以及有限 mesh/material RID映射、flattened selection rows、对应 generation持有状态、slot storage/SurfaceBinding/preparation rows。原 ExternalOutputConfig 保存两个 Vector 和五个 PackedByteArray；不能把临时 parser容器的堆分配无计量地长期留在 owner。可在构造后清空这些动态容器并由 slab目录提供所有消费者所需原选择数据；这需要同步更新读取路径，而非只是删除 constructor CRASH。原外部输入数组不允许长期借用。
4. 实际 acquire generation-wide mesh surface custody：按原 mesh/surface次序形成稳定flattened row，保存真实 primitive/format/AABB/UV及所选material generation等。入口是 `MeshStorage::mesh_surface_hold_acquire` (`mesh_storage.h:570`)；原 `NativeSurfaceHold` 是不可复制所有者，目录也有 freeze/capture/retirement/return 操作 (`:558-566`)。持有与 teardown必须走其原逻辑，不能仅存RID或设置is_valid flag。public `particles_external_surface_count` 返回这个认证目录大小；surface_acquire 用有限 SurfaceCell + pin借用稳定row，填原 `ExternalSurfaceSelection` (`external_output_packet.h:81-103`) 的 geometry_source/material_selection/material_generation等。release继续现有surface_return。
5. Owner placement、directory安装、RID reservation、generation root安装的现有commit顺序与失败回收保留 (`external.cpp:399-433`)。generation pin/metadata lease转交成功后才输出particles/generation。前缀失败要保留实际native/hold custody到原服务回收；不能析构仍持hold的行或直接free metadata。当前owner析构明确禁止残存目录、pin、slot storage/surfaces (`owner.h:218-226`)。
6. 与 native slot/alias 链对接：acquire后稳定slot使用 `ExternalOutputStorage::allocate_instance192_owned` (`external_output_storage.cpp:98-148`)，先reserve整个group，再逐一 `_production_output_owned_storage_create` 真实工厂，所有实际成功前缀留原disposal group。planner `:37-59` 有current/previous 192-byte records、meta和guard/scratch/status/commands；仅 agreed three writable roles对外alias，protected资源不冒充role。不能用非owned `allocate_instance192` 代替。capacity0仍需真实source/action/pin admission。

## ShaderRD、pipeline及material的真实接通条件

现成 factory链可复用，不能用fixture“ready”代替：

- Clustered `RenderForwardClustered::external_pipeline_prepare` (`render_forward_clustered.cpp:5365-5504`) 和 Mobile 同名函数 (`render_forward_mobile.cpp:3767...`) 要求owner已开真实finite preparation row、source未seal、原surface generation匹配、pipeline pin/group/root存在及稳定geometry source。Create/slot preparation需实际填这些关联，普通构造一个ExternalPipelinePreparation并不会获得授权。
- Shader从原selected material拿 `material_get_native_preparation_data`，原 `get_shader_variant(...instance192=true)` 经 ShaderRD `enable_group`/`version_get_shader`。Clustered对应 `scene_shader_forward_clustered.cpp:557-565`；`shader_rd.h:209-235` 会初始化/等待实际compile group；原字节码factory在 `shader_rd.cpp:420,688`，placeholder在`:719`。应保证原material shader版本真实可用、无dirty/任务；placeholder RID、同version或test minimal mode都不能算ready。
- ShaderRD shader资源归它原版本所有者管理，当前已存在 material immutable hold机制；没有理由把这些shader换成伪造的output owned shader。需要确切版本ready和原material custody才能避免 shader/version重编译释放。`MaterialStorage::material_native_hold_acquire` (`material_storage.cpp:1178-1230`) 核查原material generation、dirty/next_pass、`version_native_shader_is_ready`、pipeline/shader匹配、真实uniform_set layout。仅selected material RID不保留该闭包。
- renderer准备先 `mesh_surface_hold_derive` 获取真实vertex input layout，再 `create_native_pipeline`；经 `ExternalPipelinePreparation::_create_pipeline` (`external_output_packet.cpp:116-128`) 调 `_production_output_owned_render_pipeline_create`。该真实owned工厂 (`rendering_device_immutable_hold.cpp:1736-1788`) 需要原reserved disposal cell、context、pin/group，直接发布native pipeline到原cell，防止后捕获失败失去前缀。
- pipeline root/group归原 `SurfaceBinding` (`owner.h:79-94`)，preparation中的created_pipeline只是关联copy。material hold或后续leaf失败仍保留geometry/pipeline前缀给原服务回收，不能在栈上销毁已创建pipeline。
- 精确pass/framebuffer/subpass/vertex/specialization/cull等属于有真实render opportunity的prepare；Create输入本身没有这些实际tuple，不能凭Create成功就声称所有raster pipeline已准备。至少保证实际使用前逐opportunity执行真实preparation；不能cache hash相等替代。当前renderer实现明确单view、subpass0、拒绝motion/multiview等约束，必须维持其支持面。

## 对外能力/成功的界限

Create可以表示真实Instance192 generation及其有限custody建立成功，但 capabilities必须依据接通的公共功能。若希望广告可渲染Instance192，必须一并实现公共 surface/view acquire、resolve/read binding、checked append及原end-list closure/last-use回收；当前这些stub全拒绝，不能通过设置capabilities掩盖。

若本轮只完成generation create+native output storage/aliases，明确保持raster capabilities0，并在状态/收据写出public rendering未接通；不要将其称为公共Instance192完整可渲染通过。也不要把“SceneOnly门禁被删除”的单元测试算真实工厂验收。

验收证据应分层：原typed public Create成功且实际layout/lease/root/selection holds来自原receiver；真实shader variant/pipeline工厂与material/mesh hold关联；多个surface/slot失败前缀回收；公共borrow/resolve/list paths；最后真实GPU rendering另列结果。headless/doctest/内部fixture通过均不能替代最后一项。
