# Sustcore

# 编译, 运行与调试

## 编译

首先, 请确保你拥有riscv64与loongarch64的交叉编译器, 并且版本为GCC 15.
目标三元组分别为riscv64-unknown-elf与loongarch64-unknown-elf.

输入 `make build` 即可编译

## 运行

输入 `make run` 即可运行. 更推荐 `make all`, 编译并运行.

## 调试

输入 `make run_dbg` 即可运行. 更推荐 `make dbg`, 编译并调试.
需要用 gdb 连接到 `localhost:1234` 上. 可参考我给出的配置文件在VSCode上进行配置.

# 代码规范

## 命名规范

变量名与函数名采用 `c_style`. 类型名则采用 `UpperCamelCase`. 宏采用 `MACRO`.

## 代码规范

无参数的函数应写作 `return_type func_name(void)`.
注意注释含量. 可使用 `make stat_code` 统计代码的注释含量与密度.
命名应采用语义化命名方案, 可采用熟知缩写.

# 环境配置

参见 [config-ref](./config-ref/README.md)