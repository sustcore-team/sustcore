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

int main() {
    constexpr auto frequency = units::frequency::from_mhz(2400);
    static_assert(frequency.to_mhz() == 2400);

    constexpr auto date = units::days_to_ymd(0);
    static_assert(date.year == 1970 && date.month == 1 && date.day == 1);

    return 0;
}
