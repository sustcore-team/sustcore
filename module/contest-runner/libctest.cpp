/**
 * @file libctest.cpp
 * @author theflysong
 * @brief libctest 测试运行逻辑
 * @version alpha-1.0.0
 * @date 2026-06-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "runner.h"

#include <cstdio>

namespace contest_runner {
    namespace {
        constexpr size_t PATH_BUFFER_SIZE = 256;

        const char *STATIC_TESTS[] = {
            "argv",
            "basename",
            "clocale_mbfuncs",
            "clock_gettime",
            "dirname",
            "env",
            "fdopen",
            "fnmatch",
            "fscanf",
            "fwscanf",
            "iconv_open",
            "inet_pton",
            "mbc",
            "memstream",
            "pthread_cancel_points",
            "pthread_cancel",
            "pthread_cond",
            "pthread_tsd",
            "qsort",
            "random",
            "search_hsearch",
            "search_insque",
            "search_lsearch",
            "search_tsearch",
            "setjmp",
            "snprintf",
            "socket",
            "sscanf",
            "sscanf_long",
            "stat",
            "strftime",
            "string",
            "string_memcpy",
            "string_memmem",
            "string_memset",
            "string_strchr",
            "string_strcspn",
            "string_strstr",
            "strptime",
            "strtod",
            "strtod_simple",
            "strtof",
            "strtol",
            "strtold",
            "swprintf",
            "tgmath",
            "time",
            "tls_align",
            "udiv",
            "ungetc",
            "utime",
            "wcsstr",
            "wcstol",
            "daemon_failure",
            "dn_expand_empty",
            "dn_expand_ptr_0",
            "fflush_exit",
            "fgets_eof",
            "fgetwc_buffering",
            "fpclassify_invalid_ld80",
            "ftello_unflushed_append",
            "getpwnam_r_crash",
            "getpwnam_r_errno",
            "iconv_roundtrips",
            "inet_ntop_v4mapped",
            "inet_pton_empty_last_field",
            "iswspace_null",
            "lrand48_signextend",
            "lseek_large",
            "malloc_0",
            "mbsrtowcs_overflow",
            "memmem_oob_read",
            "memmem_oob",
            "mkdtemp_failure",
            "mkstemp_failure",
            "printf_1e9_oob",
            "printf_fmt_g_round",
            "printf_fmt_g_zeros",
            "printf_fmt_n",
            "pthread_robust_detach",
            "pthread_cancel_sem_wait",
            "pthread_cond_smasher",
            "pthread_condattr_setclock",
            "pthread_exit_cancel",
            "pthread_once_deadlock",
            "pthread_rwlock_ebusy",
            "putenv_doublefree",
            "regex_backref_0",
            "regex_bracket_icase",
            "regex_ere_backref",
            "regex_escaped_high_byte",
            "regex_negated_range",
            "regexec_nosub",
            "rewind_clear_error",
            "rlimit_open_files",
            "scanf_bytes_consumed",
            "scanf_match_literal_eof",
            "scanf_nullbyte_char",
            "setvbuf_unget",
            "sigprocmask_internal",
            "sscanf_eof",
            "statvfs",
            "strverscmp",
            "syscall_sign_extend",
            "uselocale_0",
            "wcsncpy_read_overflow",
            "wcsstr_false_negative",
            nullptr,
        };

        const char *DYNAMIC_TESTS[] = {
            "argv",
            "basename",
            "clocale_mbfuncs",
            "clock_gettime",
            "dirname",
            "dlopen",
            "env",
            "fdopen",
            "fnmatch",
            "fscanf",
            "fwscanf",
            "iconv_open",
            "inet_pton",
            "mbc",
            "memstream",
            "pthread_cancel_points",
            "pthread_cancel",
            "pthread_cond",
            "pthread_tsd",
            "qsort",
            "random",
            "search_hsearch",
            "search_insque",
            "search_lsearch",
            "search_tsearch",
            "sem_init",
            "setjmp",
            "snprintf",
            "socket",
            "sscanf",
            "sscanf_long",
            "stat",
            "strftime",
            "string",
            "string_memcpy",
            "string_memmem",
            "string_memset",
            "string_strchr",
            "string_strcspn",
            "string_strstr",
            "strptime",
            "strtod",
            "strtod_simple",
            "strtof",
            "strtol",
            "strtold",
            "swprintf",
            "tgmath",
            "time",
            "tls_init",
            "tls_local_exec",
            "udiv",
            "ungetc",
            "utime",
            "wcsstr",
            "wcstol",
            "daemon_failure",
            "dn_expand_empty",
            "dn_expand_ptr_0",
            "fflush_exit",
            "fgets_eof",
            "fgetwc_buffering",
            "fpclassify_invalid_ld80",
            "ftello_unflushed_append",
            "getpwnam_r_crash",
            "getpwnam_r_errno",
            "iconv_roundtrips",
            "inet_ntop_v4mapped",
            "inet_pton_empty_last_field",
            "iswspace_null",
            "lrand48_signextend",
            "lseek_large",
            "malloc_0",
            "mbsrtowcs_overflow",
            "memmem_oob_read",
            "memmem_oob",
            "mkdtemp_failure",
            "mkstemp_failure",
            "printf_1e9_oob",
            "printf_fmt_g_round",
            "printf_fmt_g_zeros",
            "printf_fmt_n",
            "pthread_robust_detach",
            "pthread_cond_smasher",
            "pthread_condattr_setclock",
            "pthread_exit_cancel",
            "pthread_once_deadlock",
            "pthread_rwlock_ebusy",
            "putenv_doublefree",
            "regex_backref_0",
            "regex_bracket_icase",
            "regex_ere_backref",
            "regex_escaped_high_byte",
            "regex_negated_range",
            "regexec_nosub",
            "rewind_clear_error",
            "rlimit_open_files",
            "scanf_bytes_consumed",
            "scanf_match_literal_eof",
            "scanf_nullbyte_char",
            "setvbuf_unget",
            "sigprocmask_internal",
            "sscanf_eof",
            "statvfs",
            "strverscmp",
            "syscall_sign_extend",
            "tls_get_new_dtv",
            "uselocale_0",
            "wcsncpy_read_overflow",
            "wcsstr_false_negative",
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

        void run_libctest_cases(TestRunStats &stats, const RunnerContext &ctx,
                                const OpenDirHandle &cwd,
                                const char *runtest_path,
                                const char *entry_name,
                                const char *cases[]) {
            for (size_t i = 0; cases[i] != nullptr; ++i) {
                ++stats.total;
                const char *argv[] = {"runtest.exe", "-w", entry_name, cases[i],
                                      nullptr};
                int status         = 0;
                auto err = run_program(ctx, cwd, runtest_path, argv, status);
                if (err != RunProgramError::NONE) {
                    ++stats.failed;
                    printf(
                        "contest-runner: libctest failed entry=%s case=%s "
                        "error=%s\n",
                        entry_name, cases[i], run_error_string(err));
                    continue;
                }

                if (!run_status_success(status)) {
                    ++stats.failed;
                    printf(
                        "contest-runner: libctest failed entry=%s case=%s "
                        "status=0x%x\n",
                        entry_name, cases[i], status);
                    continue;
                }

                ++stats.passed;
            }
        }
    }  // namespace

    TestRunStats run_libctest(const RunnerContext &ctx) {
        TestRunStats stats{};
        printf("#### OS COMP TEST GROUP START libctest-%s ####\n",
               ctx.libc_name);

        OpenDirHandle cwd{};
        if (!open_cwd_dir(ctx.libc_root, cwd)) {
            printf("#### OS COMP TEST GROUP END libctest-%s ####\n",
                   ctx.libc_name);
            return stats;
        }

        char runtest_path[PATH_BUFFER_SIZE]{};
        if (!make_path(runtest_path, sizeof(runtest_path), ctx.libc_root,
                       "runtest.exe"))
        {
            printf("contest-runner: invalid libctest root %s\n", ctx.libc_root);
            close_cwd_dir(cwd);
            printf("#### OS COMP TEST GROUP END libctest-%s ####\n",
                   ctx.libc_name);
            return stats;
        }

        run_libctest_cases(stats, ctx, cwd, runtest_path, "entry-static.exe",
                           STATIC_TESTS);
        run_libctest_cases(stats, ctx, cwd, runtest_path, "entry-dynamic.exe",
                           DYNAMIC_TESTS);

        close_cwd_dir(cwd);
        printf("#### OS COMP TEST GROUP END libctest-%s ####\n",
               ctx.libc_name);
        return stats;
    }
}  // namespace contest_runner
