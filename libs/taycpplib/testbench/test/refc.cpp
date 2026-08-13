/**
 * @file refc.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 计数器、引用计数和 pin guard 的所有权语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 */

#include <tay/counter.h>
#include <tay/refcount.h>

#include <cassert>
#include <utility>

namespace {
    struct counted : tay::refc<counted> {
        int deaths = 0;

        void on_death() noexcept {
            ++deaths;
        }
    };

    struct modern_counted : tay::ref_counted<modern_counted, unsigned> {
        int retirements = 0;

        void on_zero_references() noexcept {
            ++retirements;
        }
    };

    struct pinnable {
        bool available = true;
        int pins       = 0;

        [[nodiscard]] bool try_pin() noexcept {
            if (!available)
                return false;
            ++pins;
            return true;
        }

        void unpin() noexcept {
            --pins;
        }
    };
}  // namespace

int main() {
    tay::counter<unsigned> ids{3};
    assert(ids.next() == 3);
    unsigned id = 0;
    assert(ids.try_next(4, id) && id == 4);
    assert(!ids.try_next(4, id));
    ids.reset(7);
    assert(ids.increment() == 8);
    assert(ids.decrement() == 7);

    tay::reference_counter<unsigned> references;
    references.acquire();
    references.acquire();
    assert(references.count() == 2);
    assert(!references.release());
    assert(references.release() && references.empty());
    assert(!references.try_acquire());

    counted object;
    {
        tay::refc_ptr<counted> first{&object};
        assert(object.ref_count() == 1);
        tay::refc_ptr<counted> second{first};
        assert(object.ref_count() == 2);
        tay::refc_ptr<counted> moved{std::move(second)};
        assert(object.ref_count() == 2 && second.get() == nullptr);

        tay::refc_ptr<counted> assigned;
        assigned = first;
        assert(object.ref_count() == 3);
        assigned = std::move(moved);
        assert(object.ref_count() == 2 && !moved);
    }
    assert(object.deaths == 1);

    modern_counted modern;
    modern.retain();
    assert(modern.try_retain() && modern.ref_count() == 2);
    modern.release();
    modern.release();
    assert(modern.retirements == 1 && !modern.try_retain());

    pinnable target;
    {
        auto pin = tay::pin_guard<pinnable>::try_pin(target);
        assert(pin && pin.get() == &target && target.pins == 1);
        auto moved = std::move(pin);
        assert(!pin && moved && target.pins == 1);
        auto assigned = tay::pin_guard<pinnable>::try_pin(target);
        assert(assigned && target.pins == 2);
        assigned = std::move(moved);
        assert(!moved && assigned && target.pins == 1);
    }
    assert(target.pins == 0);
    target.available = false;
    assert(!tay::pin_guard<pinnable>::try_pin(target));

    return 0;
}
