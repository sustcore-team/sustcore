/**
 * @file usrboot_error.h
 * @brief 定义 usrboot 镜像校验和首个用户任务装载错误。
 */

#pragma once

#include <error.h>
#include <tay/utility.h>
#include <tay/variant.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace init {
    class UsrbootError final {
    public:
        using kernel_domain_error_tag = void;

        enum class Segment : u8_t {
            RX,
            RW,
            RO,
            STACK,
        };

        enum class Object : u8_t {
            ADDRESS_SPACE,
            PROCESS,
            CSPACE,
            MEMORY_SEGMENT,
        };

        struct ImageTooSmall {
            size_t observed = 0;
            size_t required = 0;
        };
        struct InvalidMagic {
            u64_t observed = 0;
        };
        struct InvalidHeader {
            size_t image_size = 0;
            u64_t body_size   = 0;
            addr_t entry      = 0;
        };
        struct InvalidSegmentSize {
            Segment segment   = Segment::RX;
            u64_t memory_size = 0;
            u64_t file_size   = 0;
        };
        struct SegmentAddressOverflow {
            Segment segment        = Segment::RX;
            addr_t virtual_address = 0;
            u64_t memory_size      = 0;
        };
        struct SegmentOutsideUserRange {
            Segment segment = Segment::RX;
            addr_t begin    = 0;
            addr_t end      = 0;
        };
        struct SegmentFileRangeInvalid {
            Segment segment   = Segment::RX;
            u64_t offset      = 0;
            u64_t file_size   = 0;
            size_t image_size = 0;
        };
        struct SegmentUnaligned {
            Segment segment        = Segment::RX;
            addr_t virtual_address = 0;
        };
        struct ObjectCreationFailed {
            Object object             = Object::PROCESS;
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct ProcessConfigurationFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct VmaCreationFailed {
            Segment segment           = Segment::RX;
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct SegmentWriteFailed {
            Segment segment           = Segment::RX;
            size_t offset             = 0;
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct InitialStackFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct ThreadCreationFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct UserContextConfigurationFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct ProcessSubmissionFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };
        struct ThreadAttachFailed {
            kernel::KernelError cause = kernel::KernelError::TayError::INTERNAL;
        };

        UsrbootError()                                    = delete;
        UsrbootError(const UsrbootError &)                = default;
        UsrbootError &operator=(const UsrbootError &)     = default;
        UsrbootError(UsrbootError &&) noexcept            = default;
        UsrbootError &operator=(UsrbootError &&) noexcept = default;
        ~UsrbootError() noexcept                          = default;

        template <typename Alternative>
        [[nodiscard]] bool is() const noexcept {
            return value_.template is<Alternative>();
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return value_.visit(std::forward<Visitor>(visitor));
        }

        [[nodiscard]] kernel::KernelError code() const noexcept {
            using Reason = kernel::KernelError::UsrbootError;
            return visit(tay::overloaded{
                [](const ImageTooSmall &) noexcept { return Reason::IMAGE_TOO_SMALL; },
                [](const InvalidMagic &) noexcept { return Reason::INVALID_MAGIC; },
                [](const InvalidHeader &) noexcept { return Reason::INVALID_HEADER; },
                [](const InvalidSegmentSize &) noexcept { return Reason::INVALID_SEGMENT_SIZE; },
                [](const SegmentAddressOverflow &) noexcept {
                    return Reason::SEGMENT_ADDRESS_OVERFLOW;
                },
                [](const SegmentOutsideUserRange &) noexcept {
                    return Reason::SEGMENT_OUTSIDE_USER_RANGE;
                },
                [](const SegmentFileRangeInvalid &) noexcept {
                    return Reason::SEGMENT_FILE_RANGE_INVALID;
                },
                [](const SegmentUnaligned &) noexcept { return Reason::SEGMENT_UNALIGNED; },
                [](const ObjectCreationFailed &) noexcept {
                    return Reason::OBJECT_CREATION_FAILED;
                },
                [](const ProcessConfigurationFailed &) noexcept {
                    return Reason::PROCESS_CONFIGURATION_FAILED;
                },
                [](const VmaCreationFailed &) noexcept { return Reason::VMA_CREATION_FAILED; },
                [](const SegmentWriteFailed &) noexcept { return Reason::SEGMENT_WRITE_FAILED; },
                [](const InitialStackFailed &) noexcept { return Reason::INITIAL_STACK_FAILED; },
                [](const ThreadCreationFailed &) noexcept {
                    return Reason::THREAD_CREATION_FAILED;
                },
                [](const UserContextConfigurationFailed &) noexcept {
                    return Reason::USER_CONTEXT_CONFIGURATION_FAILED;
                },
                [](const ProcessSubmissionFailed &) noexcept {
                    return Reason::PROCESS_SUBMISSION_FAILED;
                },
                [](const ThreadAttachFailed &) noexcept { return Reason::THREAD_ATTACH_FAILED; },
            });
        }

        [[nodiscard]] const char *message() const noexcept {
            return visit(tay::overloaded{
                [](const ImageTooSmall &) noexcept { return "usrboot image is too small"; },
                [](const InvalidMagic &) noexcept { return "usrboot magic is invalid"; },
                [](const InvalidHeader &) noexcept { return "usrboot header is invalid"; },
                [](const InvalidSegmentSize &) noexcept {
                    return "usrboot segment size is invalid";
                },
                [](const SegmentAddressOverflow &) noexcept {
                    return "usrboot segment address overflows";
                },
                [](const SegmentOutsideUserRange &) noexcept {
                    return "usrboot segment is outside user address range";
                },
                [](const SegmentFileRangeInvalid &) noexcept {
                    return "usrboot segment file range is invalid";
                },
                [](const SegmentUnaligned &) noexcept { return "usrboot segment is unaligned"; },
                [](const ObjectCreationFailed &) noexcept {
                    return "usrboot object creation failed";
                },
                [](const ProcessConfigurationFailed &) noexcept {
                    return "usrboot process configuration failed";
                },
                [](const VmaCreationFailed &) noexcept { return "usrboot VMA creation failed"; },
                [](const SegmentWriteFailed &) noexcept { return "usrboot segment write failed"; },
                [](const InitialStackFailed &) noexcept { return "usrboot initial stack failed"; },
                [](const ThreadCreationFailed &) noexcept {
                    return "usrboot thread creation failed";
                },
                [](const UserContextConfigurationFailed &) noexcept {
                    return "usrboot user context configuration failed";
                },
                [](const ProcessSubmissionFailed &) noexcept {
                    return "usrboot process submission failed";
                },
                [](const ThreadAttachFailed &) noexcept { return "usrboot thread attach failed"; },
            });
        }

    private:
        using Storage =
            tay::variant<ImageTooSmall, InvalidMagic, InvalidHeader, InvalidSegmentSize,
                         SegmentAddressOverflow, SegmentOutsideUserRange, SegmentFileRangeInvalid,
                         SegmentUnaligned, ObjectCreationFailed, ProcessConfigurationFailed,
                         VmaCreationFailed, SegmentWriteFailed, InitialStackFailed,
                         ThreadCreationFailed, UserContextConfigurationFailed,
                         ProcessSubmissionFailed, ThreadAttachFailed>;

    public:
        template <typename Alternative>
            requires std::is_constructible_v<Storage, Alternative>
        UsrbootError(Alternative alternative) noexcept : value_(std::move(alternative)) {}

    private:
        Storage value_;
    };

    static_assert(sizeof(UsrbootError) <= 40);
    static_assert(std::is_nothrow_move_constructible_v<UsrbootError>);
}  // namespace init
