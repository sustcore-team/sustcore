/**
 * @file main.cpp
 * @author theflysong
 * @brief contest runner 主文件
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sustcore/bootstrap.h>
#include <sys/wait.h>

#include <cstdio>
#include <cstring>

#include "runner.h"

namespace contest_runner {
    [[nodiscard]]
    bool prepare_cloned_cap(CapIdx source_cap, uint64_t new_perm,
                            CapIdx &prepared_cap) {
        prepared_cap = cap::null;
        if (source_cap == cap::null || source_cap == cap::error) {
            return false;
        }

        auto cap_res = sys_cap_clone(source_cap).to_result();
        if (!cap_res.has_value()) {
            return false;
        }

        prepared_cap = cap_res.value();
        auto downgrade_res =
            sys_cap_downgrade(prepared_cap, new_perm).to_result();
        if (!downgrade_res.has_value()) {
            (void)sys_cap_remove(prepared_cap).to_result();
            prepared_cap = cap::null;
            return false;
        }
        return true;
    }

    const char *ENVP[] = {"LD_LIBRARY_PATH=/lib:/lib64", nullptr};

    [[nodiscard]]
    CapIdx spawn_linux_program(int fd, CapIdx prepared_root_dir_cap,
                               CapIdx prepared_cwd_dir_cap,
                               CapIdx prepared_parent_pcb_cap,
                               const char *cwd_path, const char *program_path,
                               const char *argv[]) {
        if (fd < 0 || prepared_root_dir_cap == cap::null ||
            prepared_root_dir_cap == cap::error ||
            prepared_cwd_dir_cap == cap::null ||
            prepared_cwd_dir_cap == cap::error ||
            prepared_parent_pcb_cap == cap::null ||
            prepared_parent_pcb_cap == cap::error)
        {
            return cap::error;
        }
        if (cwd_path == nullptr || cwd_path[0] == '\0' ||
            program_path == nullptr || program_path[0] == '\0')
        {
            return cap::error;
        }

        struct RootDirBootstrap {
            bsheader header;
            BootstrapCapExplainPayloadHead explain;
            char desc[3];
        } root_bootstrap{
            .header =
                bsheader{
                    .size = sizeof(RootDirBootstrap),
                    .type = boot::TYPE_CAPEXP,
                },
            .explain =
                BootstrapCapExplainPayloadHead{
                    .cap_idx  = prepared_root_dir_cap,
                    .cap_type = PayloadType::VDIR,
                    .cap_perm = ~b64(0),
                },
            .desc = "#/",
        };

        struct CwdDirBootstrap {
            bsheader header;
            BootstrapCapExplainPayloadHead explain;
            char desc[5];
        } cwd_bootstrap_cap{
            .header =
                bsheader{
                    .size = sizeof(CwdDirBootstrap),
                    .type = boot::TYPE_CAPEXP,
                },
            .explain =
                BootstrapCapExplainPayloadHead{
                    .cap_idx  = prepared_cwd_dir_cap,
                    .cap_type = PayloadType::VDIR,
                    .cap_perm = ~b64(0),
                },
            .desc = "#cwd",
        };

        struct ParentPcbBootstrap {
            bsheader header;
            BootstrapCapExplainPayloadHead explain;
            char desc[8];
        } parent_bootstrap{
            .header =
                bsheader{
                    .size = sizeof(ParentPcbBootstrap),
                    .type = boot::TYPE_CAPEXP,
                },
            .explain =
                BootstrapCapExplainPayloadHead{
                    .cap_idx  = prepared_parent_pcb_cap,
                    .cap_type = PayloadType::PCB,
                    .cap_perm = PERM_PCB_GETPID | PERM_BASIC_CLONE,
                },
            .desc = "#parent",
        };

        char cwd_desc[256]{};
        int cwd_desc_len =
            snprintf(cwd_desc, sizeof(cwd_desc), "#cwd:%s", cwd_path);
        if (cwd_desc_len <= 0 ||
            static_cast<size_t>(cwd_desc_len) >= sizeof(cwd_desc))
        {
            return cap::error;
        }

        char exe_desc[256]{};
        int exe_desc_len =
            snprintf(exe_desc, sizeof(exe_desc), "#exe:%s", program_path);
        if (exe_desc_len <= 0 ||
            static_cast<size_t>(exe_desc_len) >= sizeof(exe_desc))
        {
            return cap::error;
        }

        alignas(bsheader) char
            cwd_path_bootstrap[sizeof(bsheader) + sizeof(cwd_desc)]{};
        auto *cwd_header = reinterpret_cast<bsheader *>(cwd_path_bootstrap);
        cwd_header->size =
            sizeof(bsheader) + static_cast<size_t>(cwd_desc_len) + 1;
        cwd_header->type = boot::TYPE_PATHEXP;
        memcpy(cwd_path_bootstrap + sizeof(bsheader), cwd_desc,
               static_cast<size_t>(cwd_desc_len) + 1);

        alignas(bsheader) char
            exe_path_bootstrap[sizeof(bsheader) + sizeof(exe_desc)]{};
        auto *exe_header = reinterpret_cast<bsheader *>(exe_path_bootstrap);
        exe_header->size =
            sizeof(bsheader) + static_cast<size_t>(exe_desc_len) + 1;
        exe_header->type = boot::TYPE_PATHEXP;
        memcpy(exe_path_bootstrap + sizeof(bsheader), exe_desc,
               static_cast<size_t>(exe_desc_len) + 1);

        CapIdx initial_caps[] = {prepared_root_dir_cap, prepared_cwd_dir_cap,
                                 prepared_parent_pcb_cap, cap::null};
        const char *bsargv[]  = {
            reinterpret_cast<const char *>(&root_bootstrap),
            reinterpret_cast<const char *>(&cwd_bootstrap_cap),
            reinterpret_cast<const char *>(&parent_bootstrap),
            cwd_path_bootstrap,
            exe_path_bootstrap,
            nullptr,
        };
        ExecveRequest request{
            .image_cap = kmod_getcap(fd),
            .execfn    = program_path,
            .caps      = initial_caps,
            .argv      = argv,
            .envp      = &ENVP[0],
            .bsargv    = bsargv,
        };
        auto child_pcb_res =
            sys_create_linux_process(SCHED_CLASS_FCFS, &request).to_result();
        if (!child_pcb_res.has_value()) {
            return cap::error;
        }
        return child_pcb_res.value();
    }

    [[nodiscard]]
    bool force_symlink(const char *path, const char *target) {
        if (path == nullptr || target == nullptr || path[0] == '\0' ||
            target[0] == '\0')
        {
            return false;
        }
        (void)kmod_unlink(path);
        return kmod_symlink(path, target) == 0;
    }

    CapIdx bootstrap_root_dir() {
        CapIdx cap = cap::null;
        bool found = false;
        bool ok    = bootstrap_foreach_record(
            __bsargv, __bsargc, [&](const BootstrapRecordView &view) {
                if (found || view.header->type != boot::TYPE_CAPEXP) {
                    return;
                }
                BootstrapCapExplainView cap_explain{};
                if (!bootstrap_parse_cap_explain(view, cap_explain) ||
                    cap_explain.cap_type != PayloadType::VDIR ||
                    cap_explain.cap_desc == nullptr ||
                    cap_explain.cap_desc[0] != '#')
                {
                    return;
                }
                if (strcmp(cap_explain.cap_desc + 1, "/") != 0) {
                    return;
                }
                cap   = cap_explain.cap_idx;
                found = true;
            });
        return ok && found ? cap : cap::null;
    }

    bool init_runner_context_caps(RunnerContext &ctx) {
        cleanup_runner_context_caps(ctx);
        if (ctx.root_dir_cap == cap::null || ctx.root_dir_cap == cap::error) {
            return false;
        }

        CapInfo root_info{};
        auto root_lookup_res =
            sys_cap_lookup(ctx.root_dir_cap, &root_info).to_result();
        if (!root_lookup_res.has_value()) {
            return false;
        }

        if (!prepare_cloned_cap(ctx.root_dir_cap, root_info.permissions,
                                ctx.prepared_root_dir_cap))
        {
            cleanup_runner_context_caps(ctx);
            return false;
        }

        auto parent_res =
            sys_cap_derive(__pcb_cap, PERM_PCB_GETPID | PERM_BASIC_CLONE)
                .to_result();
        if (!parent_res.has_value()) {
            cleanup_runner_context_caps(ctx);
            return false;
        }
        ctx.prepared_parent_pcb_cap = parent_res.value();
        return true;
    }

    void cleanup_runner_context_caps(RunnerContext &ctx) {
        if (ctx.prepared_root_dir_cap != cap::null &&
            ctx.prepared_root_dir_cap != cap::error)
        {
            (void)sys_cap_remove(ctx.prepared_root_dir_cap);
        }
        if (ctx.prepared_parent_pcb_cap != cap::null &&
            ctx.prepared_parent_pcb_cap != cap::error)
        {
            (void)sys_cap_remove(ctx.prepared_parent_pcb_cap);
        }
        ctx.prepared_root_dir_cap   = cap::null;
        ctx.prepared_parent_pcb_cap = cap::null;
    }

    bool open_cwd_dir(const char *path, OpenDirHandle &cwd) {
        cwd    = {};
        cwd.fd = kmod_opendir(path);
        if (cwd.fd < 0) {
            printf("contest-runner: opendir failed %s\n", path);
            return false;
        }

        cwd.cap = kmod_getcap(cwd.fd);
        if (cwd.cap == cap::null || cwd.cap == cap::error) {
            printf("contest-runner: cwd cap invalid %s\n", path);
            kmod_fclose(cwd.fd);
            cwd = {};
            return false;
        }

        CapInfo cwd_info{};
        auto cwd_lookup_res = sys_cap_lookup(cwd.cap, &cwd_info).to_result();
        if (!cwd_lookup_res.has_value() ||
            !prepare_cloned_cap(cwd.cap, cwd_info.permissions,
                                cwd.prepared_cap))
        {
            printf("contest-runner: cwd prepared cap invalid %s\n", path);
            kmod_fclose(cwd.fd);
            cwd = {};
            return false;
        }

        cwd.path = path;
        return true;
    }

    void close_cwd_dir(OpenDirHandle &cwd) {
        if (cwd.prepared_cap != cap::null && cwd.prepared_cap != cap::error) {
            (void)sys_cap_remove(cwd.prepared_cap).to_result();
        }
        if (cwd.fd >= 0) {
            kmod_fclose(cwd.fd);
        }
        cwd = {};
    }

    RunProgramError run_program(const RunnerContext &ctx,
                                const OpenDirHandle &cwd,
                                const char *program_path, const char *argv[],
                                int &status) {
        int fd = kmod_fopen(program_path, "x");
        if (fd < 0) {
            return RunProgramError::OPEN_FAILED;
        }

        CapIdx child_pcb = spawn_linux_program(
            fd, ctx.prepared_root_dir_cap, cwd.prepared_cap,
            ctx.prepared_parent_pcb_cap, cwd.path, program_path, argv);
        kmod_fclose(fd);
        if (child_pcb == cap::null || child_pcb == cap::error) {
            return RunProgramError::SPAWN_FAILED;
        }

        return wait_program(child_pcb, status);
    }

    RunProgramError spawn_program(const RunnerContext &ctx,
                                  const OpenDirHandle &cwd,
                                  const char *program_path, const char *argv[],
                                  CapIdx &child_pcb) {
        child_pcb = cap::null;
        int fd    = kmod_fopen(program_path, "x");
        if (fd < 0) {
            return RunProgramError::OPEN_FAILED;
        }

        child_pcb = spawn_linux_program(
            fd, ctx.prepared_root_dir_cap, cwd.prepared_cap,
            ctx.prepared_parent_pcb_cap, cwd.path, program_path, argv);
        kmod_fclose(fd);
        if (child_pcb == cap::null || child_pcb == cap::error) {
            return RunProgramError::SPAWN_FAILED;
        }
        return RunProgramError::NONE;
    }

    RunProgramError wait_program(CapIdx child_pcb, int &status) {
        status = 0;
        if (child_pcb == cap::null || child_pcb == cap::error) {
            return RunProgramError::WAIT_FAILED;
        }

        CapIdx wait_caps[] = {child_pcb, cap::null};
        auto wait_ret =
            sys_tcb_wait(__main_tcb_cap, wait_caps, &status, 0).to_result();
        if (!wait_ret.has_value()) {
            return RunProgramError::WAIT_FAILED;
        }
        CapIdx exited_cap = wait_ret.value();
        assert(exited_cap != cap::null && exited_cap != cap::error);
        return RunProgramError::NONE;
    }

    bool run_status_success(int status) {
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    int run_exit_code(int status) {
        return WIFEXITED(status) ? WEXITSTATUS(status) : status;
    }

    const char *run_error_string(RunProgramError error) {
        switch (error) {
            case RunProgramError::NONE:         return "none";
            case RunProgramError::OPEN_FAILED:  return "open";
            case RunProgramError::SPAWN_FAILED: return "spawn";
            case RunProgramError::WAIT_FAILED:  return "wait";
            default:                            return "unknown";
        }
    }

    void accumulate_stats(TestRunStats &total, const TestRunStats &part) {
        total.total  += part.total;
        total.passed += part.passed;
        total.failed += part.failed;
    }

    bool dolink(const char *pathA, const char *pathB) {
        bool ok = force_symlink(pathA, pathB);
        printf("%s link %s -> %s\n", ok ? "successfully" : "failed to", pathA,
               pathB);
        return ok;
    }

    bool dounlink(const char *path) {
        bool ok = kmod_unlink(path) == 0;
        printf("%s unlink %s\n", ok ? "successfully" : "failed to", path);
        return ok;
    }

#if defined(__ARCH_riscv64__)
#define LIB_PATH   "/lib"
#define LD_SO_PATH "/lib/ld-linux-riscv64-lp64d.so.1"
#elif defined(__ARCH_loongarch64__)
#define LIB_PATH   "/lib64"
#define LD_SO_PATH "/lib64/ld-linux-loongarch-lp64d.so.1"
#endif

#define GLIBC_LIB_PATH  "/testing/glibc/lib"
#define MUSL_LIB_PATH   "/testing/musl/lib"
#define MUSL_LD_SO_PATH "/testing/musl/lib/libc.so"

    bool glibc_env_setup() {
        bool ok  = true;
        ok      &= dolink(LIB_PATH, GLIBC_LIB_PATH);
        return ok;
    }

    bool glibc_env_cleanup() {
        bool ok  = true;
        ok      &= dounlink(LIB_PATH);
        return ok;
    }

    bool run_glibc(CapIdx root_dir_cap, contest_runner::TestRunStats &total) {
        RunnerContext ctx{
            .root_dir_cap = root_dir_cap,
            .libc_root    = "/testing/glibc",
            .libc_name    = "glibc",
        };
        if (!contest_runner::init_runner_context_caps(ctx)) {
            printf("contest-runner: failed to prepare runner caps for glibc\n");
            return false;
        }
        if (!glibc_env_setup()) {
            printf("contest-runner: failed to setup glibc env\n");
            contest_runner::cleanup_runner_context_caps(ctx);
            return false;
        }

        contest_runner::accumulate_stats(total, contest_runner::run_basic(ctx));
        contest_runner::accumulate_stats(total,
                                         contest_runner::run_busybox(ctx));
        // contest_runner::accumulate_stats(total,
        //  contest_runner::run_libctest(ctx));
        // contest_runner::accumulate_stats(total,
        // contest_runner::run_ltp(ctx));
        contest_runner::cleanup_runner_context_caps(ctx);

        if (!glibc_env_cleanup()) {
            printf("contest-runner: failed to cleanup glibc env\n");
            return false;
        }

        return true;
    }

    bool musl_env_setup() {
        bool ok  = true;
        ok      &= dolink(LIB_PATH, MUSL_LIB_PATH);
        ok      &= dolink(LD_SO_PATH, MUSL_LD_SO_PATH);
        return ok;
    }

    bool musl_env_cleanup() {
        bool ok  = true;
        ok      &= dounlink(LIB_PATH);
        ok      &= dounlink(LD_SO_PATH);
        return ok;
    }

    bool run_musl(CapIdx root_dir_cap, contest_runner::TestRunStats &total) {
        RunnerContext ctx{
            .root_dir_cap = root_dir_cap,
            .libc_root    = "/testing/musl",
            .libc_name    = "musl",
        };
        if (!contest_runner::init_runner_context_caps(ctx)) {
            printf("contest-runner: failed to prepare runner caps for musl\n");
            return false;
        }
        if (!musl_env_setup()) {
            printf("contest-runner: failed to setup musl env\n");
            contest_runner::cleanup_runner_context_caps(ctx);
            return false;
        }

        contest_runner::accumulate_stats(total, contest_runner::run_basic(ctx));
        // contest_runner::accumulate_stats(total,
        //                                  contest_runner::run_busybox(ctx));
        // contest_runner::accumulate_stats(total,
        //  contest_runner::run_libctest(ctx));
        // contest_runner::accumulate_stats(total,
        // contest_runner::run_ltp(ctx));
        contest_runner::cleanup_runner_context_caps(ctx);
        return true;
    }
}  // namespace contest_runner

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
                         const bsheader *bsargv[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)bsargv;

    CapIdx root_dir_cap = contest_runner::bootstrap_root_dir();
    if (root_dir_cap == cap::null || root_dir_cap == cap::error) {
        printf("contest-runner: bootstrap root dir capability missing\n");
        return 1;
    }

    contest_runner::TestRunStats total{};
    bool ok  = true;
    // ok      &= contest_runner::run_glibc(root_dir_cap, total);
    ok      &= contest_runner::run_musl(root_dir_cap, total);

    printf("contest-runner: all done total=%lu passed=%lu failed=%lu\n",
           static_cast<unsigned long>(total.total),
           static_cast<unsigned long>(total.passed),
           static_cast<unsigned long>(total.failed));
    (void)sys_shutdown();
    return total.failed == 0 ? 0 : 1;
}
