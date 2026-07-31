#include <tay/lock.h>

#include <array>
#include <cassert>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
    struct test_lock {
        void lock() noexcept {
            assert(!locked);
            locked = true;
            ++lock_count;
        }

        [[nodiscard]] bool try_lock() noexcept {
            ++try_count;
            if (locked) {
                return false;
            }
            locked = true;
            return true;
        }

        void unlock() noexcept {
            assert(locked);
            locked = false;
            ++unlock_count;
        }

        bool locked      = false;
        int lock_count   = 0;
        int try_count    = 0;
        int unlock_count = 0;
    };

    template <class Lock>
    class observed_locker {
    public:
        explicit observed_locker(Lock& lock) noexcept : lock_(&lock) {
            ++constructions;
            lock_->lock();
        }

        observed_locker(const observed_locker&)            = delete;
        observed_locker& operator=(const observed_locker&) = delete;
        observed_locker(observed_locker&&)                 = delete;
        observed_locker& operator=(observed_locker&&)      = delete;

        ~observed_locker() noexcept {
            lock_->unlock();
            ++destructions;
        }

        static inline int constructions = 0;
        static inline int destructions  = 0;

    private:
        Lock* lock_;
    };

    std::vector<int>* context_events = nullptr;

    struct early_guard {
        early_guard() noexcept {
            context_events->push_back(100);
        }

        ~early_guard() noexcept {
            context_events->push_back(-100);
        }
    };

    struct late_guard {
        late_guard() noexcept {
            context_events->push_back(200);
        }

        ~late_guard() noexcept {
            context_events->push_back(-200);
        }
    };

    struct recording_lock {
        void lock() noexcept {
            context_events->push_back(1000);
        }

        [[nodiscard]] bool try_lock() noexcept {
            context_events->push_back(1001);
            return true;
        }

        void unlock() noexcept {
            context_events->push_back(-1000);
        }
    };

    struct record {
        record(int count, bool ready) : count(count), ready(ready) {}

        int count;
        bool ready;
    };

    template <class Lock>
    void test_concurrent_lock() {
        Lock lock;
        assert(lock.try_lock());
        assert(!lock.try_lock());
        lock.unlock();

        constexpr int thread_count = 4;
        constexpr int increments   = 20000;
        int value                  = 0;
        std::array<std::thread, thread_count> threads;
        for (auto& thread : threads) {
            thread = std::thread([&] {
                for (int i = 0; i < increments; ++i) {
                    tay::lock_guard held{lock};
                    ++value;
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        assert(value == thread_count * increments);
    }

    void test_lock_guard() {
        test_lock lock;
        {
            tay::lock_guard held{lock};
            assert(lock.locked);
        }
        assert(!lock.locked);

        lock.lock();
        {
            tay::lock_guard adopted{lock, tay::adopt_lock};
            assert(lock.locked);
        }
        assert(!lock.locked);
        assert(lock.lock_count == 2);
        assert(lock.unlock_count == 2);
    }

    void test_unique_lock() {
        test_lock first;
        {
            tay::unique_lock held{first};
            assert(held && held.owns_lock());
            assert(held.mutex() == &first);
            held.unlock();
            assert(!held && !first.locked);
            held.lock();
        }
        assert(!first.locked);

        {
            tay::unique_lock deferred{first, tay::defer_lock};
            assert(!deferred && deferred.mutex() == &first);
            assert(deferred.try_lock());
        }

        first.lock();
        {
            tay::unique_lock failed{first, tay::try_to_lock};
            assert(!failed && failed.mutex() == &first);
        }
        first.unlock();

        first.lock();
        {
            tay::unique_lock adopted{first, tay::adopt_lock};
            assert(adopted);
        }

        test_lock second;
        tay::unique_lock source{first};
        tay::unique_lock moved{std::move(source)};
        assert(!source && source.mutex() == nullptr);
        assert(moved && moved.mutex() == &first);

        tay::unique_lock destination{second};
        destination = std::move(moved);
        assert(!second.locked);
        assert(!moved && moved.mutex() == nullptr);
        assert(destination && destination.mutex() == &first);

        auto* self  = &destination;
        destination = std::move(*self);
        assert(destination && first.locked);

        tay::unique_lock<test_lock> empty;
        destination.swap(empty);
        assert(!destination && destination.mutex() == nullptr);
        assert(empty && empty.mutex() == &first);

        auto* released = empty.release();
        assert(released == &first);
        assert(!empty && empty.mutex() == nullptr && first.locked);
        released->unlock();
    }

    void test_context_lock_guard() {
        using guard_type =
            tay::context_lock_guard<recording_lock,
                                    tay::guard_stage<200, late_guard>,
                                    tay::guard_stage<100, early_guard>>;

        std::vector<int> events;
        context_events = &events;
        recording_lock lock;
        {
            guard_type held{lock};
            assert((events == std::vector<int>{100, 200, 1000}));
        }
        assert((events == std::vector<int>{100, 200, 1000, -1000, -200, -100}));
        context_events = nullptr;
    }

    void test_synchronized() {
        tay::synchronized<record, test_lock> state{7, false};
        {
            auto access   = state.lock();
            access->ready = true;
            ++access->count;
            assert((*access).count == 8);
            assert(access.get() == &*access);
        }

        const auto& readonly = state;
        {
            auto access = readonly.lock();
            static_assert(
                std::is_same_v<decltype(access.get()), const record*>);
            assert(access->ready && access->count == 8);
        }

        using locker          = observed_locker<test_lock>;
        locker::constructions = 0;
        locker::destructions  = 0;
        tay::synchronized<int, test_lock, observed_locker> observed{3};
        {
            auto access = observed.lock();
            ++*access;
        }
        assert(locker::constructions == 1);
        assert(locker::destructions == 1);
        assert(*observed.lock() == 4);
    }
}  // namespace

using basic_context_guard =
    tay::context_lock_guard<test_lock, tay::guard_stage<100, early_guard>>;
using synchronized_access =
    decltype(std::declval<tay::synchronized<int>&>().lock());

static_assert(!std::is_copy_constructible_v<tay::spinlock>);
static_assert(!std::is_move_constructible_v<tay::spinlock>);
static_assert(!std::is_copy_constructible_v<tay::ticket_spinlock>);
static_assert(!std::is_move_constructible_v<tay::ticket_spinlock>);
static_assert(!std::is_copy_constructible_v<tay::lock_guard<test_lock>>);
static_assert(!std::is_move_constructible_v<tay::lock_guard<test_lock>>);
static_assert(!std::is_copy_constructible_v<basic_context_guard>);
static_assert(!std::is_move_constructible_v<basic_context_guard>);
static_assert(!std::is_copy_constructible_v<tay::unique_lock<test_lock>>);
static_assert(std::is_move_constructible_v<tay::unique_lock<test_lock>>);
static_assert(std::is_move_assignable_v<tay::unique_lock<test_lock>>);
static_assert(!std::is_copy_constructible_v<synchronized_access>);
static_assert(!std::is_move_constructible_v<synchronized_access>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const tay::synchronized<int>&>().lock().get()),
        const int*>);

int main() {
    test_concurrent_lock<tay::spinlock>();
    test_concurrent_lock<tay::ticket_spinlock>();
    test_lock_guard();
    test_unique_lock();
    test_context_lock_guard();
    test_synchronized();
    return 0;
}
