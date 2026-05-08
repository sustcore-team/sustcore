/**
 * @file vfile.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief virtual file object
 * @version alpha-1.0.0
 * @date 2026-04-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <vfs/vfs.h>

class VFileOperator {
protected:
    cap::Capability *_cap;
    VFile *_file;
    VINode *_vind;
    template <b64 perm>
    bool imply() const {
        return _cap->perm().basic_imply(perm);
    }

public:
    explicit VFileOperator(cap::Capability *cap)
        : _cap(cap), _file(cap->payload_as<VFile>()), _vind(nullptr) {
        assert(_file != nullptr);
        _vind = _file->vind();
        assert(_vind != nullptr);
    }
    ~VFileOperator() = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    Result<size_t> read(off_t offset, void *buf, size_t len);
    Result<size_t> write(off_t offset, const void *buf, size_t len);
    Result<size_t> size();
    Result<void> sync();

    /**
     * @brief 读取一定长度的数据到缓冲区
     * 
     * @param offset 偏移
     * @param buf 缓冲区
     * @param len 需要读取的字节数
     * @return Result<size_t> 实际读取的字节数. 当实际读取的字节数不等于需要读取的字节数时, 将会返回一个错误码
     */
    Result<size_t> read_exact(off_t offset, void *buf, size_t len) {
        auto read_res = read(offset, buf, len);
        if (!read_res.has_value()) {
            propagate_return(read_res);
        }
        size_t got = read_res.value();
        if (got != len) {
            unexpect_return(ErrCode::IO_ERROR);
        }
        return got;
    }
};
