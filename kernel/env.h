/**
 * @file env.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief environment
 * @version alpha-1.0.0
 * @date 2026-04-06
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <mem/vma.h>
#include <sustcore/addr.h>

struct Environment
{
    PhyAddr pgd = PhyAddr::null;
    TM *tm;
};

Environment &env();