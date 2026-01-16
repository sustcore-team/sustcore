/**
 * @file elfloader.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ELF加载器
 * @version alpha-1.0.0
 * @date 2025-12-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <basec/logger.h>
#include <elfloader.h>
#include <mem/vmm.h>
#include <string.h>
#include <sus/bits.h>
#include <sus/paging.h>

const char *get_type(Elf64_Half e_type) {
    switch (e_type) {
        case ET_NONE: return "NONE (无文件类型)";
        case ET_REL:  return "REL (可重定位文件)";
        case ET_EXEC: return "EXEC (可执行文件)";
        case ET_DYN:  return "DYN (共享对象文件)";
        case ET_CORE: return "CORE (核心文件)";
        default:      return "UNKNOWN";
    }
}

const char *get_machine(Elf64_Half e_machine) {
    switch (e_machine) {
        case EM_NONE:       return "无特定指令集";
        case EM_386:        return "Intel 80386";
        case EM_MIPS:       return "MIPS";
        case EM_PPC:        return "PowerPC";
        case EM_ARM:        return "ARM";
        case EM_IA64:       return "IA64";
        case EM_AMD_X86_64: return "ARM x86-64";
        case EM_AARCH64:    return "ARM AARCH64";
        case EM_RISCV:      return "RISC-V";
        case EM_LOONGARCH:  return "龙芯";
        default:            return "UNKNOWN";
    }
}

void to_type_string(Elf64_Word p_type, char *buffer) {
    switch (p_type) {
        case PT_NULL:    sprintf(buffer, "NULL"); break;
        case PT_LOAD:    sprintf(buffer, "LOAD"); break;
        case PT_DYNAMIC: sprintf(buffer, "DYNAMIC"); break;
        case PT_INTERP:  sprintf(buffer, "INTERP"); break;
        case PT_NOTE:    sprintf(buffer, "NOTE"); break;
        case PT_SHLIB:   sprintf(buffer, "SHLIB"); break;
        case PT_PHDR:    sprintf(buffer, "PHDR"); break;
        case PT_TLS:     sprintf(buffer, "TLS"); break;
        default:         sprintf(buffer, "UNKNOWN(%p)", p_type); break;
    }
}

void to_rwx_string(Elf64_Word p_flags, char *buffer) {
    buffer[0] = (p_flags & PF_R) ? 'R' : '-';
    buffer[1] = (p_flags & PF_W) ? 'W' : '-';
    buffer[2] = (p_flags & PF_X) ? 'X' : '-';
    buffer[3] = '\0';
}

void display_elf_program(Elf64_Phdr *phdr) {
    // 显示程序头信息
    char type_str[256];
    char rwx_str[8];
    to_type_string(phdr->p_type, type_str);
    to_rwx_string(phdr->p_flags, rwx_str);
    log_info("    类型: %s", type_str);
    log_info("    偏移: 0x%lx", phdr->p_offset);
    log_info("    VADDR: 0x%lx", phdr->p_vaddr);
    log_info("    PADDR: 0x%lx", phdr->p_paddr);
    log_info("    文件大小: 0x%lx", phdr->p_filesz);
    log_info("    内存大小: 0x%lx", phdr->p_memsz);
    log_info("    标志: %s", rwx_str);
    log_info("    对齐: 0x%lx", phdr->p_align);
}

void display_elf_section(Elf64_Shdr *shdr) {
    // 显示节头信息
    char rwx_str[8];
    to_rwx_string((Elf64_Word)shdr->sh_flags, rwx_str);
    log_info("    名称索引: %d", shdr->sh_name);
    log_info("    类型: 0x%x", shdr->sh_type);
    log_info("    标志: %s", rwx_str);
    log_info("    地址: 0x%lx", shdr->sh_addr);
    log_info("    偏移: 0x%lx", shdr->sh_offset);
    log_info("    大小: 0x%lx", shdr->sh_size);
    log_info("    链接: %d", shdr->sh_link);
    log_info("    信息: %d", shdr->sh_info);
    log_info("    对齐: 0x%lx", shdr->sh_addralign);
    log_info("    条目大小: 0x%lx", shdr->sh_entsize);
}

void display_elf_headers(Elf64_Ehdr *header) {
    // 显示ELF头信息
    log_info("ELF Header:");
    // 首先显示magic number
    log_info("  魔数: %02x %02x %02x %02x %02x %02x %02x %02x",
             header->e_ident[0], header->e_ident[1], header->e_ident[2],
             header->e_ident[3], header->e_ident[4], header->e_ident[5],
             header->e_ident[6], header->e_ident[7]);
    // 再显示文件类型
    log_info("  类型: %s", get_type(header->e_type));

    // 再显示目标架构
    log_info("  机器架构: %s", get_machine(header->e_machine));

    // 文件版本
    log_info("  版本: 0x%x", header->e_version);

    // 显示入口点地址
    log_info("  入口点地址: %p", header->e_entry);

    // 显示程序头表偏移量
    log_info("  程序头表偏移: %p", header->e_phoff);
    // 显示程序头表项大小和数量
    log_info("  程序头表项大小: %d", header->e_phentsize);
    log_info("  程序头表项数量: %d", header->e_phnum);

    void *addr = (void *)header;

    // 显示各个表的信息
    for (int i = 0; i < header->e_phnum; i++) {
        Elf64_Phdr *pro_header =
            (Elf64_Phdr *)(addr + header->e_phoff + i * header->e_phentsize);
        log_info("  程序头表[%d] (@%p):", i, pro_header);
        display_elf_program(pro_header);
    }

    // 显示节头表偏移量
    log_info("  节头表偏移: %p", header->e_shoff);
    // 显示节头表项大小和数量
    log_info("  节头表项大小: %d", header->e_shentsize);
    log_info("  节头表项数量: %d", header->e_shnum);

    // 显示各个节头的信息
    for (int i = 0; i < header->e_shnum; i++) {
        Elf64_Shdr *sec_header =
            (Elf64_Shdr *)(addr + header->e_shoff + i * header->e_shentsize);
        log_info("  节头表[%d] (@%p):", i, sec_header);
        display_elf_section(sec_header);
    }
}

int rwx_to_page_mode(bool r, bool w, bool x) {
    // 目前只支持 RO, RW, RX, RWX 四种权限组合
    if (r && !w && !x) {
        return RWX_MODE_R;
    } else if (r && w && !x) {
        return RWX_MODE_RW;
    } else if (r && !w && x) {
        return RWX_MODE_RX;
    } else if (r && w && x) {
        return RWX_MODE_RWX;
    } else {
        log_error("rwx_to_page_mode: 不支持的权限组合 r=%d, w=%d, x=%d", r, w,
                  x);
        return -1;
    }
}

void load_elf_segment(Elf64_Phdr *phdr, void *elf_base_addr,
                      program_info *prog_info) {
    if (prog_info->tm->pgd == nullptr) {
        log_error("load_elf_segment: program_info的pgd未设置");
        return;
    }

    // 首先判断该段是否为LOAD类型
    if (phdr->p_type != PT_LOAD) {
        return;
    }

    // 对于LOAD类型的段, 根据其权限设置页表项
    bool r = phdr->p_flags & PF_R;
    bool w = phdr->p_flags & PF_W;
    bool x = phdr->p_flags & PF_X;

    int rwx_code = rwx_to_page_mode(r, w, x);
    if (rwx_code == -1) {
        log_error("load_elf_segment: 无法解析段权限 p_flags=0x%x",
                  phdr->p_flags);
        return;
    }

    if (!(r || w || x)) {
        log_error("load_elf_segment: LOAD段没有任何权限 p_flags=0x%x",
                  phdr->p_flags);
        return;
    }

    VMAType type = VMAT_NONE;

    if (r & w & !x) {
        type = VMAT_DATA;
    } else if (r & !w & x) {
        type = VMAT_CODE;
    }

    // 根据rwx权限向TM中添加VMA
    add_vma(prog_info->tm, (void *)(phdr->p_vaddr), phdr->p_memsz, type);
    // 分配内存存放该段
    alloc_pages_for(prog_info->tm, (void *)(phdr->p_vaddr), SIZE2PAGES(phdr->p_memsz), rwx_code,
                    true);

    // 计算源地址与目标地址
    void *src_addr  = (void *)((umb_t)elf_base_addr + phdr->p_offset);
    void *dest_addr = (void *)(phdr->p_vaddr);

    // 复制文件段内容到内存
    memcpy_k2u(prog_info->tm, dest_addr, src_addr, phdr->p_filesz);

    char rwx_str[8];
    to_rwx_string(phdr->p_flags, rwx_str);
    log_info("加载段(rwx=%s, type=PT_LOAD)到虚拟内存地址[%p, %p)", rwx_str,
             dest_addr, (void *)((umb_t)dest_addr + phdr->p_memsz));

    // 如果内存大小大于文件大小, 则将剩余部分清零
    if (phdr->p_memsz > phdr->p_filesz) {
        memset_u(prog_info->tm, (void *)((umb_t)dest_addr + phdr->p_filesz), 0,
                 phdr->p_memsz - phdr->p_filesz);
    }

    // 更新program_start与program_end
    prog_info->program_start = ((umb_t)prog_info->program_start) > phdr->p_vaddr
                                   ? (void *)(phdr->p_vaddr)
                                   : prog_info->program_start;
    prog_info->program_end =
        ((umb_t)prog_info->program_end) < (phdr->p_vaddr + phdr->p_memsz)
            ? (void *)(phdr->p_vaddr + phdr->p_memsz)
            : prog_info->program_end;
}

program_info load_elf_program(void *elf_base_addr) {
    // 初始化程序信息
    program_info __prog_info;
    program_info *prog_info = &__prog_info;
    memset(prog_info, 0, sizeof(program_info));

    Elf64_Ehdr *header = (Elf64_Ehdr *)elf_base_addr;

    // 首先验证ELF魔数
    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F')
    {
        log_error("load_elf_program: 非法的ELF文件魔数");
        return __prog_info;
    }

    // 验证文件类型是否为可执行文件
    if (header->e_type != ET_EXEC) {
        log_error("load_elf_program: 非可执行文件类型 e_type=0x%x",
                  header->e_type);
        return __prog_info;
    }

    // 验证目标架构是否为当前架构
    if (header->e_machine != EM_CURRENT_ARCH) {
        log_error("load_elf_program: 不支持的目标架构 e_machine=0x%x",
                  header->e_machine);
        return __prog_info;
    }

    // 分配页表
    prog_info->tm = setup_task_memory();
    if (prog_info->tm->pgd == nullptr) {
        log_error("load_elf_program: 无法分配页表");
        return __prog_info;
    }

    // 初始化program_start与program_end
    prog_info->program_start = (void *)0xFFFFFFFFFFFFFFFF;
    prog_info->program_end   = (void *)0x0;

    // 遍历程序头表
    for (int i = 0; i < header->e_phnum; i++) {
        Elf64_Phdr *phdr =
            (Elf64_Phdr *)((umb_t)elf_base_addr + header->e_phoff +
                           i * header->e_phentsize);
        load_elf_segment(phdr, elf_base_addr, prog_info);
    }

    // 设置入口点
    prog_info->entrypoint = (void *)(header->e_entry);

    return __prog_info;
}