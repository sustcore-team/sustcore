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

#include <cstddef>
#include <fs/tarfs.hpp>

namespace tarfs {

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

    FSOptional<IDentry *> TarDirectory::lookup(const char *name) {
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

        for (auto p = node_->header_ + 1;;) {
            // 当 pfx 不再是 tar块 的前缀时，说明已经超出目录范围了
            if (!prefix(p->header.name, pfx.c_str()))
                break;
            if (!p->is_header())
                return FSErrCode::UNKNOWN_ERROR;

            node_->children_.emplace_back(p);
            if (full_path == p->header.name) {
                ret = node_->children_.at(node_->children_.size() - 1);
            }

            size_t file_size   = parse_octal(p->header.size);
            size_t file_block  = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            p                 += file_block + 1;
        }

        if (ret == nullptr)
            return FSErrCode::INVALID_PARAM;  // 没有找到对应目录项
        else
            return ret;
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
        size_t size        = device->block_sz() * device->block_cnt();
        uint8_t *data      = new uint8_t[size];
        size_t read_blocks = device->read_blocks(0, data, device->block_cnt());
        if (read_blocks != device->block_cnt()) {
            delete[] data;
            return FSErrCode::IO_ERROR;
        }
        return new TarSuperblock(data, size, this);
    }

};  // namespace tarfs