# Independent source review

Reviewer: /root/resource_snapshot_review. Read-only review of the full current Curve/Gradient module, original registration and tests against M1.3/M2 and canonical ABI. Reviewer did not edit, build or run tests.

Initial actionable P2 findings: scalar output aliases can return OK with summed counts; analytical and pre-sort/partial-sampling acceptance branches were missing. Author reproduced the alias defect as a 9-assertion behavioral RED, then repaired entry validation and added the named controls.

Final reviewer response (verbatim):

复审当前源码后，未发现新的可执行 P1/P2 问题。

- 原标量输出别名问题已由首次写入前的完整 span 检查修复。对无法独立表达计数的重叠、空或未对齐 span，返回 BAD_ARGUMENT 且不触碰输出是合理边界；应在交接说明中明确该例外。正常、互斥输出在合法形状的失败路径仍保留实际 revision 与完整元素前缀。
- 已补充先前缺少的非线性 Curve、采样溢出前缀、Gradient 插值/色彩空间及未排序资源容量拒绝控制；这些源码期望具有判别性。
- 未发现漏掉的 Curve/Gradient 实际修改入口；信号阻塞/延迟、排序与 bake、非回绕耗尽逻辑仍符合 M1.3。
- canonical ABI 副本保持逐字一致，三个具名 proc 在原场景注册入口注册。代码未扩大为任意 dangling-pointer 验证或 owner/window 授权。

本次仅独立源码复审，未编辑、构建或运行测试；不据此认证 canonical 构建、原生 caller/E04 接线、GPU 或 G1。
