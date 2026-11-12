# Scoped receipt draft — vertex array fetch ranges

2026-11-07；待根代理统一归档。范围限 `vertex_array_create` 完整 fetch range 和专用 friend/test。

生产变化：把 binding offset、attribute offset、元素大小与 stride/count 共同纳入接纳校验，以受保护的 subtraction 避免 range 加法溢出；实例上限只计完整元素；stride0 对应可重复的常量元素。保留 point-size emulation 使用的 all-instance count0。

独立审查：`work/native-preparation-vertex-review.md`，无范围内阻断项；审查者未构建、未执行测试。极大 stride/count 测试被 offset guard 遮蔽的覆盖限制已明确记录，不作为独立动态算术证明。

红证据：`work/native-preparation-red/RUNS.json`、`SOURCE_IDENTITY.json`；`work/native-preparation-vertex-red.log` exit1，2 cases /369 assertions，35 failed；`work/native-preparation-constant-red.log` exit1，1 case /47 assertions，5 failed。red binary SHA256 `c0f6ac8b49730ace0fa8925483cec50894fee2bc7131cb6a8754697175c21481`。

绿证据由根代理执行：`work/native-preparation-green-runs.json`，focused exit0，19 cases /7909 assertions；combined exit0，198 cases /88268 assertions。日志分别为 `work/native-preparation-focused-green.log`、`work/native-preparation-combined-green.log`。binary 156746616 bytes，SHA256 `1a9d51daf742439f4531e23053df4e1678caf680f9497e330429d7e125b3202d`。组合统计是整个筛选集合，不是 vertex 专属数量。

身份：HEAD `e433d4ef40e1e6e3700412b82d76b10fc6206a91`；当前函数文本 SHA256 `f4cd173c471c9ea6a0669dc4198288ec657201d1a6c51bf4e19492f173b1190a`；test SHA256 `f9c8d02924221bf74832ea12be73cc93b8d295f651e3a7b079b70172f3e4fff1`。完整身份和证明见审查文档，归档时沿用根代理的最终 source manifest。

边界：CPU/headless public-factory admission，不代表真实 GPU 通过；没有执行上传、draw 或 Metal backend。此收据不覆盖同 cpp 的 pipeline 或 ShaderRD 修改，也不覆盖 native prep 之外的安全承诺。
