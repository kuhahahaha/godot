# Godot fork 工作规范

新增能力、编写/修订 FR 或 PLAN 时，先读 `.agents/skills/godot-fr/SKILL.md`。它适配 ANGE 的 FR 完整流程，但构建、测试和渲染事实以本 Godot fork 源码为准。

- FR/PLAN 规划、实施授权、实际验收是不同状态；用户只要求规划时，不改生产源码。
- 先确认本次文档主目录。当前粒子计划主本在 `/Users/macbookair/Documents/Work/Dev/ANGEParticleDesign/docs/`；本仓库不复制另一套粒子决策。其它功能按其任务指定位置维护。
- 不把 Godot 的 doctest、dummy/headless 通过当作真实 GPU 通过；主 RD 与 local RD 的提交/生命周期规则分别核查。
- 先读当前 Git 状态。保留他人改动，按自身文件提交；不运行 `git add .`，不因修规范去修改 ANGE、UE 或安装目录。
- 技能、规范和检查脚本也须有适用性检查；检查器不替代源码审查或测试完整性审查。
