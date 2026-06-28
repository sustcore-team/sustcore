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

#include <kmod/syscall.h>
#include <sustcore/bootstrap.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

int mark_cnt = 0;

constexpr size_t kGetdentsBufferSize = 2048;
constexpr size_t kMaxPrintDepth      = 4;

// 在 bootstrap 信息中寻找根目录能力
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
const char *bootstrap_stdout_device_dir() {
    const char *path = nullptr;
    bool found       = false;
    bool ok          = bootstrap_foreach_record(
        __bsargv, __bsargc, [&](const BootstrapRecordView &view) {
            if (found || view.header->type != boot::TYPE_PATHEXP) {
                return;
            }
            BootstrapPathExplainView path_view{};
            if (!bootstrap_parse_path_explain(view, path_view)) {
                return;
            }
            if (strncmp(path_view.path_desc, "#stdout:", 8) != 0) {
                return;
            }
            path  = path_view.path_desc + 8;
            found = true;
        });
    return ok && found ? path : nullptr;
}

struct SpawnRequest {
    const char *path;
    const char *dispname;
    bool is_linuxproc;
};

[[nodiscard]]
CapIdx spawn_with_root_dir(int fd, size_t sched_class, CapIdx root_dir_cap) {
    if (fd < 0 || root_dir_cap == cap::null || root_dir_cap == cap::error) {
        return cap::error;
    }

    auto child_root_res = sys_cap_clone(root_dir_cap).to_result();
    if (!child_root_res.has_value()) {
        return cap::error;
    }
    CapIdx child_root_cap = child_root_res.value();

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
    ExecveRequest request{
        .image_cap = kmod_getcap(fd),
        .execfn    = nullptr,
        .caps      = initial_caps,
        .argv      = nullptr,
        .envp      = nullptr,
        .bsargv    = bsargv,
    };
    auto child_pcb_res = sys_create_process(sched_class, &request).to_result();
    (void)sys_cap_remove(child_root_cap).to_result();
    return child_pcb_res.has_value() ? child_pcb_res.value() : cap::error;
}

[[nodiscard]]
CapIdx spawn_linux_with_root_dir(int fd, size_t sched_class,
                                 CapIdx root_dir_cap) {
    if (fd < 0 || root_dir_cap == cap::null || root_dir_cap == cap::error) {
        return cap::error;
    }

    auto child_root_res = sys_cap_clone(root_dir_cap).to_result();
    if (!child_root_res.has_value()) {
        return cap::error;
    }
    CapIdx child_root_cap = child_root_res.value();

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
    ExecveRequest request{
        .image_cap = kmod_getcap(fd),
        .execfn    = nullptr,
        .caps      = initial_caps,
        .argv      = nullptr,
        .envp      = nullptr,
        .bsargv    = bsargv,
    };
    auto child_pcb_res =
        sys_create_linux_process(sched_class, &request).to_result();
    (void)sys_cap_remove(child_root_cap).to_result();
    return child_pcb_res.has_value() ? child_pcb_res.value() : cap::error;
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

bool force_symlink(const char *path, const char *target) {
    if (path == nullptr || target == nullptr || path[0] == '\0' ||
        target[0] == '\0')
    {
        return false;
    }
    (void)kmod_unlink(path);
    return kmod_symlink(path, target) == 0;
}

std::vector<std::string> get_alldents(CapIdx dir_cap) {
    std::vector<std::string> entries{};
    if (dir_cap == cap::null || dir_cap == cap::error) {
        return entries;
    }

    char buffer[kGetdentsBufferSize];
    size_t doff = 0;
    while (doff != DIR_ENTRY_END) {
        auto getdents_res =
            sys_vfs_getdents(dir_cap, buffer, sizeof(buffer), doff).to_result();
        size_t bytes_written =
            getdents_res.has_value() ? getdents_res.value() : 0;
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
            auto *name_end     = find_name_end(name, name_len);
            if (name_end == nullptr || name[0] == '\0') {
                batch_valid = false;
                break;
            }

            entries.emplace_back(name);
            ++parsed_entries;

            if (header->next_offset == DIR_ENTRY_END) {
                reached_end = true;
                break;
            }
            if (header->next_offset == 0 ||
                header->next_offset < sizeof(dir_entry_header) ||
                offset + header->next_offset > bytes_written)
            {
                batch_valid = false;
                break;
            }
            offset += header->next_offset;
        }

        if (parsed_entries == 0 || !batch_valid) {
            break;
        }
        if (reached_end) {
            break;
        }
        doff += parsed_entries;
    }
    return entries;
}

void print_tree(CapIdx dir_cap, const char *path, size_t depth = 0) {
    if (dir_cap == cap::null || dir_cap == cap::error || path == nullptr) {
        return;
    }

    auto names = get_alldents(dir_cap);
    for (const auto &name : names) {
        char child_path[512]{};
        if (strcmp(path, "/") == 0) {
            snprintf(child_path, sizeof(child_path), "/%s", name.c_str());
        } else {
            snprintf(child_path, sizeof(child_path), "%s/%s", path,
                     name.c_str());
        }

        NodeMeta st{};
        bool stat_ok      = sys_vfs_lstat(dir_cap, name.c_str(), &st);
        const bool is_dir = stat_ok && st.type == EntryType::DIR;
        const char *kind =
            !stat_ok
                ? "UNK "
                : (st.type == EntryType::DIR
                       ? "DIR "
                       : (st.type == EntryType::SYMLINK ? "LNK " : "FILE"));
        char link_target[256]{};
        bool has_link_target = false;
        if (stat_ok && st.type == EntryType::SYMLINK) {
            auto readlink_res =
                sys_vfs_readlink(dir_cap, name.c_str(), link_target,
                                 sizeof(link_target) - 1)
                    .to_result();
            size_t got = readlink_res.has_value() ? readlink_res.value() : 0;
            if (got < sizeof(link_target)) {
                link_target[got] = '\0';
                has_link_target  = true;
            }
        }
        print_indent(depth);
        if (has_link_target) {
            printf("%s %s -> %s\n", kind, child_path, link_target);
        } else {
            printf("%s %s\n", kind, child_path);
        }

        if (is_dir && depth + 1 < kMaxPrintDepth) {
            auto subdir_res =
                sys_vfs_opendir(dir_cap, name.c_str(), flags::O_READ)
                    .to_result();
            CapIdx subdir =
                subdir_res.has_value() ? subdir_res.value() : cap::error;
            if (subdir != cap::null && subdir != cap::error) {
                print_tree(subdir, child_path, depth + 1);
                (void)sys_cap_remove(subdir).to_result();
            }
        }
    }
}

void create_blk_linkings(CapIdx root_dir_cap) {
    if (root_dir_cap == cap::null || root_dir_cap == cap::error) {
        printf("init: bootstrap root dir capability missing\n");
        return;
    }

    auto dev_dir_res =
        sys_vfs_mkdir(root_dir_cap, "dev/",
                      flags::O_READ | flags::O_WRITE | flags::O_EXECUTE)
            .to_result();
    CapIdx dev_dir = dev_dir_res.has_value() ? dev_dir_res.value() : cap::error;
    if (dev_dir != cap::null && dev_dir != cap::error) {
        (void)sys_cap_remove(dev_dir).to_result();
    }

    auto sysdev_res =
        sys_vfs_opendir(root_dir_cap, "sys/dev", flags::O_READ).to_result();
    CapIdx sysdev_cap =
        sysdev_res.has_value() ? sysdev_res.value() : cap::error;
    if (sysdev_cap == cap::null || sysdev_cap == cap::error) {
        printf("init: open /sys/dev failed\n");
        return;
    }

    const char *vd_paths[] = {"/dev/vda", "/dev/vdb", "/dev/vdc", "/dev/vdd"};
    auto dev_names         = get_alldents(sysdev_cap);
    size_t blk_count       = 0;

    for (const auto &device_name : dev_names) {
        if (blk_count >= 4) {
            break;
        }

        auto device_dir_res =
            sys_vfs_opendir(sysdev_cap, device_name.c_str(), flags::O_READ)
                .to_result();
        CapIdx device_dir =
            device_dir_res.has_value() ? device_dir_res.value() : cap::error;
        if (device_dir == cap::null || device_dir == cap::error) {
            continue;
        }

        auto child_names = get_alldents(device_dir);
        bool has_vblk    = false;
        for (const auto &child_name : child_names) {
            if (child_name == "vblk") {
                has_vblk = true;
                break;
            }
        }
        (void)sys_cap_remove(device_dir).to_result();
        if (!has_vblk) {
            continue;
        }

        std::string source_path = "/sys/dev/" + device_name + "/vblk";
        if (force_symlink(vd_paths[blk_count], source_path.c_str())) {
            printf("init: link %s -> %s\n", vd_paths[blk_count],
                   source_path.c_str());
        } else {
            printf("init: create %s failed\n", vd_paths[blk_count]);
        }
        ++blk_count;
    }

    (void)sys_cap_remove(sysdev_cap).to_result();
}

void mount_testing_ext4(CapIdx root_dir_cap) {
    if (root_dir_cap == cap::null || root_dir_cap == cap::error) {
        printf("init: bootstrap root dir capability missing\n");
        return;
    }

    auto blk_cap_res =
        sys_vfs_open(root_dir_cap, "dev/vda", flags::O_READ).to_result();
    CapIdx blk_cap = blk_cap_res.has_value() ? blk_cap_res.value() : cap::error;
    if (blk_cap == cap::null || blk_cap == cap::error) {
        printf("init: open /dev/vda failed\n");
        return;
    }

    auto testing_dir_res =
        sys_vfs_mkdir(root_dir_cap, "testing/",
                      flags::O_READ | flags::O_WRITE | flags::O_EXECUTE)
            .to_result();
    CapIdx testing_dir =
        testing_dir_res.has_value() ? testing_dir_res.value() : cap::error;
    if (testing_dir != cap::null && testing_dir != cap::error) {
        (void)sys_cap_remove(testing_dir);
    }

    auto mnt_cap_res = sys_mnt_create(blk_cap, "ext4", 0, nullptr).to_result();
    CapIdx mnt_cap = mnt_cap_res.has_value() ? mnt_cap_res.value() : cap::error;
    bool mounted   = false;
    if (mnt_cap == cap::null || mnt_cap == cap::error) {
        printf("init: create ext4 mount failed\n");
    } else {
        mounted = sys_mnt_mount(mnt_cap, root_dir_cap, "testing/", 0);
        (void)sys_cap_remove(mnt_cap);
    }
    if (mounted) {
        printf("init: mount /testing succeeded\n");
    } else {
        printf("init: mount /testing failed\n");
    }
    (void)sys_cap_remove(blk_cap);
}

void setup_stdout_link() {
    const char *stdout_device_dir = bootstrap_stdout_device_dir();
    if (stdout_device_dir == nullptr || stdout_device_dir[0] == '\0') {
        return;
    }

    std::string stdout_target = std::string(stdout_device_dir) + "/serial";
    if (force_symlink("/dev/stdout", stdout_target.c_str())) {
        printf("init: link /dev/stdout -> %s\n", stdout_target.c_str());
    } else {
        printf("init: create /dev/stdout failed\n");
    }
}

void run_requests(const std::vector<SpawnRequest> &requests,
                  CapIdx root_dir_cap) {
    for (const auto &request : requests) {
        int fd = kmod_fopen(request.path, "x");
        if (fd < 0) {
            printf("init: %s 未找到, 跳过 %s\n", request.path,
                   request.dispname);
            continue;
        }

        CapIdx child_pcb =
            request.is_linuxproc
                ? spawn_linux_with_root_dir(fd, SCHED_CLASS_FCFS, root_dir_cap)
                : spawn_with_root_dir(fd, SCHED_CLASS_FCFS, root_dir_cap);
        if (child_pcb == cap::null || child_pcb == cap::error) {
            printf("init: 创建 %s 失败\n", request.dispname);
            kmod_fclose(fd);
            continue;
        }

        size_t pid = sys_getpid(child_pcb).value();
        printf("init: 创建 %s, pid=%lu\n", request.dispname,
               static_cast<unsigned long>(pid));
        kmod_fclose(fd);

        CapIdx wait_caps[] = {child_pcb, cap::null};

        auto wait_ret =
            sys_tcb_wait(__main_tcb_cap, wait_caps, nullptr, 0).to_result();
        if (!wait_ret.has_value()) {
            printf("init: 等待 %s 失败\n", request.dispname);
            continue;
        }

        CapIdx exited_cap = wait_ret.value();
        assert(exited_cap != cap::null && exited_cap != cap::error);
        printf("init: %s 已完成!\n", request.dispname);
    }
}

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
                         const bsheader *bsargv[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)bsargv;
    mark_cnt = 0;
    printf("进入 init 模块!\n");

    CapIdx root_dir_cap = bootstrap_root_dir();
    if (root_dir_cap == cap::null || root_dir_cap == cap::error) {
        printf("init: bootstrap root dir capability missing\n");
        exit(-1);
    }

    create_blk_linkings(root_dir_cap);
    setup_stdout_link();

    // make a link /bin/ls -> /dev/stdout
    // 只是为了让 which ls 能够正常工作
    kmod_symlink("/bin/ls", "/dev/stdout");

    mount_testing_ext4(root_dir_cap);

    // print_tree(root_dir_cap, "/");

    std::vector<SpawnRequest> requests{
        // SpawnRequest{
        //     .path       = "/initrd/test_fork.mod",
        //     .dispname   = "test_fork",
        //     .is_linuxproc = false,
        // },
        // SpawnRequest{
        //     .path       = "/initrd/test_endpoint_master.mod",
        //     .dispname   = "test_endpoint_master",
        //     .is_linuxproc = false,
        // },
        // SpawnRequest{
        //     .path       = "/initrd/test_call_service.mod",
        //     .dispname   = "test_call_service",
        //     .is_linuxproc = false,
        // },
        // SpawnRequest{
        //     .path       = "/initrd/test_file_rw_a.mod",
        //     .dispname   = "test_file_rw_a",
        //     .is_linuxproc = false,
        // },
        // SpawnRequest{
        //     .path         = "/initrd/test-elf-demand.mod",
        //     .dispname     = "test-elf-demand",
        //     .is_linuxproc = false,
        // },
        // SpawnRequest{
        //     .path         = "/initrd/test-elf-demand-perf.mod",
        //     .dispname     = "test-elf-demand-perf",
        //     .is_linuxproc = false,
        // },
        // SpawnRequest{
        //     .path         = "/initrd/test-procfs.mod",
        //     .dispname     = "test-procfs",
        //     .is_linuxproc = false,
        // },
        // SpawnRequest{
        //     .path       = "/initrd/tmp/write",
        //     .dispname   = "write",
        //     .is_linuxproc = true,
        // },
        // SpawnRequest{
        //     .path         = "/initrd/contest-runner.mod",
        //     .dispname     = "contest-runner",
        //     .is_linuxproc = false,
        // },
        SpawnRequest{
            .path         = "/initrd/contest-runner.mod",
            .dispname     = "contest-runner",
            .is_linuxproc = false,
        },
        // SpawnRequest{
        //     .path         = "/initrd/test-meminfo.mod",
        //     .dispname     = "test-meminfo",
        //     .is_linuxproc = false,
        // },
        SpawnRequest{
            .path       = "/initrd/test-linux.mod",
            .dispname   = "test-linux",
            .is_linuxproc = true,
        }
    };

    // try write file /dev/stdout
    int fd = 0;
    fd     = kmod_fopen("/dev/stdout", "w");
    if (fd >= 0) {
        kmod_fwrite(fd, "Hello, STDOUT!\n", 15);
        printf(
            "init: write \"Hello, STDOUT!\\n\" to "
            "/dev/stdout\n");
        kmod_fclose(fd);
    } else {
        printf("init: can't open `/dev/stdout` !\n");
    }

    printf("init: 初始化, 开始启动\n");
    run_requests(requests, root_dir_cap);
    printf("init: 全部运行完成! done!\n");
    sys_shutdown();
    return 0;
}
