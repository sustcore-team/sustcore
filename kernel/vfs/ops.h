/**
 * @file ops.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief VFS操作
 * @version alpha-1.0.0
 * @date 2026-02-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/block.h>
#include <sus/types.h>
#include <sustcore/errcode.h>

#include <concepts>
#include <cstddef>

enum class SeekWhence { SET = 0, CUR = 1, END = 2 };

class IFile;
class IDirectory;
class IMetadata;
class IDentry;
class IINode;
class ISuperblock;
class IFsDriver;

template <typename T>
concept ISyncable = requires(T a) {
    {
        a.sync()
    } -> std::same_as<Result<void>>;
};

template <typename T>
concept IMetadataProvider = requires(T a) {
    {
        a.metadata()
    } -> std::same_as<Result<IMetadata *>>;
};

/**
 * @brief 文件接口
 *
 */
class IFile {
public:
    virtual ~IFile() = default;

    /**
     * @brief 从文件中读取长度为len的数据
     *
     * @param buf 读取数据的缓冲区
     * @param len 读取数据的长度
     * @return Result<size_t> 实际读取的数据长度
     */
    [[deprecated("使用三个参数的read(offset, void *buf, size_t len)版本")]]
    virtual Result<size_t> read(void *buf, size_t len) = 0;

    /**
     * @brief 从文件的offset位置开始读取长度为len的数据
     *
     * @param offset 读取的起始位置
     * @param buf 读取数据的缓冲区
     * @param len 读取数据的长度
     * @return Result<size_t> 实际读取的数据长度
     */
    virtual Result<size_t> read(off_t offset, void *buf, size_t len) = 0;

    /**
     * @brief 向文件中写入长度为len的数据
     *
     * @param buf 写入数据的缓冲区
     * @param len 写入数据的长度
     * @return Result<size_t> 实际写入的数据长度
     */
    [[deprecated("使用三个参数的write(offset, void *buf, size_t len)版本")]]
    virtual Result<size_t> write(const void *buf, size_t len) = 0;

    /**
     * @brief 向文件的offset位置开始写入长度为len的数据
     *
     * @param offset 写入的起始位置
     * @param buf 写入数据的缓冲区
     * @param len 写入数据的长度
     * @return Result<size_t> 实际写入的数据长度
     */
    virtual Result<size_t> write(off_t offset, const void *buf,
                                     size_t len) = 0;

    /**
     * @brief 在文件中移动文件指针
     *
     * @param offset 偏移量
     * @param whence 偏移方式
     * @return Result<size_t> 移动后的文件指针位置
     */
    [[deprecated("seek()应当是VFS的职责")]]
    virtual Result<off_t> seek(off_t offset, SeekWhence whence) = 0;

    /**
     * @brief 获取文件大小
     *
     * @return Result<size_t> 文件大小
     */
    virtual Result<size_t> size() = 0;

    /**
     * @brief 同步文件数据到存储设备
     *
     */
    virtual Result<void> sync(void) = 0;
};

/**
 * @brief 目录接口
 *
 */
class IDirectory {
public:
    virtual ~IDirectory()                                  = default;
    /**
     * @brief 在目录中查找指定名称的目录项
     *
     * @param name 目录项名称
     * @return Result<IDentry *> 查询到的目录项
     */
    virtual Result<IDentry *> lookup(const char *name) = 0;
    /**
     * @brief 在目录中创建一个新的目录项
     *
     * @param name 目录项名称
     * @param is_dir 是否为目录
     * @return Result<IDentry *> 创建的目录项
     */
    virtual Result<IDentry *> create(const char *name, bool is_dir) = 0;
    /**
     * @brief 同步目录数据到存储设备
     *
     * @return Result<void> 错误码
     */
    virtual Result<void> sync()                                        = 0;
};

/**
 * @brief 元数据接口
 *
 */
class IMetadata {
public:
    virtual ~IMetadata() = default;
};

/**
 * @brief inode接口
 *
 */
class IINode {
public:
    virtual ~IINode()                                   = default;
    /**
     * @brief 将该目录项作为目录打开
     *
     * @return Result<IDirectory *> 目录对象
     */
    virtual Result<IDirectory *> as_directory(void) = 0;
    /**
     * @brief 将该目录项作为文件打开
     *
     * @return Result<IFile *> 文件对象
     */
    virtual Result<IFile *> as_file(void)           = 0;
    /**
     * @brief 获得元数据
     *
     * @return Result<IMetadata *> 元数据对象
     */
    virtual Result<IMetadata *> metadata(void)      = 0;
};

/**
 * @brief 目录项接口
 *
 */
class IDentry {
public:
    virtual ~IDentry()                             = default;
    /**
     * @brief 获得目录项名称
     *
     * @return Result<const char *> 目录项名称
     */
    virtual Result<const char *> name(void)    = 0;
    /**
     * @brief 移除该目录项
     *
     * @return Result<void> 错误码
     */
    virtual Result<void> remove(void)                 = 0;
    /**
     * @brief 重命名该目录项
     *
     * @param new_name 新名称
     */
    virtual Result<void> rename(const char *new_name) = 0;
    /**
     * @brief 获得该目录项对应的inode
     *
     * @return IINode* 目录项对应的inode
     */
    virtual Result<IINode *> inode(void)       = 0;
};

/**
 * @brief 超级块
 *
 */
class ISuperblock {
public:
    virtual ~ISuperblock()                         = default;
    /**
     * @brief 获得所属的文件系统驱动
     *
     * @return IFsDriver* 文件系统驱动
     */
    virtual IFsDriver *fs(void)                    = 0;
    /**
     * @brief 同步超级块数据到存储设备
     *
     * @return Result<void>
     */
    virtual Result<void> sync(void)                   = 0;
    /**
     * @brief 获得根目录项
     *
     * @return Result<IINode *> 根目录项对象
     */
    virtual Result<IINode *> root(void)        = 0;
    /**
     * @brief 获得元数据
     *
     * @return Result<IMetadata *> 元数据对象
     */
    virtual Result<IMetadata *> metadata(void) = 0;
};

class IFsDriver {
public:
    virtual ~IFsDriver()             = default;
    /**
     * @brief 获得文件系统名称
     *
     * @return const char* 文件系统名称
     */
    virtual const char *name() const = 0;
    /**
     * @brief 探测文件系统
     *
     * @param device 设备
     * @param options 选项
     */
    virtual Result<void> probe(IBlockDevice *device, const char *options) = 0;
    /**
     * @brief 挂载文件系统
     *
     * @param device 设备
     * @param options 选项
     * @return Result<ISuperblock *> 文件系统超级块
     */
    virtual Result<ISuperblock *> mount(IBlockDevice *device,
                                            const char *options)       = 0;
    /**
     * @brief 解挂文件系统
     *
     * @param sb 超级块
     */
    virtual Result<void> unmount(ISuperblock *&sb)                        = 0;
};

static_assert(ISyncable<IFile>);
static_assert(ISyncable<IDirectory>);
static_assert(ISyncable<ISuperblock>);

static_assert(IMetadataProvider<IINode>);
static_assert(IMetadataProvider<ISuperblock>);