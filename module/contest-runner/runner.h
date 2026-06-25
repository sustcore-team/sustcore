/**
 * @file runner.h
 * @author theflysong
 * @brief contest runner 内部运行接口
 * @version alpha-1.0.0
 * @date 2026-06-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <kmod/syscall.h>

#include <cstddef>

namespace contest_runner {
    struct TestRunStats {
        size_t total  = 0;
        size_t passed = 0;
        size_t failed = 0;
    };

    struct RunnerContext {
        CapIdx root_dir_cap   = cap::null;
        const char *libc_root = nullptr;
        const char *libc_name = nullptr;
    };

    struct OpenDirHandle {
        int fd         = -1;
        CapIdx cap     = cap::null;
        const char *path = nullptr;
    };

    enum class RunProgramError {
        NONE,
        OPEN_FAILED,
        SPAWN_FAILED,
        WAIT_FAILED,
    };

    [[nodiscard]]
    CapIdx bootstrap_root_dir();

    [[nodiscard]]
    bool open_cwd_dir(const char *path, OpenDirHandle &cwd);

    void close_cwd_dir(OpenDirHandle &cwd);

    [[nodiscard]]
    RunProgramError run_program(const RunnerContext &ctx,
                                const OpenDirHandle &cwd,
                                const char *program_path,
                                const char *argv[], int &status);

    [[nodiscard]]
    bool run_status_success(int status);

    [[nodiscard]]
    int run_exit_code(int status);

    const char *run_error_string(RunProgramError error);

    void accumulate_stats(TestRunStats &total, const TestRunStats &part);

    [[nodiscard]]
    TestRunStats run_basic(const RunnerContext &ctx);

    [[nodiscard]]
    TestRunStats run_busybox(const RunnerContext &ctx);

    [[nodiscard]]
    TestRunStats run_libctest(const RunnerContext &ctx);

    [[nodiscard]]
    TestRunStats run_ltp(const RunnerContext &ctx);
}  // namespace contest_runner
