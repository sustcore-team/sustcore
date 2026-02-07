/**
 * @file capcall.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief capability calls
 * @version alpha-1.0.0
 * @date 2026-02-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/capability.h>
#include <cap/capcall.h>

auto CSpaceCalls::_clone(Cap *src, CapHolder *owner, CapIdx idx) -> Cap * {
    // TODO: 将其添加到派生树中
    return new Cap(owner, idx, _payload(src), _perm(src));
}

auto CSpaceCalls::_migrate(Cap *src, CapHolder *owner, CapIdx idx) -> Cap * {
    // TODO: 将其替换到派生树中
    return new Cap(owner, idx, _payload(src), _perm(src));
}

auto CSpaceCalls::basic::payload(Cap *cap) -> CapOptional<CSpaceBase *> {
    return Base::basic::payload(cap);
}

auto CSpaceCalls::basic::clone(Cap *src, CapHolder *owner,
                               CapIdx idx) -> CapOptional<Cap *> {
    using namespace perm::basic;
    // 检查权限
    if (!imply<CLONE>(src)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _clone(src, owner, idx);
}

auto CSpaceCalls::basic::migrate(Cap *src, CapHolder *owner,
                                 CapIdx idx) -> CapOptional<Cap *> {
    using namespace perm::basic;
    if (!imply<MIGRATE>(src)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }
    return _migrate(src, owner, idx);
}

auto CSpaceCalls::basic::downgrade(Cap *src, const PermissionBits &new_perm)
    -> CapErrCode {
    if (!_perm(src).imply(new_perm)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }

    _perm(src) = new_perm;
    return CapErrCode::SUCCESS;
}

auto CSpaceCalls::basic::create(CSpaceBase *space, CapHolder *owner, CapIdx idx,
                                const PermissionBits &bits)
    -> CapOptional<Cap *> {
    return new Cap(owner, idx, space, bits);
}