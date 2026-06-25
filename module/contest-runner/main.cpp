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

#include <kmod/syscall.h>
#include <sustcore/bootstrap.h>
#include <sys/wait.h>

#include <cstdint>
#include <cstdio>

#include "basic.h"

namespace {
    constexpr uint64_t PERM_BASIC_MIGRATE_ONCE = 0x0008;
    constexpr uint64_t PERM_PCB_GETPID         = 0x01'0000;

    constexpr const char *TEST_ROOTS[] = {
        "/testing/glibc/basic",
        "/testing/musl/basic",
        nullptr,
    };

    struct TestRunStats {
        size_t total  = 0;
        size_t passed = 0;
        size_t failed = 0;
    };

    [[nodiscard]]
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

    [[nodiscard]]
    CapIdx spawn_linux_test(int fd, CapIdx root_dir_cap, CapIdx cwd_dir_cap,
                            const char *cwd_path) {
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
        if (child_cwd_dir_cap == cap::null || child_cwd_dir_cap == cap::error) {
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
        } bootstrap{
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
        } cwd_dir_bootstrap{
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
        } parent_pcb_bootstrap{
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

        alignas(bsheader) char cwd_bootstrap[sizeof(bsheader) + sizeof(cwd_desc)]{};
        auto *cwd_header = reinterpret_cast<bsheader *>(cwd_bootstrap);
        cwd_header->size = sizeof(bsheader) + static_cast<size_t>(cwd_desc_len) + 1;
        cwd_header->type = boot::TYPE_PATHEXP;
        memcpy(cwd_bootstrap + sizeof(bsheader), cwd_desc,
               static_cast<size_t>(cwd_desc_len) + 1);

        CapIdx initial_caps[] = {child_root_cap, child_cwd_dir_cap,
                                 child_parent_pcb_cap, cap::null};
        const char *bsargv[]  = {reinterpret_cast<const char *>(&bootstrap),
                                 reinterpret_cast<const char *>(&cwd_dir_bootstrap),
                                 reinterpret_cast<const char *>(&parent_pcb_bootstrap),
                                 cwd_bootstrap, nullptr};
        CapIdx child_pcb      = sys_create_linux_process(
            kmod_getcap(fd), SCHED_CLASS_FCFS, initial_caps, nullptr, nullptr,
            bsargv);
        sys_cap_remove(child_root_cap);
        sys_cap_remove(child_cwd_dir_cap);
        if (child_pcb == cap::null || child_pcb == cap::error) {
            sys_cap_remove(child_parent_pcb_cap);
        }
        return child_pcb;
    }

    void run_testcase(const char *root, const char *testcase,
                      CapIdx root_dir_cap, CapIdx cwd_dir_cap,
                      TestRunStats &stats) {
        ++stats.total;

        char path[256]{};
        snprintf(path, sizeof(path), "%s/%s", root, testcase);
        printf("contest-runner: start %s\n", path);

        int fd = kmod_fopen(path, "x");
        if (fd < 0) {
            ++stats.failed;
            printf("contest-runner: open failed %s\n", path);
            return;
        }

        CapIdx child_pcb = spawn_linux_test(fd, root_dir_cap, cwd_dir_cap, root);
        kmod_fclose(fd);
        if (child_pcb == cap::null || child_pcb == cap::error) {
            ++stats.failed;
            printf("contest-runner: spawn failed %s\n", path);
            return;
        }

        CapIdx wait_caps[] = {child_pcb, cap::null};
        int status         = 0;
        CapIdx exited_cap =
            sys_tcb_wait(__main_tcb_cap, wait_caps, &status, 0);
        if (exited_cap == cap::null || exited_cap == cap::error) {
            ++stats.failed;
            printf("contest-runner: wait failed %s\n", path);
            return;
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ++stats.failed;
            printf("contest-runner: failed %s status=0x%x\n", path, status);
            return;
        }

        ++stats.passed;
        printf("contest-runner: passed %s status=0x%x\n", path, status);
    }

    [[nodiscard]]
    TestRunStats run_suite(const char *root, CapIdx root_dir_cap) {
        TestRunStats stats{};
        int cwd_fd = kmod_opendir(root);
        if (cwd_fd < 0) {
            printf("contest-runner: opendir failed %s\n", root);
            stats.total  = 0;
            stats.failed = 0;
            return stats;
        }
        CapIdx cwd_dir_cap = kmod_getcap(cwd_fd);
        if (cwd_dir_cap == cap::null || cwd_dir_cap == cap::error) {
            printf("contest-runner: cwd cap invalid %s\n", root);
            kmod_fclose(cwd_fd);
            return stats;
        }

        printf("contest-runner: suite begin %s\n", root);
        for (size_t i = 0; basic::testcases[i] != nullptr; ++i) {
            run_testcase(root, basic::testcases[i], root_dir_cap, cwd_dir_cap,
                         stats);
        }
        kmod_fclose(cwd_fd);
        printf("contest-runner: suite done %s total=%lu passed=%lu failed=%lu\n",
               root, static_cast<unsigned long>(stats.total),
               static_cast<unsigned long>(stats.passed),
               static_cast<unsigned long>(stats.failed));
        return stats;
    }
}  // namespace

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
                         const bsheader *bsargv[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)bsargv;

    CapIdx root_dir_cap = bootstrap_root_dir();
    if (root_dir_cap == cap::null || root_dir_cap == cap::error) {
        printf("contest-runner: bootstrap root dir capability missing\n");
        return 1;
    }

    TestRunStats total{};
    for (size_t i = 0; TEST_ROOTS[i] != nullptr; ++i) {
        auto stats = run_suite(TEST_ROOTS[i], root_dir_cap);
        total.total += stats.total;
        total.passed += stats.passed;
        total.failed += stats.failed;
    }

    printf("contest-runner: all done total=%lu passed=%lu failed=%lu\n",
           static_cast<unsigned long>(total.total),
           static_cast<unsigned long>(total.passed),
           static_cast<unsigned long>(total.failed));
    sys_shutdown();
    return total.failed == 0 ? 0 : 1;
}
