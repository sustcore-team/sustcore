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
#include <tay/panic.h>

#include <compare>
#include <limits>

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

    /**
     * @brief 将相对 Unix epoch 的天数转换为公历年月日。
     * @param days_since_epoch 从 1970-01-01 起算的有符号天数。
     * @return 对应的公历年月日。
     * @note 使用 Howard Hinnant 的 March-based civil calendar 算法。
     */
    [[nodiscard]] constexpr time_ymd days_to_ymd(const i64_t days_since_epoch) noexcept {
        constexpr i64_t CIVIL_EPOCH_OFFSET         = 719468;
        constexpr i64_t YEARS_PER_ERA              = 400;
        constexpr i64_t YEARS_PER_CENTURY          = 100;
        constexpr i64_t YEARS_PER_LEAP_CYCLE       = 4;
        constexpr i64_t DAYS_PER_ERA               = 146097;
        constexpr i64_t DAYS_PER_CENTURY           = 36524;
        constexpr i64_t DAYS_PER_LEAP_CYCLE        = 1461;
        constexpr i64_t DAYS_PER_COMMON_YEAR       = 365;
        constexpr i64_t MONTH_ESTIMATE_FACTOR      = 5;
        constexpr i64_t MONTH_ESTIMATE_OFFSET      = 2;
        constexpr i64_t DAYS_PER_FIVE_MONTHS       = 153;
        constexpr i64_t MARCH_BASED_MONTHS_IN_YEAR = 10;
        constexpr i64_t MARCH_MONTH_NUMBER         = 3;
        constexpr i64_t MARCH_TO_JANUARY_OFFSET    = 9;
        constexpr i64_t LAST_MONTH_BEFORE_MARCH    = 2;
        constexpr i64_t FIRST_DAY_OF_MONTH         = 1;

        const i64_t civil_days = days_since_epoch + CIVIL_EPOCH_OFFSET;
        const i64_t era =
            (civil_days >= 0 ? civil_days : civil_days - DAYS_PER_ERA + 1) / DAYS_PER_ERA;
        const i64_t day_of_era = civil_days - era * DAYS_PER_ERA;
        const i64_t year_of_era =
            (day_of_era - day_of_era / (DAYS_PER_LEAP_CYCLE - 1) + day_of_era / DAYS_PER_CENTURY -
             day_of_era / (DAYS_PER_ERA - 1)) /
            DAYS_PER_COMMON_YEAR;

        // 以三月作为一年的首月，使闰日位于算法年的末尾。
        const i64_t march_based_year = year_of_era + era * YEARS_PER_ERA;
        const i64_t day_of_year =
            day_of_era - (DAYS_PER_COMMON_YEAR * year_of_era + year_of_era / YEARS_PER_LEAP_CYCLE -
                          year_of_era / YEARS_PER_CENTURY);
        const i64_t march_based_month =
            (MONTH_ESTIMATE_FACTOR * day_of_year + MONTH_ESTIMATE_OFFSET) / DAYS_PER_FIVE_MONTHS;
        const i64_t day = day_of_year -
                          (DAYS_PER_FIVE_MONTHS * march_based_month + MONTH_ESTIMATE_OFFSET) /
                              MONTH_ESTIMATE_FACTOR +
                          FIRST_DAY_OF_MONTH;
        const i64_t month = march_based_month + (march_based_month < MARCH_BASED_MONTHS_IN_YEAR
                                                     ? MARCH_MONTH_NUMBER
                                                     : -MARCH_TO_JANUARY_OFFSET);
        const i64_t year  = march_based_year + (month <= LAST_MONTH_BEFORE_MARCH ? 1 : 0);

        return time_ymd{.year = year, .month = month, .day = day};
    }

    /**
     * @brief 将公历年月日转换为相对 Unix epoch 的天数。
     * @param ymd 待转换的公历年月日。
     * @return 从 1970-01-01 起算的有符号天数。
     */
    [[nodiscard]] constexpr i64_t ymd_to_days(const time_ymd ymd) noexcept {
        constexpr i64_t CIVIL_EPOCH_OFFSET         = 719468;
        constexpr i64_t YEARS_PER_ERA              = 400;
        constexpr i64_t LAST_YEAR_OF_ERA           = YEARS_PER_ERA - 1;
        constexpr u64_t YEARS_PER_CENTURY          = 100;
        constexpr u64_t YEARS_PER_LEAP_CYCLE       = 4;
        constexpr u64_t DAYS_PER_ERA               = 146097;
        constexpr u64_t DAYS_PER_COMMON_YEAR       = 365;
        constexpr u64_t MONTH_ESTIMATE_FACTOR      = 5;
        constexpr u64_t MONTH_ESTIMATE_OFFSET      = 2;
        constexpr u64_t DAYS_PER_FIVE_MONTHS       = 153;
        constexpr i64_t LAST_MONTH_BEFORE_MARCH    = 2;
        constexpr i64_t MARCH_BASED_FORWARD_OFFSET = 3;
        constexpr i64_t MARCH_BASED_WRAP_OFFSET    = 9;
        constexpr i64_t FIRST_DAY_OF_MONTH         = 1;

        const i64_t month = ymd.month;
        const i64_t day   = ymd.day;
        const i64_t year  = ymd.year - (month <= LAST_MONTH_BEFORE_MARCH ? FIRST_DAY_OF_MONTH : 0);

        const i64_t era         = (year >= 0 ? year : year - LAST_YEAR_OF_ERA) / YEARS_PER_ERA;
        const u64_t year_of_era = static_cast<u64_t>(year - era * YEARS_PER_ERA);
        const u64_t march_based_month = static_cast<u64_t>(
            month + (month > LAST_MONTH_BEFORE_MARCH ? -MARCH_BASED_FORWARD_OFFSET
                                                     : MARCH_BASED_WRAP_OFFSET));
        const u64_t day_of_year =
            (DAYS_PER_FIVE_MONTHS * march_based_month + MONTH_ESTIMATE_OFFSET) /
                MONTH_ESTIMATE_FACTOR +
            static_cast<u64_t>(day - FIRST_DAY_OF_MONTH);
        const u64_t day_of_era = year_of_era * DAYS_PER_COMMON_YEAR +
                                 year_of_era / YEARS_PER_LEAP_CYCLE -
                                 year_of_era / YEARS_PER_CENTURY + day_of_year;
        return era * static_cast<i64_t>(DAYS_PER_ERA) + static_cast<i64_t>(day_of_era) -
               CIVIL_EPOCH_OFFSET;
    }

    struct time;

    /**
     * @brief 以纳秒表示的有符号时间间隔。
     *
     * 算术和单位换算直接使用 i64_t，不检查结果范围。调用方必须保证结果可表示；
     * 有符号整数溢出不具有可依赖的回绕语义。
     */
    struct duration {
    protected:
        i64_t nanoseconds_ = 0;
        explicit constexpr duration(i64_t nanoseconds) noexcept : nanoseconds_(nanoseconds) {}

    public:
        explicit constexpr duration() noexcept = default;
        explicit constexpr operator i64_t() const noexcept {
            return to_nanoseconds();
        }

        [[nodiscard]] constexpr i64_t to_nanoseconds() const noexcept {
            return nanoseconds_;
        }
        [[nodiscard]] constexpr i64_t to_microseconds() const noexcept {
            return nanoseconds_ / 1'000;
        }
        [[nodiscard]] constexpr i64_t to_milliseconds() const noexcept {
            return nanoseconds_ / 1'000'000;
        }
        [[nodiscard]] constexpr i64_t to_seconds() const noexcept {
            return nanoseconds_ / 1'000'000'000;
        }
        [[nodiscard]] constexpr i64_t to_minutes() const noexcept {
            return to_seconds() / 60;
        }
        [[nodiscard]] constexpr i64_t to_hours() const noexcept {
            return to_minutes() / 60;
        }
        [[nodiscard]] constexpr i64_t to_days() const noexcept {
            return to_hours() / 24;
        }

        [[nodiscard]] static constexpr duration from_nanoseconds(i64_t nanoseconds) noexcept {
            return duration(nanoseconds);
        }
        [[nodiscard]] static constexpr duration from_microseconds(i64_t microseconds) noexcept {
            return from_nanoseconds(microseconds * 1'000);
        }
        [[nodiscard]] static constexpr duration from_milliseconds(i64_t milliseconds) noexcept {
            return from_nanoseconds(milliseconds * 1'000'000);
        }
        [[nodiscard]] static constexpr duration from_seconds(i64_t seconds) noexcept {
            return from_nanoseconds(seconds * 1'000'000'000);
        }
        [[nodiscard]] static constexpr duration from_minutes(i64_t minutes) noexcept {
            return from_seconds(minutes * 60);
        }
        [[nodiscard]] static constexpr duration from_hours(i64_t hours) noexcept {
            return from_minutes(hours * 60);
        }
        [[nodiscard]] static constexpr duration from_days(i64_t days) noexcept {
            return from_hours(days * 24);
        }

        [[nodiscard]] constexpr duration operator+() const noexcept {
            return *this;
        }
        [[nodiscard]] constexpr duration operator-() const noexcept {
            return duration(-nanoseconds_);
        }
        [[nodiscard]] constexpr duration operator+(duration other) const noexcept {
            return duration(nanoseconds_ + other.nanoseconds_);
        }
        [[nodiscard]] constexpr duration operator-(duration other) const noexcept {
            return duration(nanoseconds_ - other.nanoseconds_);
        }
        [[nodiscard]] constexpr duration operator*(i64_t multiplier) const noexcept {
            return duration(nanoseconds_ * multiplier);
        }
        [[nodiscard]] constexpr duration operator/(i64_t divisor) const noexcept {
            return duration(nanoseconds_ / divisor);
        }
        [[nodiscard]] constexpr i64_t operator/(duration other) const noexcept {
            return nanoseconds_ / other.nanoseconds_;
        }
        [[nodiscard]] constexpr i64_t operator*(frequency value) const noexcept {
            return nanoseconds_ * static_cast<i64_t>(value.to_milihz()) /
                   static_cast<i64_t>(NANOSECONDS_PER_MILLIHERTZ);
        }

        [[nodiscard]] friend constexpr bool operator==(duration left,
                                                       duration right) noexcept  = default;
        [[nodiscard]] friend constexpr auto operator<=>(duration left,
                                                        duration right) noexcept = default;
    };

    [[nodiscard]] constexpr time operator+(time point, duration delta) noexcept;
    [[nodiscard]] constexpr time operator+(duration delta, time point) noexcept;
    [[nodiscard]] constexpr time operator-(time point, duration delta) noexcept;

    /**
     * @brief 以纳秒表示、从固定 epoch 起算的非负绝对时刻。
     *
     * 算术和单位换算直接使用 u64_t，不检查结果范围；溢出或下溢会按无符号整数
     * 规则回绕。调用方必须确认回绕符合预期，并保证 time 差值可由 duration 表示。
     */
    struct time {
    protected:
        u64_t nanoseconds_ = 0;
        explicit constexpr time(u64_t nanoseconds) noexcept : nanoseconds_(nanoseconds) {}

    public:
        explicit constexpr time() noexcept = default;
        explicit constexpr operator u64_t() const noexcept {
            return to_nanoseconds();
        }

        [[nodiscard]] constexpr u64_t to_nanoseconds() const noexcept {
            return nanoseconds_;
        }
        [[nodiscard]] constexpr u64_t to_microseconds() const noexcept {
            return nanoseconds_ / 1'000;
        }
        [[nodiscard]] constexpr u64_t to_milliseconds() const noexcept {
            return nanoseconds_ / 1'000'000;
        }
        [[nodiscard]] constexpr u64_t to_seconds() const noexcept {
            return nanoseconds_ / 1'000'000'000;
        }
        [[nodiscard]] constexpr u64_t to_minutes() const noexcept {
            return to_seconds() / 60;
        }
        [[nodiscard]] constexpr u64_t to_hours() const noexcept {
            return to_minutes() / 60;
        }
        [[nodiscard]] constexpr u64_t to_days() const noexcept {
            return to_hours() / 24;
        }

        [[nodiscard]] constexpr time_ymd to_ymd() const noexcept {
            return days_to_ymd(static_cast<i64_t>(to_days()));
        }

        [[nodiscard]] constexpr formatted_time to_formatted_time() const noexcept {
            const auto ymd = to_ymd();
            return formatted_time{
                .year   = ymd.year,
                .month  = ymd.month,
                .day    = ymd.day,
                .hour   = static_cast<i64_t>(to_hours() % 24),
                .minute = static_cast<i64_t>(to_minutes() % 60),
                .second = static_cast<i64_t>(to_seconds() % 60),
            };
        }

        [[nodiscard]] static constexpr time from_nanoseconds(u64_t nanoseconds) noexcept {
            return time(nanoseconds);
        }
        [[nodiscard]] static constexpr time from_microseconds(u64_t microseconds) noexcept {
            return from_nanoseconds(microseconds * 1'000);
        }
        [[nodiscard]] static constexpr time from_milliseconds(u64_t milliseconds) noexcept {
            return from_nanoseconds(milliseconds * 1'000'000);
        }
        [[nodiscard]] static constexpr time from_seconds(u64_t seconds) noexcept {
            return from_nanoseconds(seconds * 1'000'000'000);
        }
        [[nodiscard]] static constexpr time from_minutes(u64_t minutes) noexcept {
            return from_seconds(minutes * 60);
        }
        [[nodiscard]] static constexpr time from_hours(u64_t hours) noexcept {
            return from_minutes(hours * 60);
        }
        [[nodiscard]] static constexpr time from_days(u64_t days) noexcept {
            return from_hours(days * 24);
        }
        [[nodiscard]] static constexpr time from_ymd(time_ymd ymd) noexcept {
            const auto days = ymd_to_days(ymd);
            if (days < 0)
                tay::panic("time cannot represent a date before its epoch");
            return from_days(static_cast<u64_t>(days));
        }
        [[nodiscard]] static constexpr time from_formatted_time(formatted_time value) noexcept {
            return from_ymd(time_ymd{
                       .year  = value.year,
                       .month = value.month,
                       .day   = value.day,
                   }) +
                   duration::from_hours(value.hour) + duration::from_minutes(value.minute) +
                   duration::from_seconds(value.second);
        }
        [[nodiscard]] static constexpr time max() noexcept {
            return time(std::numeric_limits<u64_t>::max());
        }

        [[nodiscard]] friend constexpr bool operator==(time left, time right) noexcept  = default;
        [[nodiscard]] friend constexpr auto operator<=>(time left, time right) noexcept = default;
    };

    [[nodiscard]] constexpr duration operator-(time left, time right) noexcept {
        return duration::from_nanoseconds(
            static_cast<i64_t>(left.to_nanoseconds() - right.to_nanoseconds()));
    }

    [[nodiscard]] constexpr time operator+(time point, duration delta) noexcept {
        return time::from_nanoseconds(point.to_nanoseconds() + delta.to_nanoseconds());
    }

    [[nodiscard]] constexpr time operator+(duration delta, time point) noexcept {
        return point + delta;
    }

    [[nodiscard]] constexpr time operator-(time point, duration delta) noexcept {
        return time::from_nanoseconds(point.to_nanoseconds() - delta.to_nanoseconds());
    }

    /** @brief 由正 duration 计算饱和绝对 deadline。 */
    [[nodiscard]] constexpr time saturated_add(time point, duration delta) noexcept {
        if (delta < duration{})
            tay::panic("saturated time addition requires a non-negative duration");
        const auto magnitude = static_cast<u64_t>(delta.to_nanoseconds());
        return magnitude > std::numeric_limits<u64_t>::max() - point.to_nanoseconds()
                   ? time::max()
                   : time::from_nanoseconds(point.to_nanoseconds() + magnitude);
    }

    /** @brief 根据计数和时间间隔计算频率。 */
    [[nodiscard]] constexpr frequency operator/(u64_t count, duration interval) noexcept {
        if (interval <= duration{})
            tay::panic("frequency calculation requires a positive duration");
        const auto denominator = static_cast<u64_t>(interval.to_nanoseconds());
        return frequency::from_milihz(count * (NANOSECONDS_PER_MILLIHERTZ / denominator));
    }

    /** @brief 根据计数和频率计算时间间隔。 */
    [[nodiscard]] constexpr duration operator/(u64_t count, frequency value) noexcept {
        if (value.to_milihz() == 0)
            tay::panic("duration calculation requires a non-zero frequency");
        const auto factor = NANOSECONDS_PER_MILLIHERTZ / value.to_milihz();
        return duration::from_nanoseconds(static_cast<i64_t>(count * factor));
    }
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

constexpr units::duration operator""_ns(unsigned long long nanoseconds) {
    return units::duration::from_nanoseconds(static_cast<i64_t>(nanoseconds));
}

constexpr units::duration operator""_us(unsigned long long microseconds) {
    return units::duration::from_microseconds(static_cast<i64_t>(microseconds));
}

constexpr units::duration operator""_ms(unsigned long long milliseconds) {
    return units::duration::from_milliseconds(static_cast<i64_t>(milliseconds));
}

constexpr units::duration operator""_s(unsigned long long seconds) {
    return units::duration::from_seconds(static_cast<i64_t>(seconds));
}

constexpr units::duration operator""_min(unsigned long long minutes) {
    return units::duration::from_minutes(static_cast<i64_t>(minutes));
}

constexpr units::duration operator""_h(unsigned long long hours) {
    return units::duration::from_hours(static_cast<i64_t>(hours));
}
