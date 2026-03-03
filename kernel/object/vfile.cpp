/**
 * @file vfile.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief vfile
 * @version alpha-1.0.0
 * @date 2026-03-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <object/vfile.h>
#include <sustcore/capability.h>
#include <sustcore/errcode.h>
#include <vfs/ops.h>

FSOptional<size_t> VFile::read(void* buffer, size_t size) {
    const off_t _off = _offset;
    auto read_opt    = _ifile->read(_off, buffer, size);
    read_opt.if_present([this](auto sz) -> void { this->_offset += sz; });
    return read_opt;
}

FSOptional<size_t> VFile::write(const void* buffer, size_t size) {
    const off_t _off = _offset;
    auto write_opt   = _ifile->write(_off, buffer, size);
    write_opt.if_present([this](auto sz) -> void { this->_offset += sz; });
    return write_opt;
}

FSErrCode VFile::seek(off_t offset, SeekWhence whence) {
    off_t new_offset = -1;
    auto size_opt    = _ifile->size();
    if (!size_opt.present()) {
        return size_opt.error();
    }
    const size_t filesz = size_opt.value();
    switch (whence) {
        case SeekWhence::SET: new_offset = offset; break;
        case SeekWhence::CUR: new_offset = _offset + offset; break;
        case SeekWhence::END: {
            new_offset = (off_t)filesz + offset;
            break;
        }
        default: return FSErrCode::INVALID_PARAM;
    }
    if (new_offset < 0 || new_offset > (off_t)filesz) {
        return FSErrCode::INVALID_PARAM;
    }
    _offset = new_offset;
    return FSErrCode::SUCCESS;
}

FSOptional<size_t> VFile::size() {
    return _ifile->size();
}

FSErrCode VFile::flush() {
    return _ifile->sync();
}

ErrOptional<size_t> VFileOperation::read(void* buffer, size_t size) {
    using namespace perm::vfile;
    if (!imply<READ>()) {
        return ErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _vfile->read(buffer, size);
}

ErrOptional<size_t> VFileOperation::write(const void* buffer, size_t size) {
    using namespace perm::vfile;
    if (!imply<WRITE>()) {
        return ErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _vfile->write(buffer, size);
}

ErrCode VFileOperation::seek(off_t offset, SeekWhence whence) {
    using namespace perm::vfile;
    if (!imply<SEEK>()) {
        return ErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _vfile->seek(offset, whence);
}

ErrOptional<size_t> VFileOperation::size() {
    using namespace perm::vfile;
    if (!imply<SIZE>()) {
        return ErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _vfile->size();
}

ErrCode VFileOperation::flush() {
    using namespace perm::vfile;
    if (!imply<FLUSH>()) {
        return ErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _vfile->flush();
}