/**
 * @file main.cpp
 * @brief VFS page cache mechanism test
 */

#include <kmod/syscall.h>

#include <cstdio>
#include <cstring>

namespace {
    constexpr const char *TEST_FILE = "/test_img/page_cache_test_file";
    constexpr size_t PAGE_SIZE      = 4096;
    constexpr size_t TEST_PAGES     = 9;
    constexpr size_t TEST_SIZE      = PAGE_SIZE * TEST_PAGES;
    char g_data[TEST_SIZE];
    char g_read[PAGE_SIZE];

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
            size_t page = i / PAGE_SIZE;
            g_data[i] = static_cast<char>('A' + ((page + i) % 26));
        }
    }

    void print_stats(const char *stage, const VFSPageCacheStats &value) {
        printf("test_page_cache: %s hits=%u misses=%u invalidations=%u writebacks=%u evictions=%u backing_reads=%u backing_writes=%u cached=%u/%u\n",
               stage, static_cast<unsigned>(value.hits),
               static_cast<unsigned>(value.misses),
               static_cast<unsigned>(value.invalidations),
               static_cast<unsigned>(value.writebacks),
               static_cast<unsigned>(value.evictions),
               static_cast<unsigned>(value.backing_reads),
               static_cast<unsigned>(value.backing_writes),
               static_cast<unsigned>(value.cached_pages),
               static_cast<unsigned>(value.max_pages));
    }

    void check_cache_bound(const VFSPageCacheStats &value, const char *msg) {
        check(value.max_pages == 4, "page cache max should be four pages");
        check(value.cached_pages <= value.max_pages, msg);
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
    check(wrote == sizeof(g_data), "multi-page write-back write failed");

    VFSPageCacheStats after_write = stats();
    print_stats("after write", after_write);
    check_cache_bound(after_write, "write should respect cache page bound");
    check(after_write.misses == TEST_PAGES,
          "write should allocate each file page through cache");
    check(after_write.evictions >= TEST_PAGES - after_write.max_pages,
          "write should evict pages past the four-page cap");
    check(after_write.writebacks >= 1,
          "dirty eviction should write back at least one page");
    check(after_write.invalidations == 0, "write should not invalidate cache");

    for (size_t page = 0; page < TEST_PAGES; ++page) {
        memset(g_read, 0, sizeof(g_read));
        size_t got = sys_vfs_read(file_cap, page * PAGE_SIZE, g_read,
                                  sizeof(g_read));
        check(got == sizeof(g_read), "read full page after eviction failed");
        check(memcmp(g_read, g_data + page * PAGE_SIZE, sizeof(g_read)) == 0,
              "read after dirty eviction data mismatch");
    }

    VFSPageCacheStats after_reads = stats();
    print_stats("after full reads", after_reads);
    check_cache_bound(after_reads, "reads should respect cache page bound");
    check(after_reads.misses > after_write.misses,
          "reading evicted pages should miss and refill");
    check(after_reads.evictions > after_write.evictions,
          "reading more than four pages should keep evicting");

    size_t hot_page = TEST_PAGES - 1;
    memset(g_read, 0, sizeof(g_read));
    size_t got = sys_vfs_read(file_cap, hot_page * PAGE_SIZE, g_read,
                              sizeof(g_read));
    check(got == sizeof(g_read), "hot page read failed");
    check(memcmp(g_read, g_data + hot_page * PAGE_SIZE, sizeof(g_read)) == 0,
          "hot page data mismatch");

    VFSPageCacheStats after_hot = stats();
    print_stats("after hot reread", after_hot);
    check(after_hot.hits == after_reads.hits + 1,
          "rereading resident hot page should hit");
    check(after_hot.misses == after_reads.misses,
          "rereading resident hot page should not miss");

    memset(g_read, 0, sizeof(g_read));
    got = sys_vfs_read(file_cap, 0, g_read, sizeof(g_read));
    check(got == sizeof(g_read), "cold page refill failed");
    check(memcmp(g_read, g_data, sizeof(g_read)) == 0,
          "cold page refill data mismatch");

    memset(g_read, 0, sizeof(g_read));
    got = sys_vfs_read(file_cap, hot_page * PAGE_SIZE, g_read,
                       sizeof(g_read));
    check(got == sizeof(g_read), "active hot page read failed");
    check(memcmp(g_read, g_data + hot_page * PAGE_SIZE, sizeof(g_read)) == 0,
          "active hot page data mismatch");

    VFSPageCacheStats after_lru = stats();
    print_stats("after lru check", after_lru);
    check(after_lru.misses == after_hot.misses + 1,
          "cold refill should miss once");
    check(after_lru.hits == after_hot.hits + 1,
          "active hot page should survive cold refill");

    check(sys_vfs_sync(file_cap), "sync should flush dirty page");

    VFSPageCacheStats after_sync = stats();
    print_stats("after sync", after_sync);
    check_cache_bound(after_sync, "sync should keep cache within bound");
    check(after_sync.writebacks >= after_lru.writebacks,
          "sync should not lose writeback accounting");
    check(after_sync.invalidations == after_lru.invalidations,
          "sync should keep clean cached page");

    kmod_fclose(fd);
    check(kmod_unlink(TEST_FILE) == 0, "cleanup unlink failed");
    printf("test_page_cache: PASS\n");
    exit(0);
    return 0;
}
