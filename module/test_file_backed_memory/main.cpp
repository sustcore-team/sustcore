/**
 * @file main.cpp
 * @brief File-backed MemoryPayload lifetime test
 */

#include <kmod/syscall.h>

#include <cstdio>
#include <cstring>

namespace {
    constexpr const char *TEST_FILE = "/test_img/file_backed_memory_test";
    constexpr size_t PAGE_SIZE      = 4096;
    constexpr size_t FILE_SIZE      = PAGE_SIZE * 2;
    constexpr uintptr_t MAP_ADDR    = 0x000700000000ULL;
    constexpr uint64_t MEMORY_GROWTH_FIXED = 0;
    constexpr uint64_t PROT_READ = 0x1;

    char g_data[FILE_SIZE];

    void fail(const char *msg) {
        printf("test_file_backed_memory: FAIL %s\n", msg);
        exit(-1);
    }

    void check(bool condition, const char *msg) {
        if (!condition) {
            fail(msg);
        }
    }

    void fill_data() {
        for (size_t i = 0; i < FILE_SIZE; ++i) {
            size_t page = i / PAGE_SIZE;
            g_data[i] = static_cast<char>('a' + ((page * 7 + i) % 26));
        }
    }
}  // namespace

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
                         const bsheader *bsargv[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)bsargv;

    printf("test_file_backed_memory: start pid=%u\n", sys_getpid(__pcb_cap));
    fill_data();

    kmod_unlink(TEST_FILE);
    int fd = kmod_mkfile(TEST_FILE, "w+");
    check(fd >= 0, "create test file failed");

    CapIdx file_cap = kmod_getcap(fd);
    check(file_cap != cap::null && file_cap != cap::error,
          "file capability missing");

    size_t wrote = sys_vfs_write(file_cap, 0, g_data, sizeof(g_data));
    check(wrote == sizeof(g_data), "write test file failed");
    check(sys_vfs_sync(file_cap), "sync test file failed");

    CapIdx mem_cap = sys_mem_create(file_cap, PAGE_SIZE, false, false,
                                    MEMORY_GROWTH_FIXED, PAGE_SIZE);
    check(mem_cap != cap::null && mem_cap != cap::error,
          "file-backed memory create failed");

    kmod_fclose(fd);

    auto *mapped = reinterpret_cast<const char *>(MAP_ADDR);
    check(sys_pcb_map(__pcb_cap, mem_cap, 0, const_cast<char *>(mapped),
                      PAGE_SIZE, PROT_READ),
          "map file-backed memory failed");

    check(std::memcmp(mapped, g_data + PAGE_SIZE, PAGE_SIZE) == 0,
          "mapped page does not match file page after closing file cap");

    check(sys_mem_unmap(mem_cap, const_cast<char *>(mapped)),
          "unmap file-backed memory failed");
    check(sys_cap_remove(mem_cap), "remove memory cap failed");
    kmod_unlink(TEST_FILE);

    printf("test_file_backed_memory: PASS\n");
    exit(0);
    return 0;
}
