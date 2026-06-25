
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

#include <cap/capability.h>
#include <cap/cholder.h>
#include <elf.h>
#include <exe/elfloader.h>
#include <logger.h>
#include <mem/vma.h>
#include <object/memory.h>
#include <object/perm.h>
#include <sustcore/addr.h>
#include <vfs/vfs.h>

#include <cstring>
#include <limits>
#include <string>

namespace loader::elf {
    namespace {
        constexpr size_t SEGMENT_IO_CHUNK_SIZE = PAGESIZE;

        struct RuntimeLoadSegment {
            size_t index;
            Elf64_Phdr phdr;
            VirAddr runtime_vaddr;
            VirAddr map_begin;
            VirAddr map_end;
            size_t page_prefix;
        };

#if defined(__ARCH_riscv64__)
        constexpr Elf64_Half SUPPORTED_MACHINE = EM_RISCV;
#elif defined(__ARCH_loongarch64__)
        constexpr Elf64_Half SUPPORTED_MACHINE = EM_LOONGARCH;
#else
#error "Unsupported architecture for ELF loader"
#endif

        /**
         * @brief 判断程序头是否表示可加载段.
         */
        [[nodiscard]]
        constexpr bool is_load_segment(const Elf64_Phdr &phdr) noexcept {
            return phdr.p_type == PT_LOAD;
        }

        [[nodiscard]]
        constexpr bool is_interp_segment(const Elf64_Phdr &phdr) noexcept {
            return phdr.p_type == PT_INTERP;
        }

        [[nodiscard]]
        constexpr bool is_phdr_segment(const Elf64_Phdr &phdr) noexcept {
            return phdr.p_type == PT_PHDR;
        }

        /**
         * @brief 计算两个 size_t 中的较小值.
         */
        [[nodiscard]]
        constexpr size_t min_size(size_t lhs, size_t rhs) noexcept {
            return lhs < rhs ? lhs : rhs;
        }

        /**
         * @brief 定位与段起始地址对应的 VMA.
         *
         * @param tmm 目标任务内存管理器.
         * @param segvaddr 段起始虚拟地址.
         * @param memsz 段内存大小.
         * @return 命中的 VMA.
         */
        [[nodiscard]]
        Result<std::reference_wrapper<VMA>> locate_segment_vma(
            TaskMemoryManager &tmm, VirAddr segvaddr, size_t memsz) {
            auto locate_res = tmm.locate(segvaddr);
            propagate(locate_res);
            VMA *vma = locate_res.value();
            if (vma == nullptr || vma->memory_payload() == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            size_t vma_offset = segvaddr - vma->varea.begin;
            if (vma_offset > vma->size() || memsz > vma->size() - vma_offset) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return std::ref(*vma);
        }

        /**
         * @brief 读取文件数据并分块写入 MemoryPayload.
         *
         * @param fop ELF 文件对象.
         * @param memory 目标 payload.
         * @param file_offset 文件偏移.
         * @param mem_offset payload 内偏移.
         * @param len 需要写入的总长度.
         */
        [[nodiscard]]
        Result<size_t> read_exact(VFile &file, off_t offset, void *buf,
                                  size_t len) {
            auto read_res = VFS::inst().read(file, offset, buf, len);
            propagate(read_res);
            if (read_res.value() != len) {
                unexpect_return(ErrCode::IO_ERROR);
            }
            return len;
        }

        Result<void> load_file_into_memory(VFile &file, cap::MemoryPayload &memory,
                                           off_t file_offset,
                                           size_t mem_offset, size_t len) {
            char buffer[SEGMENT_IO_CHUNK_SIZE];
            size_t loaded = 0;
            while (loaded < len) {
                size_t chunk = min_size(len - loaded, sizeof(buffer));
                auto read_res = read_exact(file, file_offset + loaded, buffer,
                                           chunk);
                propagate(read_res);
                auto write_res = memory.write(mem_offset + loaded, buffer, chunk);
                propagate(write_res);
                loggers::ELFLOADER::DEBUG(
                    "ELF段文件写入: file_off=%lu mem_off=%lu chunk=%lu",
                    static_cast<unsigned long>(file_offset + loaded),
                    mem_offset + loaded, chunk);
                loaded += chunk;
            }
            void_return();
        }

        /**
         * @brief 将 payload 指定范围按块清零.
         *
         * @param memory 目标 payload.
         * @param mem_offset payload 内偏移.
         * @param len 需要清零的总长度.
         */
        [[nodiscard]]
        Result<void> zero_fill_memory(cap::MemoryPayload &memory,
                                      size_t mem_offset, size_t len) {
            char zeros[SEGMENT_IO_CHUNK_SIZE] = {};
            size_t filled                     = 0;
            while (filled < len) {
                size_t chunk = min_size(len - filled, sizeof(zeros));
                auto write_res =
                    memory.write(mem_offset + filled, zeros, chunk);
                propagate(write_res);
                loggers::ELFLOADER::DEBUG(
                    "ELF段清零: mem_off=%lu chunk=%lu", mem_offset + filled,
                    chunk);
                filled += chunk;
            }
            void_return();
        }

        /**
         * @brief 读取并打印 VMA 开头若干字节内容.
         *
         * @param vma 目标 VMA.
         * @param dump_size 最大打印字节数.
         */
        [[nodiscard]]
        Result<void> dump_vma_prefix(const VMA &vma, size_t dump_size) {
            auto *memory = vma.memory_payload();
            if (memory == nullptr || vma.varea.nullable() || dump_size == 0) {
                void_return();
            }

            unsigned char buf[16] = {};
            size_t actual_size    = min_size(dump_size, sizeof(buf));
            auto read_res = memory->read(vma.mem_offset, buf, actual_size);
            propagate(read_res);
            actual_size = read_res.value();

            std::string hex_dump;
            for (size_t i = 0; i < actual_size; ++i) {
                char item[4];
                sprintf(item, "%02x ", buf[i]);
                hex_dump += item;
            }

            loggers::ELFLOADER::DEBUG("  VMA类型: %s, 起始地址: %p",
                                     to_string(vma.type),
                                     vma.varea.begin.addr());
            loggers::ELFLOADER::DEBUG("    前%u字节: %s", actual_size,
                                     hex_dump.c_str());
            void_return();
        }
    }  // namespace

    constexpr const unsigned char ELF_MAGIC[] = {0x7f, 'E', 'L', 'F'};
    constexpr const bool check_magic(const unsigned char *ident) noexcept {
        return memcmp(ident, (unsigned char *)ELF_MAGIC, sizeof(ELF_MAGIC)) ==
               0;
    }

    constexpr VMA::Type phdr_to_vma_type(Elf64_Word flags) noexcept {
        const bool exec  = (flags & PF_X) != 0;
        return exec ? VMA::Type::CODE : VMA::Type::DATA;
    }

    constexpr VMA::Prot phdr_to_vma_prot(Elf64_Word flags) noexcept {
        VMA::Prot prot = 0;
        if ((flags & PF_R) != 0) {
            prot |= VMA::PROT_R;
        }
        if ((flags & PF_W) != 0) {
            prot |= VMA::PROT_W;
        }
        if ((flags & PF_X) != 0) {
            prot |= VMA::PROT_X;
        }
        return prot;
    }

    inline bool overflow_add_u64(uint64_t a, uint64_t b) noexcept {
        return a > (~uint64_t(0) - b);
    }

    /**
     * @brief 校验 ELF64 文件头及程序头表边界.
     */
    [[nodiscard]]
    Result<void> validate_elf64(const Elf64_Ehdr &ehdr, size_t file_size,
                                bool accept_dyn) noexcept {
        if (!check_magic(ehdr.e_ident)) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        if (ehdr.e_ehsize != sizeof(Elf64_Ehdr) ||
            ehdr.e_phentsize != sizeof(Elf64_Phdr) || ehdr.e_phnum == 0)
        {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        if (ehdr.e_machine != SUPPORTED_MACHINE) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        if (ehdr.e_type != ET_EXEC &&
            !(accept_dyn && ehdr.e_type == ET_DYN))
        {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        // 检查程序头表是否越界. 
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

    /**
     * @brief 将所有 PT_LOAD 段内容写入对应的 MemoryPayload.
     */
    [[nodiscard]]
    Result<void> loadsegs(VFile &file, TaskMemoryManager &tmm,
                          const std::vector<RuntimeLoadSegment> &segments) {
        for (const auto &segment : segments) {
            auto vma_res = locate_segment_vma(
                tmm, segment.map_begin,
                static_cast<size_t>(segment.map_end - segment.map_begin));
            propagate(vma_res);
            VMA &vma = vma_res.value().get();
            size_t mem_offset = vma.mem_offset + segment.page_prefix;

            loggers::ELFLOADER::DEBUG(
                "加载ELF段: idx=%u map=[%p,%p) vaddr=%p filesz=%lu memsz=%lu "
                "prefix=%lu mem_off=%lu",
                segment.index, segment.map_begin.addr(),
                segment.map_end.addr(), segment.runtime_vaddr.addr(),
                static_cast<unsigned long>(segment.phdr.p_filesz),
                static_cast<unsigned long>(segment.phdr.p_memsz),
                static_cast<unsigned long>(segment.page_prefix), mem_offset);

            auto *memory = vma.memory_payload();
            if (memory == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            auto load_res = load_file_into_memory(
                file, *memory, segment.phdr.p_offset, mem_offset,
                segment.phdr.p_filesz);
            propagate(load_res);

            if (segment.phdr.p_memsz > segment.phdr.p_filesz) {
                size_t zero_sz =
                    segment.phdr.p_memsz - segment.phdr.p_filesz;
                auto zero_res =
                    zero_fill_memory(*memory, mem_offset + segment.phdr.p_filesz,
                                     zero_sz);
                propagate(zero_res);
            }
            auto first_page_res = memory->lookup_page(mem_offset);
            if (first_page_res.has_value()) {
                loggers::ELFLOADER::DEBUG(
                    "ELF段首物理页: mem=%p mem_off=%lu paddr=%p", memory,
                    mem_offset, first_page_res.value().addr());
            }
        }

        void_return();
    }

    Result<void> load_segments(TaskSpec &spec, const LoadPrm &prm,
                               bool create_heap) {
        return load_segments(spec, prm, create_heap,
                             VirAddr(static_cast<addr_t>(0)), false, nullptr);
    }

    Result<void> load_segments(TaskSpec &spec, const LoadPrm &prm,
                               bool create_heap, VirAddr load_base,
                               bool accept_dyn, std::string *interp_path) {
        if (spec.tmm.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (prm.src_path.empty()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        // Get file capabilty from CHolder
        auto access_res = spec.holder->access(prm.image_file_cap);
        propagate(access_res);
        auto *file = access_res.value()->payload_as<VFile>();
        if (file == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (!access_res.value()->imply(perm::vfile::EXEC)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        auto fsz_res = VFS::inst().size(*file);
        if (!fsz_res.has_value()) {
            propagate_return(fsz_res);
        }
        const size_t file_size = fsz_res.value();
        if (file_size < sizeof(Elf64_Ehdr)) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        Elf64_Ehdr ehdr{};
        auto read_hdr_res = read_exact(*file, 0, &ehdr, sizeof(ehdr));
        propagate(read_hdr_res);

        auto valid_res = validate_elf64(ehdr, file_size, accept_dyn);
        propagate(valid_res);
        const bool dyn_image = ehdr.e_type == ET_DYN;
        spec.dyn            = dyn_image;
        spec.load_base      = dyn_image ? load_base
                                        : VirAddr(static_cast<addr_t>(0));
        spec.phdr_num       = ehdr.e_phnum;
        spec.phdr_entsize   = ehdr.e_phentsize;
        spec.program_entrypoint =
            dyn_image ? VirAddr(load_base.arith() + ehdr.e_entry)
                      : VirAddr(ehdr.e_entry);
        spec.entrypoint = spec.program_entrypoint;

        addr_t max_pload_end = 0;
        bool has_pt_phdr     = false;
        bool phdr_covered    = false;
        std::vector<RuntimeLoadSegment> runtime_segments{};

        // 解析程序头表并为TM添加相应的VMA
        for (size_t i = 0; i < ehdr.e_phnum; ++i) {
            Elf64_Phdr phdr{};
            off_t offset       = ehdr.e_phoff + i * sizeof(Elf64_Phdr);
            auto read_phdr_res = read_exact(*file, offset, &phdr,
                                            sizeof(phdr));
            propagate(read_phdr_res);

            if (is_interp_segment(phdr) && interp_path != nullptr) {
                std::string interp(phdr.p_filesz, '\0');
                auto interp_read_res =
                    read_exact(*file, phdr.p_offset, interp.data(),
                               phdr.p_filesz);
                propagate(interp_read_res);
                if (!interp.empty() && interp.back() == '\0') {
                    interp.pop_back();
                }
                *interp_path = std::move(interp);
            }

            if (is_phdr_segment(phdr)) {
                has_pt_phdr   = true;
                spec.phdr_vaddr = dyn_image
                                      ? VirAddr(load_base.arith() + phdr.p_vaddr)
                                      : VirAddr(phdr.p_vaddr);
            }

            if (!is_load_segment(phdr)) {
                continue;
            }

            // 检查段边界
            if (overflow_add_u64(phdr.p_offset, phdr.p_filesz) ||
                phdr.p_offset + phdr.p_filesz > file_size)
            {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            if (overflow_add_u64(phdr.p_vaddr, phdr.p_memsz)) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            VirAddr segvaddr(dyn_image ? load_base.arith() + phdr.p_vaddr
                                       : phdr.p_vaddr);
            VirAddr aligned_segvaddr = segvaddr.page_align_down();
            size_t page_prefix =
                static_cast<size_t>(segvaddr - aligned_segvaddr);
            size_t map_memsz = page_align_up(
                static_cast<size_t>(phdr.p_memsz) + page_prefix);
            VirAddr segvend = aligned_segvaddr + map_memsz;

            // 确认该空间地址是用户空间地址
            if (!is_user_vaddr(aligned_segvaddr) || !is_user_vaddr(segvend)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            // 为该段在TM中添加一个VMA
            VMA::Type vma_type = phdr_to_vma_type(phdr.p_flags);
            VMA::Prot vma_prot = phdr_to_vma_prot(phdr.p_flags);
            auto *segment_mem  = new cap::MemoryPayload(
                map_memsz, false, false, VMA::Growth::FIXED);
            auto add_res = spec.tmm->add_vma(
                vma_type, VMA::Growth::FIXED,
                VirArea(aligned_segvaddr, segvend),
                segment_mem, vma_prot);
            if (!add_res.has_value()) {
                delete segment_mem;
                loggers::ELFLOADER::ERROR("无法为段%d添加VMA: %d", i,
                                         add_res.error());
                while (true);
            }
            loggers::ELFLOADER::DEBUG(
                "创建ELF VMA: type=%s area=[%p,%p) mem=%p memsz=%lu mem_off=%lu",
                to_string(vma_type), aligned_segvaddr.addr(), segvend.addr(),
                segment_mem, static_cast<unsigned long>(map_memsz), 0UL);

            runtime_segments.push_back(RuntimeLoadSegment{
                .index        = i,
                .phdr         = phdr,
                .runtime_vaddr = segvaddr,
                .map_begin     = aligned_segvaddr,
                .map_end       = segvend,
                .page_prefix   = page_prefix,
            });

            if (has_pt_phdr && spec.phdr_vaddr.nonnull() &&
                spec.phdr_vaddr >= aligned_segvaddr &&
                spec.phdr_vaddr + ehdr.e_phnum * ehdr.e_phentsize <= segvend)
            {
                phdr_covered = true;
            }

            if (segvend.arith() > max_pload_end) {
                max_pload_end = segvend.arith();
            }
        }

        if (dyn_image && has_pt_phdr && !phdr_covered) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (create_heap) {
            spec.heap_vaddr = VirAddr(max_pload_end).page_align_up();
        }
        if (!create_heap) {
            spec.linuxss_image_end = VirAddr(max_pload_end).page_align_up();
        }

        if (create_heap) {
            VirAddr heap_start = spec.heap_vaddr;
            auto *heap_mem =
                new cap::MemoryPayload(0, false, false, VMA::Growth::FLEXUP);
            auto heap_cap_res = spec.holder->insert_to_free(heap_mem);
            if (!heap_cap_res.has_value()) {
                delete heap_mem;
                propagate_return(heap_cap_res);
            }
            auto heap_res = spec.tmm->add_vma(
                VMA::Type::HEAP, VMA::Growth::FLEXUP,
                VirArea(heap_start, heap_start), heap_mem,
                VMA::PROT_R | VMA::PROT_W);
            if (!heap_res.has_value()) {
                loggers::ELFLOADER::ERROR("无法初始化堆VMA: %d",
                                         heap_res.error());
                propagate_return(heap_res);
            }
            loggers::ELFLOADER::DEBUG(
                "创建HEAP VMA: area=[%p,%p) mem=%p memsz=%lu",
                heap_start.addr(), heap_start.addr(), heap_mem, 0UL);
            spec.heap_vaddr   = heap_start;
            spec.heap_mem_cap = heap_cap_res.value();
        }

        loggers::ELFLOADER::DEBUG("ELF加载完成, TM中的VMA列表:");
        for (const auto &vma : spec.tmm->vmas()) {
            loggers::ELFLOADER::DEBUG("  VMA类型: %s, 地址: %p~%p, 大小: %u B",
                                    to_string(vma.type), vma.varea.begin.addr(),
                                    vma.varea.end.addr(), vma.size());
        }

        auto load_res = loadsegs(*file, *spec.tmm, runtime_segments);
        propagate(load_res);

        for (auto &vma : spec.tmm->vmas()) {
            PageMan::RWX rwx = VMA::prot_to_rwx(vma.prot);
            spec.tmm->pman().modify_range_flags<PageMan::make_mask(0b001111)>(
                vma.varea.begin, vma.size(),
                PageMan::page_flags(rwx, true, false));
        }

        loggers::ELFLOADER::DEBUG("每个VMA的前16字节内容:");
        for (const auto &vma : spec.tmm->vmas()) {
            if (vma.varea.nullable()) {
                continue;
            }
            auto dump_res = dump_vma_prefix(vma, 16);
            propagate(dump_res);
        }

        void_return();
    }

    Result<void> load(TaskSpec &spec, const LoadPrm &prm) {
        return load_segments(spec, prm, true);
    }

}  // namespace loader::elf
