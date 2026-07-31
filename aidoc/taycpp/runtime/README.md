# 最小 C/C++ 运行时

四个库不是彼此替代关系：taycpplib 构建在最小语言支持和 C ABI 之上；
mini-cppstd/mini-cstd 提供 freestanding 编译所需的标准名字表面；tayclib
提供项目特有的 C 工具。

- [tayclib 与 mini-cstd](c-runtime.md)
- [mini-cppstd](cpp-runtime.md)

这些库都只覆盖当前内核需要的子集。看到熟悉的标准头文件名并不意味着符合
完整 hosted 标准库的全部语义、locale、线程、异常或 I/O 要求。
