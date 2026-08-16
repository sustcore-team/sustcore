/**
 * @file error.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Capability 与 CSpace 的细粒度错误。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <error.h>
#include <sustcore/capability.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <type_traits>
#include <utility>

namespace cap {
    /** @brief Capability 操作可以恢复或向上层报告的结构化错误。 */
    class CapError final {
    public:
        using kernel_domain_error_tag = void;

        struct InvalidToken {
            CapToken token{};
        };
        struct MissingCNode {
            CapToken token{};
            u8_t cnode_index = 0;
        };
        struct InvalidSlot {
            CapToken token{};
            u16_t slot_index = 0;
        };
        struct StaleToken {
            CapToken token{};
            u32_t observed_generation = 0;
        };
        struct TypeMismatch {
            CapToken token{};
            ObjectType expected = ObjectType::NONE;
            ObjectType actual   = ObjectType::NONE;
        };
        struct InsufficientRights {
            CapToken token{};
            u64_t required  = 0;
            u64_t available = 0;
        };
        struct NoSlots {
            u8_t cnode_index = 0;
        };
        struct Busy {
            u8_t cnode_index = 0;
        };
        struct OutOfMemory {};

        enum class Operation : u8_t {
            INVALID_CNODE_SIZE,
            COOKIE_EXHAUSTED,
            GENERATION_EXHAUSTED,
            CNODE_DIRECTORY_FULL,
            ROOT_DETACH,
            NODE_OWNED,
            NODE_DESTROYING,
            OBJECT_RETIRING,
            PIN_FAILED,
            CDT_INCONSISTENT,
        };

        struct OperationRejected {
            Operation operation = Operation::INVALID_CNODE_SIZE;
        };

        CapError()                                = delete;
        CapError(const CapError &)                = default;
        CapError &operator=(const CapError &)     = default;
        CapError(CapError &&) noexcept            = default;
        CapError &operator=(CapError &&) noexcept = default;
        ~CapError() noexcept                      = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::CapError;
            return visit(tay::overloaded{
                [](const InvalidToken &) noexcept { return Reason::INVALID_TOKEN; },
                [](const MissingCNode &) noexcept { return Reason::MISSING_CNODE; },
                [](const InvalidSlot &) noexcept { return Reason::INVALID_SLOT; },
                [](const StaleToken &) noexcept { return Reason::STALE_TOKEN; },
                [](const TypeMismatch &) noexcept { return Reason::TYPE_MISMATCH; },
                [](const InsufficientRights &) noexcept { return Reason::INSUFFICIENT_RIGHTS; },
                [](const NoSlots &) noexcept { return Reason::NO_SLOTS; },
                [](const Busy &) noexcept { return Reason::BUSY; },
                [](const OutOfMemory &) noexcept { return Reason::OUT_OF_MEMORY; },
                [](const OperationRejected &) noexcept { return Reason::OPERATION_REJECTED; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const InvalidToken &) noexcept { return "invalid capability token"; },
                [](const MissingCNode &) noexcept { return "capability CNode is missing"; },
                [](const InvalidSlot &) noexcept { return "capability slot is invalid"; },
                [](const StaleToken &) noexcept { return "capability token is stale"; },
                [](const TypeMismatch &) noexcept { return "capability object type mismatch"; },
                [](const InsufficientRights &) noexcept {
                    return "capability rights are insufficient";
                },
                [](const NoSlots &) noexcept { return "capability CNode has no free slots"; },
                [](const Busy &) noexcept { return "capability CNode is busy"; },
                [](const OutOfMemory &) noexcept { return "capability allocation failed"; },
                [](const OperationRejected &) noexcept { return "capability operation rejected"; },
            });
        }

    private:
        using Storage =
            tay::variant<InvalidToken, MissingCNode, InvalidSlot, StaleToken, TypeMismatch,
                         InsufficientRights, NoSlots, Busy, OutOfMemory, OperationRejected>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        CapError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(CapError) <= 32);
    static_assert(std::is_nothrow_move_constructible_v<CapError>);
}  // namespace cap
