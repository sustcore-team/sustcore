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

#include <cstdio>
#include <string>
#include <vector>

#include "basic.h"

namespace {
    constexpr const char *TEST_ROOTS[] = {
        "/testing/glibc/basic",
        "/testing/musl/basic",
        nullptr,
    };

    constexpr const char *KMOD_TESTS[] = {
        // "/initrd/test_page_cache.mod",
        // "/initrd/test_page_cache_perf.mod",
        // "/initrd/test_file_backed_memory.mod",
        // "/initrd/test_fs_score.mod",
        nullptr,
    };

    struct FailureInfo {
        std::string path;
        int status;
    };

    struct TestRunStats {
        size_t total  = 0;
        size_t passed = 0;
        size_t failed = 0;
        std::vector<FailureInfo> failures;
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
    CapIdx spawn_kmod_test(int fd, CapIdx root_dir_cap) {
        if (fd < 0 || root_dir_cap == cap::null || root_dir_cap == cap::error) {
            return cap::error;
        }

        CapIdx child_root_cap = sys_cap_clone(root_dir_cap);
        if (child_root_cap == cap::null || child_root_cap == cap::error) {
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

        CapIdx initial_caps[] = {child_root_cap, cap::null};
        const char *bsargv[]  = {reinterpret_cast<const char *>(&bootstrap),
                                 nullptr};
        CapIdx child_pcb      = sys_create_process(
            kmod_getcap(fd), SCHED_CLASS_RR, initial_caps, nullptr, nullptr,
            bsargv);
        sys_cap_remove(child_root_cap);
        return child_pcb;
    }

    [[nodiscard]]
    CapIdx spawn_linux_test(int fd, CapIdx root_dir_cap,
                            const char *cwd_path) {
        if (fd < 0 || root_dir_cap == cap::null || root_dir_cap == cap::error) {
            return cap::error;
        }
        if (cwd_path == nullptr || cwd_path[0] == '\0') {
            return cap::error;
        }

        CapIdx child_root_cap = sys_cap_clone(root_dir_cap);
        if (child_root_cap == cap::null || child_root_cap == cap::error) {
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

        const size_t cwd_path_len = strlen(cwd_path) + 1;
        alignas(bsheader) char cwd_bootstrap[sizeof(bsheader) + 256]{};
        if (cwd_path_len > sizeof(cwd_bootstrap) - sizeof(bsheader)) {
            sys_cap_remove(child_root_cap);
            return cap::error;
        }
        auto *cwd_header = reinterpret_cast<bsheader *>(cwd_bootstrap);
        cwd_header->size = sizeof(bsheader) + cwd_path_len;
        cwd_header->type = boot::TYPE_CWDPATH;
        memcpy(cwd_bootstrap + sizeof(bsheader), cwd_path, cwd_path_len);

        CapIdx initial_caps[] = {child_root_cap, cap::null};
        const char *bsargv[]  = {reinterpret_cast<const char *>(&bootstrap),
                                 cwd_bootstrap, nullptr};
        CapIdx child_pcb      = sys_create_linux_process(
            kmod_getcap(fd), SCHED_CLASS_FCFS, initial_caps, nullptr, nullptr,
            bsargv);
        sys_cap_remove(child_root_cap);
        return child_pcb;
    }

    void run_kmod_testcase(const char *path, CapIdx root_dir_cap,
                           TestRunStats &stats) {
        ++stats.total;
        printf("contest-runner: start %s\n", path);

        int fd = kmod_fopen(path, "x");
        if (fd < 0) {
            ++stats.failed;
            stats.failures.push_back({std::string(path), -1});
            printf("contest-runner: open failed %s\n", path);
            return;
        }

        CapIdx child_pcb = spawn_kmod_test(fd, root_dir_cap);
        kmod_fclose(fd);
        if (child_pcb == cap::null || child_pcb == cap::error) {
            ++stats.failed;
            stats.failures.push_back({std::string(path), -1});
            printf("contest-runner: spawn failed %s\n", path);
            return;
        }

        CapIdx wait_caps[] = {child_pcb, cap::null};
        int status         = 0;
        CapIdx exited_cap =
            sys_tcb_wait(__main_tcb_cap, wait_caps, &status, 0);
        if (exited_cap == cap::null || exited_cap == cap::error) {
            ++stats.failed;
            stats.failures.push_back({std::string(path), -1});
            printf("contest-runner: wait failed %s\n", path);
            return;
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ++stats.failed;
            stats.failures.push_back({std::string(path), status});
            printf("contest-runner: failed %s status=0x%x\n", path, status);
            return;
        }

        ++stats.passed;
        printf("contest-runner: passed %s status=0x%x\n", path, status);
    }

    [[nodiscard]]
    TestRunStats run_kmod_suite(CapIdx root_dir_cap) {
        TestRunStats stats{};
        printf("contest-runner: kmod suite begin\n");
        for (size_t i = 0; KMOD_TESTS[i] != nullptr; ++i) {
            run_kmod_testcase(KMOD_TESTS[i], root_dir_cap, stats);
        }
        printf("contest-runner: kmod suite done total=%lu passed=%lu failed=%lu\n",
               static_cast<unsigned long>(stats.total),
               static_cast<unsigned long>(stats.passed),
               static_cast<unsigned long>(stats.failed));
        return stats;
    }

    void run_testcase(const char *root, const char *testcase,
                      CapIdx root_dir_cap,
                      TestRunStats &stats) {
        ++stats.total;

        char path[256]{};
        snprintf(path, sizeof(path), "%s/%s", root, testcase);
        printf("contest-runner: start %s\n", path);

        int fd = kmod_fopen(path, "x");
        if (fd < 0) {
            ++stats.failed;
            stats.failures.push_back({std::string(path), -1});
            printf("contest-runner: open failed %s\n", path);
            return;
        }

        CapIdx child_pcb = spawn_linux_test(fd, root_dir_cap, root);
        kmod_fclose(fd);
        if (child_pcb == cap::null || child_pcb == cap::error) {
            ++stats.failed;
            stats.failures.push_back({std::string(path), -1});
            printf("contest-runner: spawn failed %s\n", path);
            return;
        }

        CapIdx wait_caps[] = {child_pcb, cap::null};
        int status         = 0;
        CapIdx exited_cap =
            sys_tcb_wait(__main_tcb_cap, wait_caps, &status, 0);
        if (exited_cap == cap::null || exited_cap == cap::error) {
            ++stats.failed;
            stats.failures.push_back({std::string(path), -1});
            printf("contest-runner: wait failed %s\n", path);
            return;
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ++stats.failed;
            stats.failures.push_back({std::string(path), status});
            printf("contest-runner: failed %s status=0x%x\n", path, status);
            return;
        }

        ++stats.passed;
        printf("contest-runner: passed %s status=0x%x\n", path, status);
    }

    [[nodiscard]]
    TestRunStats run_suite(const char *root, CapIdx root_dir_cap) {
        TestRunStats stats{};
        printf("contest-runner: suite begin %s\n", root);
        for (size_t i = 0; basic::testcases[i] != nullptr; ++i) {
            run_testcase(root, basic::testcases[i], root_dir_cap, stats);
        }
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
    auto kmod_stats = run_kmod_suite(root_dir_cap);
    total.total += kmod_stats.total;
    total.passed += kmod_stats.passed;
    total.failed += kmod_stats.failed;
    for (auto &f : kmod_stats.failures) {
        total.failures.push_back(std::move(f));
    }

    for (size_t i = 0; TEST_ROOTS[i] != nullptr; ++i) {
        auto stats = run_suite(TEST_ROOTS[i], root_dir_cap);
        total.total += stats.total;
        total.passed += stats.passed;
        total.failed += stats.failed;
        for (auto &f : stats.failures) {
            total.failures.push_back(std::move(f));
        }
    }

    if (!total.failures.empty()) {
        printf("contest-runner: FAILED TESTS:\n");
        for (const auto &f : total.failures) {
            printf("  %s (status=0x%x)\n", f.path.c_str(), f.status);
        }
    }
    printf("contest-runner: all done total=%lu passed=%lu failed=%lu\n",
           static_cast<unsigned long>(total.total),
           static_cast<unsigned long>(total.passed),
           static_cast<unsigned long>(total.failed));
    sys_shutdown();
    return total.failed == 0 ? 0 : 1;
}
