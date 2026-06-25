/**
 * @file ltp.cpp
 * @author theflysong
 * @brief ltp 测试运行逻辑
 * @version alpha-1.0.0
 * @date 2026-06-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "runner.h"

#include <sustcore/files.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace contest_runner {
    namespace {
        constexpr size_t PATH_BUFFER_SIZE   = 256;
        constexpr size_t DIRENT_BUFFER_SIZE = 16384;

        [[nodiscard]]
        bool make_path(char *buf, size_t bufsiz, const char *lhs,
                       const char *rhs) {
            if (buf == nullptr || bufsiz == 0 || lhs == nullptr ||
                rhs == nullptr)
            {
                return false;
            }
            int len = snprintf(buf, bufsiz, "%s/%s", lhs, rhs);
            return len > 0 && static_cast<size_t>(len) < bufsiz;
        }

        [[nodiscard]]
        bool collect_ltp_cases(CapIdx dir_cap, std::vector<std::string> &cases) {
            char buffer[DIRENT_BUFFER_SIZE]{};
            size_t offset = 0;

            while (true) {
                memset(buffer, 0, sizeof(buffer));
                size_t bytes =
                    sys_vfs_getdents(dir_cap, buffer, sizeof(buffer), offset);
                if (bytes == 0) {
                    break;
                }

                size_t parsed = 0;
                for (size_t pos = 0; pos < bytes;) {
                    if (bytes - pos < sizeof(dir_entry_header)) {
                        return false;
                    }

                    auto *header = reinterpret_cast<const dir_entry_header *>(
                        buffer + pos);
                    const char *name = buffer + pos + sizeof(dir_entry_header);
                    size_t name_room = bytes - pos - sizeof(dir_entry_header);
                    if (memchr(name, '\0', name_room) == nullptr) {
                        return false;
                    }

                    if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                        NodeMeta meta{};
                        if (!sys_vfs_stat(dir_cap, name, &meta)) {
                            return false;
                        }
                        if (meta.type == EntryType::FILE) {
                            cases.emplace_back(name);
                        }
                    }

                    ++parsed;
                    if (header->next_offset == 0 ||
                        pos + header->next_offset > bytes)
                    {
                        return false;
                    }
                    pos += header->next_offset;
                }

                if (parsed == 0) {
                    return false;
                }
                offset += parsed;
            }

            std::sort(cases.begin(), cases.end());
            return true;
        }
    }  // namespace

    TestRunStats run_ltp(const RunnerContext &ctx) {
        TestRunStats stats{};
        printf("#### OS COMP TEST GROUP START ltp-%s ####\n", ctx.libc_name);

        char ltp_root[PATH_BUFFER_SIZE]{};
        if (!make_path(ltp_root, sizeof(ltp_root), ctx.libc_root,
                       "ltp/testcases/bin"))
        {
            printf("contest-runner: invalid ltp root %s\n", ctx.libc_root);
            printf("#### OS COMP TEST GROUP END ltp-%s ####\n", ctx.libc_name);
            return stats;
        }

        OpenDirHandle cwd{};
        if (!open_cwd_dir(ltp_root, cwd)) {
            printf("#### OS COMP TEST GROUP END ltp-%s ####\n", ctx.libc_name);
            return stats;
        }

        std::vector<std::string> cases{};
        if (!collect_ltp_cases(cwd.cap, cases)) {
            printf("contest-runner: ltp enumerate failed %s\n", ltp_root);
            close_cwd_dir(cwd);
            printf("#### OS COMP TEST GROUP END ltp-%s ####\n", ctx.libc_name);
            return stats;
        }

        for (const auto &case_name : cases) {
            ++stats.total;
            printf("RUN LTP CASE %s\n", case_name.c_str());

            char case_path[PATH_BUFFER_SIZE]{};
            if (!make_path(case_path, sizeof(case_path), ltp_root,
                           case_name.c_str()))
            {
                ++stats.failed;
                printf("FAIL LTP CASE %s : -1\n", case_name.c_str());
                continue;
            }

            const char *argv[] = {case_name.c_str(), nullptr};
            int status         = 0;
            auto err = run_program(ctx, cwd, case_path, argv, status);
            if (err != RunProgramError::NONE) {
                ++stats.failed;
                printf("FAIL LTP CASE %s : -1\n", case_name.c_str());
                printf("contest-runner: ltp failed error=%s case=%s\n",
                       run_error_string(err), case_name.c_str());
                continue;
            }

            int ret = run_exit_code(status);
            printf("FAIL LTP CASE %s : %d\n", case_name.c_str(), ret);
            if (run_status_success(status)) {
                ++stats.passed;
            } else {
                ++stats.failed;
            }
        }

        close_cwd_dir(cwd);
        printf("#### OS COMP TEST GROUP END ltp-%s ####\n", ctx.libc_name);
        return stats;
    }
}  // namespace contest_runner
