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
    constexpr size_t __NR_close         = 57;
    constexpr size_t __NR_lseek         = 62;
    constexpr size_t __NR_read          = 63;
    constexpr size_t __NR_write         = 64;

    // open flags
    constexpr int O_RDONLY              = 0;
    constexpr int O_WRONLY              = 1;
    constexpr int O_CREAT               = 0100;

    // lseek whence
    constexpr int SEEK_SET              = 0;
    constexpr int SEEK_CUR              = 1;
    constexpr int SEEK_END              = 2;

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

static long linux_lseek(int fd, long offset, int whence) {
    return linux_syscall(static_cast<size_t>(fd),
                         static_cast<size_t>(offset),
                         static_cast<size_t>(whence),
                         0, 0, 0, __NR_lseek);
}

extern "C" [[noreturn]] void test_linux_main() {
    // Original test: write hello three times
    const char *msg = "Hello, linux subsystem!\n";
    puts(msg);
    puts(msg);
    puts(msg);

    // Test 1: open existing file for reading
    puts("Test 1: open existing file...\n");
    int fd = static_cast<int>(linux_openat(AT_FDCWD, "initrd/linux-subsystem.mod",
                               O_RDONLY, 0));
    if (fd >= 3) {
        test_pass("open existing file");
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

    // Summary
    puts("\n=== TEST SUMMARY ===\n");
    if (g_tests_failed == 0) {
        puts("ALL TESTS PASSED\n");
    } else {
        puts("SOME TESTS FAILED\n");
    }

    while (true) {
    }
}
