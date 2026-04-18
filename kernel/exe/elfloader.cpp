
/**
 * @file elfloader.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ELF文件加载器
 * @version alpha-1.0.0
 * @date 2026-04-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/riscv64/csr.h>
#include <cap/capability.h>
#include <cap/cholder.h>
#include <elf.h>
#include <env.h>
#include <exe/elfloader.h>
#include <kio.h>
#include <mem/kaddr.h>
#include <object/csa.h>
#include <object/vfile.h>
#include <sustcore/addr.h>

#include <cstring>
#include <limits>
#include <string>

namespace key {
    using namespace env::key;
    struct elfloader : public tm {
    public:
        elfloader() = default;
    };
}  // namespace key

namespace loader::elf {
    constexpr const unsigned char ELF_MAGIC[] = {0x7f, 'E', 'L', 'F'};
    constexpr const bool check_magic(const unsigned char *ident) {
        return memcmp(ident, (unsigned char *)ELF_MAGIC, sizeof(ELF_MAGIC)) ==
               0;
    }

    constexpr VMA::Type phdr_to_vma_type(Elf64_Word flags) {
        // 可执行段优先映射为 CODE，其余按可写属性归类到 DATA。
        if ((flags & PF_X) != 0) {
            return VMA::Type::CODE;
        }
        return VMA::Type::DATA;
    }

    inline bool overflow_add_u64(uint64_t a, uint64_t b) {
        return a > (~uint64_t(0) - b);
    }

    Result<void> validate_elf64(const Elf64_Ehdr &ehdr, size_t file_size) {
        if (!check_magic(ehdr.e_ident)) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        if (ehdr.e_ehsize != sizeof(Elf64_Ehdr) ||
            ehdr.e_phentsize != sizeof(Elf64_Phdr) || ehdr.e_phnum == 0)
        {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        // TODO: extends it into different archs according to current running
        // arch.
        if (ehdr.e_machine != EM_RISCV) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        // only support executable files
        // dynamic linking is not supported yet.
        if (ehdr.e_type != ET_EXEC) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        // 检查程序头表是否越界。
        const uint64_t ph_bytes =
            static_cast<uint64_t>(ehdr.e_phnum) * sizeof(Elf64_Phdr);
        if (overflow_add_u64(ehdr.e_phoff, ph_bytes)) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        if (ehdr.e_phoff + ph_bytes > file_size) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        void_return();
    }

    // 将段内内容加载到内存中
    Result<void> loadsegs(VFileOperator &fop, Elf64_Ehdr ehdr) {
        // 解析程序头表并将段内容加载到内存中
        for (size_t i = 0; i < ehdr.e_phnum; ++i) {
            Elf64_Phdr phdr{};
            // 定位到程序头
            off_t offset       = ehdr.e_phoff + i * sizeof(Elf64_Phdr);
            auto read_phdr_res = fop.read_exact(offset, &phdr, sizeof(phdr));
            propagate(read_phdr_res);

            if (phdr.p_type != PT_LOAD) {
                continue;  // 目前仅处理可加载段
            }

            // 读入段内容到内存
            VirAddr segvaddr(phdr.p_vaddr);
            auto read_res =
                fop.read_exact(phdr.p_offset, segvaddr.addr(), phdr.p_filesz);
            propagate(read_res);

            // 如果内存大小大于文件大小，说明需要将剩余部分清零
            if (phdr.p_memsz > phdr.p_filesz) {
                size_t zero_sz   = phdr.p_memsz - phdr.p_filesz;
                void *zero_start = (segvaddr + phdr.p_filesz).addr();
                memset(zero_start, 0, zero_sz);
            }
        }

        void_return();
    }

    Result<void> load(TaskSpec &spec, const LoadPrm &prm) {
        if (spec.tm.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (prm.src_path.empty()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto *vfs = env::inst().vfs();

        // Get file capabilty from CHolder
        auto access_res = spec.holder->access(prm.image_file_cap);
        propagate(access_res);
        VFileOperator fop(access_res.value());

        auto fsz_res = fop.size();
        if (!fsz_res.has_value()) {
            propagate_return(fsz_res);
        }
        const size_t file_size = fsz_res.value();
        if (file_size < sizeof(Elf64_Ehdr)) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        Elf64_Ehdr ehdr{};
        auto read_hdr_res = fop.read_exact(0, &ehdr, sizeof(ehdr));
        propagate(read_hdr_res);

        auto valid_res = validate_elf64(ehdr, file_size);
        propagate(valid_res);

        spec.entrypoint = VirAddr(ehdr.e_entry);

        // 解析程序头表并为TM添加相应的VMA
        for (size_t i = 0; i < ehdr.e_phnum; ++i) {
            Elf64_Phdr phdr{};
            // 定位到程序头
            off_t offset       = ehdr.e_phoff + i * sizeof(Elf64_Phdr);
            auto read_phdr_res = fop.read_exact(offset, &phdr, sizeof(phdr));
            propagate(read_phdr_res);

            if (phdr.p_type != PT_LOAD) {
                continue;  // 目前仅处理可加载段
            }

            // 检查段边界
            if (overflow_add_u64(phdr.p_offset, phdr.p_filesz) ||
                phdr.p_offset + phdr.p_filesz > file_size)
            {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            VirAddr segvaddr(phdr.p_vaddr);

            // 确认该空间地址是用户空间地址
            if (!is_user_vaddr(segvaddr)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            // 为该段在TM中添加一个VMA
            VMA::Type vma_type = phdr_to_vma_type(phdr.p_flags);
            auto add_res = spec.tm->add_vma(vma_type, segvaddr, phdr.p_memsz);
            if (!add_res.has_value()) {
                loggers::SUSTCORE::ERROR("无法为段%d添加VMA: %d", i,
                                         add_res.error());
                while (true);
            }
            add_res->get()->loading = true;  // 标记该VMA正在加载
        }
        // 输出TM中的VMA信息以供调试
        loggers::SUSTCORE::INFO("ELF加载完成，TM中的VMA列表:");
        for (const auto &vma : spec.tm->vma_list) {
            loggers::SUSTCORE::INFO(
                "  VMA类型: %s, 起始地址: %p, 大小: %u B",
                vma.type == VMA::Type::CODE ? "CODE" : "DATA", vma.vaddr.addr(),
                vma.size);
        }

        // 记录当前的TM, 以便稍后恢复
        TM *origin_tm      = env::inst().tm();
        PhyAddr origin_pgd = env::inst().pgd();

        // 切换到加载程序的TM
        env::inst().tm(key::elfloader()) = spec.tm.get();
        // 切换到加载程序的页表
        spec.tm->pman().switch_root();

        // 开始加载段
        auto load_res = loadsegs(fop, ehdr);
        propagate(load_res);

        // 将各个段的VMA的loading标记为false
        // 同时将其对应的内存权限改回正常值
        for (auto &vma : spec.tm->vma_list) {
            vma.loading      = false;
            PageMan::RWX rwx = VMA::seg2rwx(vma.type);
            // TODO: 将 u 设置为 true 以保证其为用户页
            // 但是目前要进行测试，暂时设置为 false 以避免内核态不能访问用户页导致的各种问题
            spec.tm->pman().modify_range_flags<PageMan::make_mask(0b001111)>(
                vma.vaddr, vma.size, rwx, false, false);
        }

        // 输出每个VMA的开头几个字节以供调试
        loggers::SUSTCORE::INFO("每个VMA的前16字节内容:");
        for (const auto &vma : spec.tm->vma_list) {
            ker_paddr::SumGuard sum_guard;  // 确保可以访问用户空间地址
            loggers::SUSTCORE::INFO(
                "  VMA类型: %s, 起始地址: %p",
                vma.type == VMA::Type::CODE ? "CODE" : "DATA",
                vma.vaddr.addr());
            const size_t dump_size = vma.size < 16 ? vma.size : 16;
            const unsigned char *data =
                reinterpret_cast<const unsigned char *>(vma.vaddr.addr());
            std::string hex_dump;
            for (size_t i = 0; i < dump_size; ++i) {
                char buf[4];
                sprintf(buf, "%02x ", data[i]);
                hex_dump += buf;
            }
            loggers::SUSTCORE::INFO("    前%d字节: %s", dump_size,
                                    hex_dump.c_str());
        }

        // 恢复到原来的TM, 以便继续执行内核代码
        env::inst().tm(key::elfloader()) = origin_tm;
        // 恢复到原来的内核页表
        PageMan(origin_pgd).switch_root();

        void_return();
    }

}  // namespace loader::elf
