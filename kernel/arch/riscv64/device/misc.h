/**
 * @file misc.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 杂项
 * @version alpha-1.0.0
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

/**
 * @brief 获得时钟频率
 *
 * @return int 时钟频率(Hz)
 */
int get_clock_freq_hz(void);

struct TimerInfo {
    int freq;
    int expected_freq;
    int increasment;
};

extern TimerInfo timer_info;

/**
* @brief 初始化计时器
*
* @param freq 计时器频率(Hz)
* @param expected_freq 期望频率(mHz(10^-3 Hz))
*/
void init_timer(int freq, int expected_freq);