/**
 * @file busybox.cpp
 * @author theflysong
 * @brief busybox 测试运行逻辑
 * @version alpha-1.0.0
 * @date 2026-06-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "runner.h"

#include <cstdio>
#include <cstring>

namespace contest_runner {
    namespace {
        constexpr size_t PATH_BUFFER_SIZE = 256;
        constexpr size_t CMD_BUFFER_SIZE  = 512;

        const char *BUSYBOX_COMMANDS[] = {
            R"(echo "#### independent command test")",
            R"(ash -c exit)",
            R"(sh -c exit)",
            R"(basename /aaa/bbb)",
            R"(cal)",
            R"(clear)",
            R"(date)",
            R"(df)",
            R"(dirname /aaa/bbb)",
            R"(dmesg)",
            R"(du)",
            R"(expr 1 + 1)",
            R"(false)",
            R"(true)",
            R"(which ls)",
            R"(uname)",
            R"(uptime)",
            R"(printf "abc\n")",
            R"(ps)",
            R"(pwd)",
            R"(free)",
            R"(hwclock)",
            R"(sh -c 'sleep 5' & ./busybox kill $!)",
            R"(ls)",
            R"(sleep 1)",
            R"(echo "#### file opration test")",
            R"(touch test.txt)",
            R"(echo "hello world" > test.txt)",
            R"(cat test.txt)",
            R"(cut -c 3 test.txt)",
            R"(od test.txt)",
            R"(head test.txt)",
            R"(tail test.txt)",
            R"(hexdump -C test.txt)",
            R"(md5sum test.txt)",
            R"(echo "ccccccc" >> test.txt)",
            R"(echo "bbbbbbb" >> test.txt)",
            R"(echo "aaaaaaa" >> test.txt)",
            R"(echo "2222222" >> test.txt)",
            R"(echo "1111111" >> test.txt)",
            R"(echo "bbbbbbb" >> test.txt)",
            R"(sort test.txt | ./busybox uniq)",
            R"(stat test.txt)",
            R"(strings test.txt)",
            R"(wc test.txt)",
            R"([ -f test.txt ])",
            R"(more test.txt)",
            R"(rm test.txt)",
            R"(mkdir test_dir)",
            R"(mv test_dir test)",
            R"(rmdir test)",
            R"(grep hello busybox_cmd.txt)",
            R"(cp busybox_cmd.txt busybox_cmd.bak)",
            R"(rm busybox_cmd.bak)",
            R"(find -name "busybox_cmd.txt")",
            nullptr,
        };

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

    TestRunStats run_busybox(const RunnerContext &ctx) {
        TestRunStats stats{};
        printf("#### OS COMP TEST GROUP START busybox-%s ####\n",
               ctx.libc_name);

        OpenDirHandle cwd{};
        if (!open_cwd_dir(ctx.libc_root, cwd)) {
            printf("#### OS COMP TEST GROUP END busybox-%s ####\n",
                   ctx.libc_name);
            return stats;
        }

        char busybox_path[PATH_BUFFER_SIZE]{};
        if (!make_path(busybox_path, sizeof(busybox_path), ctx.libc_root,
                       "busybox"))
        {
            printf("contest-runner: invalid busybox root %s\n", ctx.libc_root);
            close_cwd_dir(cwd);
            printf("#### OS COMP TEST GROUP END busybox-%s ####\n",
                   ctx.libc_name);
            return stats;
        }

        for (size_t i = 0; BUSYBOX_COMMANDS[i] != nullptr; ++i) {
            ++stats.total;
            printf("contest-runner: busybox run %s\n", BUSYBOX_COMMANDS[i]);

            char shell_command[CMD_BUFFER_SIZE]{};
            int cmd_len = snprintf(shell_command, sizeof(shell_command),
                                   "./busybox %s", BUSYBOX_COMMANDS[i]);
            if (cmd_len <= 0 ||
                static_cast<size_t>(cmd_len) >= sizeof(shell_command))
            {
                ++stats.failed;
                printf("testcase busybox %s fail\n", BUSYBOX_COMMANDS[i]);
                continue;
            }

            const char *argv[] = {"busybox", "sh", "-c", shell_command,
                                  nullptr};
            int status         = 0;
            auto err = run_program(ctx, cwd, busybox_path, argv, status);
            if (err != RunProgramError::NONE) {
                ++stats.failed;
                printf("testcase busybox %s fail\n", BUSYBOX_COMMANDS[i]);
                printf("contest-runner: busybox failed error=%s command=%s\n",
                       run_error_string(err), BUSYBOX_COMMANDS[i]);
                continue;
            }

            if (run_exit_code(status) != 0 &&
                strcmp(BUSYBOX_COMMANDS[i], "false") != 0)
            {
                ++stats.failed;
                printf("testcase busybox %s fail\n", BUSYBOX_COMMANDS[i]);
                continue;
            }

            ++stats.passed;
            printf("testcase busybox %s success\n", BUSYBOX_COMMANDS[i]);
        }

        close_cwd_dir(cwd);
        printf("#### OS COMP TEST GROUP END busybox-%s ####\n", ctx.libc_name);
        return stats;
    }
}  // namespace contest_runner
