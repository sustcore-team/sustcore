# 构建与验证

先生成当前配置并验证主机工具链：

```sh
make init
make configure config=custom
make validate-host
```

taycpplib 常用验证：

```sh
make host-test lib=taycpplib
make host-example lib=taycpplib
make check-lib lib=taycpplib
make build-lib-matrix lib=taycpplib
```

- `host-test` 构建并运行元数据注册的功能测试。
- `host-example` 顺序构建和运行示例。
- `check-lib` 检查当前变体，包括独立头文件检查和 freestanding checks。
- `build-lib-matrix` 覆盖库声明支持的 host/freestanding 与目标架构组合。

新增公开头文件时，应同步登记
`libs/taycpplib/testbench/headercheck/metadata.toml`；新增 freestanding 语义应
加入 `testbench/freestanding/metadata.toml`。测试和示例分别由对应目录的
`metadata.toml` 注册，并在局部 Makefile 中将 program id 映射到源文件。

当前活动构建系统是 `script/` 驱动的 Make/TOML 管线。`.vscode/sustcore`
仅作历史 API 参考，不能用于判断当前编译或链接行为。
