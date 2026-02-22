/**
 * @file units.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 单位
 * @version alpha-1.0.0
 * @date 2026-02-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

namespace units {
    // 实际上, 这个结构体只是一个uint64_t
    struct frequency {
    protected:
        uint64_t milihertz;
        explicit constexpr frequency(uint64_t mili_hz) : milihertz(mili_hz) {}

    public:
        explicit constexpr frequency() : milihertz(0) {}
        constexpr operator uint64_t() const {
            return to_hz();
        }

        constexpr uint64_t to_milihz() const {
            return milihertz;
        }
        constexpr uint64_t to_hz() const {
            return milihertz / 1'000;
        }
        constexpr uint64_t to_khz() const {
            return to_hz() / 1'000;
        }
        constexpr uint64_t to_mhz() const {
            return to_khz() / 1'000;
        }
        constexpr uint64_t to_ghz() const {
            return to_mhz() / 1'000;
        }

        static constexpr frequency from_milihz(uint64_t h) {
            return frequency(h);
        }
        static constexpr frequency from_hz(uint64_t h) {
            return from_milihz(h * 1'000);
        }
        static constexpr frequency from_khz(uint64_t kh) {
            return from_hz(kh * 1'000);
        }
        static constexpr frequency from_mhz(uint64_t mh) {
            return from_khz(mh * 1'000);
        }
        static constexpr frequency from_ghz(uint64_t gh) {
            return from_mhz(gh * 1'000);
        }

        constexpr frequency operator+(const frequency &other) const {
            return frequency(milihertz + other.milihertz);
        }

        constexpr frequency operator-(const frequency &other) const {
            return frequency(milihertz - other.milihertz);
        }

        constexpr frequency operator*(uint64_t multiplier) const {
            return frequency(milihertz * multiplier);
        }

        constexpr frequency operator/(uint64_t divisor) const {
            return frequency(milihertz / divisor);
        }

        constexpr uint64_t operator/(const frequency &other) const {
            return milihertz / other.milihertz;
        }
    };

    // 实际上, 这个结构体只是一个uint64_t
    struct tick {
    protected:
        uint64_t ticks;
        explicit constexpr tick(uint64_t t) : ticks(t) {}

    public:
        explicit constexpr tick() : ticks(0) {}
        constexpr operator uint64_t() const {
            return ticks;
        }

        constexpr uint64_t to_ticks() const {
            return ticks;
        }
        static constexpr tick from_ticks(uint64_t t) {
            return tick(t);
        }

        constexpr tick operator+(const tick &other) const {
            return tick(ticks + other.ticks);
        }

        constexpr tick operator-(const tick &other) const {
            return tick(ticks - other.ticks);
        }

        constexpr tick operator*(uint64_t multiplier) const {
            return tick(ticks * multiplier);
        }

        constexpr tick operator/(uint64_t divisor) const {
            return tick(ticks / divisor);
        }

        constexpr uint64_t operator/(const tick &other) const {
            return ticks / other.ticks;
        }
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

constexpr units::tick operator""_ticks(unsigned long long t) {
    return units::tick::from_ticks(t);
}