/**
 * @file process_error.h
 * @brief 定义 Process 资源绑定与提交错误。
 */

#pragma once

#include <error.h>
#include <task/state.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <type_traits>
#include <utility>

namespace task {
    class ProcessError final {
    public:
        using kernel_domain_error_tag = void;

        struct OutOfMemory {};
        struct InvalidState {
            ProcessState state = ProcessState::CREATED;
        };
        struct KernelProcessOperation {};
        struct AddressSpaceAlreadySet {};
        struct CSpaceAlreadySet {};
        struct MissingAddressSpace {};
        struct MissingCSpace {};
        struct AlreadySubmitted {};

        ProcessError()                                    = delete;
        ProcessError(const ProcessError &)                = default;
        ProcessError &operator=(const ProcessError &)     = default;
        ProcessError(ProcessError &&) noexcept            = default;
        ProcessError &operator=(ProcessError &&) noexcept = default;
        ~ProcessError() noexcept                          = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::ProcessError;
            return visit(tay::overloaded{
                [](const OutOfMemory &) noexcept { return Reason::OUT_OF_MEMORY; },
                [](const InvalidState &) noexcept { return Reason::INVALID_STATE; },
                [](const KernelProcessOperation &) noexcept {
                    return Reason::KERNEL_PROCESS_OPERATION;
                },
                [](const AddressSpaceAlreadySet &) noexcept {
                    return Reason::ADDRESS_SPACE_ALREADY_SET;
                },
                [](const CSpaceAlreadySet &) noexcept { return Reason::CSPACE_ALREADY_SET; },
                [](const MissingAddressSpace &) noexcept { return Reason::MISSING_ADDRESS_SPACE; },
                [](const MissingCSpace &) noexcept { return Reason::MISSING_CSPACE; },
                [](const AlreadySubmitted &) noexcept { return Reason::ALREADY_SUBMITTED; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const OutOfMemory &) noexcept { return "process allocation failed"; },
                [](const InvalidState &) noexcept { return "process state rejects the operation"; },
                [](const KernelProcessOperation &) noexcept {
                    return "operation is not allowed on the kernel process";
                },
                [](const AddressSpaceAlreadySet &) noexcept {
                    return "process address space is already set";
                },
                [](const CSpaceAlreadySet &) noexcept { return "process CSpace is already set"; },
                [](const MissingAddressSpace &) noexcept { return "process has no address space"; },
                [](const MissingCSpace &) noexcept { return "process has no CSpace"; },
                [](const AlreadySubmitted &) noexcept { return "process is already submitted"; },
            });
        }

    private:
        using Storage =
            tay::variant<OutOfMemory, InvalidState, KernelProcessOperation, AddressSpaceAlreadySet,
                         CSpaceAlreadySet, MissingAddressSpace, MissingCSpace, AlreadySubmitted>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        ProcessError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(ProcessError) <= 16);
    static_assert(std::is_nothrow_move_constructible_v<ProcessError>);
}  // namespace task
