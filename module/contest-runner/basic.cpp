/**
 * @file basic.cpp
 * @author theflysong
 * @brief basic 测试运行逻辑
 * @version alpha-1.0.0
 * @date 2026-06-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "runner.h"

#include <cstdio>
#include <cstring>

#include "basic.h"

namespace contest_runner {
    namespace {
        constexpr size_t PATH_BUFFER_SIZE = 256;

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
    }  // namespace

    TestRunStats run_basic(const RunnerContext &ctx) {
        TestRunStats stats{};
        printf("#### OS COMP TEST GROUP START basic-%s ####\n", ctx.libc_name);

        char basic_root[PATH_BUFFER_SIZE]{};
        if (!make_path(basic_root, sizeof(basic_root), ctx.libc_root, "basic"))
        {
            printf("contest-runner: invalid basic root %s\n", ctx.libc_root);
            printf("#### OS COMP TEST GROUP END basic-%s ####\n", ctx.libc_name);
            return stats;
        }

        OpenDirHandle cwd{};
        if (!open_cwd_dir(basic_root, cwd)) {
            printf("#### OS COMP TEST GROUP END basic-%s ####\n", ctx.libc_name);
            return stats;
        }

        for (size_t i = 0; basic::testcases[i] != nullptr; ++i) {
            ++stats.total;
            printf("Testing %s :\n", basic::testcases[i]);

            char testcase_path[PATH_BUFFER_SIZE]{};
            if (!make_path(testcase_path, sizeof(testcase_path), basic_root,
                           basic::testcases[i]))
            {
                ++stats.failed;
                printf("contest-runner: basic path too long %s\n",
                       basic::testcases[i]);
                continue;
            }

            int status = 0;
            auto err =
                run_program(ctx, cwd, testcase_path, nullptr, status);
            if (err != RunProgramError::NONE) {
                ++stats.failed;
                printf("contest-runner: basic failed %s error=%s\n",
                       basic::testcases[i], run_error_string(err));
                continue;
            }

            if (!run_status_success(status)) {
                ++stats.failed;
                printf("contest-runner: basic failed %s status=0x%x\n",
                       basic::testcases[i], status);
                continue;
            }

            ++stats.passed;
        }

        close_cwd_dir(cwd);
        printf("#### OS COMP TEST GROUP END basic-%s ####\n", ctx.libc_name);
        return stats;
    }
}  // namespace contest_runner
