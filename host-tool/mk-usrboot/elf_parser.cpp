/**
 * @file elf_parser.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 将受支持的 ELF64 可执行文件解析为 GeneratorConfiguration
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include "elf_parser.h"

#include <elf.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mku {
    namespace {
        static_assert(std::endian::native == std::endian::little,
                      "mk-usrboot requires a little-endian Host");

        [[nodiscard]] Error invalid_elf(std::string_view message) noexcept {
            return Error{ErrorCode::INVALID_ELF, message};
        }

        [[nodiscard]] bool add_u64(std::uint64_t left, std::uint64_t right,
                                   std::uint64_t &result) noexcept {
            if (UINT64_MAX - left < right) {
                return false;
            }
            result = left + right;
            return true;
        }

        [[nodiscard]] bool range_fits(std::uint64_t offset, std::uint64_t size,
                                      std::uint64_t limit) noexcept {
            std::uint64_t end;
            return add_u64(offset, size, end) && end <= limit;
        }

        [[nodiscard]] SegmentPermissions permissions_from_flags(std::uint32_t flags) noexcept {
            return SegmentPermissions{
                .readable   = (flags & PF_R) != 0,
                .writable   = (flags & PF_W) != 0,
                .executable = (flags & PF_X) != 0,
            };
        }

        [[nodiscard]] bool same_permissions(const SegmentPermissions &left,
                                            const SegmentPermissions &right) noexcept {
            return left.readable == right.readable && left.writable == right.writable &&
                   left.executable == right.executable;
        }

        [[nodiscard]] bool ranges_overlap(const GeneratorSegment &left,
                                          const GeneratorSegment &right) noexcept {
            if (left.memory_size == 0 || right.memory_size == 0) {
                return false;
            }
            std::uint64_t left_end;
            std::uint64_t right_end;
            if (!add_u64(left.virtual_address, left.memory_size, left_end) ||
                !add_u64(right.virtual_address, right.memory_size, right_end))
            {
                return true;
            }
            return left.virtual_address < right_end && right.virtual_address < left_end;
        }

        [[nodiscard]] bool is_supported_permissions(std::uint32_t flags) noexcept {
            return flags == (PF_R | PF_X) || flags == (PF_R | PF_W) || flags == PF_R;
        }
    }  // namespace

    result<GeneratorConfiguration> parse_elf(byte_view input) noexcept {
        if (input.size() < sizeof(Elf64_Ehdr)) {
            return tay::unexpected<Error>(invalid_elf("input is smaller than an ELF64 header"));
        }
        const auto *data = input.data();
        Elf64_Ehdr elf_header;
        std::memcpy(&elf_header, data, sizeof(elf_header));
        if (elf_header.e_ident[EI_MAG0] != ELFMAG0 || elf_header.e_ident[EI_MAG1] != ELFMAG1 ||
            elf_header.e_ident[EI_MAG2] != ELFMAG2 || elf_header.e_ident[EI_MAG3] != ELFMAG3 ||
            elf_header.e_ident[EI_CLASS] != ELFCLASS64 ||
            elf_header.e_ident[EI_DATA] != ELFDATA2LSB ||
            elf_header.e_ident[EI_VERSION] != EV_CURRENT)
        {
            return tay::unexpected<Error>(
                invalid_elf("input is not a current little-endian ELF64 file"));
        }

        if (elf_header.e_type != ET_EXEC) {
            return tay::unexpected<Error>(invalid_elf("ELF type must be ET_EXEC"));
        }
        if (elf_header.e_machine != EM_RISCV && elf_header.e_machine != EM_LOONGARCH) {
            return tay::unexpected<Error>(invalid_elf("unsupported ELF machine"));
        }
        if (elf_header.e_version != EV_CURRENT || elf_header.e_ehsize != sizeof(Elf64_Ehdr) ||
            elf_header.e_phentsize != sizeof(Elf64_Phdr))
        {
            return tay::unexpected<Error>(invalid_elf("unsupported ELF64 header layout"));
        }
        if (elf_header.e_phnum == 0 || elf_header.e_phnum == PN_XNUM ||
            !range_fits(elf_header.e_phoff,
                        static_cast<std::uint64_t>(elf_header.e_phnum) * elf_header.e_phentsize,
                        input.size()))
        {
            return tay::unexpected<Error>(invalid_elf("invalid program header table"));
        }

        GeneratorConfiguration configuration;
        configuration.entry       = elf_header.e_entry;
        std::size_t segment_count = 0;
        for (std::uint16_t index = 0; index < elf_header.e_phnum; ++index) {
            const auto *program_header_data =
                data + static_cast<std::size_t>(elf_header.e_phoff) +
                static_cast<std::size_t>(index) * elf_header.e_phentsize;
            Elf64_Phdr program_header;
            std::memcpy(&program_header, program_header_data, sizeof(program_header));
            if (program_header.p_type != PT_LOAD) {
                continue;
            }
            if (!is_supported_permissions(program_header.p_flags)) {
                return tay::unexpected<Error>(invalid_elf("unsupported PT_LOAD permissions"));
            }
            if (segment_count == configuration.segments.size()) {
                return tay::unexpected<Error>(invalid_elf("too many PT_LOAD segments"));
            }

            auto &segment           = configuration.segments[segment_count++];
            segment.permissions     = permissions_from_flags(program_header.p_flags);
            segment.file_offset     = program_header.p_offset;
            segment.virtual_address = program_header.p_vaddr;
            segment.file_size       = program_header.p_filesz;
            segment.memory_size     = program_header.p_memsz;
            if (segment.file_size > segment.memory_size) {
                return tay::unexpected<Error>(invalid_elf("PT_LOAD filesz exceeds memsz"));
            }
            if (!range_fits(segment.file_offset, segment.file_size, input.size())) {
                return tay::unexpected<Error>(invalid_elf("PT_LOAD file range is outside input"));
            }
            std::uint64_t virtual_end;
            if (!add_u64(segment.virtual_address, segment.memory_size, virtual_end)) {
                return tay::unexpected<Error>(invalid_elf("PT_LOAD virtual range overflows"));
            }
        }

        if (segment_count != configuration.segments.size()) {
            return tay::unexpected<Error>(invalid_elf("ELF must contain three PT_LOAD segments"));
        }
        for (std::size_t left = 0; left < configuration.segments.size(); ++left) {
            for (std::size_t right = left + 1; right < configuration.segments.size(); ++right) {
                if (same_permissions(configuration.segments[left].permissions,
                                     configuration.segments[right].permissions))
                {
                    return tay::unexpected<Error>(invalid_elf("duplicate PT_LOAD permissions"));
                }
                if (ranges_overlap(configuration.segments[left], configuration.segments[right])) {
                    return tay::unexpected<Error>(invalid_elf("PT_LOAD virtual ranges overlap"));
                }
            }
        }

        bool entry_in_executable_segment = false;
        for (const auto &segment : configuration.segments) {
            if (!segment.permissions.executable) {
                continue;
            }
            std::uint64_t segment_end;
            if (add_u64(segment.virtual_address, segment.memory_size, segment_end) &&
                configuration.entry >= segment.virtual_address && configuration.entry < segment_end)
            {
                entry_in_executable_segment = true;
            }
        }
        if (!entry_in_executable_segment) {
            return tay::unexpected<Error>(invalid_elf("ELF entry is outside executable segment"));
        }
        return configuration;
    }
}  // namespace mku
