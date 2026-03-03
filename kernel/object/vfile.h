/**
 * @file vfile.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Virtual File
 * @version alpha-1.0.0
 * @date 2026-03-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <kio.h>
#include <perm/vfile.h>
#include <sustcore/capability.h>
#include <sustcore/errcode.h>
#include <vfs/ops.h>

class VFileOperation;

class VFile : public _PayloadHelper<VFile> {
public:
    static constexpr PayloadType IDENTIFIER = PayloadType::VFILE;
    friend class VFileOperation;
    using Operation = VFileOperation;

private:
    IFile *_ifile;
    IFsDriver *_fs;
    ISuperblock *_sb;

    off_t _offset = 0;  // 当前文件偏移
public:
    constexpr VFile(IFile *ifile, IFsDriver *fs, ISuperblock *sb)
        : _ifile(ifile), _fs(fs), _sb(sb) {}
    ~VFile() {
        delete _ifile;
    }

    VFile(const VFile &)            = delete;
    VFile &operator=(const VFile &) = delete;
    VFile(VFile &&)                 = delete;
    VFile &operator=(VFile &&)      = delete;

    inline void *operator new(size_t size) noexcept {
        assert(size == sizeof(VFile));
        return KOP<VFile>::instance().alloc();
    }
    inline void operator delete(void *ptr) noexcept {
        KOP<VFile>::instance().free(static_cast<VFile *>(ptr));
    }

protected:
    FSOptional<size_t> read(void *buffer, size_t size);
    FSOptional<size_t> write(const void *buffer, size_t size);
    FSErrCode seek(off_t offset, SeekWhence whence);
    FSOptional<size_t> size();
    FSErrCode flush();
};

class VFileOperation {
private:
    Capability *_cap;
    VFile *_vfile;

    template <b64 perm>
    [[nodiscard]]
    bool imply() const {
        return _cap->perm().basic_imply(perm);
    }

public:
    constexpr VFileOperation(Capability *cap)
        : _cap(cap), _vfile(cap->payload<VFile>()) {}
    ~VFileOperation() = default;

    VFileOperation(const VFileOperation &)            = default;
    VFileOperation &operator=(const VFileOperation &) = default;
    VFileOperation(VFileOperation &&)                 = default;
    VFileOperation &operator=(VFileOperation &&)      = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    ErrOptional<size_t> read(void *buffer, size_t size);
    ErrOptional<size_t> write(const void *buffer, size_t size);
    ErrCode seek(off_t offset, SeekWhence whence);
    ErrOptional<size_t> size();
    ErrCode flush();
};