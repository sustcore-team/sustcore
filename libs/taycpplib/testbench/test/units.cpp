/**
 * @file units.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay 单位类型的换算和比较语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/units.h>

#include <concepts>

namespace {
    template <class Left, class Right>
    concept Addable = requires(Left left, Right right) { left + right; };
}  // namespace

int main() {
    constexpr auto frequency = units::frequency::from_mhz(2400);
    static_assert(frequency.to_mhz() == 2400);

    constexpr auto date = units::days_to_ymd(0);
    static_assert(date.year == 1970 && date.month == 1 && date.day == 1);
    constexpr auto pre_epoch = units::days_to_ymd(-1);
    static_assert(pre_epoch.year == 1969 && pre_epoch.month == 12 && pre_epoch.day == 31);
    constexpr auto leap_day =
        units::days_to_ymd(units::ymd_to_days({.year = 2000, .month = 2, .day = 29}));
    static_assert(leap_day.year == 2000 && leap_day.month == 2 && leap_day.day == 29);
    constexpr auto non_leap_century =
        units::days_to_ymd(units::ymd_to_days({.year = 1900, .month = 3, .day = 1}));
    static_assert(non_leap_century.year == 1900 && non_leap_century.month == 3 &&
                  non_leap_century.day == 1);

    static_assert(sizeof(units::time) == sizeof(u64_t));
    static_assert(sizeof(units::duration) == sizeof(i64_t));
    static_assert(std::same_as<decltype(1_ns), units::duration>);
    static_assert(std::same_as<decltype(1_us), units::duration>);
    static_assert(std::same_as<decltype(1_ms), units::duration>);
    static_assert(std::same_as<decltype(1_s), units::duration>);
    static_assert(std::same_as<decltype(1_min), units::duration>);
    static_assert(std::same_as<decltype(1_h), units::duration>);

    static_assert(1_us == 1'000_ns);
    static_assert(1_ms == 1'000_us);
    static_assert(1_s == 1'000_ms);
    static_assert(1_min == 60_s);
    static_assert(1_h == 60_min);
    static_assert((-2_s).to_nanoseconds() == -2'000'000'000);

    constexpr auto epoch = units::time::from_nanoseconds(0);
    constexpr auto later = epoch + 2_s;
    static_assert(later.to_nanoseconds() == 2'000'000'000);
    static_assert(later - epoch == 2_s);
    static_assert(epoch - later == -2_s);
    static_assert(later - 500_ms == epoch + 1'500_ms);
    static_assert(500_ms + epoch == epoch + 500_ms);
    static_assert(!Addable<units::time, units::time>);

    constexpr auto near_max = units::time::max() - 1_ns;
    static_assert(units::saturated_add(near_max, 2_ns) == units::time::max());
    static_assert(units::time::from_ymd({.year = 1970, .month = 1, .day = 1}) == epoch);
    static_assert(units::time::from_formatted_time(
                      {.year = 1970, .month = 1, .day = 1, .hour = 1, .minute = 2, .second = 3}) ==
                  epoch + 1_h + 2_min + 3_s);

    static_assert((1ULL / 1_s).to_hz() == 1);
    static_assert((1ULL / 1_kHz) == 1_ms);
    static_assert(1_s * 1_Hz == 1);

    return 0;
}
