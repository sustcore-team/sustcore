/**
 * @file tarfs.hpp
 * @author hyj0824 (12510430@mail.sustech.edu.cn)
 * @brief Tar Based initramfs
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * 生命周期由 Dentry 树管理，INode 和 Dentry 保证一一对应；
 * 所以 TarNode 糅合了 Dentry 与 INode，
 * 负责析构 File/Directory 视图
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/block.h>
#include <stdio.h>
#include <string.h>
#include <sus/list.h>
#include <sus/mstring.h>
#include <vfs/ops.h>

#include <cassert>
#include <cstdint>

namespace TarFS {

using uint8_t = unsigned char;

static constexpr size_t BLOCK_SIZE = 512;

/**
 * @brief 判断pfx是否是str的前缀
 */
bool _prefix(const char *str, const char *pfx) {
    while (*pfx != 0) {
        if (*str != *pfx) {
            return false;
        }
        str++;
        pfx++;
    }
    return true;
}

template <typename T>
size_t parse_octal(const T &field) {
    // 使用模板来适配不同长度的 octal 字段
    size_t result = 0;
    for (size_t i = 0; i < sizeof(field); ++i) {
        char c = field[i];
        if (c == '\0') {
            break;
        }
        if (c < '0' || c > '7') {
            // 非法字符，返回当前结果
            break;
        }
        result = (result << 3) + (c - '0');
    }
    return result;
}

union TarBlock {
    struct ustar_header {
        char name[100];
        char mode[8];
        char uid[8];
        char gid[8];
        char size[12];
        char mtime[12];
        char checksum[8];
        char typeflag[1];
        char linkname[100];
        char magic[6];
        char version[2];
        char uname[32];
        char gname[32];
        char devmajor[8];
        char devminor[8];
        char prefix[155];
        char pad[12];
    } header;
    uint8_t raw[512];

    bool is_header() const {
        if (strcmp(this->header.magic, "ustar") != 0) {
            // 必须是 ustar 格式
            return false;
        }
        return true;
    }

    bool is_empty() const {
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            if (raw[i] != 0)
                return false;
        }
        return true;
    }

    unsigned int calc_checksum() const {
        TarBlock temp           = *this;
        temp.header.checksum[0] = ' ';
        temp.header.checksum[1] = ' ';
        temp.header.checksum[2] = ' ';
        temp.header.checksum[3] = ' ';
        temp.header.checksum[4] = ' ';
        temp.header.checksum[5] = ' ';
        temp.header.checksum[6] = ' ';
        temp.header.checksum[7] = ' ';

        unsigned int sum = 0;
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            sum += static_cast<unsigned int>(temp.raw[i]);
        }
        return sum;
    }
};

class TarFile : public IFile {
private:
    TarNode *node_;
    const uint8_t *ptr_;

public:
    TarFile(TarNode *node) : node_(node), ptr_(node_->data()) {}

    FSOptional<size_t> read(void *buf, size_t len) override {
        size_t file_size = parse_octal(node_->header_->header.size);
        size_t offset    = ptr_ - node_->data();
        if (offset >= file_size) {
            return 0;  // 已经读到文件末尾了，读取长度为0
        }
        size_t to_read = std::min(len, file_size - offset);
        memcpy(buf, ptr_, to_read);
        ptr_ += to_read;
        return to_read;
    }

    FSOptional<size_t> write(const void *buf, size_t len) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    // ! need review (AI code)
    FSOptional<off_t> seek(off_t offset, SeekWhence whence) override {
        // off_t 是有符号的
        size_t file_size = parse_octal(node_->header_->header.size);
        size_t new_offset;
        switch (whence) {
            case SeekWhence::SET:
                if (offset < 0) {
                    return FSErrCode::INVALID_PARAM;
                }
                new_offset = static_cast<size_t>(offset);
                break;
            case SeekWhence::CUR:
                if (offset < 0 &&
                    static_cast<size_t>(-offset) > ptr_ - node_->data())
                {
                    return FSErrCode::INVALID_PARAM;
                }
                new_offset = static_cast<size_t>(ptr_ - node_->data()) +
                             static_cast<size_t>(offset);
                break;
            case SeekWhence::END:
                if (offset > 0 || static_cast<size_t>(-offset) > file_size) {
                    return FSErrCode::INVALID_PARAM;
                }
                new_offset = file_size + static_cast<size_t>(offset);
                break;
            default: return FSErrCode::INVALID_PARAM;
        }
        if (new_offset > file_size) {
            return FSErrCode::INVALID_PARAM;
        }
        ptr_ = node_->data() + new_offset;
        return static_cast<off_t>(new_offset);
    }

    FSErrCode sync(void) override {
        return FSErrCode::NOT_SUPPORTED;
    }
};

class TarDirectory : public IDirectory {
private:
    TarNode *node_;

public:
    TarDirectory(TarNode *node) : node_(node) {}

    FSOptional<IDentry *> lookup(const char *name) override {
        util::string_builder path{node_->header_->header.name};
        util::string pfx = path.build();
        if (path[path.size() - 1] != '/')
            path.append('/');
        path.append(name);
        util::string full_path = path.build();

        for (auto p : node_->children_) {
            if (strcmp(p->header_->header.name, full_path.c_str()) == 0) {
                return p;
            }
        }

        IDentry *ret = nullptr;

        for (auto p = node_->data();;) {
            // 当 pfx 不再是 tar块 的前缀时，说明已经超出目录范围了
            const TarBlock *block = reinterpret_cast<const TarBlock *>(p);
            if (!_prefix(block->header.name, pfx.c_str()))
                break;
            if (!block->is_header())
                return FSErrCode::UNKNOWN_ERROR;

            node_->children_.emplace_back(p);
            if (full_path == block->header.name) {
                ret = node_->children_.back();
            }

            size_t file_size   = parse_octal(block->header.size);
            size_t file_block  = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            p                 += BLOCK_SIZE * (file_block + 1);
        }
        return FSErrCode::INVALID_PARAM;  // 未找到对应目录项
    }

    FSOptional<IDentry *> create(const char *name, bool is_dir) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSErrCode sync(void) override {
        return FSErrCode::NOT_SUPPORTED;
    }
};

class TarNode : public IINode, public IDentry {
private:
    // 理论上，多个 Dentry 可能指向同一个 INode
    // 但在 tarfs 中，每个 Dentry 都对应一个 INode
    // 这样可以简化生命期管理，增加缓存命中
    // 否则就得上引用计数了
    bool is_dir_;
    const TarBlock *header_;
    union {
        TarDirectory *dir;
        TarFile *file;
        void *raw;
    } view_{};
    util::ArrayList<TarNode *> children_{};

    const uint8_t *data() {
        return reinterpret_cast<const uint8_t *>(header_) + BLOCK_SIZE;
    }

public:
    TarNode(const uint8_t *header) {
        header_ = reinterpret_cast<const TarBlock *>(header);
        is_dir_ = header_->header.typeflag[0] == '5';
    }

    ~TarNode() noexcept {
        delete view_.raw;
    }

    // IINode

    FSOptional<IDirectory *> as_directory(void) override {
        if (!is_dir_)
            return FSErrCode::INVALID_PARAM;
        if (!view_.dir) {
            view_.dir = new TarDirectory{this};
        }
        return view_.dir;
    }

    FSOptional<IFile *> as_file(void) override {
        if (is_dir_)
            return FSErrCode::INVALID_PARAM;
        if (!view_.file) {
            view_.file = new TarFile{this};
        }
        return view_.file;
    }

    FSOptional<IMetadata *> metadata(void) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    // IDentry

    FSOptional<const char *> name(void) override {
        return header_->header.name;
    }

    FSErrCode remove(void) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSErrCode rename(const char *new_name) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSOptional<IINode *> inode(void) override {
        return dynamic_cast<IINode *>(this);
    }

    friend class TarFile;
    friend class TarDirectory;
};

class TarSuperblock : public ISuperblock {
private:
    const uint8_t *data_;  // 只读数据段
    const size_t size_;
    TarFSDriver *fs_;
    TarNode *root_{};

public:
    TarSuperblock(const uint8_t *data, size_t size, TarFSDriver *fs)
        : data_(data), size_(size), fs_(fs) {}

    ~TarSuperblock() override {
        delete[] data_;
        delete root_;
        // 触发全部析构
    }

    IFsDriver *fs() override {
        return fs_;
    }

    FSErrCode sync() override {
        return FSErrCode::NOT_SUPPORTED;
    }

    // tarfs 的根目录就是 tar 包中的第一个 header
    // ! unsafe 不能从外部 delete root_
    // 否则除了数据指针，tarfs 对象的所有指针都会失效
    FSOptional<IINode *> root() override {
        // 返回根目录 inode
        if (!root_) {
            root_ = new TarNode(data_);
        }
        return root_;
    }

    // FSOptional<IDentry *> root_d() {
    //     if (!root_) {
    //         root_ = new TarDentry(data_);
    //     }
    //     return root_;
    // }

    FSOptional<IMetadata *> metadata() override {
        return FSErrCode::NOT_SUPPORTED;
    }
};

// Like a SuperBlock factory
class TarFSDriver : public IFsDriver {
private:
    bool is_valid(size_t size_, const uint8_t *data_) {
        // 检查文件大小是否为 BLOCK_SIZE 的整数倍
        if (size_ % BLOCK_SIZE != 0)
            return false;

        // 检查 checksum
        for (size_t offset = 0; offset < size_;) {
            const TarBlock *block =
                reinterpret_cast<const TarBlock *>(data_ + offset);
            auto stored = parse_octal(block->header.checksum);
            if (stored == block->calc_checksum()) {
                size_t file_size   = parse_octal(block->header.size);
                size_t file_block  = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
                offset            += BLOCK_SIZE * (file_block + 1);
                // offset 跳到下一个 header 块
            } else {
                if (block->is_empty()) {
                    offset += BLOCK_SIZE;
                    // 允许有空块
                } else {
                    return false;
                }
            }
        }
        return true;
    }

public:
    const char *name() const override {
        return "tarfs";
    };

    FSErrCode probe(IBlockDevice *device, const char *options) override {
        size_t size        = device->block_sz() * device->block_cnt();
        uint8_t *data      = new uint8_t[size];
        size_t read_blocks = device->read_blocks(0, data, device->block_cnt());
        if (read_blocks != device->block_cnt()) {
            delete[] data;
            return FSErrCode::IO_ERROR;
        }
        if (is_valid(size, data)) {
            delete[] data;
            return FSErrCode::SUCCESS;
        } else {
            delete[] data;
            return FSErrCode::INVALID_PARAM;
        }
    }

    FSOptional<ISuperblock *> mount(IBlockDevice *device,
                                    const char *options) override {
        size_t size        = device->block_sz() * device->block_cnt();
        uint8_t *data      = new uint8_t[size];
        size_t read_blocks = device->read_blocks(0, data, device->block_cnt());
        if (read_blocks != device->block_cnt()) {
            delete[] data;
            return FSErrCode::IO_ERROR;
        }
        return new TarSuperblock(data, size, this);
    }

    FSErrCode unmount(ISuperblock *&sb) override {
        delete sb;
        sb = nullptr;
        return FSErrCode::SUCCESS;
    }
};

};  // namespace TarFS