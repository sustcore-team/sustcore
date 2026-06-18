/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 主文件
 * @version alpha-1.0.0
 * @date 2026-04-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kmod/bootstrap.h>
#include <kmod/syscall.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

int mark_cnt = 0;

namespace {
    constexpr size_t kGetdentsBufferSize = 2048;
    constexpr size_t kMaxPrintDepth      = 4;

    [[nodiscard]]
    void *current_sp() {
        void *sp = nullptr;
#if defined(__ARCH_riscv64__)
        asm volatile("mv %0, sp" : "=r"(sp));
#elif defined(__ARCH_loongarch64__)
        asm volatile("move %0, $sp" : "=r"(sp));
#endif
        return sp;
    }

    // 在 bootstrap 信息中寻找根目录能力
    [[nodiscard]]
    CapIdx bootstrap_root_dir() {
        CapIdx cap = cap::null;
        bool found = false;
        bool ok    = bootstrap_foreach_record(
            __startup_data, __startup_size,
            [&](const BootstrapRecordView &view) {
                if (found || view.header->type != BOOTSTRAP_TYPE_DIRCAPEXPLAIN)
                {
                    return;
                }
                BootstrapCapPathView cap_path{};
                if (!bootstrap_parse_cap_path(view, cap_path)) {
                    return;
                }
                if (strcmp(cap_path.path, "/") != 0) {
                    return;
                }
                cap   = cap_path.cap;
                found = true;
            });
        return ok && found ? cap : cap::null;
    }

    // 创建新进程并传递根目录能力
    [[nodiscard]]
    CapIdx spawn_with_root_dir(int fd, size_t sched_class,
                               CapIdx root_dir_cap) {
        if (fd < 0 || root_dir_cap == cap::null || root_dir_cap == cap::error) {
            return cap::error;
        }

        CapIdx child_root_cap = sys_cap_clone(root_dir_cap);
        if (child_root_cap == cap::null || child_root_cap == cap::error) {
            return cap::error;
        }

        struct RootDirBootstrap {
            BootstrapRecordHeader header;
            CapIdx cap;
            char path[2];
        } bootstrap{
            .header =
                BootstrapRecordHeader{
                    .next = 0,
                    .type = BOOTSTRAP_TYPE_DIRCAPEXPLAIN,
                },
            .cap  = child_root_cap,
            .path = "/",
        };

        CapIdx initial_caps[] = {child_root_cap};
        CapIdx child_pcb =
            sys_create_process(kmod_getcap(fd), initial_caps, 1, sched_class,
                               &bootstrap, sizeof(bootstrap));
        sys_cap_remove(child_root_cap);
        return child_pcb;
    }

    void print_indent(size_t depth) {
        for (size_t i = 0; i < depth; ++i) {
            printf("  ");
        }
    }

    [[nodiscard]]
    const char *find_name_end(const char *name, size_t len) {
        return static_cast<const char *>(memchr(name, '\0', len));
    }

    void print_tree(CapIdx dir_cap, const char *path, size_t depth = 0) {
        if (dir_cap == cap::null || dir_cap == cap::error || path == nullptr) {
            return;
        }

        char buffer[kGetdentsBufferSize];
        size_t doff = 0;
        while (doff != DIR_ENTRY_END) {
            size_t bytes_written =
                sys_vfs_getdents(dir_cap, buffer, sizeof(buffer), doff);

            if (bytes_written == 0) {
                break;
            }

            size_t parsed_entries = 0;
            bool reached_end      = false;
            bool batch_valid      = true;

            for (size_t offset = 0; offset < bytes_written;) {
                if (bytes_written - offset < sizeof(dir_entry_header)) {
                    batch_valid = false;
                    break;
                }

                auto *header =
                    reinterpret_cast<dir_entry_header *>(&buffer[offset]);
                size_t name_offset = offset + sizeof(dir_entry_header);
                size_t name_len    = bytes_written - name_offset;
                const char *name   = &buffer[name_offset];
                if (find_name_end(name, name_len) == nullptr || name[0] == '\0')
                {
                    batch_valid = false;
                    break;
                }

                char child_path[512]{};
                if (strcmp(path, "/") == 0) {
                    snprintf(child_path, sizeof(child_path), "/%s", name);
                } else {
                    snprintf(child_path, sizeof(child_path), "%s/%s", path,
                             name);
                }

                print_indent(depth);
                printf("%s %s\n", header->is_file ? "FILE" : "DIR ",
                       child_path);

                if (!header->is_file && depth + 1 < kMaxPrintDepth) {
                    CapIdx subdir =
                        sys_vfs_opendir(dir_cap, name, flags::O_READ);
                    if (subdir != cap::null && subdir != cap::error) {
                        print_tree(subdir, child_path, depth + 1);
                        sys_cap_remove(subdir);
                    }
                }
                ++parsed_entries;

                if (header->next_offset == DIR_ENTRY_END) {
                    reached_end = true;
                    break;
                }
                if (header->next_offset == 0 ||
                    header->next_offset < sizeof(dir_entry_header) ||
                    offset + header->next_offset > bytes_written)
                {
                    break;
                }
                offset += header->next_offset;
            }

            if (parsed_entries == 0 || !batch_valid) {
                break;
            }
            if (reached_end) {
                doff = DIR_ENTRY_END;
                break;
            }
            doff += parsed_entries;
        }
    }
}  // namespace

int kmod_main() {
    mark_cnt = 0;
    printf("进入 init 模块!\n");

    CapIdx root_dir_cap = bootstrap_root_dir();
    if (root_dir_cap == cap::null || root_dir_cap == cap::error) {
        printf("init: bootstrap root dir capability missing\n");
        exit(-1);
    }

    int fd = 0;
    fd = kmod_fopen("/initrd/test_fork.mod", "x");
    if (fd >= 0) {
        if (spawn_with_root_dir(fd, SCHED_CLASS_RR, root_dir_cap) == cap::error)
        {
            printf("init: create test_fork failed\n");
        }
        kmod_fclose(fd);
    }

    fd = kmod_fopen("/initrd/test_thread.mod", "x");
    if (fd >= 0) {
        if (spawn_with_root_dir(fd, SCHED_CLASS_RR, root_dir_cap) == cap::error)
        {
            printf("init: create test_thread failed\n");
        }
        kmod_fclose(fd);
    }

    fd = kmod_fopen("/initrd/test_endpoint_master.mod", "x");
    if (fd >= 0) {
        if (spawn_with_root_dir(fd, SCHED_CLASS_RR, root_dir_cap) == cap::error)
        {
            printf("init: create test_endpoint_master failed\n");
        }
        kmod_fclose(fd);
    }

    fd = kmod_fopen("/initrd/test_call_service.mod", "x");
    if (fd >= 0) {
        if (spawn_with_root_dir(fd, SCHED_CLASS_RR, root_dir_cap) == cap::error)
        {
            printf("init: create test_call_service failed\n");
        }
        kmod_fclose(fd);
    }

    // fd = kmod_fopen("/initrd/test_rpc_server.mod", "x");
    // if (fd >= 0) {
    //     if (spawn_with_root_dir(fd, SCHED_CLASS_RR, root_dir_cap) == cap::error)
    //     {
    //         printf("init: create test_rpc_server failed\n");
    //     }
    //     kmod_fclose(fd);
    // }

    fd = kmod_fopen("/initrd/test_file_rw_a.mod", "x");
    if (fd >= 0) {
        if (spawn_with_root_dir(fd, SCHED_CLASS_RR, root_dir_cap) == cap::error)
        {
            printf("init: create test_file_rw_a failed\n");
        }
        kmod_fclose(fd);
    }

    // try write file /sys/dev/serial@10000000/serial
    // fd = kmod_fopen("/sys/dev/serial@10000000/serial", "w");
    // if (fd >= 0) {
    //     kmod_fwrite(fd, "Hello, World!\n", 14);
    //     printf(
    //         "init: write \"Hello, World!\" to "
    //         "/sys/dev/serial@10000000/serial\n");
    //     kmod_fclose(fd);
    // } else {
    //     printf("init: can't open `/sys/dev/serial@10000000/serial` !\n");
    // }

    // printf("init: 打印目录树\n");
    // print_tree(root_dir_cap, "/");

    printf("init: 启动完成, 退出\n");
    exit(0);
    return 0;
}
