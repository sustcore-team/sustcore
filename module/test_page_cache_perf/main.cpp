/**
 * @file main.cpp
 * @brief VFS page cache performance-pattern test
 */

#include <kmod/syscall.h>

#include <cstdio>
#include <cstring>

namespace {
    constexpr const char *HELLO_FILE = "/test_img/hello.py";
    constexpr const char *HOT_FILE   = "/test_img/page_cache_hot_file";
    constexpr const char *EVICT_FILE = "/test_img/page_cache_perf_evict";
    constexpr size_t PAGE_SIZE       = 4096;
    constexpr size_t REPEAT_READS    = 100;
    constexpr size_t HOT_PAGES       = 3;
    constexpr size_t COLD_PAGES      = 8;
    constexpr size_t HOT_ITERATIONS  = 64;
    constexpr size_t HOT_FILE_PAGES  = HOT_PAGES + COLD_PAGES;
    constexpr size_t EVICT_PAGES     = 8;
    constexpr char HELLO_CONTENT[]   = "hello";

    char g_read[PAGE_SIZE];
    char g_hot_data[PAGE_SIZE * HOT_FILE_PAGES];
    char g_evict_data[PAGE_SIZE * EVICT_PAGES];

    void fail(const char *msg) {
        printf("test_page_cache_perf: FAIL %s\n", msg);
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

    void print_stats(const char *stage, const VFSPageCacheStats &value) {
        printf("test_page_cache_perf: %s hits=%u misses=%u backing_reads=%u backing_writes=%u evictions=%u writebacks=%u cached=%u/%u\n",
               stage, static_cast<unsigned>(value.hits),
               static_cast<unsigned>(value.misses),
               static_cast<unsigned>(value.backing_reads),
               static_cast<unsigned>(value.backing_writes),
               static_cast<unsigned>(value.evictions),
               static_cast<unsigned>(value.writebacks),
               static_cast<unsigned>(value.cached_pages),
               static_cast<unsigned>(value.max_pages));
    }

    void fill_data() {
        for (size_t i = 0; i < sizeof(g_hot_data); ++i) {
            size_t page = i / PAGE_SIZE;
            g_hot_data[i] = static_cast<char>('A' + ((page * 5 + i) % 26));
        }
        for (size_t i = 0; i < sizeof(g_evict_data); ++i) {
            size_t page = i / PAGE_SIZE;
            g_evict_data[i] = static_cast<char>('a' + ((page * 3 + i) % 26));
        }
    }

    int create_file(const char *path, const void *data, size_t len) {
        kmod_unlink(path);
        int fd = kmod_mkfile(path, "w+");
        check(fd >= 0, "create file failed");

        CapIdx file_cap = kmod_getcap(fd);
        check(file_cap != cap::null && file_cap != cap::error,
              "file capability missing");
        size_t wrote = sys_vfs_write(file_cap, 0, data, len);
        check(wrote == len, "write file failed");
        check(sys_vfs_sync(file_cap), "sync file failed");
        return fd;
    }

    void scan_pages(CapIdx file_cap, size_t pages) {
        for (size_t page = 0; page < pages; ++page) {
            memset(g_read, 0, sizeof(g_read));
            size_t got = sys_vfs_read(file_cap, page * PAGE_SIZE, g_read,
                                      PAGE_SIZE);
            check(got == PAGE_SIZE, "scan page read failed");
        }
    }

    void run_repeated_single_page_test(CapIdx hello_cap, CapIdx evict_cap) {
        scan_pages(evict_cap, EVICT_PAGES);
        (void)stats(true);

        uint64_t start_ns = sys_time_now_ns();
        for (size_t i = 0; i < REPEAT_READS; ++i) {
            memset(g_read, 0, sizeof(g_read));
            size_t got = sys_vfs_read(hello_cap, 0, g_read,
                                      sizeof(HELLO_CONTENT) - 1);
            check(got == sizeof(HELLO_CONTENT) - 1,
                  "hello repeated read length mismatch");
            check(memcmp(g_read, HELLO_CONTENT, sizeof(HELLO_CONTENT) - 1) == 0,
                  "hello repeated read data mismatch");
        }
        uint64_t elapsed_ns = sys_time_now_ns() - start_ns;

        VFSPageCacheStats after = stats();
        print_stats("repeated hello reads", after);
        printf("test_page_cache_perf: repeated hello elapsed_ns=%lu\n",
               static_cast<unsigned long>(elapsed_ns));
        check(after.misses == 1, "repeated read should miss once");
        check(after.hits == REPEAT_READS - 1,
              "repeated read should hit after first miss");
        check(after.backing_reads == 1,
              "repeated read should issue one backing read");
    }

    void warm_hot_pages(CapIdx hot_cap) {
        for (size_t page = 0; page < HOT_PAGES; ++page) {
            for (size_t repeat = 0; repeat < 2; ++repeat) {
                memset(g_read, 0, sizeof(g_read));
                size_t got = sys_vfs_read(hot_cap, page * PAGE_SIZE, g_read,
                                          PAGE_SIZE);
                check(got == PAGE_SIZE, "warm hot page read failed");
                check(memcmp(g_read, g_hot_data + page * PAGE_SIZE,
                             PAGE_SIZE) == 0,
                      "warm hot page data mismatch");
            }
        }
    }

    void run_hotspot_test(CapIdx hot_cap) {
        warm_hot_pages(hot_cap);
        (void)stats(true);

        uint64_t start_ns = sys_time_now_ns();
        for (size_t iter = 0; iter < HOT_ITERATIONS; ++iter) {
            for (size_t page = 0; page < HOT_PAGES; ++page) {
                memset(g_read, 0, sizeof(g_read));
                size_t got = sys_vfs_read(hot_cap, page * PAGE_SIZE, g_read,
                                          PAGE_SIZE);
                check(got == PAGE_SIZE, "hot page read failed");
                check(memcmp(g_read, g_hot_data + page * PAGE_SIZE,
                             PAGE_SIZE) == 0,
                      "hot page data mismatch");
            }

            size_t cold_page = HOT_PAGES + (iter % COLD_PAGES);
            memset(g_read, 0, sizeof(g_read));
            size_t got = sys_vfs_read(hot_cap, cold_page * PAGE_SIZE, g_read,
                                      PAGE_SIZE);
            check(got == PAGE_SIZE, "cold page read failed");
            check(memcmp(g_read, g_hot_data + cold_page * PAGE_SIZE,
                         PAGE_SIZE) == 0,
                  "cold page data mismatch");
        }
        uint64_t elapsed_ns = sys_time_now_ns() - start_ns;

        VFSPageCacheStats after = stats();
        print_stats("hotspot access", after);
        printf("test_page_cache_perf: hotspot elapsed_ns=%lu\n",
               static_cast<unsigned long>(elapsed_ns));
        check(after.hits >= HOT_ITERATIONS * HOT_PAGES,
              "hotspot should keep hot pages cached");
        check(after.misses <= HOT_ITERATIONS + 1,
              "hotspot should miss mostly on cold pages");
        check(after.backing_reads <= HOT_ITERATIONS + 1,
              "hotspot should limit backing reads to cold pages");
    }
}  // namespace

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
                         const bsheader *bsargv[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)bsargv;

    printf("test_page_cache_perf: start pid=%u\n", sys_getpid(__pcb_cap));
    fill_data();

    int hello_fd = create_file(HELLO_FILE, HELLO_CONTENT,
                               sizeof(HELLO_CONTENT) - 1);
    int hot_fd = create_file(HOT_FILE, g_hot_data, sizeof(g_hot_data));
    int evict_fd = create_file(EVICT_FILE, g_evict_data, sizeof(g_evict_data));

    CapIdx hello_cap = kmod_getcap(hello_fd);
    CapIdx hot_cap = kmod_getcap(hot_fd);
    CapIdx evict_cap = kmod_getcap(evict_fd);
    check(hello_cap != cap::null && hello_cap != cap::error,
          "hello cap missing");
    check(hot_cap != cap::null && hot_cap != cap::error,
          "hot cap missing");
    check(evict_cap != cap::null && evict_cap != cap::error,
          "evict cap missing");

    run_repeated_single_page_test(hello_cap, evict_cap);
    run_hotspot_test(hot_cap);

    kmod_fclose(hello_fd);
    kmod_fclose(hot_fd);
    kmod_fclose(evict_fd);
    kmod_unlink(HELLO_FILE);
    kmod_unlink(HOT_FILE);
    kmod_unlink(EVICT_FILE);

    printf("test_page_cache_perf: PASS\n");
    exit(0);
    return 0;
}
