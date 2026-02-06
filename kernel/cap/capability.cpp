/**
 * @file capability.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力系统
 * @version alpha-1.0.0
 * @date 2026-02-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cap/capability.h>

void CSpaceBase::retain(void) {
    this->_ref_count++;
}

void CSpaceBase::release(void) {
    this->_ref_count--;
    if (this->_ref_count == 0) {
        on_zero_ref();
    }
}