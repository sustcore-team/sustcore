/**
 * @file vfile.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Virtual File Object
 * @version alpha-1.0.0
 * @date 2026-04-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <env.h>
#include <object/vfile.h>
#include <perm/vfile.h>

Result<size_t> VFileOperator::read(off_t offset, void *buf, size_t len) {
    using namespace perm::vfile;
    if (!imply<READ>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    // 调用VFS的read接口
    return env::inst().vfs()->read(_vind, offset, buf, len);
}

Result<size_t> VFileOperator::write(off_t offset, const void *buf, size_t len) {
    using namespace perm::vfile;
    if (!imply<WRITE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    // 调用VFS的write接口
    return env::inst().vfs()->write(_vind, offset, buf, len);
}

Result<size_t> VFileOperator::size() {
    using namespace perm::vfile;
    if (!imply<READ>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    // 调用VFS的size接口
    return env::inst().vfs()->size(_vind);
}

Result<void> VFileOperator::sync() {
    using namespace perm::vfile;
    if (!imply<WRITE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    // 调用VFS的sync接口
    return env::inst().vfs()->sync(_vind);
}
