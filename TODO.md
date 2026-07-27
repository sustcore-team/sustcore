# Sustcore TODO

本清单以当前 `script/` 驱动的 Make/TOML 构建流程为准；`.vscode/sustcore/` 仅作为迁移时的参考。

## Host 构建与 Testbench

- [ ] 将 host tests、sanitizer profile、header checks 和双架构 library matrix 接入持续集成。

## 库与依赖

- [ ] 支持同一 `id` 的多个版本，并让依赖解析器按版本约束选择唯一候选项。
- [ ] 允许同一目录中不同 `[[libmeta]]` 条目拥有独立的依赖声明，消除共享 `dependencies.toml` 的限制。
- [ ] 完成 C/C++ 标准头文件归属整理，并核对 `mini-cstd` 的导出头文件策略。

## 内核

- [ ] 继续将内核源码和构建描述迁移到新的 `kernel/Makefile` 树；旧 `.vscode/sustcore/` 实现只用于行为对照。
- [ ] 完成 C 与 C++ 运行时职责拆分，补齐 `basecpp` 等运行时集成。
- [ ] 补全内核源文件覆盖、链接输入和运行时初始化，达到完整内核的可构建、可启动状态。
- [ ] 验证依赖缓存注入、链接脚本选择与 boot 模式在 RISC-V SBI 和 LoongArch64 LaBoot 下的端到端行为。

## 模块与 Initrd

- [ ] 为 `kernel/initrd.toml`、模块构建和 CPIO 产物增加集成校验，覆盖普通文件、多个模块和路径安全性。
- [ ] 明确模块 ABI、加载流程及内核侧模块管理接口，并据此扩展现有 `init` 模块。

## 文档与验证

- [ ] 维护从初始化、配置、构建到 `runonly` / `dbgonly` 的可复现验证流程，并记录两种架构的前置工具要求。
- [ ] 建立自动化验证矩阵：配置生成、依赖解析、库构建、内核链接和可用时的 QEMU 启动冒烟测试。
