/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief linux subsystem testing program
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cstddef>

extern "C" long linux_write(size_t fd, const void *buf, size_t len);
extern "C" long linux_syscall(size_t a0, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5, size_t a6);

namespace {
    constexpr size_t STDOUT_FD          = 1;
    constexpr int AT_FDCWD              = -100;

    // Syscall numbers
    constexpr size_t __NR_openat        = 56;
    constexpr size_t __NR_fchownat      = 54;
    constexpr size_t __NR_close         = 57;
    constexpr size_t __NR_lseek         = 62;
    constexpr size_t __NR_read          = 63;
    constexpr size_t __NR_write         = 64;
    constexpr size_t __NR_pipe2         = 59;
    constexpr size_t __NR_statfs        = 43;
    constexpr size_t __NR_fstatfs       = 44;
    constexpr size_t __NR_clone         = 220;
    constexpr size_t __NR_wait4         = 260;
    constexpr size_t __NR_getcwd        = 17;
    constexpr size_t __NR_execve        = 221;
    constexpr size_t __NR_exit_group    = 94;

    constexpr long TMPFS_MAGIC          = 0x01021994;
    constexpr long PROC_SUPER_MAGIC     = 0x9FA0;
    constexpr long TARFS_SUPER_MAGIC    = 0x74617266;

    // open flags
    constexpr int O_RDONLY              = 0;
    constexpr int O_WRONLY              = 1;
    constexpr int O_CREAT               = 0100;
    constexpr int O_NONBLOCK            = 00004000;
    constexpr int AT_EMPTY_PATH         = 0x1000;
    constexpr int AT_SYMLINK_NOFOLLOW   = 0x100;
    constexpr unsigned NO_CHANGE        = 0xFFFFFFFFU;

    // lseek whence
    constexpr int SEEK_SET              = 0;
    constexpr int SEEK_CUR              = 1;
    constexpr int SEEK_END              = 2;

    struct linux_fsid_t {
        int val[2];
    };

    struct linux_statfs {
        long f_type;
        long f_bsize;
        unsigned long f_blocks;
        unsigned long f_bfree;
        unsigned long f_bavail;
        unsigned long f_files;
        unsigned long f_ffree;
        linux_fsid_t f_fsid;
        long f_namelen;
        long f_frsize;
        long f_flags;
        long f_spare[4];
    };

    void clear_statfs(linux_statfs &st) {
        st.f_type   = 0;
        st.f_bsize  = 0;
        st.f_blocks = 0;
        st.f_bfree  = 0;
        st.f_bavail = 0;
        st.f_files  = 0;
        st.f_ffree  = 0;
        st.f_fsid.val[0] = 0;
        st.f_fsid.val[1] = 0;
        st.f_namelen     = 0;
        st.f_frsize      = 0;
        st.f_flags       = 0;
        for (int i = 0; i < 4; ++i) {
            st.f_spare[i] = 0;
        }
    }

    bool statfs_unsupported_zero(const linux_statfs &st) {
        if (st.f_bfree != 0 || st.f_bavail != 0 || st.f_files != 0 ||
            st.f_ffree != 0 || st.f_fsid.val[0] != 0 ||
            st.f_fsid.val[1] != 0 || st.f_namelen != 0 ||
            st.f_flags != 0)
        {
            return false;
        }
        for (int i = 0; i < 4; ++i) {
            if (st.f_spare[i] != 0) {
                return false;
            }
        }
        return true;
    }

    size_t strlen(const char *s) {
        size_t len = 0;
        while (s[len] != '\0') {
            ++len;
        }
        return len;
    }

    void puts(const char *s) {
        linux_write(STDOUT_FD, s, strlen(s));
    }

    int strcmp(const char *lhs, const char *rhs) {
        if (lhs == nullptr || rhs == nullptr) {
            return lhs == rhs ? 0 : (lhs == nullptr ? -1 : 1);
        }
        size_t i = 0;
        while (lhs[i] != '\0' && rhs[i] != '\0') {
            if (lhs[i] != rhs[i]) {
                return lhs[i] < rhs[i] ? -1 : 1;
            }
            ++i;
        }
        if (lhs[i] == rhs[i]) {
            return 0;
        }
        return lhs[i] < rhs[i] ? -1 : 1;
    }

    // Test counters
    int g_tests_passed = 0;
    int g_tests_failed = 0;

    void test_pass(const char *name) {
        puts("  PASS: ");
        puts(name);
        puts("\n");
        g_tests_passed++;
    }

    void test_fail(const char *name, const char *detail) {
        puts("  FAIL: ");
        puts(name);
        puts(": ");
        puts(detail);
        puts("\n");
        g_tests_failed++;
    }

    [[noreturn]]
    void finish_tests() {
        puts("\n=== TEST SUMMARY ===\n");
        if (g_tests_failed == 0) {
            puts("ALL TESTS PASSED\n");
        } else {
            puts("SOME TESTS FAILED\n");
        }
        linux_syscall(static_cast<size_t>(g_tests_failed == 0 ? 0 : 1),
                      0, 0, 0, 0, 0, __NR_exit_group);
        while (true) {
        }
    }
}  // namespace

static long linux_openat(int dirfd, const char *path, int flags, int mode) {
    return linux_syscall(static_cast<size_t>(dirfd),
                         reinterpret_cast<size_t>(path),
                         static_cast<size_t>(flags),
                         static_cast<size_t>(mode),
                         0, 0, __NR_openat);
}

static long linux_close(int fd) {
    return linux_syscall(static_cast<size_t>(fd), 0, 0, 0, 0, 0, __NR_close);
}

static long linux_read(int fd, void *buf, size_t count) {
    return linux_syscall(static_cast<size_t>(fd),
                         reinterpret_cast<size_t>(buf),
                         count, 0, 0, 0, __NR_read);
}

static long linux_write_sys(int fd, const void *buf, size_t count) {
    return linux_syscall(static_cast<size_t>(fd),
                         reinterpret_cast<size_t>(buf),
                         count, 0, 0, 0, __NR_write);
}

static long linux_pipe2(int pipefd[2], int flags) {
    return linux_syscall(reinterpret_cast<size_t>(pipefd),
                         static_cast<size_t>(flags),
                         0, 0, 0, 0, __NR_pipe2);
}

static long linux_clone(size_t flags) {
    return linux_syscall(flags, 0, 0, 0, 0, 0, __NR_clone);
}

static long linux_wait4(int pid, int *status, int options, void *rusage) {
    return linux_syscall(static_cast<size_t>(pid),
                         reinterpret_cast<size_t>(status),
                         static_cast<size_t>(options),
                         reinterpret_cast<size_t>(rusage),
                         0, 0, __NR_wait4);
}

static long linux_lseek(int fd, long offset, int whence) {
    return linux_syscall(static_cast<size_t>(fd),
                         static_cast<size_t>(offset),
                         static_cast<size_t>(whence),
                         0, 0, 0, __NR_lseek);
}

static long linux_getcwd(char *buf, size_t size) {
    return linux_syscall(reinterpret_cast<size_t>(buf), size, 0, 0, 0, 0,
                         __NR_getcwd);
}

static long linux_execve(const char *path, const char *const argv[],
                         const char *const envp[]) {
    return linux_syscall(reinterpret_cast<size_t>(path),
                         reinterpret_cast<size_t>(argv),
                         reinterpret_cast<size_t>(envp),
                         0, 0, 0, __NR_execve);
}

static long linux_fchownat(int dirfd, const char *path, unsigned uid,
                           unsigned gid, int flags) {
    return linux_syscall(static_cast<size_t>(dirfd),
                         reinterpret_cast<size_t>(path),
                         static_cast<size_t>(uid),
                         static_cast<size_t>(gid),
                         static_cast<size_t>(flags),
                         0, __NR_fchownat);
}

static long linux_statfs_sys(const char *path, linux_statfs *buf) {
    return linux_syscall(reinterpret_cast<size_t>(path),
                         reinterpret_cast<size_t>(buf),
                         0, 0, 0, 0, __NR_statfs);
}

static long linux_fstatfs_sys(int fd, linux_statfs *buf) {
    return linux_syscall(static_cast<size_t>(fd),
                         reinterpret_cast<size_t>(buf),
                         0, 0, 0, 0, __NR_fstatfs);
}

static void test_statfs_basic(int tar_fd) {
    puts("Test 8.75: statfs/fstatfs filesystem types...\n");

    linux_statfs st;
    clear_statfs(st);
    long root_ret = linux_statfs_sys("/", &st);
    if (root_ret == 0 && st.f_type == TMPFS_MAGIC && st.f_blocks != 0 &&
        st.f_bsize == 4096 && st.f_frsize == 4096)
    {
        test_pass("statfs tmpfs root");
    } else {
        test_fail("statfs tmpfs root", "unexpected root fs data");
    }
    if (root_ret == 0 && statfs_unsupported_zero(st)) {
        test_pass("statfs unsupported fields zero");
    } else {
        test_fail("statfs unsupported fields zero", "expected zero fields");
    }

    clear_statfs(st);
    long proc_ret = linux_statfs_sys("/proc", &st);
    if (proc_ret == 0 && st.f_type == PROC_SUPER_MAGIC && st.f_blocks != 0) {
        test_pass("statfs procfs");
    } else {
        test_fail("statfs procfs", "unexpected procfs data");
    }

    clear_statfs(st);
    long initrd_ret = linux_statfs_sys("/initrd", &st);
    if (initrd_ret == 0 && st.f_type == TARFS_SUPER_MAGIC &&
        st.f_blocks == 0)
    {
        test_pass("statfs tarfs");
    } else {
        test_fail("statfs tarfs", "unexpected tarfs data");
    }

    clear_statfs(st);
    long fd_ret = linux_fstatfs_sys(tar_fd, &st);
    if (fd_ret == 0 && st.f_type == TARFS_SUPER_MAGIC) {
        test_pass("fstatfs opened file");
    } else {
        test_fail("fstatfs opened file", "unexpected fstatfs type");
    }

    clear_statfs(st);
    long missing_ret = linux_statfs_sys("/missing-statfs-path", &st);
    if (missing_ret == -2) {
        test_pass("statfs missing path");
    } else {
        test_fail("statfs missing path", "expected -ENOENT");
    }

    long bad_fd_ret = linux_fstatfs_sys(999, &st);
    if (bad_fd_ret == -9) {
        test_pass("fstatfs invalid fd");
    } else {
        test_fail("fstatfs invalid fd", "expected -EBADF");
    }
}

static void test_pipe_basic() {
    puts("Test 9.1: pipe basic read/write...\n");
    int fds[2] = {-1, -1};
    long ret = linux_pipe2(fds, 0);
    if (ret == 0 && fds[0] >= 3 && fds[1] >= 3) {
        test_pass("pipe2 create");
    } else {
        test_fail("pipe2 create", "expected two fds");
        return;
    }

    const char *msg = "pipe-data";
    long wn = linux_write_sys(fds[1], msg, 9);
    char buf[16]{};
    long rn = linux_read(fds[0], buf, 9);
    if (wn == 9 && rn == 9 && strcmp(buf, "pipe-data") == 0) {
        test_pass("pipe read/write");
    } else {
        test_fail("pipe read/write", "unexpected payload");
    }
    linux_close(fds[0]);
    linux_close(fds[1]);
}

static void test_pipe_eof_and_epipe() {
    puts("Test 9.2: pipe EOF and EPIPE...\n");
    int fds[2] = {-1, -1};
    if (linux_pipe2(fds, 0) != 0) {
        test_fail("pipe EOF setup", "pipe2 failed");
        return;
    }

    linux_close(fds[1]);
    char b = 0;
    long rn = linux_read(fds[0], &b, 1);
    if (rn == 0) {
        test_pass("pipe EOF after writer close");
    } else {
        test_fail("pipe EOF after writer close", "expected zero read");
    }
    linux_close(fds[0]);

    if (linux_pipe2(fds, 0) != 0) {
        test_fail("pipe EPIPE setup", "pipe2 failed");
        return;
    }
    linux_close(fds[0]);
    long wn = linux_write_sys(fds[1], "x", 1);
    if (wn < 0) {
        test_pass("pipe EPIPE after reader close");
    } else {
        test_fail("pipe EPIPE after reader close", "expected negative write");
    }
    linux_close(fds[1]);
}

static void test_pipe_nonblock() {
    puts("Test 9.3: pipe nonblocking empty read...\n");
    int fds[2] = {-1, -1};
    if (linux_pipe2(fds, O_NONBLOCK) != 0) {
        test_fail("pipe nonblock setup", "pipe2 failed");
        return;
    }
    char b = 0;
    long rn = linux_read(fds[0], &b, 1);
    if (rn == -11) {
        test_pass("pipe nonblock empty read EAGAIN");
    } else {
        test_fail("pipe nonblock empty read EAGAIN", "expected -EAGAIN");
    }
    linux_close(fds[0]);
    linux_close(fds[1]);
}

static void test_pipe_fork() {
    puts("Test 9.4: pipe fork communication...\n");
    int fds[2] = {-1, -1};
    if (linux_pipe2(fds, 0) != 0) {
        test_fail("pipe fork setup", "pipe2 failed");
        return;
    }

    long pid = linux_clone(17);
    if (pid < 0) {
        test_fail("pipe fork clone", "clone failed");
        linux_close(fds[0]);
        linux_close(fds[1]);
        return;
    }
    if (pid == 0) {
        linux_close(fds[0]);
        (void)linux_write_sys(fds[1], "child", 5);
        linux_close(fds[1]);
        linux_syscall(0, 0, 0, 0, 0, 0, __NR_exit_group);
        while (true) {
        }
    }

    linux_close(fds[1]);
    char buf[8]{};
    long rn = linux_read(fds[0], buf, 5);
    int status = 0;
    long waited = linux_wait4(static_cast<int>(pid), &status, 0, nullptr);
    if (rn == 5 && strcmp(buf, "child") == 0 && waited == pid) {
        test_pass("pipe fork communication");
    } else {
        test_fail("pipe fork communication", "unexpected child message");
    }
    linux_close(fds[0]);
}

static void test_fchownat_basic() {
    puts("Test 9.5: fchownat basic paths...\n");

    int fd = static_cast<int>(linux_openat(AT_FDCWD, "tmp/linux-fchownat.txt",
                                           O_WRONLY | O_CREAT, 0));
    if (fd < 0) {
        test_fail("fchownat setup create", "openat failed");
        return;
    }
    linux_close(fd);

    long chown_ret = linux_fchownat(AT_FDCWD, "tmp/linux-fchownat.txt", 1000,
                                    NO_CHANGE, 0);
    if (chown_ret == 0) {
        test_pass("fchownat relative path");
    } else {
        test_fail("fchownat relative path", "expected success");
    }

    int empty_fd = static_cast<int>(linux_openat(AT_FDCWD, "tmp", O_RDONLY, 0));
    if (empty_fd < 0) {
        test_fail("fchownat empty-path setup", "openat tmp failed");
        return;
    }

    long empty_ret =
        linux_fchownat(empty_fd, "", NO_CHANGE, 1000, AT_EMPTY_PATH);
    if (empty_ret == 0) {
        test_pass("fchownat AT_EMPTY_PATH on directory");
    } else {
        test_fail("fchownat AT_EMPTY_PATH on directory", "expected success");
    }
    linux_close(empty_fd);

    long bad_flag_ret = linux_fchownat(AT_FDCWD, "tmp/linux-fchownat.txt", 0, 0,
                                       AT_SYMLINK_NOFOLLOW << 8);
    if (bad_flag_ret == -22) {
        test_pass("fchownat invalid flags");
    } else {
        test_fail("fchownat invalid flags", "expected -EINVAL");
    }
}

extern "C" [[noreturn]] void test_linux_main(size_t argc, const char *argv[],
                                             const char *envp[]) {
    if (argc >= 3 && argv != nullptr && strcmp(argv[1], "after-exec") == 0) {
        puts("Test 10: execve resumed image...\n");
        if (strcmp(argv[0], "/initrd/test-linux.mod") == 0 &&
            strcmp(argv[2], "payload") == 0 && envp != nullptr &&
            strcmp(envp[0], "EXECVE_TEST=1") == 0)
        {
            test_pass("execve argv/envp");
        } else {
            test_fail("execve argv/envp", "unexpected startup arguments");
        }
        finish_tests();
    }

    // Original test: write hello three times
    const char *msg = "Hello, linux subsystem!\n";
    puts(msg);
    puts(msg);
    puts(msg);

    // Test 1: open existing file for reading
    puts("Test 1: open existing file...\n");
    char cwd[128];
    long cwd_ret = linux_getcwd(cwd, sizeof(cwd));
    if (cwd_ret == reinterpret_cast<long>(cwd)) {
        test_pass("getcwd");
    } else {
        test_fail("getcwd", "expected buffer pointer");
    }

    char small_cwd[1];
    long small_cwd_ret = linux_getcwd(small_cwd, sizeof(small_cwd));
    if (small_cwd_ret < 0) {
        test_pass("getcwd small buffer");
    } else {
        test_fail("getcwd small buffer", "expected negative");
    }

    int fd = static_cast<int>(linux_openat(AT_FDCWD, "/initrd/linux-subsystem.mod",
                               O_RDONLY, 0));
    if (fd >= 3) {
        test_pass("open existing file");
        test_statfs_basic(fd);
    } else {
        test_fail("open existing file", "expected fd >= 3");
    }

    // Test 2: read from opened file
    puts("Test 2: read from file...\n");
    char buf[32];
    long n = linux_read(fd, buf, 16);
    if (n > 0) {
        test_pass("read from file");
    } else {
        test_fail("read from file", "expected positive count");
    }

    // Test 3: lseek SEEK_SET to beginning
    puts("Test 3: lseek SEEK_SET...\n");
    long pos = linux_lseek(fd, 0, SEEK_SET);
    if (pos == 0) {
        test_pass("lseek SEEK_SET");
    } else {
        test_fail("lseek SEEK_SET", "expected 0");
    }

    // Test 4: lseek SEEK_CUR after seeking to 0
    puts("Test 4: lseek SEEK_CUR...\n");
    pos = linux_lseek(fd, 0, SEEK_CUR);
    if (pos == 0) {
        test_pass("lseek SEEK_CUR");
    } else {
        test_fail("lseek SEEK_CUR", "expected 0");
    }

    // Test 5: lseek SEEK_END
    puts("Test 5: lseek SEEK_END...\n");
    pos = linux_lseek(fd, 0, SEEK_END);
    if (pos > 0) {
        test_pass("lseek SEEK_END");
    } else {
        test_fail("lseek SEEK_END", "expected positive offset");
    }

    // Test 6: close valid fd
    puts("Test 6: close file...\n");
    long ret = linux_close(fd);
    if (ret == 0) {
        test_pass("close file");
    } else {
        test_fail("close file", "expected 0");
    }

    // Test 7: open nonexistent file returns error
    puts("Test 7: open nonexistent file...\n");
    long bad_fd = linux_openat(AT_FDCWD, "nonexistent_file_xyz", O_RDONLY, 0);
    if (bad_fd < 0) {
        test_pass("open nonexistent");
    } else {
        test_fail("open nonexistent", "expected negative");
    }

    // Test 8: close invalid fd returns error
    puts("Test 8: close invalid fd...\n");
    ret = linux_close(999);
    if (ret < 0) {
        test_pass("close invalid fd");
    } else {
        test_fail("close invalid fd", "expected negative");
    }

    puts("Test 8.5: openat unsupported dirfd...\n");
    long bad_dirfd = linux_openat(3, "linux-subsystem.mod", O_RDONLY, 0);
    if (bad_dirfd < 0) {
        test_pass("openat unsupported dirfd");
    } else {
        test_fail("openat unsupported dirfd", "expected negative");
    }

    // Test 9: create file, write, close, reopen, read, verify content
    puts("Test 9: create + write + readback...\n");
    int fd2 = static_cast<int>(linux_openat(AT_FDCWD, "tmp/testfile.txt",
                               O_WRONLY | O_CREAT, 0));
    if (fd2 >= 3) {
        test_pass("create file");
        const char *test_data = "Hello, file!";
        long wn = linux_write(fd2, test_data, 12);
        if (wn == 12) {
            test_pass("write to file");
        } else {
            test_fail("write to file", "expected 12 bytes");
        }
        linux_close(fd2);

        // Read back
        int fd3 = static_cast<int>(linux_openat(AT_FDCWD, "tmp/testfile.txt",
                                   O_RDONLY, 0));
        if (fd3 >= 3) {
            char readbuf[32];
            long rn = linux_read(fd3, readbuf, 12);
            if (rn == 12) {
                bool match = true;
                for (int i = 0; i < 12; i++) {
                    if (readbuf[i] != test_data[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    test_pass("read back file content");
                } else {
                    test_fail("read back file content", "data mismatch");
                }
            } else {
                test_fail("read back file", "unexpected count");
            }
            linux_close(fd3);
        } else {
            test_fail("open for readback", "expected fd >= 3");
        }
    } else {
        test_fail("create file", "expected fd >= 3");
    }

    test_pipe_basic();
    test_pipe_eof_and_epipe();
    test_pipe_nonblock();
    test_pipe_fork();
    test_fchownat_basic();

    puts("Test 10: execve self...\n");
    const char *exec_argv[] = {"/initrd/test-linux.mod", "after-exec",
                               "payload", nullptr};
    const char *exec_envp[] = {"EXECVE_TEST=1", nullptr};
    long exec_ret = linux_execve("/initrd/test-linux.mod", exec_argv,
                                 exec_envp);
    if (exec_ret < 0) {
        test_fail("execve self", "execve returned error");
    } else {
        test_fail("execve self", "execve unexpectedly returned success");
    }
    finish_tests();
}
