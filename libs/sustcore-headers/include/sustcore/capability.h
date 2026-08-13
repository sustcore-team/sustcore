/**
 * @file capability.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Sustcore 内核与用户程序共享的 Capability ABI、布局常量和 token 编解码。
 * @version 0.1.0-dev.1
 * @date 2026-08-04
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/algobase.h>
#include <tay/bits.h>

#include <cstddef>

namespace cap {

    /** @brief 内核对象的稳定 ABI 类型编号；已有编号不得重排或复用。 */
    enum class ObjectType : u16_t {
        NONE          = 0,
        INTEGER       = 1,
        PROCESS       = 2,
        THREAD        = 3,
        ADDRESS_SPACE = 4,
        CSPACE        = 5,
        ENDPOINT      = 6,
        REPLY         = 7,
        NOTIFICATION  = 8,
        FRAME         = 9,
        MEMORY        = 10,
        PAGER         = 11,
        IRQ           = 12,
        IO_PORT       = 13,
        BLOCK_DEVICE  = 14,
        VNODE         = 15,
        OPEN_FILE     = 16,
    };

    /** @brief Capability 的通用生命周期与传递权限。 */
    enum class BasicRights : u64_t {
        NONE      = 0,
        READ      = 1ull << 0,
        WRITE     = 1ull << 1,
        GRANT     = 1ull << 2,
        MANAGE    = 1ull << 3,
        COPY      = 1ull << 4,
        MINT      = 1ull << 5,
        MOVE      = 1ull << 6,
        MOVE_ONCE = 1ull << 7,
        REVOKE    = 1ull << 8,
        INSPECT   = 1ull << 9,
    };

    [[nodiscard]] constexpr BasicRights operator|(BasicRights lhs, BasicRights rhs) noexcept {
        return static_cast<BasicRights>(static_cast<u64_t>(lhs) | static_cast<u64_t>(rhs));
    }

    [[nodiscard]] constexpr BasicRights operator&(BasicRights lhs, BasicRights rhs) noexcept {
        return static_cast<BasicRights>(static_cast<u64_t>(lhs) & static_cast<u64_t>(rhs));
    }

    constexpr BasicRights &operator|=(BasicRights &lhs, BasicRights rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr u64_t rights_value(BasicRights rights) noexcept {
        return static_cast<u64_t>(rights);
    }

    /** @brief 兼容旧代码的无作用域权限常量。 */
    enum Rights : u64_t {
        RIGHT_NONE      = rights_value(BasicRights::NONE),
        RIGHT_COPY      = rights_value(BasicRights::COPY),
        RIGHT_MINT      = rights_value(BasicRights::MINT),
        RIGHT_MOVE      = rights_value(BasicRights::MOVE),
        RIGHT_MOVE_ONCE = rights_value(BasicRights::MOVE_ONCE),
        RIGHT_GRANT     = rights_value(BasicRights::GRANT),
        RIGHT_REVOKE    = rights_value(BasicRights::REVOKE),
        RIGHT_MANAGE    = rights_value(BasicRights::MANAGE),
        RIGHT_INSPECT   = rights_value(BasicRights::INSPECT),
        RIGHT_READ      = rights_value(BasicRights::READ),
        RIGHT_WRITE     = rights_value(BasicRights::WRITE),
    };

    /** @name Capability 与 CNode 共享布局常量
     *  这些值同时约束内核存储和用户态 ABI 描述。
     * @{ */
    inline constexpr size_t CAPABILITY_SIZE      = 64;
    inline constexpr size_t CAPABILITY_ALIGNMENT = CAPABILITY_SIZE;
    inline constexpr size_t CNODE_CELL_SIZE      = CAPABILITY_SIZE;
    inline constexpr size_t CNODE_CELL_ALIGNMENT = CNODE_CELL_SIZE;
    inline constexpr size_t CNODE_METADATA_SIZE  = CNODE_CELL_SIZE;

    inline constexpr size_t CNODE_PAGE_SIZE          = 4096;
    inline constexpr size_t CNODE_MIN_PAGE_COUNT     = 1;
    inline constexpr size_t CNODE_DEFAULT_PAGE_COUNT = 2;
    inline constexpr size_t CNODE_MAX_PAGE_COUNT     = 32;

    inline constexpr size_t SMALL_CNODE_SIZE     = 512;
    inline constexpr size_t SMALL_CNODE_CAPACITY = SMALL_CNODE_SIZE / CNODE_CELL_SIZE - 1;

    inline constexpr size_t MAX_CNODES     = 256;
    inline constexpr u8_t ROOT_CNODE_INDEX = 0;
    /** @} */

    /**
     * @brief 判断页数能否表示一个普通 CNode。
     * @param page_count CNode 占用的页数。
     * @return 页数为允许范围内的二次幂时返回 true。
     */
    [[nodiscard]] constexpr bool is_supported_cnode_page_count(size_t page_count) noexcept {
        return tay::is_power_of_two(page_count) && page_count >= CNODE_MIN_PAGE_COUNT &&
               page_count <= CNODE_MAX_PAGE_COUNT;
    }

    /**
     * @brief 计算指定页数下除 metadata cell 外的 Capability 容量。
     * @return 可用于 Capability 的 slot 数量。
     * @pre `page_count` 已通过 `is_supported_cnode_page_count()` 校验。
     */
    [[nodiscard]] constexpr size_t cnode_capacity_for_pages(size_t page_count) noexcept {
        return page_count * (CNODE_PAGE_SIZE / CNODE_CELL_SIZE) - 1;
    }

    /** @brief 判断容量是否对应 SmallCNode 或一种受支持的整页 CNode。 */
    [[nodiscard]] constexpr bool is_supported_cnode_capacity(size_t capacity) noexcept {
        if (capacity == SMALL_CNODE_CAPACITY)
            return true;
        const auto cell_count = capacity + 1;
        if (cell_count % (CNODE_PAGE_SIZE / CNODE_CELL_SIZE) != 0)
            return false;
        return is_supported_cnode_page_count(cell_count * CNODE_CELL_SIZE / CNODE_PAGE_SIZE);
    }

    inline constexpr size_t CNODE_1_PAGE_CAPACITY  = cnode_capacity_for_pages(1);
    inline constexpr size_t CNODE_2_PAGE_CAPACITY  = cnode_capacity_for_pages(2);
    inline constexpr size_t CNODE_4_PAGE_CAPACITY  = cnode_capacity_for_pages(4);
    inline constexpr size_t CNODE_8_PAGE_CAPACITY  = cnode_capacity_for_pages(8);
    inline constexpr size_t CNODE_16_PAGE_CAPACITY = cnode_capacity_for_pages(16);
    inline constexpr size_t CNODE_32_PAGE_CAPACITY = cnode_capacity_for_pages(32);
    inline constexpr size_t CNODE_MAX_CAPACITY     = CNODE_32_PAGE_CAPACITY;

    /** @name CapToken 位域布局
     * @{ */
    inline constexpr u8_t CAP_TOKEN_SLOT_BITS       = 16;
    inline constexpr u8_t CAP_TOKEN_CNODE_BITS      = 8;
    inline constexpr u8_t CAP_TOKEN_GENERATION_BITS = 24;
    inline constexpr u8_t CAP_TOKEN_COOKIE_BITS     = 16;
    inline constexpr size_t CAP_TOKEN_SIZE          = sizeof(u64_t);
    inline constexpr size_t CAP_TOKEN_BITS          = CAP_TOKEN_SIZE * 8;

    inline constexpr u8_t CAP_TOKEN_SLOT_SHIFT       = 0;
    inline constexpr u8_t CAP_TOKEN_CNODE_SHIFT      = CAP_TOKEN_SLOT_SHIFT + CAP_TOKEN_SLOT_BITS;
    inline constexpr u8_t CAP_TOKEN_GENERATION_SHIFT = CAP_TOKEN_CNODE_SHIFT + CAP_TOKEN_CNODE_BITS;
    inline constexpr u8_t CAP_TOKEN_COOKIE_SHIFT =
        CAP_TOKEN_GENERATION_SHIFT + CAP_TOKEN_GENERATION_BITS;

    /** @brief 生成 CapToken 单个位域使用的低位掩码。 */
    [[nodiscard]] constexpr u64_t cap_token_mask(u8_t bits) noexcept {
        return (u64_t{1} << bits) - 1;
    }

    inline constexpr u64_t CAP_TOKEN_SLOT_MASK       = cap_token_mask(CAP_TOKEN_SLOT_BITS);
    inline constexpr u64_t CAP_TOKEN_CNODE_MASK      = cap_token_mask(CAP_TOKEN_CNODE_BITS);
    inline constexpr u64_t CAP_TOKEN_GENERATION_MASK = cap_token_mask(CAP_TOKEN_GENERATION_BITS);
    inline constexpr u64_t CAP_TOKEN_COOKIE_MASK     = cap_token_mask(CAP_TOKEN_COOKIE_BITS);
    inline constexpr u32_t CAP_TOKEN_MAX_GENERATION = static_cast<u32_t>(CAP_TOKEN_GENERATION_MASK);
    /** @} */

    static_assert(CAP_TOKEN_SLOT_BITS + CAP_TOKEN_CNODE_BITS + CAP_TOKEN_GENERATION_BITS +
                      CAP_TOKEN_COOKIE_BITS ==
                  CAP_TOKEN_BITS);
    static_assert(SMALL_CNODE_SIZE == (SMALL_CNODE_CAPACITY + 1) * CNODE_CELL_SIZE);
    static_assert(CNODE_PAGE_SIZE % CNODE_CELL_SIZE == 0);

    /**
     * @brief 用户可见的固定宽度 Capability 凭证。
     * @note token 只在所属 CSpace 中有意义，不能据此跨进程查找对象。
     */
    struct CapToken {
        u64_t raw                                                          = 0;
        constexpr auto operator==(const CapToken &) const noexcept -> bool = default;
    };

    static_assert(sizeof(CapToken) == CAP_TOKEN_SIZE);

    /** @brief `CapToken` 解码后的字段和基础结构有效性。 */
    struct CapTokenFields {
        u16_t cspace_cookie = 0;
        u32_t generation    = 0;
        u8_t cnode_index    = 0;
        u16_t slot_index    = 0;
        bool valid          = false;
    };

    /**
     * @brief 将 CSpace cookie、generation、CNode index 和 slot index 编码为 token。
     * @note 本函数只负责截取和编码位域；调用者必须保证 generation 与 slot 均非零。
     */
    [[nodiscard]] constexpr CapToken encode_token(u16_t cookie, u32_t generation, u8_t cnode_index,
                                                  u16_t slot_index) noexcept {
        return CapToken{
            ((static_cast<u64_t>(cookie) & CAP_TOKEN_COOKIE_MASK) << CAP_TOKEN_COOKIE_SHIFT) |
            ((static_cast<u64_t>(generation) & CAP_TOKEN_GENERATION_MASK)
             << CAP_TOKEN_GENERATION_SHIFT) |
            ((static_cast<u64_t>(cnode_index) & CAP_TOKEN_CNODE_MASK) << CAP_TOKEN_CNODE_SHIFT) |
            ((static_cast<u64_t>(slot_index) & CAP_TOKEN_SLOT_MASK) << CAP_TOKEN_SLOT_SHIFT)};
    }

    /**
     * @brief 解码 token，并完成与对象状态无关的基础有效性检查。
     * @return `valid` 仅表示 raw、generation 和 slot 非零；cookie、范围、类型与权限仍须由内核校验。
     */
    [[nodiscard]] constexpr CapTokenFields decode_token(CapToken token) noexcept {
        const auto generation = static_cast<u32_t>((token.raw >> CAP_TOKEN_GENERATION_SHIFT) &
                                                   CAP_TOKEN_GENERATION_MASK);
        const auto slot =
            static_cast<u16_t>((token.raw >> CAP_TOKEN_SLOT_SHIFT) & CAP_TOKEN_SLOT_MASK);
        return CapTokenFields{
            .cspace_cookie =
                static_cast<u16_t>((token.raw >> CAP_TOKEN_COOKIE_SHIFT) & CAP_TOKEN_COOKIE_MASK),
            .generation = generation,
            .cnode_index =
                static_cast<u8_t>((token.raw >> CAP_TOKEN_CNODE_SHIFT) & CAP_TOKEN_CNODE_MASK),
            .slot_index = slot,
            .valid      = token.raw != 0 && generation != 0 && slot != 0,
        };
    }

}  // namespace cap
