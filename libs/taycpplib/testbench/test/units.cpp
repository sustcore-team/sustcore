#include <tay/units.h>

int main() {
    constexpr auto frequency = units::frequency::from_mhz(2400);
    static_assert(frequency.to_mhz() == 2400);

    constexpr auto date = units::days_to_ymd(0);
    static_assert(date.year == 1970 && date.month == 1 && date.day == 1);

    return 0;
}
