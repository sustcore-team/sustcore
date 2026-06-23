/**
 * @file main.cpp
 * @brief VFS page cache mechanism test
 */

#include <kmod/syscall.h>

#include <cstdio>
#include <cstring>

namespace {
    constexpr const char *TEST_FILE = "/testing/page_cache_test_file";
    constexpr size_t TEST_SIZE      = 128;
    char g_data[TEST_SIZE];
    char g_read[64];

    void fail(const char *msg) {
        printf("test_page_cache: FAIL %s\n", msg);
        exit(-1);
    }

    void check(bool condition, const char *msg) {
        if (!condition) {
            fail(msg);
        }
    }

    VFSPageCacheStats stats(bool reset = false) {
        VFSPageCacheStats out{};
        check(sys_vfs_page_cache_stats(0, &out, reset),
              "page cache stats syscall failed");
        return out;
    }

    void fill_data() {
        for (size_t i = 0; i < TEST_SIZE; ++i) {
            g_data[i] = static_cast<char>('A' + (i % 26));
        }
    }
}  // namespace

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
                         const bsheader *bsargv[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)bsargv;

    printf("test_page_cache: start pid=%u\n", sys_getpid(__pcb_cap));
    fill_data();

    kmod_unlink(TEST_FILE);
    int fd = kmod_mkfile(TEST_FILE, "w+");
    check(fd >= 0, "create test file failed");

    CapIdx file_cap = kmod_getcap(fd);
    check(file_cap != cap::null && file_cap != cap::error,
          "file capability missing");

    (void)stats(true);

    size_t wrote = sys_vfs_write(file_cap, 0, g_data, sizeof(g_data));
    check(wrote == sizeof(g_data), "write-back write failed");

    VFSPageCacheStats after_write = stats();
    printf("test_page_cache: after write hits=%u misses=%u invalidations=%u writebacks=%u evictions=%u cached=%u/%u\n",
           static_cast<unsigned>(after_write.hits),
           static_cast<unsigned>(after_write.misses),
           static_cast<unsigned>(after_write.invalidations),
           static_cast<unsigned>(after_write.writebacks),
           static_cast<unsigned>(after_write.evictions),
           static_cast<unsigned>(after_write.cached_pages),
           static_cast<unsigned>(after_write.max_pages));
    check(after_write.misses == 1, "write should allocate one cached page");
    check(after_write.writebacks == 0, "write should not write back immediately");
    check(after_write.invalidations == 0, "write should not invalidate cache");

    memset(g_read, 0, sizeof(g_read));
    size_t got = sys_vfs_read(file_cap, 0, g_read, 32);
    check(got == 32 && memcmp(g_read, g_data, 32) == 0,
          "read after dirty write data mismatch");

    VFSPageCacheStats after_first = stats();
    printf("test_page_cache: after first read hits=%u misses=%u invalidations=%u writebacks=%u evictions=%u cached=%u/%u\n",
           static_cast<unsigned>(after_first.hits),
           static_cast<unsigned>(after_first.misses),
           static_cast<unsigned>(after_first.invalidations),
           static_cast<unsigned>(after_first.writebacks),
           static_cast<unsigned>(after_first.evictions),
           static_cast<unsigned>(after_first.cached_pages),
           static_cast<unsigned>(after_first.max_pages));
    check(after_first.hits == after_write.hits + 1,
          "read after dirty write should hit cached page");
    check(after_first.misses == after_write.misses,
          "read after dirty write should not miss");

    memset(g_read, 0, sizeof(g_read));
    got = sys_vfs_read(file_cap, 16, g_read, 32);
    check(got == 32 && memcmp(g_read, g_data + 16, 32) == 0,
          "second read data mismatch");

    VFSPageCacheStats after_second = stats();
    printf("test_page_cache: after second read hits=%u misses=%u invalidations=%u writebacks=%u evictions=%u cached=%u/%u\n",
           static_cast<unsigned>(after_second.hits),
           static_cast<unsigned>(after_second.misses),
           static_cast<unsigned>(after_second.invalidations),
           static_cast<unsigned>(after_second.writebacks),
           static_cast<unsigned>(after_second.evictions),
           static_cast<unsigned>(after_second.cached_pages),
           static_cast<unsigned>(after_second.max_pages));
    check(after_second.hits == after_first.hits + 1,
          "second read should hit cached page");
    check(after_second.misses == after_first.misses,
          "second read should not add miss");

    check(sys_vfs_sync(file_cap), "sync should flush dirty page");

    VFSPageCacheStats after_sync = stats();
    printf("test_page_cache: after sync hits=%u misses=%u invalidations=%u writebacks=%u evictions=%u cached=%u/%u\n",
           static_cast<unsigned>(after_sync.hits),
           static_cast<unsigned>(after_sync.misses),
           static_cast<unsigned>(after_sync.invalidations),
           static_cast<unsigned>(after_sync.writebacks),
           static_cast<unsigned>(after_sync.evictions),
           static_cast<unsigned>(after_sync.cached_pages),
           static_cast<unsigned>(after_sync.max_pages));
    check(after_sync.writebacks == after_second.writebacks + 1,
          "sync should write back dirty page");
    check(after_sync.invalidations == after_second.invalidations,
          "sync should keep clean cached page");

    kmod_fclose(fd);
    check(kmod_unlink(TEST_FILE) == 0, "cleanup unlink failed");
    printf("test_page_cache: PASS\n");
    exit(0);
    return 0;
}
