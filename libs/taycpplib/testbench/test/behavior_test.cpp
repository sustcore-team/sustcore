#include <cassert>
#include <utility>
#include <tay/algobase.h>
#include <tay/owner.h>
#include <tay/range.h>
#include <tay/refcount.h>
#include <tay/units.h>

struct counted : tay::refc<counted> {
    int deaths = 0;
    void on_death() { ++deaths; }
};

int main() {
    static_assert(tay::min(2, 3) == 2);
    static_assert(tay::max(2, 3) == 3);
    static_assert(tay::abs(-4) == 4);
    static_assert(tay::clamp(8, 1, 5) == 5);

    constexpr tay::range<int> outer{1, 8};
    constexpr tay::range<int> inner{3, 6};
    static_assert(outer.size() == 7 && tay::within(outer, inner));
    static_assert(tay::intersection(outer, tay::range<int>{5, 10}) == tay::range<int>{5, 8});

    int value = 9;
    tay::owner owned{&value};
    assert(owned && *owned == 9 && owned.get() == &value);

    counted object;
    {
        tay::refc_ptr<counted> first{&object};
        assert(object.ref_count() == 1);
        tay::refc_ptr<counted> second{first};
        assert(object.ref_count() == 2);
        tay::refc_ptr<counted> moved{std::move(second)};
        assert(object.ref_count() == 2 && second.get() == nullptr);
    }
    assert(object.deaths == 1);

    constexpr auto frequency = units::frequency::from_mhz(2400);
    static_assert(frequency.to_mhz() == 2400);
    constexpr auto date = units::days_to_ymd(0);
    static_assert(date.year == 1970 && date.month == 1 && date.day == 1);

    return 0;
}
