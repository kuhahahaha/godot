# Scoped receipt draft — pipeline tuple / ShaderRD

状态：DRAFT / ROOT GREEN EVIDENCE READ。2026-11-07。

范围仅限 checked pipeline tuple 兼容性与 ShaderRD 释放后 mutex 指针访问修复；不覆盖同文件的 vertex-range 修改或其它 native preparation 更改。

生产行为：checked (`p_capture=true`) bind 在 cached pipeline 早退、CPU 绑定状态变化及 graph append 前，以 framebuffer format / subpass 任一不一致拒绝。public false 保留现有 debug AND/release 语义。ShaderRD 保存独立分配的 mutex 指针，在 Version RID slot 析构/复用后只操作该保存指针。

独立源码审查：`work/native-preparation-pipeline-shader-review.md`。审查范围内无阻断发现；无构建或测试执行。

红证据：`work/native-preparation-pipeline-red.log`，配套身份与命令见 `work/native-preparation-red/SOURCE_IDENTITY.json`、`RUNS.json`。记录退出码 1；1 case failed，105 assertions，12 failed。红阶段包含 mutex 修复；不得把此日志作为 mutex bug red reproduction。

绿证据（主代理执行，本审查读取记录）：`work/native-preparation-green-runs.json`、`work/native-preparation-focused-green.log`、`work/native-preparation-combined-green.log`。focused 19 cases / 7909 assertions；combined 198 cases / 88268 assertions，均 exit 0。二进制 156746616 bytes，SHA-256 `1a9d51daf742439f4531e23053df4e1678caf680f9497e330429d7e125b3202d`。最终源身份与归档由主代理统一收口；此文件仍是 scoped receipt 草稿。

边界：headless/doctest 不代表真实 GPU 验证。真实 GPU、TSan、任意同 RID 并发释放安全均未获本收据证明。mutex 修复由源码生命周期论证支持，主代理 headless 回归通过；没有独立 mutex bug 红复现或并发压力验收。
