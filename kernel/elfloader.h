/**
 * @file elfloader.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ELF文件加载器
 * @version alpha-1.0.0
 * @date 2025-12-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <elf.h>
#include <mem/vmm.h>

/**
 * @brief 显示ELF文件头信息
 *
 * @param header ELF文件头地址
 */
void display_elf_headers(Elf64_Ehdr *header);

typedef struct {
    // 进程内存信息
    TM *tm;
    // 入口点
    void *entrypoint;
    // 程序整体
    // 即 [program_start, program_end)是覆盖程序各个段的最小区间
    void *program_start, *program_end;
} program_info;

/**
 * @brief 加载ELF段到内存
 *
 * @param phdr 段头指针
 * @param elf_base_addr ELF文件基地址
 * @param prog_info 程序信息指针
 */
void load_elf_segment(Elf64_Phdr *phdr, void *elf_base_addr,
                      program_info *prog_info);

/**
 * @brief 加载ELF可执行文件
 *
 * @param elf_base_addr ELF文件基地址
 * @return program_info 加载后的程序信息
 */
program_info load_elf_program(void *elf_base_addr);