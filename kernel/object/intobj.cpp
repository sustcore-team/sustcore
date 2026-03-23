/**
 * @file intobj.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试对象, 用于测试能力系统
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/capability.h>
#include <object/csa.h>
#include <object/intobj.h>
#include <sustcore/errcode.h>

Result<int> IntOp::read() const {
    using namespace perm::intobj;
    if (!imply<READ>()) {
        CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    return _obj->_read();
}

Result<void> IntOp::write(int v) {
    using namespace perm::intobj;
    if (!imply<WRITE>()) {
        CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_write(v);
    return {};
}

Result<void> IntOp::increase() {
    using namespace perm::intobj;
    if (!imply<INCREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_increase();
    return {};
}

Result<void> IntOp::decrease() {
    using namespace perm::intobj;
    if (!imply<DECREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_decrease();
    return {};
}

Result<int> SIntOp::read() const {
    using namespace perm::sintobj;
    if (!imply<READ>()) {
        CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    return _obj->_read();
}

Result<void> SIntOp::write(int v) {
    using namespace perm::sintobj;
    if (!imply<WRITE>()) {
        CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_write(v);
    return {};
}

Result<void> SIntOp::increase() {
    using namespace perm::sintobj;
    if (!imply<INCREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_increase();
    return {};
}

Result<void> SIntOp::decrease() {
    using namespace perm::sintobj;
    if (!imply<DECREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_decrease();
    return {};
}