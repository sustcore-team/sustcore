/**
 * @file tarfs.hpp
 * @author hyj0824 (12510430@mail.sustech.edu.cn)
 * @brief Tar Based initramfs
 * @version alpha-1.0.0
 * @date 2026-02-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <device/block.h>
#include <stdio.h>
#include <string.h>
#include <vfs/ops.h>

#include <cassert>
#include <cstdint>

using uint8_t = unsigned char;

static constexpr size_t BLOCK_SIZE = 512;

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
public:
    FSOptional<size_t> read(void *buf, size_t len) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSOptional<size_t> write(const void *buf, size_t len) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSOptional<off_t> seek(off_t offset, SeekWhence whence) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSErrCode sync(void) override {
        return FSErrCode::NOT_SUPPORTED;
    }
};

class TarDirectory : public IDirectory {
public:
    FSOptional<IDentry *> lookup(const char *name) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSOptional<IDentry *> create(const char *name, bool is_dir) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSErrCode sync(void) override {
        return FSErrCode::NOT_SUPPORTED;
    }
};

class TarDentry : public IDentry {
private:
    TarINode *inode_;

public:
    FSOptional<const char *> name(void) override {
        return inode_->header_->header.name;
    }

    FSErrCode remove(void) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSErrCode rename(const char *new_name) override {
        return FSErrCode::NOT_SUPPORTED;
    }

    FSOptional<IINode *> inode(void) override {
        return inode_;
    }
};

// INode 就是 header
class TarINode : public IINode {
private:
    const TarBlock *header_;
    const uint8_t *data_;  // 指向 header 之后的数据
    union {
        TarDirectory *dir;
        TarFile *file;
    } view_{};

public:
    TarINode(const uint8_t *header, const uint8_t *data) {
        header_ = reinterpret_cast<const TarBlock *>(header);
        data_   = data;
    }

    FSOptional<IDirectory *> as_directory(void) override {
        if (header_->header.typeflag[0] != '5') {
            return FSErrCode::INVALID_PARAM;  // 不是目录
        }
        if (!view_.dir) {
            view_.dir = new TarDirectory();
        }
        return view_.dir;
    }

    FSOptional<IFile *> as_file(void) override {
        if (header_->header.typeflag[0] != '0' &&
            header_->header.typeflag[0] != '\0')
        {
            return FSErrCode::INVALID_PARAM;  // 不是普通文件
        }
        if (!view_.file) {
            view_.file = new TarFile();
        }
        return view_.file;
    }

    FSOptional<IMetadata *> metadata(void) override {
        return FSErrCode::NOT_SUPPORTED;
    }
    friend class TarDentry;
};

class TarSuperblock : public ISuperblock {
private:
    const uint8_t *data_;  // 只读数据
    size_t size_;
    TarFSDriver *fs_;
    TarINode *root_{};

public:
    TarSuperblock(const uint8_t *data, size_t size, TarFSDriver *fs)
        : data_(data), size_(size), fs_(fs) {}

    ~TarSuperblock() override {
        delete[] data_;
        delete root_;
    }

    IFsDriver *fs() override {
        return fs_;
    }

    FSErrCode sync() override {
        return FSErrCode::NOT_SUPPORTED;
    }

    // tarfs 的根目录就是 tar 包中的第一个 header
    // ! unsafe 不能从外部 delete INode
    FSOptional<IINode *> root() override {
        // 返回根目录 inode
        if (!root_) {
            root_ = new TarINode(data_, data_ + BLOCK_SIZE);
        }
        return root_;
    }

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
                    // 允许文件有空块
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