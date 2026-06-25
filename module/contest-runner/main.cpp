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
    namespace {
        constexpr uint64_t PERM_BASIC_MIGRATE_ONCE = 0x0008;
        constexpr uint64_t PERM_PCB_GETPID         = 0x01'0000;

        [[nodiscard]]
        CapIdx spawn_linux_program(int fd, CapIdx root_dir_cap,
                                   CapIdx cwd_dir_cap, const char *cwd_path,
                                   const char *argv[]) {
            if (fd < 0 || root_dir_cap == cap::null || root_dir_cap == cap::error ||
                cwd_dir_cap == cap::null || cwd_dir_cap == cap::error)
            {
                return cap::error;
            }
            if (cwd_path == nullptr || cwd_path[0] == '\0') {
                return cap::error;
            }

            CapIdx child_root_cap = sys_cap_clone(root_dir_cap);
            if (child_root_cap == cap::null || child_root_cap == cap::error) {
                return cap::error;
            }
            CapIdx child_cwd_dir_cap = sys_cap_clone(cwd_dir_cap);
            if (child_cwd_dir_cap == cap::null ||
                child_cwd_dir_cap == cap::error)
            {
                sys_cap_remove(child_root_cap);
                return cap::error;
            }
            CapIdx child_parent_pcb_cap =
                sys_cap_derive(__pcb_cap,
                               PERM_PCB_GETPID | PERM_BASIC_MIGRATE_ONCE);
            if (child_parent_pcb_cap == cap::null ||
                child_parent_pcb_cap == cap::error)
            {
                sys_cap_remove(child_root_cap);
                sys_cap_remove(child_cwd_dir_cap);
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
                        .cap_idx  = child_root_cap,
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
                        .cap_idx  = child_cwd_dir_cap,
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
                        .cap_idx  = child_parent_pcb_cap,
                        .cap_type = PayloadType::PCB,
                        .cap_perm = PERM_PCB_GETPID | PERM_BASIC_MIGRATE_ONCE,
                    },
                .desc = "#parent",
            };

            char cwd_desc[256]{};
            int cwd_desc_len = snprintf(cwd_desc, sizeof(cwd_desc), "#cwd:%s",
                                        cwd_path);
            if (cwd_desc_len <= 0 ||
                static_cast<size_t>(cwd_desc_len) >= sizeof(cwd_desc))
            {
                sys_cap_remove(child_root_cap);
                sys_cap_remove(child_cwd_dir_cap);
                sys_cap_remove(child_parent_pcb_cap);
                return cap::error;
            }

            alignas(bsheader) char cwd_path_bootstrap[sizeof(bsheader) +
                                                      sizeof(cwd_desc)]{};
            auto *cwd_header = reinterpret_cast<bsheader *>(cwd_path_bootstrap);
            cwd_header->size =
                sizeof(bsheader) + static_cast<size_t>(cwd_desc_len) + 1;
            cwd_header->type = boot::TYPE_PATHEXP;
            memcpy(cwd_path_bootstrap + sizeof(bsheader), cwd_desc,
                   static_cast<size_t>(cwd_desc_len) + 1);

            CapIdx initial_caps[] = {child_root_cap, child_cwd_dir_cap,
                                     child_parent_pcb_cap, cap::null};
            const char *bsargv[]  = {
                reinterpret_cast<const char *>(&root_bootstrap),
                reinterpret_cast<const char *>(&cwd_bootstrap_cap),
                reinterpret_cast<const char *>(&parent_bootstrap),
                cwd_path_bootstrap,
                nullptr,
            };
            CapIdx child_pcb      = sys_create_linux_process(
                kmod_getcap(fd), SCHED_CLASS_FCFS, initial_caps, argv, nullptr,
                bsargv);
            sys_cap_remove(child_root_cap);
            sys_cap_remove(child_cwd_dir_cap);
            if (child_pcb == cap::null || child_pcb == cap::error) {
                sys_cap_remove(child_parent_pcb_cap);
            }
            return child_pcb;
        }
    }  // namespace

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

    bool open_cwd_dir(const char *path, OpenDirHandle &cwd) {
        cwd = {};
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

        cwd.path = path;
        return true;
    }

    void close_cwd_dir(OpenDirHandle &cwd) {
        if (cwd.fd >= 0) {
            kmod_fclose(cwd.fd);
        }
        cwd = {};
    }

    RunProgramError run_program(const RunnerContext &ctx,
                                const OpenDirHandle &cwd,
                                const char *program_path, const char *argv[],
                                int &status) {
        status = 0;
        int fd = kmod_fopen(program_path, "x");
        if (fd < 0) {
            return RunProgramError::OPEN_FAILED;
        }

        CapIdx child_pcb = spawn_linux_program(fd, ctx.root_dir_cap, cwd.cap,
                                               cwd.path, argv);
        kmod_fclose(fd);
        if (child_pcb == cap::null || child_pcb == cap::error) {
            return RunProgramError::SPAWN_FAILED;
        }

        CapIdx wait_caps[] = {child_pcb, cap::null};
        CapIdx exited_cap  = sys_tcb_wait(__main_tcb_cap, wait_caps, &status, 0);
        if (exited_cap == cap::null || exited_cap == cap::error) {
            return RunProgramError::WAIT_FAILED;
        }
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
            case RunProgramError::NONE:        return "none";
            case RunProgramError::OPEN_FAILED: return "open";
            case RunProgramError::SPAWN_FAILED: return "spawn";
            case RunProgramError::WAIT_FAILED: return "wait";
            default:                           return "unknown";
        }
    }

    void accumulate_stats(TestRunStats &total, const TestRunStats &part) {
        total.total += part.total;
        total.passed += part.passed;
        total.failed += part.failed;
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

    struct LibcTarget {
        const char *name;
        const char *root;
    };
    constexpr LibcTarget LIBC_TARGETS[] = {
        {.name="glibc", .root="/testing/glibc"},
        {.name="musl", .root="/testing/musl"},
        {.name=nullptr, .root=nullptr},
    };

    contest_runner::TestRunStats total{};
    for (size_t i = 0; LIBC_TARGETS[i].name != nullptr; ++i) {
        contest_runner::RunnerContext ctx{
            .root_dir_cap = root_dir_cap,
            .libc_root    = LIBC_TARGETS[i].root,
            .libc_name    = LIBC_TARGETS[i].name,
        };

        contest_runner::accumulate_stats(total,
                                         contest_runner::run_basic(ctx));
        // contest_runner::accumulate_stats(total,
        //                                  contest_runner::run_busybox(ctx));
        // contest_runner::accumulate_stats(total,
        //                                  contest_runner::run_libctest(ctx));
        // contest_runner::accumulate_stats(total,
        //                                  contest_runner::run_ltp(ctx));
    }

    printf("contest-runner: all done total=%lu passed=%lu failed=%lu\n",
           static_cast<unsigned long>(total.total),
           static_cast<unsigned long>(total.passed),
           static_cast<unsigned long>(total.failed));
    sys_shutdown();
    return total.failed == 0 ? 0 : 1;
}
