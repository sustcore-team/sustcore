/**
 * @file page_types.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 页表权限公共类型
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>

enum class PageRWX : umb_t {
    P    = 0b000,
    R    = 0b001,
    W    = 0b010,
    X    = 0b100,
    RO   = R,
    RW   = R | W,
    RX   = R | X,
    RWX  = R | W | X,
    NONE = 0b000,
};

constexpr bool operator&(PageRWX lhs, PageRWX rhs) {
    return (static_cast<umb_t>(lhs) & static_cast<umb_t>(rhs)) != 0;
}

constexpr PageRWX operator|(PageRWX lhs, PageRWX rhs) {
    return static_cast<PageRWX>(static_cast<umb_t>(lhs) |
                                static_cast<umb_t>(rhs));
}

enum class PageModifier : umb_t {
    NONE = 0b000000,
    R    = 0b000001,
    W    = 0b000010,
    X    = 0b000100,
    U    = 0b001000,
    G    = 0b010000,
    P    = 0b100000,
    RWX  = R | W | X,
    ALL  = R | W | X | U | G | P,
};

constexpr bool operator&(PageModifier lhs, PageModifier rhs) {
    return (static_cast<umb_t>(lhs) & static_cast<umb_t>(rhs)) != 0;
}

constexpr PageModifier operator|(PageModifier lhs, PageModifier rhs) {
    return static_cast<PageModifier>(static_cast<umb_t>(lhs) |
                                     static_cast<umb_t>(rhs));
}

struct PageFlags {
    PageRWX rwx;
    bool u;
    bool g;
    bool p;
};

[[nodiscard]]
constexpr PageRWX page_rwx(bool r, bool w, bool x) {
    if (r && w && x)
        return PageRWX::RWX;
    else if (r && w)
        return PageRWX::RW;
    else if (r && x)
        return PageRWX::RX;
    else if (r)
        return PageRWX::RO;
    else
        return PageRWX::P;
}

[[nodiscard]]
constexpr bool page_is_readable(PageRWX rwx) {
    return rwx & PageRWX::R;
}

[[nodiscard]]
constexpr bool page_is_writable(PageRWX rwx) {
    return rwx & PageRWX::W;
}

[[nodiscard]]
constexpr bool page_is_executable(PageRWX rwx) {
    return rwx & PageRWX::X;
}

[[nodiscard]]
constexpr PageFlags make_page_flags(PageRWX rwx, bool u, bool g,
                                    bool p = true) {
    return PageFlags{
        .rwx = rwx,
        .u   = u,
        .g   = g,
        .p   = p,
    };
}

[[nodiscard]]
constexpr PageRWX without_write(PageRWX rwx) {
    switch (rwx) {
        case PageRWX::RW:  return PageRWX::RO;
        case PageRWX::RWX: return PageRWX::RX;
        default:           return rwx;
    }
}

[[nodiscard]]
constexpr PageModifier make_page_modifier(bool r, bool w, bool x, bool u,
                                          bool g, bool p) {
    PageModifier mask = PageModifier::NONE;
    if (r)
        mask = mask | PageModifier::R;
    if (w)
        mask = mask | PageModifier::W;
    if (x)
        mask = mask | PageModifier::X;
    if (u)
        mask = mask | PageModifier::U;
    if (g)
        mask = mask | PageModifier::G;
    if (p)
        mask = mask | PageModifier::P;
    return mask;
}

[[nodiscard]]
constexpr PageModifier make_page_modifier(b64 mask) {
    return make_page_modifier(mask & 0b000001, mask & 0b000010,
                              mask & 0b000100, mask & 0b001000,
                              mask & 0b010000, mask & 0b100000);
}
