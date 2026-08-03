/**
 * @file container_of.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供 C++ 对象成员反查所属对象的 container_of 工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/bits.h>

namespace tay {
    template <typename T, typename C>
    C *container_of(T *p, T C::*member_ptr) {
        static_assert(sizeof(T C::*) == sizeof(addr_t), "Broken ABI");

        addr_t offset;
        memcpy(&offset, &member_ptr, sizeof(addr_t));
        auto r = reinterpret_cast<char *>(p);
        return reinterpret_cast<C *>(r - offset);
    }
}  // namespace tay