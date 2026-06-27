/**
 * @file main.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief Ext4 read-only filesystem tests
 * @version alpha-1.0.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sustcore/bootstrap.h>
#include <kmod/syscall.h>

#include <cstdio>
#include <cstring>

namespace {
    constexpr const char *EXT4_ROOT      = "/test_img";
    constexpr const char *EXT4_ETC       = "/test_img/etc";
    constexpr const char *PASSWD_PATH    = "/test_img/etc/passwd";
    constexpr const char *BUSYBOX_PATH   = "/test_img/bin/busybox";
    constexpr const char *FASTLINK_PATH  = "/test_img/lib/libc.musl-riscv64.so.1";
    constexpr const char *FASTLINK_VALUE = "ld-musl-riscv64.so.1";
    char g_dirent_buffer[16384];
    char g_entry_name_buffer[256];

    [[nodiscard]]
    CapIdx bootstrap_root_dir() {
        CapIdx cap = cap::null;
        bool found = false;
        bool ok    = bootstrap_foreach_record(
            __bsargv, __bsargc,
            [&](const BootstrapRecordView &view) {
                if (found || view.header->type != boot::TYPE_CAPEXP)
                {
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
    bool contains_text(const char *buf, size_t len, const char *needle) {
        if (buf == nullptr || needle == nullptr) {
            return false;
        }
        const size_t needle_len = strlen(needle);
        if (needle_len == 0 || needle_len > len) {
            return false;
        }
        for (size_t i = 0; i + needle_len <= len; ++i) {
            if (memcmp(buf + i, needle, needle_len) == 0) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]]
    bool dir_has_entry(CapIdx dir_cap, const char *entry_name,
                       bool expect_file) {
        if (entry_name == nullptr) {
            return false;
        }
        const size_t entry_name_len = strlen(entry_name);
        if (entry_name_len >= sizeof(g_entry_name_buffer)) {
            return false;
        }
        memcpy(g_entry_name_buffer, entry_name, entry_name_len + 1);
        size_t doff = 0;
        while (doff != DIR_ENTRY_END) {
            CapInfo cap_info {};
            if (!sys_cap_lookup(dir_cap, &cap_info) ||
                cap_info.type != PayloadType::VDIR)
            {
                printf("test_ext4_read: getdents cap invalid cap=%u type=%u doff=%u\n",
                       static_cast<unsigned>(dir_cap),
                       static_cast<unsigned>(cap_info.type),
                       static_cast<unsigned>(doff));
                return false;
            }
            memset(g_dirent_buffer, 0, sizeof(g_dirent_buffer));
            auto getdents_res =
                sys_vfs_getdents(dir_cap, g_dirent_buffer,
                                 sizeof(g_dirent_buffer), doff)
                    .to_result();
            if (!getdents_res.has_value()) {
                return false;
            }
            const size_t bytes = getdents_res.value();
            if (bytes == 0) {
                return false;
            }

            size_t parsed = 0;
            for (size_t offset = 0; offset < bytes;) {
                if (bytes - offset < sizeof(dir_entry_header)) {
                    return false;
                }
                auto *header =
                    reinterpret_cast<const dir_entry_header *>(
                        g_dirent_buffer + offset);
                const char *name =
                    g_dirent_buffer + offset + sizeof(dir_entry_header);
                const size_t name_room =
                    bytes - offset - sizeof(dir_entry_header);
                if (memchr(name, '\0', name_room) == nullptr) {
                    return false;
                }

                const size_t name_len = strlen(name);
                if (name_len == entry_name_len &&
                    memcmp(name, g_entry_name_buffer, entry_name_len) == 0)
                {
                    NodeMeta st {};
                    if (!sys_vfs_stat(dir_cap, name, &st))
                    {
                        return false;
                    }
                    return expect_file ? st.type == EntryType::FILE
                                       : st.type == EntryType::DIR;
                }
                ++parsed;

                if (header->next_offset == DIR_ENTRY_END) {
                    return false;
                }
                if (header->next_offset == 0 ||
                    offset + header->next_offset > bytes)
                {
                    return false;
                }
                offset += header->next_offset;
            }
            if (parsed == 0) {
                return false;
            }
            doff += parsed;
        }
        return false;
    }

    [[nodiscard]]
    bool test_root_directory(CapIdx root_cap) {
        auto ext4_dir_res =
            sys_vfs_opendir(root_cap, "test_img", flags::O_READ).to_result();
        CapIdx ext4_dir =
            ext4_dir_res.has_value() ? ext4_dir_res.value() : cap::error;
        if (ext4_dir == cap::null || ext4_dir == cap::error) {
            printf("test_ext4_read: opendir %s failed\n", EXT4_ROOT);
            return false;
        }

        const bool ok = dir_has_entry(ext4_dir, "etc", false) &&
                        dir_has_entry(ext4_dir, "bin", false) &&
                        dir_has_entry(ext4_dir, "lib", false) &&
                        dir_has_entry(ext4_dir, ".dockerenv", true);
        (void)sys_cap_remove(ext4_dir).to_result();
        if (!ok) {
            printf("test_ext4_read: root directory entries mismatch\n");
            return false;
        }
        printf("test_ext4_read: root directory ok\n");
        return true;
    }

    [[nodiscard]]
    bool test_etc_directory(CapIdx root_cap) {
        auto ext4_dir_res =
            sys_vfs_opendir(root_cap, "test_img/etc", flags::O_READ).to_result();
        CapIdx ext4_dir =
            ext4_dir_res.has_value() ? ext4_dir_res.value() : cap::error;
        if (ext4_dir == cap::null || ext4_dir == cap::error) {
            printf("test_ext4_read: opendir %s failed\n", EXT4_ETC);
            return false;
        }

        const bool ok = dir_has_entry(ext4_dir, "passwd", true) &&
                        dir_has_entry(ext4_dir, "group", true) &&
                        dir_has_entry(ext4_dir, "init.d", false) &&
                        dir_has_entry(ext4_dir, "apk", false);
        (void)sys_cap_remove(ext4_dir).to_result();
        if (!ok) {
            printf("test_ext4_read: etc directory entries mismatch\n");
            return false;
        }
        printf("test_ext4_read: etc directory ok\n");
        return true;
    }

    [[nodiscard]]
    bool test_passwd() {
        int fd = kmod_fopen(PASSWD_PATH, "r");
        if (fd < 0) {
            printf("test_ext4_read: open %s failed\n", PASSWD_PATH);
            return false;
        }

        char buf[1024] {};
        size_t total = 0;
        while (total + 1 < sizeof(buf)) {
            const size_t got =
                kmod_fread(fd, buf + total, sizeof(buf) - total - 1);
            if (got == 0) {
                break;
            }
            total += got;
        }
        kmod_fclose(fd);

        if (!contains_text(buf, total, "root:x:0:0:root:/root:/bin/sh") ||
            !contains_text(buf, total, "nobody:x:65534:65534"))
        {
            printf("test_ext4_read: passwd content mismatch total=%u\n",
                   static_cast<unsigned>(total));
            return false;
        }
        printf("test_ext4_read: passwd read ok len=%u\n",
               static_cast<unsigned>(total));
        return true;
    }

    [[nodiscard]]
    bool test_large_extent_file() {
        int fd = kmod_fopen(BUSYBOX_PATH, "r");
        if (fd < 0) {
            printf("test_ext4_read: open %s failed\n", BUSYBOX_PATH);
            return false;
        }

        char buf[5000] {};
        const size_t got = kmod_fread(fd, buf, sizeof(buf));
        kmod_fclose(fd);

        if (got != sizeof(buf) || buf[0] != '\x7f' || buf[1] != 'E' ||
            buf[2] != 'L' || buf[3] != 'F')
        {
            printf("test_ext4_read: busybox ELF prefix mismatch got=%u\n",
                   static_cast<unsigned>(got));
            return false;
        }
        unsigned second_block_sum = 0;
        for (size_t i = 4096; i < got; ++i) {
            second_block_sum += static_cast<unsigned char>(buf[i]);
        }
        if (second_block_sum == 0) {
            printf("test_ext4_read: busybox second block looks empty\n");
            return false;
        }
        printf("test_ext4_read: large extent file ok\n");
        return true;
    }

    [[nodiscard]]
    bool test_fast_symlink() {
        int fd = kmod_fopen(FASTLINK_PATH, "r");
        if (fd < 0) {
            printf("test_ext4_read: open %s failed\n", FASTLINK_PATH);
            return false;
        }

        char buf[64] {};
        const size_t got = kmod_fread(fd, buf, sizeof(buf) - 1);
        kmod_fclose(fd);
        buf[got] = '\0';

        if (strcmp(buf, FASTLINK_VALUE) != 0) {
            printf("test_ext4_read: fast symlink mismatch got=\"%s\"\n", buf);
            return false;
        }
        printf("test_ext4_read: fast symlink ok -> %s\n", buf);
        return true;
    }
}  // namespace

extern "C" int kmod_main(int argc, const char *argv[], const char *envp[],
              const bsheader *bsargv[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)bsargv;
    printf("test_ext4_read: start pid=%u\n", sys_getpid(__pcb_cap).value());

    CapIdx root_cap = bootstrap_root_dir();
    if (root_cap == cap::null || root_cap == cap::error) {
        printf("test_ext4_read: bootstrap root dir missing\n");
        exit(-1);
    }

    if (!test_root_directory(root_cap) || !test_etc_directory(root_cap) ||
        !test_passwd() ||
        !test_large_extent_file() || !test_fast_symlink())
    {
        printf("test_ext4_read: FAILED\n");
        exit(-1);
    }

    printf("test_ext4_read: PASS\n");
    exit(0);
    return 0;
}
