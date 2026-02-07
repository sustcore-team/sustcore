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
        uint64_t hertz;

        explicit constexpr frequency(uint64_t h = 0) : hertz(h) {}
        constexpr operator uint64_t() const { return hertz; }

        constexpr uint64_t to_hz() const { return hertz; }
        constexpr uint64_t to_khz() const { return hertz / 1'000; }
        constexpr uint64_t to_mhz() const { return hertz / 1'000'000; }
        constexpr uint64_t to_ghz() const { return hertz / 1'000'000'000; }

        static constexpr frequency from_hz(uint64_t h) { return frequency(h); }
        static constexpr frequency from_khz(uint64_t kh) { return frequency(kh * 1'000); }
        static constexpr frequency from_mhz(uint64_t mh) { return frequency(mh * 1'000'000); }
        static constexpr frequency from_ghz(uint64_t gh) { return frequency(gh * 1'000'000'000); }
    };
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