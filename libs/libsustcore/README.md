# libsustcore - Capability-Based Basic Syscall Library for Sustcore

Libsustcore 是基于能力的基本系统调用库, 其提供了 C-Styled 的 Sustcore 系统调用接口以及在此基础上的 C++ 风格的 OOP 接口, 以便于用户在 Sustcore 上进行应用开发. 该库的设计目标是提供一个轻量级、易用且高效的系统调用接口, 使得开发者能够方便地访问 Sustcore 的底层功能.

## C-Styled 接口

C-Styled 接口提供了一组函数, 这些函数直接映射到 Sustcore 的系统调用, 并且使用 C 语言的风格进行设计.
这些函数通常以 `sustcore_<cap>_` 前缀风格命名, 其中 `<cap>` 表示对应的能力类型. 例如, `sustcore_file_open` 函数用于打开文件.

## C++-Styled 接口

C++-Styled 接口在 C-Styled 接口的基础上提供了面向对象的封装, 使得开发者可以使用类和对象的方式来进行系统调用. 这些接口通常以类的形式存在, 并且提供了成员函数来操作对应的能力对象. 例如, `sus::File` 类封装了文件操作相关的系统调用, 提供了如 `open`, `read`, `write` 等成员函数, 并将 Capability Token 作为类的成员变量进行管理, 同时将 C-Styled 的返回值转换为 tay::expected 类型, 以便于在 C++ 中进行异常处理和错误管理.

一个例子是:

```cpp
tay::expected<sus::File, sus::error_code> open_res = sus::File::open("example.txt", sus::FileMode::Read);
if (! open_res)
{
    // Error handling...
}
sus::File file = open_res.value();
file.read(buffer, size);

// remove the capability...
sus::delete(file);
```