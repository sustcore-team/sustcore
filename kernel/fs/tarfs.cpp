/**
 * @file tarfs.cpp
 * @author hyj0824 (12510430@mail.sustech.edu.cn)
 * @brief tarfs 相关实现
 * @version alpha-1.0.0
 * @date 2026-02-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/block.h>
#include <fs/tarfs.h>
#include <mem/alloc.h>
#include <sus/defer.h>
#include <sus/mstring.h>

#include <cstddef>

namespace tarfs {
    namespace koa {
        static util::Defer<KOA<TarFile>> FILE;
        AutoDeferPost(FILE);

        static util::Defer<KOA<TarDirectory>> DIRECTORY;
        AutoDeferPost(DIRECTORY);

        static util::Defer<KOA<TarNode>> NODE;
        AutoDeferPost(NODE);
    }  // namespace koa

    TarFile::TarFile(TarNode *node) {
        node_ = node;
        data_ = reinterpret_cast<const uint8_t *>(node->header_) + BLOCK_SIZE;
        size_t size = parse_octal(node->header_->header.size);
        end_        = data_ + size;
        ptr_        = data_;
    }

    FSOptional<size_t> TarFile::read(void *buf, size_t len) {
        if (ptr_ == end_) {
            return 0;  // 已经读到文件末尾了，读取长度为0
        }
        size_t to_read = std::min(len, static_cast<size_t>(end_ - ptr_));
        memcpy(buf, ptr_, to_read);
        ptr_ += to_read;
        return to_read;
    }

    FSOptional<off_t> TarFile::seek(off_t offset, SeekWhence whence) {
        // off_t 是有符号的
        auto new_ptr = ptr_;
        switch (whence) {
            case SeekWhence::SET: new_ptr = data_ + offset; break;
            case SeekWhence::CUR: new_ptr = ptr_ + offset; break;
            case SeekWhence::END: new_ptr = end_ + offset; break;
            default:              return FSErrCode::INVALID_PARAM;
        }
        if (new_ptr < data_ || new_ptr > end_) {
            return FSErrCode::INVALID_PARAM;
        }
        ptr_ = new_ptr;
        return static_cast<off_t>(new_ptr - data_);
    }

    void *TarFile::operator new(size_t sz) noexcept {
        assert(sz == sizeof(TarFile));
        return koa::FILE->alloc();
    }

    void TarFile::operator delete(void *ptr) noexcept {
        koa::FILE->free(static_cast<TarFile *>(ptr));
    }

    FSOptional<IDentry *> TarDirectory::lookup(const char *name) {
        if (*name == '\0')
            return node_;  // 约定空字符串表示目录自身

        for (auto p : node_->children_) {
            if (p->name_ == name) {
                return p;
            }
        }

        // 缓存中没有，则遍历寻找
        IDentry *ret = nullptr;
        util::string_builder sb(sizeof(TarBlock::header.name));
        sb.append(node_->header_->header.name);
        sb.append(name);
        util::string path = sb.build();

        for (auto p = node_->header_ + 1;;) {
            if (!p->is_header())
                break;

            // 末尾相差一个 '/' 也算匹配，只有 b 可能以 '/' 结尾，也比 a 长
            auto is_same = [](const char *a, const char *b) {
                while (*a != '\0') {
                    if (*a != *b) {
                        return false;
                    }
                    a++;
                    b++;
                }
                return *b == '/' || *b == '\0';
            };

            if (is_same(path.c_str(), p->header.name)) {
                ret = new TarNode(p);
                node_->children_.push_back(static_cast<TarNode *>(ret));
                break;
            }

            size_t file_size   = parse_octal(p->header.size);
            size_t file_block  = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            p                 += file_block + 1;
        }

        if (ret == nullptr)
            return FSErrCode::INVALID_PARAM;  // 没有找到对应目录项
        else
            return ret;

        return FSErrCode::ENTRY_NOT_FOUND;
    }

    void *TarDirectory::operator new(size_t sz) noexcept {
        assert(sz == sizeof(TarDirectory));
        return koa::DIRECTORY->alloc();
    }

    void TarDirectory::operator delete(void *ptr) noexcept {
        koa::DIRECTORY->free(static_cast<TarDirectory *>(ptr));
    }

    void *TarNode::operator new(size_t sz) noexcept {
        assert(sz == sizeof(TarNode));
        return koa::NODE->alloc();
    }

    void TarNode::operator delete(void *ptr) noexcept {
        koa::NODE->free(static_cast<TarNode *>(ptr));
    }

    bool TarFSDriver::is_valid(size_t size_, const uint8_t *data_) {
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

    FSErrCode TarFSDriver::probe(IBlockDevice *device, const char *options) {
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

    // 注意，没有做检验。需要确保 probe 过是 Tarfs
    FSOptional<ISuperblock *> TarFSDriver::mount(IBlockDevice *device,
                                                 const char *options) {
        size_t size   = device->block_sz() * device->block_cnt();
        uint8_t *data = nullptr;
        if (device->is<RamDiskDevice>()) {
            // 直接使用其内存作为数据源，减少复制
            data = static_cast<uint8_t *>(device->as<RamDiskDevice>()->base());
        } else {
            data          = new uint8_t[size];
            size_t blocks = device->read_blocks(0, data, device->block_cnt());
            if (blocks != device->block_cnt()) {
                delete[] data;
                return FSErrCode::IO_ERROR;
            }
        }
        return new TarSuperblock(data, size, this);
    }

};  // namespace tarfs
