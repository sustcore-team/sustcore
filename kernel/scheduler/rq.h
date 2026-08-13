/**
 * @file rq.h
 * @brief 基于 Tay 侵入式链表的非拥有式 FIFO 运行队列
 */

#pragma once

#include <obj/thread.h>
#include <tay/list.h>

namespace scheduler {
    struct RQHookLocator {
        task::Thread::rq_hook &operator()(task::Thread &thread) const noexcept {
            return thread.rq_hook_;
        }
        const task::Thread::rq_hook &operator()(const task::Thread &thread) const noexcept {
            return thread.rq_hook_;
        }
    };

    class RunQueue final {
    public:
        [[nodiscard]] bool push(task::Thread *thread) noexcept {
            if (thread == nullptr || threads_.linked(thread))
                return false;
            threads_.push_back(thread);
            return true;
        }

        [[nodiscard]] task::Thread *pop() noexcept {
            if (threads_.empty())
                return nullptr;
            return threads_.pop_front();
        }

        [[nodiscard]] bool remove(task::Thread *thread) noexcept {
            if (thread == nullptr || !threads_.linked(thread))
                return false;
            (void)threads_.remove(thread);
            return true;
        }

        [[nodiscard]] bool empty() const noexcept {
            return threads_.empty();
        }

        [[nodiscard]] size_t size() const noexcept {
            return threads_.size();
        }

    private:
        using thread_list = tay::intrusive_list<task::Thread, RQHookLocator>;
        thread_list threads_{};
    };
}  // namespace scheduler
