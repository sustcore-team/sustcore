#include <tay/guard.h>
#include <tay/owner.h>
#include <tay/unique_ptr.h>

#include <cassert>
#include <type_traits>
#include <utility>

namespace {
    struct tracked {
        explicit tracked(int* destroyed, int value = 0)
            : destroyed(destroyed), value(value) {}

        virtual ~tracked() {
            ++*destroyed;
        }

        int* destroyed;
        int value;
    };

    struct derived : tracked {
        using tracked::tracked;
    };

    struct array_element {
        static inline int destroyed = 0;

        ~array_element() {
            ++destroyed;
        }

        int value = 0;
    };
}  // namespace

static_assert(!std::is_copy_constructible_v<tay::guard<void (*)()>>);
static_assert(!std::is_move_constructible_v<tay::guard<void (*)()>>);
static_assert(!std::is_copy_constructible_v<tay::unique_ptr<int>>);
static_assert(std::is_move_constructible_v<tay::unique_ptr<int>>);
static_assert(!std::is_copy_constructible_v<tay::unique_ptr<int[]>>);
static_assert(std::is_move_constructible_v<tay::unique_ptr<int[]>>);

int main() {
    int cleanups = 0;
    {
        tay::guard cleanup{[&] { ++cleanups; }};
        assert(cleanup.active());
    }
    assert(cleanups == 1);

    {
        tay::guard cleanup{[&] { ++cleanups; }};
        cleanup.release();
        assert(!cleanup.active());
    }
    assert(cleanups == 1);

    int destroyed = 0;
    {
        auto ptr = tay::make_unique<tracked>(&destroyed, 7);
        assert(ptr && ptr->value == 7 && (*ptr).value == 7);

        tay::unique_ptr<tracked> moved{std::move(ptr)};
        assert(!ptr && moved);

        moved.reset(new tracked{&destroyed, 9});
        assert(destroyed == 1 && moved->value == 9);

        tay::unique_ptr<tracked> other;
        swap(moved, other);
        assert(!moved && other->value == 9);

        auto owned = other.release_owner();
        assert(!other && owned);
        tay::unique_ptr<tracked> adopted{std::move(owned)};
        assert(adopted && !owned);
    }
    assert(destroyed == 2);

    {
        tay::unique_ptr<derived> child{new derived{&destroyed, 11}};
        tay::unique_ptr<tracked> base{std::move(child)};
        assert(!child && base->value == 11);
    }
    assert(destroyed == 3);

    array_element::destroyed = 0;
    {
        auto values = tay::make_unique<array_element[]>(3);
        values[0].value = 4;
        values[2].value = 8;
        assert(values[0].value == 4 && values[2].value == 8);

        tay::unique_ptr<array_element[]> moved{std::move(values)};
        assert(!values && moved);

        auto raw = moved.release();
        assert(!moved && raw[2].value == 8);
        moved.reset(raw);
    }
    assert(array_element::destroyed == 3);

    return 0;
}
