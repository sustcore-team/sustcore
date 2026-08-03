/**
 * @file units.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Tay C++ 库使用的单位类型和换算工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/bits.h>

namespace units {
    // 实际上, 这个结构体只是一个u64_t
    struct frequency {
    protected:
        u64_t milihertz;
        explicit constexpr frequency(u64_t mili_hz) : milihertz(mili_hz) {}

    public:
        explicit constexpr frequency() : milihertz(0) {}
        explicit constexpr operator u64_t() const {
            return to_hz();
        }

        constexpr u64_t to_milihz() const {
            return milihertz;
        }
        constexpr u64_t to_hz() const {
            return milihertz / 1'000;
        }
        constexpr u64_t to_khz() const {
            return to_hz() / 1'000;
        }
        constexpr u64_t to_mhz() const {
            return to_khz() / 1'000;
        }
        constexpr u64_t to_ghz() const {
            return to_mhz() / 1'000;
        }

        static constexpr frequency from_milihz(u64_t h) {
            return frequency(h);
        }
        static constexpr frequency from_hz(u64_t h) {
            return from_milihz(h * 1'000);
        }
        static constexpr frequency from_khz(u64_t kh) {
            return from_hz(kh * 1'000);
        }
        static constexpr frequency from_mhz(u64_t mh) {
            return from_khz(mh * 1'000);
        }
        static constexpr frequency from_ghz(u64_t gh) {
            return from_mhz(gh * 1'000);
        }

        constexpr frequency operator+(const frequency &other) const {
            return frequency(milihertz + other.milihertz);
        }

        constexpr frequency operator-(const frequency &other) const {
            return frequency(milihertz - other.milihertz);
        }

        constexpr frequency operator*(u64_t multiplier) const {
            return frequency(milihertz * multiplier);
        }

        constexpr frequency operator/(u64_t divisor) const {
            return frequency(milihertz / divisor);
        }

        constexpr u64_t operator/(const frequency &other) const {
            return milihertz / other.milihertz;
        }
    };

    using tick = u64_t;

    constexpr u64_t NANOSECONDS_PER_MILLIHERTZ = 1'000'000'000'000ULL;
    struct formatted_time {
        i64_t year;
        i64_t month;
        i64_t day;
        i64_t hour;
        i64_t minute;
        i64_t second;
    };

    struct time_ymd {
        i64_t year;
        i64_t month;
        i64_t day;
    };

    // 将纪元天数转换为年月日，算法源自 Howard Hinnant
    constexpr time_ymd days_to_ymd(i64_t days_since_epoch) {
        int year, month, day;

        // 算法常量，用于调整闰年周期
        [[maybe_unused]] const int days_per_400_years = 146097;
        [[maybe_unused]] const int days_per_100_years = 36524;
        [[maybe_unused]] const int days_per_4_years   = 1461;

        // 算法偏移量，用于简化计算
        const int epoch_offset = 719468;  // 1970-03-01 对应的调整天数

        // 1. 算法第一步：转换并适应纪元偏移
        int z = days_since_epoch + epoch_offset;

        // 2. 确定所在的400年周期 ("纪元")
        int era = (z >= 0 ? z : z - days_per_400_years + 1) / days_per_400_years;

        // 3. 计算在当前400年周期内的天数 ("年份偏移")
        int doe = z - era * days_per_400_years;

        // 4. 推算当前周期内的年份 ("年偏移")
        int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;

        // 5. 计算最终的年份
        year = static_cast<int>(yoe) + era * 400;

        // 6. 计算当前年份内的天数 ("年积日")
        int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);

        // 7. 推算月份和日期
        int mp = (5 * doy + 2) / 153;
        day    = doy - (153 * mp + 2) / 5 + 1;
        month  = mp + (mp < 10 ? 3 : -9);

        // 8. 调整年份 (针对1月和2月)
        if (month <= 2)
            year += 1;

        return time_ymd{.year = year, .month = month, .day = day};
    }

    constexpr i64_t ymd_to_days(time_ymd ymd) {
        i64_t year  = ymd.year;
        i64_t month = ymd.month;
        i64_t day   = ymd.day;

        year            -= month <= 2 ? 1 : 0;
        const i64_t era  = (year >= 0 ? year : year - 399) / 400;
        const u64_t yoe  = static_cast<u64_t>(year - era * 400);
        const u64_t moy  = static_cast<u64_t>(month + (month > 2 ? -3 : 9));
        const u64_t doy  = (153 * moy + 2) / 5 + static_cast<u64_t>(day - 1);
        const u64_t doe  = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + static_cast<i64_t>(doe) - 719468;
    }

    struct time {
    protected:
        i64_t nanoseconds;
        explicit constexpr time(i64_t ns) : nanoseconds(ns) {}

    public:
        explicit constexpr time() : nanoseconds(0) {}
        explicit constexpr operator i64_t() const {
            return to_nanoseconds();
        }

        [[nodiscard]]
        constexpr i64_t to_nanoseconds() const {
            return nanoseconds;
        }
        [[nodiscard]]
        constexpr i64_t to_microseconds() const {
            return nanoseconds / 1'000;
        }
        [[nodiscard]]
        constexpr i64_t to_milliseconds() const {
            return nanoseconds / 1'000'000;
        }
        [[nodiscard]]
        constexpr i64_t to_seconds() const {
            return to_milliseconds() / 1'000;
        }
        [[nodiscard]]
        constexpr i64_t to_minutes() const {
            return to_seconds() / 60;
        }
        [[nodiscard]]
        constexpr i64_t to_hours() const {
            return to_minutes() / 60;
        }
        [[nodiscard]]
        constexpr i64_t to_days() const {
            return to_hours() / 24;
        }

        [[nodiscard]]
        constexpr time_ymd to_ymd() const {
            return days_to_ymd(to_days());
        }

        [[nodiscard]]
        constexpr formatted_time to_formatted_time() const {
            time_ymd ymd = to_ymd();
            return formatted_time{
                .year   = ymd.year,
                .month  = ymd.month,
                .day    = ymd.day,
                .hour   = to_hours() % 24,
                .minute = to_minutes() % 60,
                .second = to_seconds() % 60,
            };
        }

        static constexpr time from_nanoseconds(i64_t ns) {
            return time(ns);
        }
        static constexpr time from_microseconds(i64_t us) {
            return from_nanoseconds(us * 1'000);
        }
        static constexpr time from_milliseconds(i64_t ms) {
            return from_microseconds(ms * 1'000);
        }
        static constexpr time from_seconds(i64_t s) {
            return from_milliseconds(s * 1'000);
        }
        static constexpr time from_minutes(i64_t m) {
            return from_seconds(m * 60);
        }
        static constexpr time from_hours(i64_t h) {
            return from_minutes(h * 60);
        }
        static constexpr time from_days(i64_t d) {
            return from_hours(d * 24);
        }
        static constexpr time from_ymd(time_ymd ymd) {
            return from_days(ymd_to_days(ymd));
        }
        static constexpr time from_formatted_time(formatted_time ft) {
            return from_ymd(time_ymd{
                       .year  = ft.year,
                       .month = ft.month,
                       .day   = ft.day,
                   }) +
                   from_hours(ft.hour) + from_minutes(ft.minute) + from_seconds(ft.second);
        }

        constexpr time operator+(const time &other) const {
            return time(nanoseconds + other.nanoseconds);
        }

        constexpr time operator-(const time &other) const {
            return time(nanoseconds - other.nanoseconds);
        }

        constexpr time operator*(i64_t multiplier) const {
            return time(nanoseconds * multiplier);
        }

        constexpr time operator/(i64_t divisor) const {
            return time(nanoseconds / divisor);
        }

        constexpr i64_t operator/(const time &other) const {
            return nanoseconds / other.nanoseconds;
        }

        constexpr i64_t operator*(const frequency &f) const {
            // time * frequency = (ns) * (mili Hz) = (ns) * (1000/s) = 10^(-12)
            // s
            return (nanoseconds * f.to_milihz()) / NANOSECONDS_PER_MILLIHERTZ;
        }
    };  // namespace units

    // calculate the frequenct
    constexpr frequency operator/(u64_t count, const time &t) {
        // 1 / ns = 1 / (1e-9 s) = 1e9 Hz
        // count / (t ns) = (count / t) * 1e9 Hz = count * (1e9 / t) Hz
        return frequency::from_milihz(count * (NANOSECONDS_PER_MILLIHERTZ / t.to_nanoseconds()));
    };

    // calculate the time
    constexpr time operator/(u64_t count, const frequency &f) {
        // 1 / Hz = 1 / (1/s) = s
        // count / (f Hz) = count * (1 / f) s = count * (1e9 / f) ns
        return time::from_nanoseconds(count * (NANOSECONDS_PER_MILLIHERTZ / f.to_milihz()));
    };
}  // namespace units

constexpr units::frequency operator""_mHz(unsigned long long h) {
    return units::frequency::from_milihz(h);
}

constexpr units::frequency operator""_Hz(unsigned long long h) {
    return units::frequency::from_hz(h);
}

constexpr units::frequency operator""_kHz(unsigned long long kh) {
    return units::frequency::from_khz(kh);
}

constexpr units::frequency operator""_MHz(unsigned long long mh) {
    return units::frequency::from_mhz(mh);
}

constexpr units::frequency operator""_GHz(unsigned long long gh) {
    return units::frequency::from_ghz(gh);
}

constexpr units::time operator""_ns(unsigned long long ns) {
    return units::time::from_nanoseconds(ns);
}
