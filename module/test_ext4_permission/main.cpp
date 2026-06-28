/**
 * @file main.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief Ext4 permission test module — getattr, setattr (fchownat), symlink
 *        nofollow, and error paths
 * @version alpha-1.0.0
 * @date 2026-06-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sustcore/bootstrap.h>
#include <kmod/syscall.h>

#include <cstdio>
#include <cstring>

// Forward-declare sys_vfs_fchownat — present in libs/linuxss-libc/syscall.h
// but not yet mirrored in include/kmod/syscall.h
extern "C" SysRet<void> sys_vfs_fchownat(CapIdx dirfd, uint32_t uid,
                                         uint32_t gid, uint32_t flags,
                                         const char *pathname);

namespace {
    constexpr const char *EXT4_PATH      = "/test_img";
    constexpr const char *EXT4_TEST_DIR  = "test_img";
    constexpr const char *PASSWD_PATH    = "/test_img/etc/passwd";
    constexpr const char *PERM_TEST_FILE = ".tst_perm";
    constexpr const char *PERM_SYMLINK   = ".tst_perm_lnk";
    constexpr const char *SYMLINK_TARGET = ".tst_perm";
    constexpr const char *NONEXISTENT    = ".tst_nonexistent_12345";
    constexpr uint32_t    AT_SYMLINK_NOFOLLOW = 0x100;
    constexpr uint32_t    NO_CHANGE           = 0xFFFFFFFF;

    CapIdx g_ext4_cap = cap::null;

    static int test_count = 0;
    static int pass_count = 0;

#define TEST_CHECK(cond, msg)                               \
    do {                                                    \
        test_count++;                                       \
        if (!(cond)) {                                      \
            printf("  FAIL [%s]\n", msg);                    \
        } else {                                            \
            printf("  PASS [%s]\n", msg);                    \
            pass_count++;                                   \
        }                                                   \
    } while (0)

    [[nodiscard]]
    CapIdx open_ext4_root(CapIdx root_cap) {
        auto res =
            sys_vfs_opendir(root_cap, EXT4_TEST_DIR,
                            flags::O_READ | flags::O_WRITE).to_result();
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

    /**
     * @brief Ensure a test file exists in the ext4 root for ownership tests.
     */
    [[nodiscard]]
    bool ensure_test_file() {
        int fd = kmod_mkfile("/test_img/.tst_perm", "w+");
        if (fd < 0) {
            fd = kmod_fopen("/test_img/.tst_perm", "r");
            if (fd < 0) {
                return false;
            }
        }
        kmod_fclose(fd);
        return true;
    }

    // ── Test Cases ────────────────────────────────────────────────

    /**
     * @brief test_getattr_file — open a regular file and verify
     *        EntryType::FILE through fstat.
     */
    [[nodiscard]]
    bool test_getattr_file() {
        int fd = kmod_fopen(PASSWD_PATH, "r");
        TEST_CHECK(fd >= 0,
                   "getattr-file: open /test_img/etc/passwd");
        if (fd < 0) {
            return false;
        }

        CapIdx file_cap = kmod_getcap(fd);
        NodeMeta meta{};
        auto stat_res = sys_vfs_fstat(file_cap, &meta).to_result();
        TEST_CHECK(stat_res.has_value(),
                   "getattr-file: fstat succeeded");
        TEST_CHECK(meta.type == EntryType::FILE,
                   "getattr-file: type is FILE (S_IFREG)");

        kmod_fclose(fd);
        return stat_res.has_value() && meta.type == EntryType::FILE;
    }

    /**
     * @brief test_getattr_dir — open a directory and verify
     *        EntryType::DIR through fstat.
     */
    [[nodiscard]]
    bool test_getattr_dir() {
        NodeMeta meta{};
        auto stat_res = sys_vfs_fstat(g_ext4_cap, &meta).to_result();
        TEST_CHECK(stat_res.has_value(),
                   "getattr-dir: fstat succeeded");
        TEST_CHECK(meta.type == EntryType::DIR,
                   "getattr-dir: type is DIR (S_IFDIR)");
        return stat_res.has_value() && meta.type == EntryType::DIR;
    }

    /**
     * @brief test_setattr_uid — fchownat with uid=1000, gid unchanged.
     *        Verifies the syscall succeeds and the file remains intact.
     */
    [[nodiscard]]
    bool test_setattr_uid() {
        auto res = sys_vfs_fchownat(g_ext4_cap, 1000, NO_CHANGE,
                                    0, PERM_TEST_FILE).to_result();
        TEST_CHECK(res.has_value(),
                   "setattr-uid: fchownat(uid=1000) succeeded");

        // Reopen to verify file is still accessible
        int fd = kmod_fopen("/test_img/.tst_perm", "r");
        CapIdx file_cap = fd >= 0 ? kmod_getcap(fd) : cap::null;
        NodeMeta meta{};
        bool intact = false;
        if (fd >= 0) {
            auto st = sys_vfs_fstat(file_cap, &meta).to_result();
            intact = st.has_value() && meta.type == EntryType::FILE;
            kmod_fclose(fd);
        }
        TEST_CHECK(intact,
                   "setattr-uid: file still accessible after chown");
        return res.has_value() && intact;
    }

    /**
     * @brief test_setattr_gid — fchownat with gid=500, uid unchanged.
     */
    [[nodiscard]]
    bool test_setattr_gid() {
        auto res = sys_vfs_fchownat(g_ext4_cap, NO_CHANGE, 500,
                                    0, PERM_TEST_FILE).to_result();
        TEST_CHECK(res.has_value(),
                   "setattr-gid: fchownat(gid=500) succeeded");

        int fd = kmod_fopen("/test_img/.tst_perm", "r");
        CapIdx file_cap = fd >= 0 ? kmod_getcap(fd) : cap::null;
        NodeMeta meta{};
        bool intact = false;
        if (fd >= 0) {
            auto st = sys_vfs_fstat(file_cap, &meta).to_result();
            intact = st.has_value() && meta.type == EntryType::FILE;
            kmod_fclose(fd);
        }
        TEST_CHECK(intact,
                   "setattr-gid: file still accessible after chown");
        return res.has_value() && intact;
    }

    /**
     * @brief test_fchownat_basic — full fchownat changing both uid and gid.
     */
    [[nodiscard]]
    bool test_fchownat_basic() {
        auto res = sys_vfs_fchownat(g_ext4_cap, 2000, 2000,
                                    0, PERM_TEST_FILE).to_result();
        TEST_CHECK(res.has_value(),
                   "fchownat-basic: fchownat(owner=2000, group=2000) succeeded");

        int fd = kmod_fopen("/test_img/.tst_perm", "r");
        CapIdx file_cap = fd >= 0 ? kmod_getcap(fd) : cap::null;
        NodeMeta meta{};
        bool intact = false;
        if (fd >= 0) {
            auto st = sys_vfs_fstat(file_cap, &meta).to_result();
            intact = st.has_value() && meta.type == EntryType::FILE;
            kmod_fclose(fd);
        }
        TEST_CHECK(intact,
                   "fchownat-basic: file still accessible after chown");
        return res.has_value() && intact;
    }

    /**
     * @brief test_fchownat_nofollow — fchownat on a symlink with
     *        AT_SYMLINK_NOFOLLOW changes the link itself, not the target.
     */
    [[nodiscard]]
    bool test_fchownat_nofollow() {
        // Create symlink pointing to the test file
        auto sym_res =
            sys_vfs_symlink(g_ext4_cap, PERM_SYMLINK,
                            SYMLINK_TARGET).to_result();
        TEST_CHECK(sym_res.has_value(),
                   "fchownat-nofollow: symlink created");
        if (!sym_res.has_value()) {
            return false;
        }

        // fchownat with AT_SYMLINK_NOFOLLOW — should affect the link itself
        auto chown_res =
            sys_vfs_fchownat(g_ext4_cap, 3000, 3000,
                             AT_SYMLINK_NOFOLLOW, PERM_SYMLINK).to_result();
        TEST_CHECK(chown_res.has_value(),
                   "fchownat-nofollow: fchownat(AT_SYMLINK_NOFOLLOW) succeeded");

        // Verify the symlink still exists and is still a symlink
        NodeMeta st{};
        auto lstat_res =
            sys_vfs_lstat(g_ext4_cap, PERM_SYMLINK, &st).to_result();
        bool link_ok = lstat_res.has_value() &&
                       st.type == EntryType::SYMLINK;
        TEST_CHECK(link_ok,
                   "fchownat-nofollow: lstat shows symlink intact");

        return sym_res.has_value() && chown_res.has_value() && link_ok;
    }

    /**
     * @brief test_fchownat_enoent — fchownat on a nonexistent path
     *        must return an error.
     */
    [[nodiscard]]
    bool test_fchownat_enoent() {
        auto res = sys_vfs_fchownat(g_ext4_cap, 0, 0,
                                    0, NONEXISTENT).to_result();
        TEST_CHECK(!res.has_value(),
                   "fchownat-enoent: fchownat(nonexistent) returns error");
        return !res.has_value();
    }

    void cleanup_test_entries() {
        (void)sys_vfs_unlink(g_ext4_cap, PERM_TEST_FILE).to_result();
        (void)sys_vfs_unlink(g_ext4_cap, PERM_SYMLINK).to_result();
    }

}  // namespace

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
              const bsheader *bsargv[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)bsargv;
    printf("test_ext4_permission: start pid=%u\n",
           sys_getpid(__pcb_cap).value());

    // ── Setup ─────────────────────────────────────────────────────
    CapIdx root_cap = bootstrap_root_dir();
    if (root_cap == cap::null || root_cap == cap::error) {
        printf("test_ext4_permission: bootstrap root dir missing\n");
        exit(-1);
    }

    g_ext4_cap = open_ext4_root(root_cap);
    if (g_ext4_cap == cap::null || g_ext4_cap == cap::error) {
        printf("test_ext4_permission: open %s failed\n", EXT4_PATH);
        exit(-1);
    }

    // Clean any leftovers from previous runs, then create a fresh test file
    cleanup_test_entries();
    if (!ensure_test_file()) {
        printf("test_ext4_permission: cannot create test file\n");
        (void)sys_cap_remove(g_ext4_cap).to_result();
        exit(-1);
    }

    // ── Run Tests ─────────────────────────────────────────────────
    printf("test_ext4_permission: running %u test cases\n", 7);

    bool ok = true;
    ok &= test_getattr_file();
    ok &= test_getattr_dir();
    ok &= test_setattr_uid();
    ok &= test_setattr_gid();
    ok &= test_fchownat_basic();
    ok &= test_fchownat_nofollow();
    ok &= test_fchownat_enoent();

    // ── Cleanup ───────────────────────────────────────────────────
    cleanup_test_entries();
    (void)sys_cap_remove(g_ext4_cap).to_result();

    // ── Report ────────────────────────────────────────────────────
    printf("test_ext4_permission: %d/%d tests passed\n",
           pass_count, test_count);

    if (!ok) {
        printf("test_ext4_permission: FAILED (%d failures)\n",
               test_count - pass_count);
        exit(-1);
    }

    printf("test_ext4_permission: PASS\n");
    exit(0);
    return 0;
}
