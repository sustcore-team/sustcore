/**
 * @file capability.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力系统
 * @version alpha-1.0.0
 * @date 2026-02-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cap/capability.h>

Capability::Capability(CapType type, PCB *owner, CapIdx idx, BasicPermission perm)
    : _type(type), _owner(owner), _idx(idx), _permissions()
{
    _permissions.push_back(new BasicPermission(perm));
}

Capability::~Capability() {
    // 将自身从继承树中剥离

    // 遍历继承树, 通知其派生能力销毁自身

    for (auto perm : _permissions) {
        delete perm;
    }
}