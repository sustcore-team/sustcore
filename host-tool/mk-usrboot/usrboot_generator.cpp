/**
 * @file usrboot_generator.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 构造 Usrboot Header
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include "usrboot_generator.h"

#include <sys/types.h>
#include <tay/string.h>
#include <unistd.h>
#include <usrboot.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace mku {
    namespace {
        [[nodiscard]] Error output_error(std::string_view message, int system_error = 0) noexcept {
            return Error{ErrorCode::OUTPUT, message, system_error};
        }

        [[nodiscard]] Error configuration_error(std::string_view message) noexcept {
            return Error{ErrorCode::INVALID_CONFIGURATION, message};
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
                                      std::size_t limit) noexcept {
            std::uint64_t end;
            return add_u64(offset, size, end) && end <= limit;
        }

        [[nodiscard]] bool is_rx(const SegmentPermissions &permissions) noexcept {
            return permissions.readable && !permissions.writable && permissions.executable;
        }

        [[nodiscard]] bool is_rw(const SegmentPermissions &permissions) noexcept {
            return permissions.readable && permissions.writable && !permissions.executable;
        }

        [[nodiscard]] bool is_ro(const SegmentPermissions &permissions) noexcept {
            return permissions.readable && !permissions.writable && !permissions.executable;
        }

        class output_file {
        public:
            output_file(const output_file &)            = delete;
            output_file &operator=(const output_file &) = delete;

            output_file(output_file &&other) noexcept : stream_(other.stream_) {
                other.stream_ = nullptr;
            }

            output_file &operator=(output_file &&other) noexcept {
                if (this != &other) {
                    static_cast<void>(close());
                    stream_       = other.stream_;
                    other.stream_ = nullptr;
                }
                return *this;
            }

            ~output_file() {
                static_cast<void>(close());
            }

            [[nodiscard]] static result<output_file> create(char *path) noexcept {
                const int descriptor = mkstemp(path);
                if (descriptor < 0) {
                    return tay::unexpected<Error>(
                        output_error("cannot create temporary output", errno));
                }
                FILE *stream = fdopen(descriptor, "wb");
                if (stream == nullptr) {
                    const int error = errno;
                    ::close(descriptor);
                    return tay::unexpected<Error>(
                        output_error("cannot open temporary output", error));
                }
                return output_file(stream);
            }

            [[nodiscard]] bool write(const void *data, std::size_t size) noexcept {
                return std::fwrite(data, 1, size, stream_) == size;
            }

            [[nodiscard]] bool flush() noexcept {
                return std::fflush(stream_) == 0 && fsync(fileno(stream_)) == 0;
            }

            [[nodiscard]] bool close() noexcept {
                if (stream_ == nullptr) {
                    return true;
                }
                FILE *stream = stream_;
                stream_      = nullptr;
                return std::fclose(stream) == 0;
            }

        private:
            explicit output_file(FILE *stream) noexcept : stream_(stream) {}
            FILE *stream_ = nullptr;
        };

        void write_le64(std::byte *output, std::uint64_t value) noexcept {
            for (std::size_t index = 0; index < sizeof(value); ++index) {
                output[index] = static_cast<std::byte>(value >> (index * 8));
            }
        }

        void write_segment(std::byte *output, const GeneratorSegment &segment,
                           std::uint64_t body_offset) noexcept {
            write_le64(output + offsetof(usrboot_segment, vaddr), segment.virtual_address);
            write_le64(output + offsetof(usrboot_segment, off), body_offset);
            write_le64(output + offsetof(usrboot_segment, filesz), segment.file_size);
            write_le64(output + offsetof(usrboot_segment, memsz), segment.memory_size);
        }

        [[nodiscard]] bool write_segment_body(output_file &output, byte_view input,
                                              const GeneratorSegment &segment) noexcept {
            if (segment.file_size == 0) {
                return true;
            }
            return output.write(input.data() + static_cast<std::size_t>(segment.file_offset),
                                static_cast<std::size_t>(segment.file_size));
        }
    }  // namespace

    result<void> generate_usrboot(byte_view input, const GeneratorConfiguration &configuration,
                                  std::string_view output_path) noexcept {
        const GeneratorSegment *rx = nullptr;
        const GeneratorSegment *rw = nullptr;
        const GeneratorSegment *ro = nullptr;
        for (const auto &segment : configuration.segments) {
            if (is_rx(segment.permissions)) {
                if (rx != nullptr) {
                    return tay::unexpected<Error>(configuration_error("duplicate RX segment"));
                }
                rx = &segment;
            } else if (is_rw(segment.permissions)) {
                if (rw != nullptr) {
                    return tay::unexpected<Error>(configuration_error("duplicate RW segment"));
                }
                rw = &segment;
            } else if (is_ro(segment.permissions)) {
                if (ro != nullptr) {
                    return tay::unexpected<Error>(configuration_error("duplicate RO segment"));
                }
                ro = &segment;
            } else {
                return tay::unexpected<Error>(
                    configuration_error("unsupported segment permissions"));
            }
        }
        if (rx == nullptr || rw == nullptr || ro == nullptr) {
            return tay::unexpected<Error>(
                configuration_error("RX, RW, and RO segments are required"));
        }
        for (const auto *segment : {rx, rw, ro}) {
            if (segment->file_size > segment->memory_size) {
                return tay::unexpected<Error>(configuration_error("segment filesz exceeds memsz"));
            }
            if (!range_fits(segment->file_offset, segment->file_size, input.size())) {
                return tay::unexpected<Error>(
                    configuration_error("segment file range is outside input"));
            }
        }

        const std::uint64_t rw_offset = rx->file_size;
        std::uint64_t ro_offset;
        std::uint64_t body_size;
        if (!add_u64(rw_offset, rw->file_size, ro_offset) ||
            !add_u64(ro_offset, ro->file_size, body_size))
        {
            return tay::unexpected<Error>(configuration_error("body size overflows"));
        }
        if (output_path.empty()) {
            return tay::unexpected<Error>(configuration_error("output path is empty"));
        }

        auto output_name_result = tay::string<>::try_create(output_path.data(), output_path.size());
        if (!output_name_result) {
            return tay::unexpected<Error>(
                configuration_error("temporary output path allocation failed"));
        }
        auto output_name      = std::move(*output_name_result);
        auto temporary_result = tay::string<>::try_create(output_name);
        if (!temporary_result) {
            return tay::unexpected<Error>(
                configuration_error("temporary output path allocation failed"));
        }
        auto temporary_path = std::move(*temporary_result);
        auto suffix_result  = temporary_path.append(".tmp.XXXXXX");
        if (!suffix_result) {
            return tay::unexpected<Error>(
                configuration_error("temporary output path allocation failed"));
        }

        auto output_result = output_file::create(temporary_path.data());
        if (!output_result) {
            unlink(temporary_path.data());
            return tay::unexpected<Error>(output_result.error());
        }
        auto output = std::move(*output_result);

        tay::static_array<std::byte, sizeof(usrboot_header)> header{};
        write_le64(header.data() + offsetof(usrboot_header, magic), USRBOOT_MAGIC);
        write_le64(header.data() + offsetof(usrboot_header, body_size), body_size);
        write_le64(header.data() + offsetof(usrboot_header, entry), configuration.entry);
        write_segment(header.data() + offsetof(usrboot_header, seg_rx), *rx, 0);
        write_segment(header.data() + offsetof(usrboot_header, seg_rw), *rw, rw_offset);
        write_segment(header.data() + offsetof(usrboot_header, seg_ro), *ro, ro_offset);

        if (!output.write(header.data(), header.size()) ||
            !write_segment_body(output, input, *rx) || !write_segment_body(output, input, *rw) ||
            !write_segment_body(output, input, *ro))
        {
            const int error = errno;
            static_cast<void>(output.close());
            unlink(temporary_path.data());
            return tay::unexpected<Error>(output_error("cannot write output", error));
        }
        if (!output.flush() || !output.close()) {
            const int error = errno;
            unlink(temporary_path.data());
            return tay::unexpected<Error>(output_error("cannot flush output", error));
        }
        if (rename(temporary_path.data(), output_name.data()) != 0) {
            const int error = errno;
            unlink(temporary_path.data());
            return tay::unexpected<Error>(output_error("cannot publish output", error));
        }
        return {};
    }
}  // namespace mku
