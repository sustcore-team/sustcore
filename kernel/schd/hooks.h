/**
 * @file hooks.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 信息收集钩
 * @version alpha-1.0.0
 * @date 2026-02-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cstddef>
#include <concepts>
#include <schd/metadata.h>

namespace schd {
    namespace hooks {
        template <typename TCBType>
        void on_tick(TCBType *thread, size_t gap_ticks) {
            using Tags = typename TCBType::MetadataType::Tags;
            if constexpr (std::derived_from<Tags, tags::on_tick>) {
                thread->on_tick(gap_ticks);
            }
        }
    }
}