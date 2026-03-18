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

CapOptional<int> IntOp::read(void) const {
    using namespace perm::intobj;
    if (!imply<READ>()) {
        CAPABILITY::ERROR("权限不足");
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _obj->_read();
}

void IntOp::write(int v) {
    using namespace perm::intobj;
    if (!imply<WRITE>()) {
        CAPABILITY::ERROR("权限不足");
        return;
    }
    _obj->_write(v);
}

void IntOp::increase(void) {
    using namespace perm::intobj;
    if (!imply<INCREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return;
    }
    _obj->_increase();
}

void IntOp::decrease(void) {
    using namespace perm::intobj;
    if (!imply<DECREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return;
    }
    _obj->_decrease();
}

CapOptional<int> SIntOp::read(void) const {
    using namespace perm::sintobj;
    if (!imply<READ>()) {
        CAPABILITY::ERROR("权限不足");
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _obj->_read();
}

void SIntOp::write(int v) {
    using namespace perm::sintobj;
    if (!imply<WRITE>()) {
        CAPABILITY::ERROR("权限不足");
        return;
    }
    _obj->_write(v);
}

void SIntOp::increase(void) {
    using namespace perm::sintobj;
    if (!imply<INCREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return;
    }
    _obj->_increase();
}

void SIntOp::decrease(void) {
    using namespace perm::sintobj;
    if (!imply<DECREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return;
    }
    _obj->_decrease();
}