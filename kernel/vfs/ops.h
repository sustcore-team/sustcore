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

#include <sus/types.h>
#include <sus/optional.h>
#include <concepts>
#include <cstddef>

enum class FSErrCode {
    SUCCESS       = 0,
    INVALID_PARAM = -1,
    NO_SPACE      = -2,
    IO_ERROR      = -3,
    NOT_SUPPORTED = -4,
    UNKNOWN_ERROR = -100
};

template<typename T>
using FSOptional = util::Optional<T, FSErrCode, FSErrCode::SUCCESS>;

enum class SeekWhence { SET = 0, CUR = 1, END = 2 };

class IFile;
class IDirectory;
class IMetadata;
class IINode;
class ISuperblock;

template<typename T>
concept ISyncable = requires(T a) {
    { a.sync() } -> std::same_as<FSErrCode>;
};

template<typename T>
concept IMetadataProvider = requires(T a) {
    { a.metadata() } -> std::same_as<FSOptional<IMetadata *>>;
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
     * @return FSOptional<size_t> 实际读取的数据长度
     */
    virtual FSOptional<size_t> read(void *buf, size_t len)          = 0;
    /**
     * @brief 向文件中写入长度为len的数据
     *
     * @param buf 写入数据的缓冲区
     * @param len 写入数据的长度
     * @return FSOptional<size_t> 实际写入的数据长度
     */
    virtual FSOptional<size_t> write(const void *buf, size_t len)   = 0;
    /**
     * @brief 在文件中移动文件指针
     *
     * @param offset 偏移量
     * @param whence 偏移方式
     * @return FSOptional<size_t> 移动后的文件指针位置
     */
    virtual FSOptional<off_t> seek(off_t offset, SeekWhence whence) = 0;
    /**
     * @brief 同步文件数据到存储设备
     *
     * @return FSErrCode 错误码
     */
    virtual FSErrCode sync(void)                                        = 0;
};

/**
 * @brief 目录接口
 *
 */
class IDirectory {
public:
    virtual ~IDirectory()                                      = default;
    /**
     * @brief 在目录中查找指定名称的目录项
     *
     * @param name 目录项名称
     * @return FSOptional<IINode *> 查询到的目录项
     */
    virtual FSOptional<IINode *> lookup(const char *name) = 0;
    /**
     * @brief 在目录中创建一个新的目录项
     *
     * @param name 目录项名称
     * @param is_dir 是否为目录
     * @return FSOptional<IINode *> 创建的目录项
     */
    virtual FSOptional<IINode *> create(const char *name, bool is_dir) = 0;
    /**
     * @brief 同步目录数据到存储设备
     *
     * @return FSErrCode 错误码
     */
    virtual FSErrCode sync(void)                                        = 0;
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
 * @brief 目录项接口
 *
 */
class IINode {
public:
    virtual ~IINode()                             = default;
    /**
     * @brief 移除该目录项
     *
     * @return FSErrCode 错误码
     */
    virtual FSErrCode remove(void)                 = 0;
    /**
     * @brief 重命名该目录项
     *
     * @param new_name 新名称
     * @return FSErrCode 错误码
     */
    virtual FSErrCode rename(const char *new_name) = 0;
    /**
     * @brief 将该目录项作为目录打开
     *
     * @return FSOptional<IDirectory *> 目录对象
     */
    virtual FSOptional<IDirectory *> as_directory(void) = 0;
    /**
     * @brief 将该目录项作为文件打开
     *
     * @return FSOptional<IFile *> 文件对象
     */
    virtual FSOptional<IFile *> as_file(void)          = 0;
    /**
     * @brief 获得元数据
     *
     * @return FSOptional<IMetadata *> 元数据对象
     */
    virtual FSOptional<IMetadata *> metadata(void)              = 0;
};

/**
 * @brief 超级块
 * 
 */
class ISuperblock {
public:
    virtual ~ISuperblock() = default;
    /**
     * @brief 同步超级块数据到存储设备
     * 
     * @return FSErrCode 
     */
    virtual FSErrCode sync(void) = 0;
    /**
     * @brief 获得根目录项
     * 
     * @return FSOptional<IINode *> 根目录项对象
     */
    virtual FSOptional<IINode *> root(void) = 0;
    /**
     * @brief 获得元数据
     *
     * @return FSOptional<IMetadata *> 元数据对象
     */
    virtual FSOptional<IMetadata *> metadata(void) = 0;
};

class BlockDevice;

class IFsDriver {
public:
    virtual ~IFsDriver() = default;
    virtual const char *name() const = 0;
    virtual FSOptional<bool> probe(BlockDevice device, const char *options) = 0;
    virtual FSOptional<ISuperblock *> mount(BlockDevice device, const char *options) = 0;
    virtual FSErrCode unmount(ISuperblock *sb) = 0;
};

static_assert(ISyncable<IFile>);
static_assert(ISyncable<IDirectory>);
static_assert(ISyncable<ISuperblock>);

static_assert(IMetadataProvider<IINode>);
static_assert(IMetadataProvider<ISuperblock>);