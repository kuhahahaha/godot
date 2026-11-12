---
name: godot-fr
description: Write, review, or execute an explicitly authorized Godot fork Feature Request and PLAN using source-verified contracts, branch-level tests, renderer-aware acceptance, and independent review. Use for Godot engine capability planning and FR lifecycle work; not for ordinary game authoring.
---

# Godot FR 流程

保留 ANGE `write-fr-plan` 的六项要求：可行、完整、集成、职责无重叠、测试充分、可独立执行。这里适配 Godot 内核，不机械移植 ANGE YAML、Flecs、CLI、Catch2 或旧版本路径。

## 先确定本轮阶段

- **规划**：输出 FR＋PLAN、源码证据、审查与状态；不改引擎功能。
- **审查**：按已写契约寻找遗漏/矛盾，附反例和依据；不能拿作者自评当独立审查。
- **实施**：仅在用户已授权实施时按计划落代码、红绿回归、组合验收和收口；计划通过不自动授予实施权限。

先读仓库 AGENTS、指定工作主本的当前任务/决策和当前 FR 依赖。确定源仓库 revision、工作树差异、文档唯一主本。每轮只展开本项相关源码，长任务的全局范围/依赖/状态维护在总览，不重复放进每份 PLAN。

## 编写与迭代

1. 区分能力缺口、现有行为缺陷和重复功能。真实缺陷先保存最小可复现夹具与期望；不要为增加FR数量包装缺陷。
2. 读 [计划契约](references/plan-contract.md)，核查现有实现/相邻功能，写 FR 的需求、边界、验收和 PLAN。跨项接口要有唯一所有者，引用而不各写一版。
3. 涉及构建/运行/渲染时读 [Godot验收适配](references/godot-validation.md)。命令须指向真实工具与适用目标；待新增工具标注计划路径和负责阶段。
4. 先建立行为分支矩阵，再设计测试。每行具备触发、独立期望、可定位用例及执行命令、能使其失败的变更；GPU同步不能仅靠“删屏障某机器必失败”证明。
5. 运行机械检查：`python3 <本技能目录>/scripts/check_plan.py <PLAN绝对路径> [...]`。检查引用、配对、必要章节、矩阵和表格结构；不能证明语义、源码引用的结论或测试完备。修订后重跑。
6. 非平凡 PLAN 使用独立上下文 Agent 审核源码可行性及测试完整性：给计划、必要契约和原始源码，不给“应该通过”的结论；审核遗漏分支、资源失败、邻居组合和假阳性。作者逐项核查修订，有分歧附证据。复审有效修改，仍有阻断项不标 ready。记录审查者、范围、版本、发现、处理和未执行测试。
7. 更新本项状态与总览；需要交付仓库变更时，按自身已核文件提交。只读审查或临时试用不因此要求修改/提交主仓库。首次交付说明真实完成层级；未运行的引擎测试始终写未运行。

## 持续任务

规划状态分别使用 `draft`、`in-review`、`ready`、`needs-decision`；实现/运行状态另列。`ready`表示计划可执行，若依赖尚未实现，在总览明确执行门槛；契约本身还要实施者猜测则不能ready。用户要求仅规划时允许所有实现状态为`not-started`。

一个 FR 是可独立构建并验证的职责，不是一个文件、一个测试或任意估算表的一行。每批完成实质计划和审查后再扩大上下文。集中问题只保存需要讨论的精确分歧，其余技术细节主动查源收敛。

## 技能维护

改动脚本运行 `python3 -m unittest discover -s <本技能目录>/scripts -p 'test_*.py'`；改动流程用一个实际 FR 或独立试用检查。入口保持简短，条件细节在引用文件中，不在多个位置复制规则。

来源：改编自 ANGE `write-fr-plan`（2026-10-14，SHA256 `a6ccf0ce382fa0b4eaac63b870d77a7292e89beae9e3fbe05d7f28d1baf6ff4f`）。本技能自包含；上游改变需定向比较，不能自动覆盖本 fork 适配。
