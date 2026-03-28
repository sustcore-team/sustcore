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
#include <sus/owner.h>
#include <sus/rtti.h>

#include <concepts>
#include <cstddef>
#include <string_view>

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
    } -> std::same_as<IMetadata&>;
};

using inode_t = size_t;

/**
 * @brief 元数据接口
 *
 * Metadata的生命周期由其提供者完全管理
 * 一个常见的实现方式是将其作为提供者的成员变量
 *
 */
class IMetadata {
public:
    virtual ~IMetadata() = default;
};

enum class INodeType : uint8_t { FILE, DIRECTORY };

/**
 * @brief inode接口
 *
 */
class IINode : public RTTIBase<IINode, INodeType> {
public:
    virtual ~IINode()                                   = default;
    /**
     * @brief 将该目录项作为目录打开
     *
     * 该方法实际上等价于as<IDirectory>()
     *
     * @return Result<IDirectory *> 目录对象
     */
    virtual Result<IDirectory *> as_directory();
    /**
     * @brief 将该目录项作为文件打开
     *
     * 该方法实际上等价于as<IFile>()
     *
     * @return Result<IFile *> 文件对象
     */
    virtual Result<IFile *> as_file();
    /**
     * @brief 获得元数据
     *
     * @return Metadata& 元数据对象
     */
    virtual IMetadata& metadata()      = 0;
    /**
     * @brief 获取inode ID
     * 
     * @return size_t inode ID
     */
    virtual inode_t inode_id() const = 0;
};

/**
 * @brief 文件接口
 *
 */
class IFile : public IINode {
public:
    static constexpr INodeType IDENTIFIER = INodeType::FILE;
    virtual INodeType type_id() const {
        return IDENTIFIER;
    }

    virtual ~IFile() = default;

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
     * @brief 获取文件大小
     *
     * @return Result<size_t> 文件大小
     */
    virtual Result<size_t> size() = 0;

    /**
     * @brief 同步文件数据到存储设备
     *
     */
    virtual Result<void> sync() = 0;
};

/**
 * @brief 目录接口
 *
 */
class IDirectory : public IINode {
public:
    static constexpr INodeType IDENTIFIER = INodeType::DIRECTORY;
    virtual INodeType type_id() const {
        return IDENTIFIER;
    }

    virtual ~IDirectory()                                  = default;
    /**
     * @brief 在目录中查找指定名称的目录项
     *
     * @param name 目录项名称
     * @return Result<inode_t> 查询到的目录项INode号
     */
    virtual Result<inode_t> lookup(std::string_view name) = 0;
    /**
     * @brief 同步目录数据到存储设备
     *
     * @return Result<void> 错误码
     */
    virtual Result<void> sync()                                        = 0;
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
     * @return IFsDriver& 文件系统驱动
     */
    virtual IFsDriver &fs()                    = 0;
    /**
     * @brief 同步超级块数据到存储设备
     *
     * @return Result<void>
     */
    virtual Result<void> sync()                   = 0;
    /**
     * @brief 获取根目录项
     *
     * @return Result<inode_t> 根目录项的inode号
     */
    virtual Result<inode_t> root()        = 0;
    /**
     * @brief 由inode号获取inode对象
     * 
     * @param inode_id inode号
     * @return Result<util::owner<INode *>> inode对象
     */
    virtual Result<util::owner<IINode *>> get_inode(inode_t inode_id) = 0;
    /**
     * @brief 获得元数据
     *
     * @return IMetadata& 元数据对象
     */
    virtual IMetadata& metadata() = 0;
    /**
     * @brief 获得Superblock ID
     *
     * @return Result<size_t> Superblock ID
     */
    virtual Result<size_t> sb_id() const = 0;
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
    virtual Result<util::owner<ISuperblock *>> mount(IBlockDevice *device,
                                            const char *options)       = 0;
    /**
     * @brief 解挂文件系统
     *
     * @param sb 超级块
     */
    virtual Result<void> unmount(ISuperblock *sb)                        = 0;
};

static_assert(ISyncable<IFile>);
static_assert(ISyncable<IDirectory>);
static_assert(ISyncable<ISuperblock>);

static_assert(IMetadataProvider<IINode>);
static_assert(IMetadataProvider<ISuperblock>);