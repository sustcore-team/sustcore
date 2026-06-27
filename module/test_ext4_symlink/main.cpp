/**
 * @file main.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief Ext4 symbolic link tests
 * @version alpha-1.0.0
 * @date 2026-06-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sustcore/bootstrap.h>
#include <kmod/syscall.h>

#include <cstdio>
#include <cstring>

namespace {
    CapIdx g_ext4_cap = cap::null;

    [[nodiscard]]
    CapIdx open_ext4_root(CapIdx root_cap) {
        auto res =
            sys_vfs_opendir(root_cap, "testing", flags::O_READ).to_result();
        if (!res.has_value()) {
            return cap::error;
        }
        CapIdx dir = res.value();
        if (dir == cap::null) {
            return cap::error;
        }
        return dir;
    }

    [[nodiscard]]
    CapIdx bootstrap_root_dir() {
        CapIdx cap = cap::null;
        bool found = false;
        bool ok    = bootstrap_foreach_record(
            __bsargv, __bsargc,
            [&](const BootstrapRecordView &view) {
                if (found || view.header->type != boot::TYPE_CAPEXP) {
                    return;
                }
                BootstrapCapExplainView cap_explain{};
                if (!bootstrap_parse_cap_explain(view, cap_explain) ||
                    cap_explain.cap_type != PayloadType::VDIR ||
                    cap_explain.cap_desc == nullptr ||
                    cap_explain.cap_desc[0] != '#') {
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
    bool test_create_inline_symlink() {
        constexpr const char *name   = ".tst_il";
        constexpr const char *target = "inline_target_content_42";

        auto res = sys_vfs_symlink(g_ext4_cap, name, target).to_result();
        if (!res.has_value()) {
            printf("  FAIL: inline symlink create failed\n");
            return false;
        }

        char buf[128] {};
        auto rl_res =
            sys_vfs_readlink(g_ext4_cap, name, buf, sizeof(buf) - 1)
                .to_result();
        if (!rl_res.has_value()) {
            printf("  FAIL: readlink inline symlink failed\n");
            return false;
        }
        buf[rl_res.value()] = '\0';
        if (strcmp(buf, target) != 0) {
            printf("  FAIL: inline symlink mismatch got=\"%s\" expect=\"%s\"\n",
                   buf, target);
            return false;
        }

        NodeMeta st {};
        auto st_res = sys_vfs_lstat(g_ext4_cap, name, &st).to_result();
        if (!st_res.has_value() || st.type != EntryType::SYMLINK) {
            printf("  FAIL: lstat inline symlink type=%llu\n",
                   static_cast<unsigned long long>(st.type));
            return false;
        }
        printf("  OK: create-inline-symlink -> \"%s\" size=%u\n", buf,
               static_cast<unsigned>(st.size));
        return true;
    }

    [[nodiscard]]
    bool test_create_medium_symlink() {
        constexpr const char *name = ".tst_md";
        constexpr const char
            *target = "medium_target__________________________________________________"
                      "_______";

        auto res = sys_vfs_symlink(g_ext4_cap, name, target).to_result();
        if (!res.has_value()) {
            printf("  FAIL: medium symlink create failed\n");
            return false;
        }

        char buf[128] {};
        auto rl_res =
            sys_vfs_readlink(g_ext4_cap, name, buf, sizeof(buf) - 1)
                .to_result();
        if (!rl_res.has_value()) {
            printf("  FAIL: readlink medium symlink failed\n");
            return false;
        }
        buf[rl_res.value()] = '\0';
        if (strcmp(buf, target) != 0) {
            printf("  FAIL: medium symlink mismatch got=\"%s\" expect=\"%s\"\n",
                   buf, target);
            return false;
        }

        NodeMeta st {};
        auto st_res = sys_vfs_lstat(g_ext4_cap, name, &st).to_result();
        if (!st_res.has_value() || st.type != EntryType::SYMLINK) {
            printf("  FAIL: lstat medium symlink type=%llu\n",
                   static_cast<unsigned long long>(st.type));
            return false;
        }
        printf("  OK: create-medium-symlink len=%u size=%u\n",
               static_cast<unsigned>(rl_res.value()),
               static_cast<unsigned>(st.size));
        return true;
    }

    [[nodiscard]]
    bool test_unlink_inline_symlink() {
        constexpr const char *name   = ".tst_ui";
        constexpr const char *target = "target_for_unlink";
        auto create_res =
            sys_vfs_symlink(g_ext4_cap, name, target).to_result();
        if (!create_res.has_value()) {
            printf("  FAIL: unlink-inline create failed\n");
            return false;
        }

        NodeMeta st_before {};
        auto st_res =
            sys_vfs_lstat(g_ext4_cap, name, &st_before).to_result();
        if (!st_res.has_value() || st_before.type != EntryType::SYMLINK) {
            printf("  FAIL: unlink-inline pre-check failed\n");
            return false;
        }

        auto unlink_res = sys_vfs_unlink(g_ext4_cap, name).to_result();
        if (!unlink_res.has_value()) {
            printf("  FAIL: unlink inline symlink failed\n");
            return false;
        }

        NodeMeta st_after {};
        auto gone_res =
            sys_vfs_lstat(g_ext4_cap, name, &st_after).to_result();
        if (gone_res.has_value()) {
            printf("  FAIL: inline symlink still exists after unlink\n");
            return false;
        }
        printf("  OK: unlink-inline-symlink removed successfully\n");
        return true;
    }

    [[nodiscard]]
    bool test_unlink_medium_symlink() {
        constexpr const char *name = ".tst_um";
        constexpr const char
            *target = "medium_target_for_unlink________________________________________"
                      "_______";
        auto create_res =
            sys_vfs_symlink(g_ext4_cap, name, target).to_result();
        if (!create_res.has_value()) {
            printf("  FAIL: unlink-medium create failed\n");
            return false;
        }

        NodeMeta st_before {};
        auto st_res =
            sys_vfs_lstat(g_ext4_cap, name, &st_before).to_result();
        if (!st_res.has_value() || st_before.type != EntryType::SYMLINK) {
            printf("  FAIL: unlink-medium pre-check failed\n");
            return false;
        }

        auto unlink_res = sys_vfs_unlink(g_ext4_cap, name).to_result();
        if (!unlink_res.has_value()) {
            printf("  FAIL: unlink medium symlink failed\n");
            return false;
        }

        NodeMeta st_after {};
        auto gone_res =
            sys_vfs_lstat(g_ext4_cap, name, &st_after).to_result();
        if (gone_res.has_value()) {
            printf("  FAIL: medium symlink still exists after unlink\n");
            return false;
        }
        printf("  OK: unlink-medium-symlink removed successfully\n");
        return true;
    }

    void cleanup_test_entries() {
        (void)sys_vfs_unlink(g_ext4_cap, ".tst_il").to_result();
        (void)sys_vfs_unlink(g_ext4_cap, ".tst_md").to_result();
        (void)sys_vfs_unlink(g_ext4_cap, ".tst_ui").to_result();
        (void)sys_vfs_unlink(g_ext4_cap, ".tst_um").to_result();
    }

}  // namespace

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
                         const bsheader *bsargv[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)bsargv;
    printf("test_ext4_symlink: start pid=%u\n",
           sys_getpid(__pcb_cap).value());

    CapIdx root_cap = bootstrap_root_dir();
    if (root_cap == cap::null || root_cap == cap::error) {
        printf("test_ext4_symlink: bootstrap root dir missing\n");
        exit(-1);
    }

    g_ext4_cap = open_ext4_root(root_cap);
    if (g_ext4_cap == cap::null || g_ext4_cap == cap::error) {
        printf("test_ext4_symlink: open /testing failed\n");
        exit(-1);
    }

    cleanup_test_entries();

    bool ok = true;
    ok &= test_create_inline_symlink();
    ok &= test_create_medium_symlink();
    ok &= test_unlink_inline_symlink();
    ok &= test_unlink_medium_symlink();

    (void)sys_cap_remove(g_ext4_cap).to_result();

    if (!ok) {
        printf("test_ext4_symlink: FAILED\n");
        exit(-1);
    }

    printf("test_ext4_symlink: PASS\n");
    exit(0);
    return 0;
}
