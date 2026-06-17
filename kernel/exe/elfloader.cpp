
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

        /**
         * @brief 判断程序头是否表示可加载段.
         */
        [[nodiscard]]
        constexpr bool is_load_segment(const Elf64_Phdr &phdr) noexcept {
            return phdr.p_type == PT_LOAD;
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
            if (vma == nullptr || vma->memory == nullptr) {
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
                loggers::SUSTCORE::DEBUG(
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
                loggers::SUSTCORE::DEBUG(
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
            if (vma.memory == nullptr || vma.varea.nullable() || dump_size == 0) {
                void_return();
            }

            unsigned char buf[16] = {};
            size_t actual_size    = min_size(dump_size, sizeof(buf));
            auto read_res = vma.memory->read(vma.mem_offset, buf, actual_size);
            propagate(read_res);
            actual_size = read_res.value();

            std::string hex_dump;
            for (size_t i = 0; i < actual_size; ++i) {
                char item[4];
                sprintf(item, "%02x ", buf[i]);
                hex_dump += item;
            }

            loggers::SUSTCORE::DEBUG("  VMA类型: %s, 起始地址: %p",
                                     to_string(vma.type),
                                     vma.varea.begin.addr());
            loggers::SUSTCORE::DEBUG("    前%u字节: %s", actual_size,
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
        // 可执行段优先映射为 CODE, 其余按可写属性归类到 DATA. 
        if ((flags & PF_X) != 0) {
            return VMA::Type::CODE;
        }
        return VMA::Type::DATA;
    }

    inline bool overflow_add_u64(uint64_t a, uint64_t b) noexcept {
        return a > (~uint64_t(0) - b);
    }

    /**
     * @brief 校验 ELF64 文件头及程序头表边界.
     */
    [[nodiscard]]
    Result<void> validate_elf64(const Elf64_Ehdr &ehdr, size_t file_size) noexcept {
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
                          const Elf64_Ehdr &ehdr) {
        for (size_t i = 0; i < ehdr.e_phnum; ++i) {
            Elf64_Phdr phdr{};
            off_t offset       = ehdr.e_phoff + i * sizeof(Elf64_Phdr);
            auto read_phdr_res = read_exact(file, offset, &phdr, sizeof(phdr));
            propagate(read_phdr_res);

            if (!is_load_segment(phdr)) {
                continue;
            }

            VirAddr segvaddr(phdr.p_vaddr);
            auto vma_res = locate_segment_vma(tmm, segvaddr, phdr.p_memsz);
            propagate(vma_res);
            VMA &vma = vma_res.value().get();
            size_t mem_offset = vma.mem_offset + (segvaddr - vma.varea.begin);

            loggers::SUSTCORE::INFO(
                "加载ELF段: idx=%u vaddr=%p filesz=%lu memsz=%lu mem_off=%lu",
                i, segvaddr.addr(), static_cast<unsigned long>(phdr.p_filesz),
                static_cast<unsigned long>(phdr.p_memsz), mem_offset);

            auto load_res = load_file_into_memory(
                file, *vma.memory, phdr.p_offset, mem_offset, phdr.p_filesz);
            propagate(load_res);

            if (phdr.p_memsz > phdr.p_filesz) {
                size_t zero_sz = phdr.p_memsz - phdr.p_filesz;
                auto zero_res =
                    zero_fill_memory(*vma.memory, mem_offset + phdr.p_filesz,
                                     zero_sz);
                propagate(zero_res);
            }
            auto first_page_res = vma.memory->lookup_page(mem_offset);
            if (first_page_res.has_value()) {
                loggers::SUSTCORE::INFO(
                    "ELF段首物理页: mem=%p mem_off=%lu paddr=%p", vma.memory,
                    mem_offset, first_page_res.value().addr());
            }
        }

        void_return();
    }

    Result<void> load(TaskSpec &spec, const LoadPrm &prm) {
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

        auto valid_res = validate_elf64(ehdr, file_size);
        propagate(valid_res);

        spec.entrypoint = VirAddr(ehdr.e_entry);

        addr_t max_pload_end = 0;

        // 解析程序头表并为TM添加相应的VMA
        for (size_t i = 0; i < ehdr.e_phnum; ++i) {
            Elf64_Phdr phdr{};
            off_t offset       = ehdr.e_phoff + i * sizeof(Elf64_Phdr);
            auto read_phdr_res = read_exact(*file, offset, &phdr,
                                            sizeof(phdr));
            propagate(read_phdr_res);

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

            VirAddr segvaddr(phdr.p_vaddr);
            VirAddr segvend(phdr.p_vaddr + phdr.p_memsz);

            // 确认该空间地址是用户空间地址
            if (!is_user_vaddr(segvaddr) || !is_user_vaddr(segvend)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            // 为该段在TM中添加一个VMA
            VMA::Type vma_type = phdr_to_vma_type(phdr.p_flags);
            auto *segment_mem  = new cap::MemoryPayload(
                phdr.p_memsz, false, false, VMA::Growth::FIXED);
            auto add_res = spec.tmm->add_vma(
                vma_type, VMA::Growth::FIXED, VirArea(segvaddr, segvend),
                segment_mem, VMA::seg2rwx(vma_type));
            if (!add_res.has_value()) {
                delete segment_mem;
                loggers::SUSTCORE::ERROR("无法为段%d添加VMA: %d", i,
                                         add_res.error());
                while (true);
            }
            add_res->get()->loading = true;  // 标记该VMA正在加载
            loggers::SUSTCORE::INFO(
                "创建ELF VMA: type=%s area=[%p,%p) mem=%p memsz=%lu mem_off=%lu",
                to_string(vma_type), segvaddr.addr(), segvend.addr(),
                segment_mem, static_cast<unsigned long>(phdr.p_memsz), 0UL);

            if (segvend.arith() > max_pload_end) {
                max_pload_end = segvend.arith();
            }
        }

        VirAddr heap_start = VirAddr(max_pload_end).page_align_up();
        auto *heap_mem =
            new cap::MemoryPayload(0, false, false, VMA::Growth::FLEXUP);
        auto heap_cap_res = spec.holder->insert_to_free(heap_mem);
        if (!heap_cap_res.has_value()) {
            delete heap_mem;
            propagate_return(heap_cap_res);
        }
        auto heap_res = spec.tmm->add_vma(VMA::Type::HEAP, VMA::Growth::FLEXUP,
                                          VirArea(heap_start, heap_start),
                                          heap_mem, PageMan::RWX::RW);
        if (!heap_res.has_value()) {
            loggers::SUSTCORE::ERROR("无法初始化堆VMA: %d", heap_res.error());
            propagate_return(heap_res);
        }
        loggers::SUSTCORE::INFO(
            "创建HEAP VMA: area=[%p,%p) mem=%p memsz=%lu", heap_start.addr(),
            heap_start.addr(), heap_mem, 0UL);
        spec.heap_vaddr   = heap_start;
        spec.heap_mem_cap = heap_cap_res.value();

        loggers::SUSTCORE::DEBUG("ELF加载完成, TM中的VMA列表:");
        for (const auto &vma : spec.tmm->vmas()) {
            loggers::SUSTCORE::DEBUG("  VMA类型: %s, 地址: %p~%p, 大小: %u B",
                                    to_string(vma.type), vma.varea.begin.addr(),
                                    vma.varea.end.addr(), vma.size());
        }

        auto load_res = loadsegs(*file, *spec.tmm, ehdr);
        propagate(load_res);

        for (auto &vma : spec.tmm->vmas()) {
            vma.loading      = false;
            PageMan::RWX rwx = VMA::seg2rwx(vma.type);
            spec.tmm->pman().modify_range_flags<PageMan::make_mask(0b001111)>(
                vma.varea.begin, vma.size(),
                PageMan::page_flags(rwx, true, false));
        }

        loggers::SUSTCORE::DEBUG("每个VMA的前16字节内容:");
        for (const auto &vma : spec.tmm->vmas()) {
            if (vma.varea.nullable()) {
                continue;
            }
            auto dump_res = dump_vma_prefix(vma, 16);
            propagate(dump_res);
        }

        void_return();
    }

}  // namespace loader::elf
