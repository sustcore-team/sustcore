/**
 * @file notif.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief notification对象
 * @version alpha-1.0.0
 * @date 2026-05-14
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <arch/riscv64/description.h>
#include <cap/capability.h>
#include <perm/notif.h>
#include <task/task_struct.h>

namespace cap {
    struct NotificationPayload : public _PayloadHelper<PayloadType::NOTIF> {
        // 信号位图, 实际长 24 位
        b32 signalbits = 0;
        WaitReasonId wait_reasons[perm::notif::MAX_SIGNALS];

        NotificationPayload();

        struct Signal {
            NotificationPayload &notif;
            size_t idx;

            constexpr Signal(NotificationPayload &notif, size_t idx)
                : notif(notif), idx(idx) {
                assert(idx < perm::notif::MAX_SIGNALS);
            }

            [[nodiscard]]
            bool get_signal() const {
                return (notif.signalbits & (1 << idx)) != 0;
            }

            operator bool() const {
                return get_signal();
            }

            constexpr bool signal() {
                notif.signalbits |= (1 << idx);
                return true;
            }

            constexpr bool unsignal() {
                notif.signalbits &= ~(1 << idx);
                return false;
            }

            // NOLINTBEGIN(cppcoreguidelines-c-copy-assignment-signature)
            constexpr bool operator=(bool value) {
                return value ? signal() : unsignal();
            }
            // NOLINTEND(cppcoreguidelines-c-copy-assignment-signature)
        };

        [[nodiscard]]
        Signal signal(size_t idx) {
            assert(idx < perm::notif::MAX_SIGNALS);
            return {*this, idx};
        }
    };

    class NotificationObject : public CapObj<NotificationPayload> {
    public:
        explicit NotificationObject(util::nonnull<Capability *> cap)
            : CapObj<NotificationPayload>(cap) {}

        Result<bool> signal(size_t idx);
        Result<bool> unsignal(size_t idx);
        Result<bool> set(size_t idx, bool state);
        Result<bool> query(size_t idx);
        Result<bool> wait(size_t idx);
    };

    
}  // namespace cap
