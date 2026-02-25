/**
 * @file testobj.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试对象, 用于测试能力系统
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <object/testobj.h>

CapOptional<int> TestObjectOperation::read(void) const {
    using namespace perm::testobj;
    if (!imply<READ>()) {
        CAPABILITY::ERROR("权限不足");
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _obj->_read();
}

void TestObjectOperation::write(int v) {
    using namespace perm::testobj;
    if (!imply<WRITE>()) {
        CAPABILITY::ERROR("权限不足");
        return;
    }
    _obj->_write(v);
}

void TestObjectOperation::increase(void) {
    using namespace perm::testobj;
    if (!imply<INCREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return;
    }
    _obj->_increase();
}

void TestObjectOperation::decrease(void) {
    using namespace perm::testobj;
    if (!imply<DECREASE>()) {
        CAPABILITY::ERROR("权限不足");
        return;
    }
    _obj->_decrease();
}