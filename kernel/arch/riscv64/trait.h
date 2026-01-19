/**
 * @file trait.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Riscv64架构Trait
 * @version alpha-1.0.0
 * @date 2026-01-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

class Riscv64Serial {
public:
    static void serial_write_char(char ch);
    static void serial_write_string(const char *str);
};

typedef Riscv64Serial ArchSerial;

class Riscv64Initialization {
public:
    static void pre_init(void);
    static void post_init(void);
};

typedef Riscv64Initialization ArchInitialization;