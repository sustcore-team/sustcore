/**
 * @file id.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ID分配器
 * @version alpha-1.0.0
 * @date 2026-01-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

namespace util {
    template<int _Invalid = 0>
    struct IDManager {
    private:
        int D_cnt;
    public:
        static constexpr int INVALID = _Invalid;
        explicit IDManager(int A_def = _Invalid)
            : D_cnt(A_def + 1)
        {
        }
        int get() {
            int id = D_cnt;
            ++ D_cnt;
            return id;
        }
    };
}