# Sustcore

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/sustcore-team/sustcore)

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

变量名与函数名采用 `c_style`. 命名空间也应采用 `c_style`, 可以使用缩写并尽量避免下划线的使用。类型名则采用 `UpperCamelCase`. 宏采用 `MACRO`.

## 代码规范

文件开头应该有文件头注释, 包含文件名, 作者, 版本等信息. 格式参考

```cpp
/**
 * @file filename
 * @author author (email)
 * @brief A brief description of the file
 * @version alpha-1.0.0
 * @date the date
 *
 * @copyright Copyright (c) 2026
 *
 */
```

可考虑使用fileHeaderComment插件自动生成文件头注释.

无参数的函数应写作 `return_type func_name(void)`.
注意注释含量. 可使用 `make stat_code` 统计代码的注释含量与密度.
命名应采用语义化命名方案, 可采用熟知缩写.

# 环境配置

参见 [config-ref](./config-ref/README.md)