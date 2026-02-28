/**
 * @file cholder.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力持有者
 * @version alpha-1.0.0
 * @date 2026-02-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <cap/cspace.h>
#include <object/csa.h>
#include <sustcore/capability.h>

static util::Defer<util::IDManager<>> CHOLDER_ID;
AutoDeferPost(CHOLDER_ID);

CHolder::CHolder(void)
    : _space(this),
      _recv_space(this),
      _csa_idx(0, 0),
      cholder_id(CHOLDER_ID->get()) {
    CapErrCode err = _space.create<CSpaceAccessor>(_csa_idx, &_space);
    assert(err == CapErrCode::SUCCESS);
}

CHolder::~CHolder() {}

CapOptional<Capability *> CHolder::access(CapIdx idx) {
    if (idx.type == SpaceType::MAJOR) {
        return _space.get(idx);
    }

    if (idx.type == SpaceType::RECV) {
        return _recv_space.get(idx);
    }

    return CapErrCode::TYPE_NOT_MATCHED;
}