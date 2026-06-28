/**
 * @file ext4.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief Ext4 block filesystem implementation
 * @version alpha-1.0.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <bio/blk.h>
#include <logger.h>
#include <task/wait.h>
#include <vfs/ext4.h>

#include <algorithm>
#include <cstring>

namespace ext4 {
    namespace {
        constexpr uint16_t EXT4_SUPER_MAGIC = 0xEF53;
        constexpr uint16_t EXT4_EXT_MAGIC   = 0xF30A;
        constexpr inode_t EXT4_ROOT_INO     = 2;

        constexpr uint16_t EXT4_S_IFMT  = 0xF000;
        constexpr uint16_t EXT4_S_IFREG = 0x8000;
        constexpr uint16_t EXT4_S_IFDIR = 0x4000;
        constexpr uint16_t EXT4_S_IFLNK = 0xA000;

        constexpr uint32_t EXT4_EXTENTS_FL        = 0x00080000;
        constexpr uint32_t EXT4_INDEX_FL          = 0x00001000;
        constexpr uint16_t EXT4_LINK_MAX          = 65000;
        constexpr uint16_t EXT4_MAX_EXTENT_DEPTH  = 5;
        constexpr uint16_t EXT4_DEFAULT_FILE_MODE = EXT4_S_IFREG | 0644U;

        constexpr uint32_t EXT4_FEATURE_INCOMPAT_FILETYPE    = 0x0002;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_RECOVER     = 0x0004;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_JOURNAL_DEV = 0x0008;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_META_BG     = 0x0010;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_EXTENTS     = 0x0040;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_64BIT       = 0x0080;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_MMP         = 0x0100;
        constexpr uint32_t EXT4_FEATURE_INCOMPAT_FLEX_BG     = 0x0200;
        constexpr uint32_t EXT4_SUPPORTED_INCOMPAT =
            EXT4_FEATURE_INCOMPAT_FILETYPE | EXT4_FEATURE_INCOMPAT_EXTENTS |
            EXT4_FEATURE_INCOMPAT_64BIT | EXT4_FEATURE_INCOMPAT_FLEX_BG;

        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER  = 0x0001;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_LARGE_FILE    = 0x0002;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_BTREE_DIR     = 0x0004;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_HUGE_FILE     = 0x0008;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_GDT_CSUM      = 0x0010;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_DIR_NLINK     = 0x0020;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE   = 0x0040;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_BIGALLOC      = 0x0200;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_METADATA_CSUM = 0x0400;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_READONLY      = 0x1000;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_PROJECT       = 0x2000;
        constexpr uint32_t EXT4_FEATURE_RO_COMPAT_VERITY        = 0x8000;
        constexpr uint32_t EXT4_SUPPORTED_RO_COMPAT =
            EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER |
            EXT4_FEATURE_RO_COMPAT_LARGE_FILE |
            EXT4_FEATURE_RO_COMPAT_BTREE_DIR |
            EXT4_FEATURE_RO_COMPAT_HUGE_FILE | EXT4_FEATURE_RO_COMPAT_GDT_CSUM |
            EXT4_FEATURE_RO_COMPAT_DIR_NLINK |
            EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE |
            EXT4_FEATURE_RO_COMPAT_METADATA_CSUM |
            EXT4_FEATURE_RO_COMPAT_READONLY | EXT4_FEATURE_RO_COMPAT_PROJECT |
            EXT4_FEATURE_RO_COMPAT_VERITY;

        constexpr uint8_t EXT4_FT_UNKNOWN  = 0;
        constexpr uint8_t EXT4_FT_REG_FILE = 1;
        constexpr uint8_t EXT4_FT_DIR      = 2;
        constexpr uint8_t EXT4_FT_SYMLINK  = 7;

        template <typename T>
        [[nodiscard]]
        T read_le(const void *ptr) {
            T value{};
            memcpy(&value, ptr, sizeof(T));
            return value;
        }

        template <typename T>
        [[nodiscard]]
        T read_le_at(const std::vector<byte> &buf, size_t offset) {
            if (offset + sizeof(T) > buf.size()) {
                return T{};
            }
            return read_le<T>(buf.data() + offset);
        }

        template <typename T>
        void write_le_at(std::vector<byte> &buf, size_t offset, T value) {
            if (offset + sizeof(T) > buf.size()) {
                return;
            }
            memcpy(buf.data() + offset, &value, sizeof(T));
        }

        [[nodiscard]]
        uint64_t join_u64(uint32_t lo, uint32_t hi) {
            return static_cast<uint64_t>(lo) |
                   (static_cast<uint64_t>(hi) << 32);
        }

        [[nodiscard]]
        size_t align4(size_t value) {
            return (value + 3U) & ~static_cast<size_t>(3U);
        }

        [[nodiscard]]
        uint16_t min_dir_rec_len(size_t name_len) {
            return static_cast<uint16_t>(
                align4(sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) +
                       sizeof(uint8_t) + name_len));
        }

        struct PACKED Ext4ExtentHeader {
            uint16_t eh_magic;
            uint16_t eh_entries;
            uint16_t eh_max;
            uint16_t eh_depth;
            uint32_t eh_generation;
        };

        struct PACKED Ext4Extent {
            uint32_t ee_block;
            uint16_t ee_len;
            uint16_t ee_start_hi;
            uint32_t ee_start_lo;
        };

        struct PACKED Ext4ExtentIdx {
            uint32_t ei_block;
            uint32_t ei_leaf_lo;
            uint16_t ei_leaf_hi;
            uint16_t ei_unused;
        };

        struct PACKED Ext4DirEntry2 {
            uint32_t inode;
            uint16_t rec_len;
            uint8_t name_len;
            uint8_t file_type;
        };

        [[nodiscard]]
        bool valid_inode_id(inode_t inode_id) {
            return inode_id != 0;
        }

        [[nodiscard]]
        Result<void> validate_entry_name(std::string_view name) {
            if (name.empty() || name.size() > 255 || name == "." ||
                name == "..")
            {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            for (char ch : name) {
                if (ch == '/') {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
            }
            void_return();
        }

        [[nodiscard]]
        uint32_t extent_len(uint16_t len_raw) {
            if (len_raw > 0x8000U) {
                return static_cast<uint32_t>(len_raw - 0x8000U);
            }
            return len_raw;
        }

        [[nodiscard]]
        bool extent_unwritten(uint16_t len_raw) {
            return len_raw > 0x8000U;
        }

        // 放在代码文件的前部（或类内部）
        void diagnose_unsupported_features(uint32_t unsupported_bits) {
            // 如果没有任何不支持的位，直接返回
            if (unsupported_bits == 0) {
                return;
            }
            // 定义常见的不兼容特性映射表（按位值从低到高排列）
            struct FeatureMap {
                uint32_t msk;
                const char *name;
            };

            const FeatureMap incompat_map[] = {
                {.msk = EXT4_FEATURE_INCOMPAT_FILETYPE, .name = "FILETYPE"},
                {.msk = EXT4_FEATURE_INCOMPAT_RECOVER, .name = "RECOVER"},
                {.msk  = EXT4_FEATURE_INCOMPAT_JOURNAL_DEV,
                 .name = "JOURNAL_DEV"},
                {.msk = EXT4_FEATURE_INCOMPAT_META_BG, .name = "META_BG"},
                {.msk = EXT4_FEATURE_INCOMPAT_EXTENTS, .name = "EXTENTS"},
                {.msk = EXT4_FEATURE_INCOMPAT_64BIT, .name = "64BIT"},
                {.msk = EXT4_FEATURE_INCOMPAT_MMP, .name = "MMP"},
                {.msk = EXT4_FEATURE_INCOMPAT_FLEX_BG, .name = "FLEX_BG"}};

            std::string errinfo = "";
            bool first          = true;
            for (const auto &[msk, name] : incompat_map) {
                if (unsupported_bits & msk) {
                    if (!first) {
                        errinfo += " | ";
                    }
                    errinfo          += name;
                    first             = false;
                    // 清除已匹配的位，以便最后打印未识别的未知位
                    unsupported_bits &= msk;
                }
            }

            // 打印错误
            loggers::SUSTCORE::ERROR(
                "Unsupported incompatible feature bits detected: (%s)%#016X",
                errinfo.empty() ? "None" : errinfo.c_str(), unsupported_bits);
        }
    }  // namespace

    Ext4File::Ext4File(Ext4Superblock &sb, inode_t inode_id) noexcept
        : _sb(&sb), _inode_id(inode_id) {}

    Result<size_t> Ext4File::read(off_t offset, void *buf, size_t len) {
        if (offset < 0 || (len != 0 && buf == nullptr)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return _sb->read_inode_data(_inode_id, static_cast<uint64_t>(offset),
                                    buf, len);
    }

    Result<size_t> Ext4File::write(off_t offset, const void *buf, size_t len) {
        if (offset < 0 || (len != 0 && buf == nullptr)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return _sb->write_inode_data(_inode_id, static_cast<uint64_t>(offset),
                                     buf, len);
    }

    Result<size_t> Ext4File::size() {
        auto size_res = _sb->inode_size(_inode_id);
        propagate(size_res);
        return static_cast<size_t>(size_res.value());
    }

    Result<void> Ext4File::sync() {
        return _sb->sync();
    }

    Result<void> Ext4File::truncate(size_t new_size) {
        return _sb->truncate(_inode_id, static_cast<uint64_t>(new_size));
    }

    IMetadata &Ext4File::metadata() {
        return _metadata;
    }

    inode_t Ext4File::inode_id() const {
        return _inode_id;
    }

    INodeCachePolicy Ext4File::inode_cache() const {
        return INodeCachePolicy::SHARED;
    }

    FileCachePolicy Ext4File::file_cache() const {
        return FileCachePolicy::SHARED;
    }

    Result<void> Ext4File::getattr(AttrSet &out) const {
        return _sb->fill_attr(_inode_id, out);
    }

    Result<void> Ext4File::setattr(AttrMask mask, const AttrSet &attrs) {
        return _sb->apply_attr(_inode_id, mask, attrs);
    }

    Ext4Symlink::Ext4Symlink(Ext4Superblock &sb, inode_t inode_id) noexcept
        : _sb(&sb), _inode_id(inode_id) {}

    Result<std::string> Ext4Symlink::target() {
        return _sb->readlink(_inode_id);
    }

    IMetadata &Ext4Symlink::metadata() {
        return _metadata;
    }

    inode_t Ext4Symlink::inode_id() const {
        return _inode_id;
    }

    INodeCachePolicy Ext4Symlink::inode_cache() const {
        return INodeCachePolicy::SHARED;
    }

    Result<void> Ext4Symlink::getattr(AttrSet &out) const {
        return _sb->fill_attr(_inode_id, out);
    }

    Result<void> Ext4Symlink::setattr(AttrMask mask, const AttrSet &attrs) {
        return _sb->apply_attr(_inode_id, mask, attrs);
    }

    Ext4Directory::Ext4Directory(Ext4Superblock &sb, inode_t inode_id) noexcept
        : _sb(&sb), _inode_id(inode_id) {}

    Result<std::vector<Ext4DirEntry>> Ext4Directory::collect_entries() {
        if (_entries_cached) {
            return _cached_entries;
        }
        auto res = _sb->read_directory(_inode_id);
        if (res.has_value()) {
            _cached_entries = res.value();
            _entries_cached = true;
        }
        return res;
    }

    Result<inode_t> Ext4Directory::lookup(std::string_view name) {
        if (name.empty() || name == ".") {
            return _inode_id;
        }

        auto entries_res = collect_entries();
        propagate(entries_res);
        for (const auto &entry : entries_res.value()) {
            if (entry.name == name) {
                return entry.inode_id;
            }
        }
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }

    Result<inode_t> Ext4Directory::mkfile(std::string_view name,
                                          const char *options) {
        (void)options;
        _entries_cached = false;
        return _sb->create_file(_inode_id, name);
    }

    Result<inode_t> Ext4Directory::mkdir(std::string_view name,
                                         const char *options) {
        (void)options;
        _entries_cached = false;
        return _sb->create_directory(_inode_id, name);
    }

    Result<inode_t> Ext4Directory::symlink(std::string_view name,
                                           std::string_view target) {
        _entries_cached = false;
        return _sb->create_symlink(_inode_id, name, target);
    }

    Result<size_t> Ext4Directory::entry_count() {
        auto entries_res = collect_entries();
        propagate(entries_res);
        return entries_res.value().size();
    }

    Result<DirectoryEntryInfo> Ext4Directory::entry_at(size_t index) {
        auto entries_res = collect_entries();
        propagate(entries_res);
        if (index >= entries_res.value().size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const auto &entry = entries_res.value()[index];
        return DirectoryEntryInfo{
            .name = entry.name,
        };
    }

    Result<void> Ext4Directory::unlink(std::string_view name) {
        auto lookup_res = lookup(name);
        propagate(lookup_res);
        const inode_t target = lookup_res.value();

        auto mode_res = _sb->inode_mode(target);
        propagate(mode_res);
        const uint16_t type = mode_res.value() & EXT4_S_IFMT;

        if (type == EXT4_S_IFREG || type == EXT4_S_IFLNK) {
            auto raw_res = _sb->read_inode_raw(target);
            propagate(raw_res);
            uint16_t lc = read_le_at<uint16_t>(raw_res.value(), 26);
            if (lc > 1) {
                write_le_at<uint16_t>(raw_res.value(), 26,
                                      static_cast<uint16_t>(lc - 1));
                auto w = _sb->write_inode_raw(target, raw_res.value());
                propagate(w);
            } else {
                auto del_res = _sb->delete_file(target);
                propagate(del_res);
            }
        } else if (type == EXT4_S_IFDIR) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        } else {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto remove_res = _sb->remove_dir_entry(_inode_id, name);
        propagate(remove_res);
        _entries_cached = false;
        void_return();
    }

    Result<void> Ext4Directory::rmdir(std::string_view name) {
        auto lookup_res = lookup(name);
        propagate(lookup_res);
        const inode_t target = lookup_res.value();
        // verify target is a directory
        auto mode_res        = _sb->inode_mode(target);
        propagate(mode_res);
        if ((mode_res.value() & EXT4_S_IFMT) != EXT4_S_IFDIR) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto del_res = _sb->delete_directory(target);
        propagate(del_res);
        auto remove_res = _sb->remove_dir_entry(_inode_id, name);
        propagate(remove_res);
        _entries_cached = false;
        void_return();
    }

    Result<void> Ext4Directory::link(std::string_view name, inode_t target) {
        _entries_cached = false;
        return _sb->create_link(_inode_id, name, target);
    }

    Result<void> Ext4Directory::rename(std::string_view old_name,
                                       IDirectory &new_parent,
                                       std::string_view new_name) {
        auto &ext4_new = static_cast<Ext4Directory &>(new_parent);
        auto res =
            _sb->rename(_inode_id, old_name, ext4_new._inode_id, new_name);
        _entries_cached          = false;
        ext4_new._entries_cached = false;
        return res;
    }

    Result<void> Ext4Directory::sync() {
        return _sb->sync();
    }

    IMetadata &Ext4Directory::metadata() {
        return _metadata;
    }

    inode_t Ext4Directory::inode_id() const {
        return _inode_id;
    }

    INodeCachePolicy Ext4Directory::inode_cache() const {
        return INodeCachePolicy::SHARED;
    }

    Result<void> Ext4Directory::getattr(AttrSet &out) const {
        return _sb->fill_attr(_inode_id, out);
    }

    Result<void> Ext4Directory::setattr(AttrMask mask, const AttrSet &attrs) {
        return _sb->apply_attr(_inode_id, mask, attrs);
    }

    Ext4Superblock::Ext4Superblock(Ext4Driver &fs, blk::BufferCache &cache,
                                   size_t dev_block_size,
                                   size_t dev_block_count, size_t sb_id)
        : _fs(&fs),
          _cache(&cache),
          _dev_block_size(dev_block_size),
          _dev_block_count(dev_block_count),
          _sb_id(sb_id) {}

    Result<void> Ext4Superblock::read_device_bytes(uint64_t offset, void *buf,
                                                   size_t len) {
        if (len == 0) {
            void_return();
        }
        if (buf == nullptr || _cache == nullptr || _dev_block_size == 0) {
            loggers::VFS::ERROR(
                "Ext4 read_device_bytes 参数非法: offset=%u len=%u buf=%p "
                "cache=%p dev_block_size=%u dev_block_count=%u",
                static_cast<unsigned>(offset), static_cast<unsigned>(len), buf,
                _cache, static_cast<unsigned>(_dev_block_size),
                static_cast<unsigned>(_dev_block_count));
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint64_t total_bytes =
            static_cast<uint64_t>(_dev_block_size) * _dev_block_count;
        if (offset > total_bytes || len > total_bytes - offset) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        auto *out   = static_cast<byte *>(buf);
        size_t done = 0;
        while (done < len) {
            const uint64_t abs = offset + done;
            const lba_t lba    = static_cast<lba_t>(abs / _dev_block_size);
            const size_t off   = static_cast<size_t>(abs % _dev_block_size);
            const size_t chunk = std::min(len - done, _dev_block_size - off);

            auto future      = _cache->get_buffer_async(lba);
            auto handler_res = wait::blocking_wait_for(future);
            propagate(handler_res);
            const size_t read =
                handler_res.value().read(off, out + done, chunk);
            if (read != chunk) {
                unexpect_return(ErrCode::IO_ERROR);
            }
            done += chunk;
        }
        void_return();
    }

    Result<void> Ext4Superblock::write_device_bytes(uint64_t offset,
                                                    const void *buf,
                                                    size_t len) {
        if (len == 0) {
            void_return();
        }
        if (buf == nullptr || _cache == nullptr || _dev_block_size == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint64_t total_bytes =
            static_cast<uint64_t>(_dev_block_size) * _dev_block_count;
        if (offset > total_bytes || len > total_bytes - offset) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        const auto *in = static_cast<const byte *>(buf);
        size_t done    = 0;
        while (done < len) {
            const uint64_t abs = offset + done;
            const lba_t lba    = static_cast<lba_t>(abs / _dev_block_size);
            const size_t off   = static_cast<size_t>(abs % _dev_block_size);
            const size_t chunk = std::min(len - done, _dev_block_size - off);

            auto future      = _cache->get_buffer_async(lba);
            auto handler_res = wait::blocking_wait_for(future);
            propagate(handler_res);
            const size_t written =
                handler_res.value().write(off, in + done, chunk);
            if (written != chunk) {
                unexpect_return(ErrCode::IO_ERROR);
            }
            done += chunk;
        }
        void_return();
    }

    Result<void> Ext4Superblock::read_fs_block(uint64_t block, void *buf,
                                               size_t len) {
        if (len > _block_size) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return read_device_bytes(block * _block_size, buf, len);
    }

    Result<void> Ext4Superblock::write_fs_block(uint64_t block, const void *buf,
                                                size_t len) {
        if (len > _block_size) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return write_device_bytes(block * _block_size, buf, len);
    }

    Result<void> Ext4Superblock::load() {
        _superblock.resize(1024);
        auto super_res =
            read_device_bytes(1024, _superblock.data(), _superblock.size());
        propagate(super_res);

        const auto magic = read_le_at<uint16_t>(_superblock, 56);
        if (magic != EXT4_SUPER_MAGIC) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        const uint32_t log_block_size = read_le_at<uint32_t>(_superblock, 24);
        if (log_block_size > 16) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        _block_size = 1024U << log_block_size;
        if (_block_size == 0 || (_block_size % _dev_block_size) != 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        _first_data_block  = read_le_at<uint32_t>(_superblock, 20);
        _blocks_per_group  = read_le_at<uint32_t>(_superblock, 32);
        _inodes_per_group  = read_le_at<uint32_t>(_superblock, 40);
        _inode_count       = read_le_at<uint32_t>(_superblock, 0);
        _inode_size        = read_le_at<uint16_t>(_superblock, 88);
        _feature_compat    = read_le_at<uint32_t>(_superblock, 92);
        _feature_incompat  = read_le_at<uint32_t>(_superblock, 96);
        _feature_ro_compat = read_le_at<uint32_t>(_superblock, 100);
        _group_desc_size   = read_le_at<uint16_t>(_superblock, 254);
        if (_inode_size == 0) {
            _inode_size = 128;
        }
        if (_group_desc_size == 0) {
            _group_desc_size = 32;
        }

        if ((_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) == 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        // TODO: 不知怎得, 这个地方炸了
        if ((_feature_incompat & ~EXT4_SUPPORTED_INCOMPAT) != 0) {
            auto unsupported_bits =
                _feature_incompat & ~EXT4_SUPPORTED_INCOMPAT;
            // 1. 调用诊断函数，打印具体哪个特性导致失败
            diagnose_unsupported_features(unsupported_bits);

            // 2. 这里还可以额外打印超级块中的原始值，方便核对
            loggers::VFS::ERROR(
                "Full _feature_incompat = 0x%X (dec: %u), Supported "
                "mask = 0x%X (dec: %u)",
                _feature_incompat, _feature_incompat, EXT4_SUPPORTED_INCOMPAT,
                EXT4_SUPPORTED_INCOMPAT);

            // TODO: 为 RECOVER 添加支持, 目前如果只有 RECOVER 则先放行,
            // 后续再完善
            if (unsupported_bits != EXT4_FEATURE_INCOMPAT_RECOVER) {
                unexpect_return(ErrCode::NOT_SUPPORTED);
            } else {
                loggers::VFS::ERROR("只有 RECOVER, 先放行");
            }
        }
        if ((_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_BIGALLOC) != 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        const uint32_t unsupported_ro =
            _feature_ro_compat & ~EXT4_SUPPORTED_RO_COMPAT;
        _read_only =
            unsupported_ro != 0 ||
            (_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_READONLY) != 0;
        if (_blocks_per_group == 0 || _inodes_per_group == 0 ||
            _inode_count == 0 || _inode_size < 128 || _group_desc_size < 32)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        const uint32_t blocks_lo = read_le_at<uint32_t>(_superblock, 4);
        const uint32_t blocks_hi = read_le_at<uint32_t>(_superblock, 336);
        _block_count             = join_u64(blocks_lo, blocks_hi);
        if (_block_count == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        _group_count = static_cast<uint32_t>(
            (_block_count - _first_data_block + _blocks_per_group - 1) /
            _blocks_per_group);

        const uint64_t desc_table_block =
            _block_size == 1024 ? 2 : _first_data_block + 1;
        _group_desc_offset = desc_table_block * _block_size;
        const size_t desc_bytes =
            static_cast<size_t>(_group_count) * _group_desc_size;
        _group_desc.resize(desc_bytes);
        auto gd_res = read_device_bytes(_group_desc_offset, _group_desc.data(),
                                        _group_desc.size());
        propagate(gd_res);

        if (_read_only) {
            loggers::VFS::WARN(
                "Ext4 以只读模式挂载: ro_compat=0x%x unsupported_ro=0x%x",
                static_cast<unsigned>(_feature_ro_compat),
                static_cast<unsigned>(unsupported_ro));
        }
        void_return();
    }

    Result<uint64_t> Ext4Superblock::inode_table_block(uint32_t group) {
        if (group >= _group_count) {
            loggers::VFS::ERROR(
                "Ext4 inode table group 越界: group=%u group_count=%u",
                static_cast<unsigned>(group),
                static_cast<unsigned>(_group_count));
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const size_t base = static_cast<size_t>(group) * _group_desc_size;
        if (base + 44 > _group_desc.size()) {
            loggers::VFS::ERROR(
                "Ext4 inode table desc 越界: group=%u base=%u desc_size=%u "
                "total=%u",
                static_cast<unsigned>(group), static_cast<unsigned>(base),
                static_cast<unsigned>(_group_desc_size),
                static_cast<unsigned>(_group_desc.size()));
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const uint32_t lo = read_le<uint32_t>(_group_desc.data() + base + 8);
        uint32_t hi       = 0;
        if (_group_desc_size >= 64) {
            hi = read_le<uint32_t>(_group_desc.data() + base + 40);
        }
        const uint64_t block = join_u64(lo, hi);
        return block;
    }

    Result<uint64_t> Ext4Superblock::inode_bitmap_block(uint32_t group) {
        if (group >= _group_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const size_t base = static_cast<size_t>(group) * _group_desc_size;
        if (base + 40 > _group_desc.size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const uint32_t lo = read_le<uint32_t>(_group_desc.data() + base + 4);
        uint32_t hi       = 0;
        if (_group_desc_size >= 64) {
            hi = read_le<uint32_t>(_group_desc.data() + base + 36);
        }
        return join_u64(lo, hi);
    }

    Result<uint64_t> Ext4Superblock::block_bitmap_block(uint32_t group) {
        if (group >= _group_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const size_t base = static_cast<size_t>(group) * _group_desc_size;
        if (base + 36 > _group_desc.size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const uint32_t lo = read_le<uint32_t>(_group_desc.data() + base);
        uint32_t hi       = 0;
        if (_group_desc_size >= 64) {
            hi = read_le<uint32_t>(_group_desc.data() + base + 32);
        }
        return join_u64(lo, hi);
    }

    Result<uint32_t> Ext4Superblock::group_free_blocks(uint32_t group) {
        if (group >= _group_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const size_t base = static_cast<size_t>(group) * _group_desc_size;
        if (base + 14 > _group_desc.size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const uint32_t lo = read_le<uint16_t>(_group_desc.data() + base + 12);
        uint32_t hi       = 0;
        if (_group_desc_size >= 64) {
            if (base + 46 > _group_desc.size()) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            hi = read_le<uint16_t>(_group_desc.data() + base + 44);
        }
        return lo | (hi << 16);
    }

    Result<void> Ext4Superblock::set_group_free_blocks(uint32_t group,
                                                       uint32_t count) {
        if (group >= _group_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const size_t base = static_cast<size_t>(group) * _group_desc_size;
        if (base + 14 > _group_desc.size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        write_le_at<uint16_t>(_group_desc, base + 12,
                              static_cast<uint16_t>(count & 0xFFFFU));
        if (_group_desc_size >= 64) {
            if (base + 46 > _group_desc.size()) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            write_le_at<uint16_t>(_group_desc, base + 44,
                                  static_cast<uint16_t>(count >> 16));
        }
        void_return();
    }

    Result<uint64_t> Ext4Superblock::alloc_block() {
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        const uint32_t blocks_per_group = _blocks_per_group;

        for (uint32_t group = 0; group < _group_count; ++group) {
            auto free_res = group_free_blocks(group);
            propagate(free_res);
            if (free_res.value() == 0) {
                continue;
            }

            auto bitmap_block_res = block_bitmap_block(group);
            propagate(bitmap_block_res);
            std::vector<byte> bitmap(_block_size);
            auto read_res = read_fs_block(bitmap_block_res.value(),
                                          bitmap.data(), bitmap.size());
            propagate(read_res);

            const uint32_t blocks_in_group = std::min<uint32_t>(
                blocks_per_group,
                static_cast<uint32_t>(_block_count -
                                      static_cast<uint64_t>(group) *
                                          blocks_per_group));
            for (uint32_t idx = 0; idx < blocks_in_group; ++idx) {
                const size_t byte_index = idx / 8U;
                if (byte_index >= bitmap.size()) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                const uint8_t mask = static_cast<uint8_t>(1U << (idx % 8U));
                if ((bitmap[byte_index] & mask) != 0) {
                    continue;
                }

                bitmap[byte_index]    |= mask;
                auto write_bitmap_res  = write_fs_block(
                    bitmap_block_res.value(), bitmap.data(), bitmap.size());
                propagate(write_bitmap_res);

                auto set_group_res =
                    set_group_free_blocks(group, free_res.value() - 1);
                propagate(set_group_res);
                const uint32_t sb_free = read_le_at<uint32_t>(_superblock, 12);
                if (sb_free == 0) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                write_le_at<uint32_t>(_superblock, 12, sb_free - 1);

                auto sync_group_res = sync_group_descriptors();
                propagate(sync_group_res);
                auto sync_super_res = sync_superblock_metadata();
                propagate(sync_super_res);

                return static_cast<uint64_t>(group) * blocks_per_group + idx +
                       _first_data_block;
            }
        }
        unexpect_return(ErrCode::OUT_OF_BOUNDARY);
    }

    Result<void> Ext4Superblock::free_block(uint64_t block_no) {
        if (block_no < _first_data_block || block_no >= _block_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const uint64_t data_block = block_no - _first_data_block;
        const uint32_t group =
            static_cast<uint32_t>(data_block / _blocks_per_group);
        const uint32_t idx =
            static_cast<uint32_t>(data_block % _blocks_per_group);
        if (group >= _group_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        auto bitmap_block_res = block_bitmap_block(group);
        propagate(bitmap_block_res);
        std::vector<byte> bitmap(_block_size);
        auto read_res = read_fs_block(bitmap_block_res.value(), bitmap.data(),
                                      bitmap.size());
        propagate(read_res);

        const size_t byte_index = idx / 8U;
        if (byte_index >= bitmap.size()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint8_t mask = static_cast<uint8_t>(1U << (idx % 8U));
        if ((bitmap[byte_index] & mask) == 0) {
            void_return();
        }
        bitmap[byte_index]    &= static_cast<uint8_t>(~mask);
        auto write_bitmap_res  = write_fs_block(bitmap_block_res.value(),
                                                bitmap.data(), bitmap.size());
        propagate(write_bitmap_res);

        auto free_res = group_free_blocks(group);
        propagate(free_res);
        auto set_group_res = set_group_free_blocks(group, free_res.value() + 1);
        propagate(set_group_res);
        const uint32_t sb_free = read_le_at<uint32_t>(_superblock, 12);
        write_le_at<uint32_t>(_superblock, 12, sb_free + 1);
        auto sync_group_res = sync_group_descriptors();
        propagate(sync_group_res);
        auto sync_super_res = sync_superblock_metadata();
        propagate(sync_super_res);
        void_return();
    }

    Result<uint32_t> Ext4Superblock::group_free_inodes(uint32_t group) {
        if (group >= _group_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const size_t base = static_cast<size_t>(group) * _group_desc_size;
        if (base + 16 > _group_desc.size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const uint32_t lo = read_le<uint16_t>(_group_desc.data() + base + 14);
        uint32_t hi       = 0;
        if (_group_desc_size >= 64) {
            if (base + 48 > _group_desc.size()) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            hi = read_le<uint16_t>(_group_desc.data() + base + 46);
        }
        return lo | (hi << 16);
    }

    Result<void> Ext4Superblock::set_group_free_inodes(uint32_t group,
                                                       uint32_t count) {
        if (group >= _group_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const size_t base = static_cast<size_t>(group) * _group_desc_size;
        if (base + 16 > _group_desc.size()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        write_le_at<uint16_t>(_group_desc, base + 14,
                              static_cast<uint16_t>(count & 0xFFFFU));
        if (_group_desc_size >= 64) {
            if (base + 48 > _group_desc.size()) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            write_le_at<uint16_t>(_group_desc, base + 46,
                                  static_cast<uint16_t>(count >> 16));
        }
        void_return();
    }

    Result<void> Ext4Superblock::sync_superblock_metadata() {
        if (_superblock.empty()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return write_device_bytes(1024, _superblock.data(), _superblock.size());
    }

    Result<void> Ext4Superblock::sync_group_descriptors() {
        if (_group_desc.empty() || _group_desc_offset == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return write_device_bytes(_group_desc_offset, _group_desc.data(),
                                  _group_desc.size());
    }

    Result<std::vector<byte>> Ext4Superblock::read_inode_raw(inode_t inode_id) {
        if (!valid_inode_id(inode_id)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (inode_id > _inode_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const inode_t zero_based = inode_id - 1;
        const uint32_t group =
            static_cast<uint32_t>(zero_based / _inodes_per_group);
        const uint32_t index =
            static_cast<uint32_t>(zero_based % _inodes_per_group);
        auto table_res = inode_table_block(group);
        if (!table_res.has_value()) {
            loggers::VFS::ERROR(
                "Ext4 read inode table 失败: inode=%u group=%u index=%u err=%s",
                static_cast<unsigned>(inode_id), static_cast<unsigned>(group),
                static_cast<unsigned>(index), to_cstring(table_res.error()));
            propagate_return(table_res);
        }

        std::vector<byte> raw(_inode_size);
        const uint64_t inode_offset =
            table_res.value() * _block_size +
            static_cast<uint64_t>(index) * _inode_size;
        auto read_res = read_device_bytes(inode_offset, raw.data(), raw.size());
        if (!read_res.has_value()) {
            loggers::VFS::ERROR(
                "Ext4 read inode raw 失败: inode=%u group=%u index=%u table=%u "
                "offset=%u size=%u err=%s",
                static_cast<unsigned>(inode_id), static_cast<unsigned>(group),
                static_cast<unsigned>(index),
                static_cast<unsigned>(table_res.value()),
                static_cast<unsigned>(inode_offset),
                static_cast<unsigned>(raw.size()),
                to_cstring(read_res.error()));
            propagate_return(read_res);
        }
        return raw;
    }

    Result<void> Ext4Superblock::write_inode_raw(inode_t inode_id,
                                                 const std::vector<byte> &raw) {
        if (!valid_inode_id(inode_id) || raw.size() != _inode_size) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (inode_id > _inode_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        const inode_t zero_based = inode_id - 1;
        const uint32_t group =
            static_cast<uint32_t>(zero_based / _inodes_per_group);
        const uint32_t index =
            static_cast<uint32_t>(zero_based % _inodes_per_group);
        auto table_res = inode_table_block(group);
        propagate(table_res);

        const uint64_t inode_offset =
            table_res.value() * _block_size +
            static_cast<uint64_t>(index) * _inode_size;
        return write_device_bytes(inode_offset, raw.data(), raw.size());
    }

    Result<void> Ext4Superblock::validate_inode_raw(
        const std::vector<byte> &raw) {
        if (raw.size() < 128) {
            loggers::VFS::ERROR("Ext4 inode raw 太短: size=%u",
                                static_cast<unsigned>(raw.size()));
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint16_t mode        = read_le_at<uint16_t>(raw, 0);
        const uint32_t delete_time = read_le_at<uint32_t>(raw, 20);
        const uint16_t link_count  = read_le_at<uint16_t>(raw, 26);
        if (mode == 0 || delete_time != 0 || link_count == 0) {
            loggers::VFS::ERROR(
                "Ext4 inode 不存在: mode=0x%x delete_time=%u link_count=%u",
                static_cast<unsigned>(mode), static_cast<unsigned>(delete_time),
                static_cast<unsigned>(link_count));
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        if (link_count > EXT4_LINK_MAX &&
            (_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_DIR_NLINK) == 0)
        {
            loggers::VFS::ERROR(
                "Ext4 inode link_count 非法: mode=0x%x delete_time=%u "
                "link_count=%u ro_compat=0x%x",
                static_cast<unsigned>(mode), static_cast<unsigned>(delete_time),
                static_cast<unsigned>(link_count),
                static_cast<unsigned>(_feature_ro_compat));
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        void_return();
    }

    Result<uint16_t> Ext4Superblock::inode_mode(inode_t inode_id) {
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        return read_le_at<uint16_t>(raw_res.value(), 0);
    }

    Result<uint64_t> Ext4Superblock::inode_size(inode_t inode_id) {
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        const uint16_t mode = read_le_at<uint16_t>(raw_res.value(), 0);
        const uint64_t lo   = read_le_at<uint32_t>(raw_res.value(), 4);
        uint64_t hi         = 0;
        if ((mode & EXT4_S_IFMT) == EXT4_S_IFREG ||
            (mode & EXT4_S_IFMT) == EXT4_S_IFLNK)
        {
            hi = read_le_at<uint32_t>(raw_res.value(), 108);
        }
        return lo | (hi << 32);
    }

    Result<void> Ext4Superblock::fill_attr(inode_t inode_id, AttrSet &out) {
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        const auto &raw = raw_res.value();

        out.mode    = read_le_at<uint16_t>(raw, 0);
        out.uid     = read_le_at<uint16_t>(raw, 2);
        out.gid     = read_le_at<uint16_t>(raw, 24);
        auto size_res = inode_size(inode_id);
        propagate(size_res);
        out.size    = size_res.value();
        out.inode   = inode_id;
        out.nlink   = read_le_at<uint16_t>(raw, 26);
        out.atime   = read_le_at<uint32_t>(raw, 8);
        out.ctime   = read_le_at<uint32_t>(raw, 12);
        out.mtime   = read_le_at<uint32_t>(raw, 16);
        out.blksize = 512;
        out.blocks  = read_le_at<uint64_t>(raw, 108) >> 9;
        void_return();
    }

    Result<void> Ext4Superblock::apply_attr(inode_t inode_id, AttrMask mask,
                                            const AttrSet &attrs) {
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        auto &raw = raw_res.value();

        if (mask & AttrMask::MODE) {
            write_le_at<uint16_t>(raw, 0, static_cast<uint16_t>(attrs.mode & 0xFFFF));
        }
        if (mask & AttrMask::UID) {
            write_le_at<uint16_t>(raw, 2, static_cast<uint16_t>(attrs.uid & 0xFFFF));
        }
        if (mask & AttrMask::GID) {
            write_le_at<uint16_t>(raw, 24, static_cast<uint16_t>(attrs.gid & 0xFFFF));
        }
        if (mask & AttrMask::ATIME) {
            write_le_at<uint32_t>(raw, 8, static_cast<uint32_t>(attrs.atime & 0xFFFFFFFF));
        }
        if (mask & AttrMask::MTIME) {
            write_le_at<uint32_t>(raw, 16, static_cast<uint32_t>(attrs.mtime & 0xFFFFFFFF));
        }
        if (mask & AttrMask::CTIME) {
            write_le_at<uint32_t>(raw, 12, static_cast<uint32_t>(attrs.ctime & 0xFFFFFFFF));
        }

        auto write_res = write_inode_raw(inode_id, raw);
        propagate(write_res);
        void_return();
    }

    Result<inode_t> Ext4Superblock::alloc_file_inode() {
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        for (uint32_t group = 0; group < _group_count; ++group) {
            auto free_res = group_free_inodes(group);
            propagate(free_res);
            if (free_res.value() == 0) {
                continue;
            }

            auto bitmap_block_res = inode_bitmap_block(group);
            propagate(bitmap_block_res);
            std::vector<byte> bitmap(_block_size);
            auto read_res = read_fs_block(bitmap_block_res.value(),
                                          bitmap.data(), bitmap.size());
            propagate(read_res);

            const uint32_t group_inode_base = group * _inodes_per_group;
            if (group_inode_base >= _inode_count) {
                break;
            }
            const uint32_t inodes_in_group = std::min<uint32_t>(
                _inodes_per_group, _inode_count - group_inode_base);
            for (uint32_t idx = 0; idx < inodes_in_group; ++idx) {
                const size_t byte_index = idx / 8U;
                if (byte_index >= bitmap.size()) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                const uint8_t mask = static_cast<uint8_t>(1U << (idx % 8U));
                if ((bitmap[byte_index] & mask) != 0) {
                    continue;
                }

                bitmap[byte_index]    |= mask;
                auto write_bitmap_res  = write_fs_block(
                    bitmap_block_res.value(), bitmap.data(), bitmap.size());
                propagate(write_bitmap_res);

                auto set_group_res =
                    set_group_free_inodes(group, free_res.value() - 1);
                propagate(set_group_res);
                const uint32_t sb_free = read_le_at<uint32_t>(_superblock, 16);
                if (sb_free == 0) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                write_le_at<uint32_t>(_superblock, 16, sb_free - 1);

                auto sync_group_res = sync_group_descriptors();
                propagate(sync_group_res);
                auto sync_super_res = sync_superblock_metadata();
                propagate(sync_super_res);

                const inode_t inode_id =
                    static_cast<inode_t>(group_inode_base + idx + 1);
                std::vector<byte> raw(_inode_size, 0);
                write_le_at<uint16_t>(raw, 0, EXT4_DEFAULT_FILE_MODE);
                write_le_at<uint16_t>(raw, 26, 1);
                write_le_at<uint32_t>(raw, 32, EXT4_EXTENTS_FL);
                write_le_at<uint16_t>(raw, 40, EXT4_EXT_MAGIC);
                write_le_at<uint16_t>(raw, 42, 0);
                write_le_at<uint16_t>(raw, 44, 4);
                write_le_at<uint16_t>(raw, 46, 0);
                uint32_t now = _next_time();
                write_le_at<uint32_t>(raw, 8, now);
                write_le_at<uint32_t>(raw, 12, now);
                write_le_at<uint32_t>(raw, 16, now);
                auto write_inode_res = write_inode_raw(inode_id, raw);
                propagate(write_inode_res);
                return inode_id;
            }
        }

        unexpect_return(ErrCode::OUT_OF_BOUNDARY);
    }

    Result<void> Ext4Superblock::release_file_inode(inode_t inode_id) {
        if (!valid_inode_id(inode_id) || inode_id > _inode_count) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const inode_t zero_based = inode_id - 1;
        const uint32_t group =
            static_cast<uint32_t>(zero_based / _inodes_per_group);
        const uint32_t idx =
            static_cast<uint32_t>(zero_based % _inodes_per_group);
        auto bitmap_block_res = inode_bitmap_block(group);
        propagate(bitmap_block_res);
        std::vector<byte> bitmap(_block_size);
        auto read_res = read_fs_block(bitmap_block_res.value(), bitmap.data(),
                                      bitmap.size());
        propagate(read_res);

        const size_t byte_index = idx / 8U;
        if (byte_index >= bitmap.size()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint8_t mask = static_cast<uint8_t>(1U << (idx % 8U));
        if ((bitmap[byte_index] & mask) == 0) {
            void_return();
        }

        bitmap[byte_index]    &= static_cast<uint8_t>(~mask);
        auto write_bitmap_res  = write_fs_block(bitmap_block_res.value(),
                                                bitmap.data(), bitmap.size());
        propagate(write_bitmap_res);

        std::vector<byte> raw(_inode_size, 0);
        auto clear_inode_res = write_inode_raw(inode_id, raw);
        propagate(clear_inode_res);

        auto free_res = group_free_inodes(group);
        propagate(free_res);
        auto set_group_res = set_group_free_inodes(group, free_res.value() + 1);
        propagate(set_group_res);
        const uint32_t sb_free = read_le_at<uint32_t>(_superblock, 16);
        write_le_at<uint32_t>(_superblock, 16, sb_free + 1);
        auto sync_group_res = sync_group_descriptors();
        propagate(sync_group_res);
        return sync_superblock_metadata();
    }

    Result<Ext4ExtentMapping> Ext4Superblock::extent_lookup_from_node(
        const byte *node, size_t node_size, uint32_t logical) {
        if (node == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (node_size < sizeof(Ext4ExtentHeader)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto *header = reinterpret_cast<const Ext4ExtentHeader *>(node);
        if (read_le<uint16_t>(&header->eh_magic) != EXT4_EXT_MAGIC) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint16_t entries = read_le<uint16_t>(&header->eh_entries);
        const uint16_t max     = read_le<uint16_t>(&header->eh_max);
        const uint16_t depth   = read_le<uint16_t>(&header->eh_depth);
        if (entries > max) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (depth > EXT4_MAX_EXTENT_DEPTH) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (entries == 0) {
            return Ext4ExtentMapping{};
        }

        if (depth == 0) {
            const size_t entries_bytes =
                static_cast<size_t>(entries) * sizeof(Ext4Extent);
            if (sizeof(Ext4ExtentHeader) + entries_bytes > node_size) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            auto *extents = reinterpret_cast<const Ext4Extent *>(
                node + sizeof(Ext4ExtentHeader));
            for (uint16_t i = 0; i < entries; ++i) {
                const uint32_t first = read_le<uint32_t>(&extents[i].ee_block);
                const uint16_t len_raw = read_le<uint16_t>(&extents[i].ee_len);
                const uint32_t len     = extent_len(len_raw);
                if (len == 0) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                if (logical < first || logical >= first + len) {
                    continue;
                }
                const uint64_t start =
                    (static_cast<uint64_t>(
                         read_le<uint16_t>(&extents[i].ee_start_hi))
                     << 32) |
                    read_le<uint32_t>(&extents[i].ee_start_lo);
                const uint64_t physical = start + (logical - first);
                if (physical >= _block_count) {
                    unexpect_return(ErrCode::OUT_OF_BOUNDARY);
                }
                return Ext4ExtentMapping{
                    .physical_block = physical,
                    .mapped         = true,
                    .unwritten      = extent_unwritten(len_raw),
                };
            }
            return Ext4ExtentMapping{};
        }

        const size_t index_bytes =
            static_cast<size_t>(entries) * sizeof(Ext4ExtentIdx);
        if (sizeof(Ext4ExtentHeader) + index_bytes > node_size) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto *indexes = reinterpret_cast<const Ext4ExtentIdx *>(
            node + sizeof(Ext4ExtentHeader));
        const Ext4ExtentIdx *chosen = nullptr;
        for (uint16_t i = 0; i < entries; ++i) {
            const uint32_t first = read_le<uint32_t>(&indexes[i].ei_block);
            if (first > logical) {
                break;
            }
            chosen = &indexes[i];
        }
        if (chosen == nullptr) {
            return Ext4ExtentMapping{};
        }

        const uint64_t leaf =
            (static_cast<uint64_t>(read_le<uint16_t>(&chosen->ei_leaf_hi))
             << 32) |
            read_le<uint32_t>(&chosen->ei_leaf_lo);
        if (leaf >= _block_count) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        std::vector<byte> child(_block_size);
        auto read_res = read_fs_block(leaf, child.data(), child.size());
        propagate(read_res);
        return extent_lookup_from_node(child.data(), child.size(), logical);
    }

    Result<Ext4ExtentMapping> Ext4Superblock::extent_lookup(inode_t inode_id,
                                                            uint32_t logical) {
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        if (raw_res.value().size() < 100) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint32_t flags = read_le_at<uint32_t>(raw_res.value(), 32);
        if ((flags & EXT4_EXTENTS_FL) == 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        return extent_lookup_from_node(raw_res.value().data() + 40, 60,
                                       logical);
    }

    Result<void> Ext4Superblock::insert_extent(inode_t inode_id,
                                               uint32_t logical,
                                               uint64_t physical,
                                               uint32_t len) {
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto &raw = raw_res.value();
        if (raw.size() < 100) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        byte *const i_block  = raw.data() + 40;
        auto *eh             = reinterpret_cast<Ext4ExtentHeader *>(i_block);
        const uint16_t magic = read_le<uint16_t>(&eh->eh_magic);
        if (magic != EXT4_EXT_MAGIC) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        const uint16_t depth           = read_le<uint16_t>(&eh->eh_depth);
        uint16_t entries               = read_le<uint16_t>(&eh->eh_entries);
        const uint16_t max_entries     = read_le<uint16_t>(&eh->eh_max);
        const uint16_t blk_max_entries = static_cast<uint16_t>(
            (_block_size - sizeof(Ext4ExtentHeader)) / sizeof(Ext4Extent));

        if (depth > EXT4_MAX_EXTENT_DEPTH) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (depth == 0) {
            auto *extents = reinterpret_cast<Ext4Extent *>(
                i_block + sizeof(Ext4ExtentHeader));

            for (uint16_t i = 0; i < entries; ++i) {
                const uint32_t first = read_le<uint32_t>(&extents[i].ee_block);
                const uint16_t len_raw = read_le<uint16_t>(&extents[i].ee_len);
                const uint32_t elen    = extent_len(len_raw);

                if (logical + len == first &&
                    physical + static_cast<uint64_t>(len) * _block_size ==
                        ((static_cast<uint64_t>(
                              read_le<uint16_t>(&extents[i].ee_start_hi))
                          << 32) |
                         read_le<uint32_t>(&extents[i].ee_start_lo)))
                {
                    write_le_at<uint32_t>(
                        raw,
                        40 + sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent),
                        logical);
                    write_le_at<uint16_t>(raw,
                                          40 + sizeof(Ext4ExtentHeader) +
                                              i * sizeof(Ext4Extent) + 4,
                                          static_cast<uint16_t>(len + elen));
                    return write_inode_raw(inode_id, raw);
                }

                if (first == logical + len &&
                    ((static_cast<uint64_t>(
                          read_le<uint16_t>(&extents[i].ee_start_hi))
                      << 32) |
                     read_le<uint32_t>(&extents[i].ee_start_lo)) ==
                        physical + static_cast<uint64_t>(len) * _block_size)
                {
                    write_le_at<uint16_t>(raw,
                                          40 + sizeof(Ext4ExtentHeader) +
                                              i * sizeof(Ext4Extent) + 4,
                                          static_cast<uint16_t>(len + elen));
                    return write_inode_raw(inode_id, raw);
                }
            }

            if (entries < max_entries) {
                uint16_t insert_pos = entries;
                for (uint16_t i = 0; i < entries; ++i) {
                    if (logical < read_le<uint32_t>(&extents[i].ee_block)) {
                        insert_pos = i;
                        break;
                    }
                }
                for (uint16_t i = entries; i > insert_pos; --i) {
                    const size_t dst =
                        40 + sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent);
                    memcpy(raw.data() + dst,
                           raw.data() + dst - sizeof(Ext4Extent),
                           sizeof(Ext4Extent));
                }
                const size_t off = 40 + sizeof(Ext4ExtentHeader) +
                                   insert_pos * sizeof(Ext4Extent);
                write_le_at<uint32_t>(raw, off, logical);
                write_le_at<uint16_t>(raw, off + 4, static_cast<uint16_t>(len));
                write_le_at<uint16_t>(raw, off + 6,
                                      static_cast<uint16_t>(physical >> 32));
                write_le_at<uint32_t>(
                    raw, off + 8,
                    static_cast<uint32_t>(physical & 0xFFFFFFFFULL));
                write_le_at<uint16_t>(raw, 42,
                                      static_cast<uint16_t>(entries + 1));
                return write_inode_raw(inode_id, raw);
            }

            // leaf full — split: allocate two new leaf blocks
            const uint16_t total = static_cast<uint16_t>(entries + 1);
            const uint16_t half  = total / 2;

            struct NewEntry {
                uint32_t block;
                uint32_t len;
                uint64_t phys;
            };
            std::vector<NewEntry> sorted;
            bool inserted = false;
            for (uint16_t i = 0; i < entries; ++i) {
                const uint32_t eb = read_le<uint32_t>(&extents[i].ee_block);
                if (!inserted && logical < eb) {
                    sorted.push_back({logical, len, physical});
                    inserted = true;
                }
                sorted.push_back(
                    {eb, extent_len(read_le<uint16_t>(&extents[i].ee_len)),
                     (static_cast<uint64_t>(
                          read_le<uint16_t>(&extents[i].ee_start_hi))
                      << 32) |
                         read_le<uint32_t>(&extents[i].ee_start_lo)});
            }
            if (!inserted) {
                sorted.push_back({logical, len, physical});
            }

            auto blk_a = alloc_block();
            propagate(blk_a);
            auto blk_b = alloc_block();
            if (!blk_b.has_value()) {
                free_block(blk_a.value());
                propagate_return(blk_b);
            }

            std::vector<byte> leaf_a(_block_size, 0);
            std::vector<byte> leaf_b(_block_size, 0);
            auto *ha = reinterpret_cast<Ext4ExtentHeader *>(leaf_a.data());
            auto *hb = reinterpret_cast<Ext4ExtentHeader *>(leaf_b.data());
            write_le_at<uint16_t>(leaf_a, 0, EXT4_EXT_MAGIC);
            write_le_at<uint16_t>(leaf_a, 4, max_entries);
            write_le_at<uint16_t>(leaf_a, 6, 0);
            write_le_at<uint16_t>(leaf_b, 0, EXT4_EXT_MAGIC);
            write_le_at<uint16_t>(leaf_b, 4, max_entries);
            write_le_at<uint16_t>(leaf_b, 6, 0);

            auto *ea = reinterpret_cast<Ext4Extent *>(leaf_a.data() +
                                                      sizeof(Ext4ExtentHeader));
            auto *eb = reinterpret_cast<Ext4Extent *>(leaf_b.data() +
                                                      sizeof(Ext4ExtentHeader));

            for (uint16_t i = 0; i < half; ++i) {
                write_le_at<uint32_t>(
                    leaf_a, sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent),
                    sorted[i].block);
                write_le_at<uint16_t>(
                    leaf_a,
                    sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent) + 4,
                    static_cast<uint16_t>(sorted[i].len));
                write_le_at<uint16_t>(
                    leaf_a,
                    sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent) + 6,
                    static_cast<uint16_t>(sorted[i].phys >> 32));
                write_le_at<uint32_t>(
                    leaf_a,
                    sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent) + 8,
                    static_cast<uint32_t>(sorted[i].phys & 0xFFFFFFFFULL));
            }
            write_le_at<uint16_t>(leaf_a, 2, half);

            for (uint16_t i = half; i < total; ++i) {
                const uint16_t j = i - half;
                write_le_at<uint32_t>(
                    leaf_b, sizeof(Ext4ExtentHeader) + j * sizeof(Ext4Extent),
                    sorted[i].block);
                write_le_at<uint16_t>(
                    leaf_b,
                    sizeof(Ext4ExtentHeader) + j * sizeof(Ext4Extent) + 4,
                    static_cast<uint16_t>(sorted[i].len));
                write_le_at<uint16_t>(
                    leaf_b,
                    sizeof(Ext4ExtentHeader) + j * sizeof(Ext4Extent) + 6,
                    static_cast<uint16_t>(sorted[i].phys >> 32));
                write_le_at<uint32_t>(
                    leaf_b,
                    sizeof(Ext4ExtentHeader) + j * sizeof(Ext4Extent) + 8,
                    static_cast<uint32_t>(sorted[i].phys & 0xFFFFFFFFULL));
            }
            write_le_at<uint16_t>(leaf_b, 2,
                                  static_cast<uint16_t>(total - half));

            auto wa =
                write_fs_block(blk_a.value(), leaf_a.data(), leaf_a.size());
            propagate(wa);
            auto wb =
                write_fs_block(blk_b.value(), leaf_b.data(), leaf_b.size());
            if (!wb.has_value()) {
                free_block(blk_b.value());
                propagate_return(wb);
            }

            // convert inode's i_block to root index node
            write_le_at<uint16_t>(raw, 42, 2);
            write_le_at<uint16_t>(raw, 46, 1);
            auto *idx = reinterpret_cast<Ext4ExtentIdx *>(
                i_block + sizeof(Ext4ExtentHeader));
            write_le_at<uint32_t>(raw, 40 + sizeof(Ext4ExtentHeader),
                                  sorted[0].block);
            write_le_at<uint32_t>(
                raw, 40 + sizeof(Ext4ExtentHeader) + 4,
                static_cast<uint32_t>(blk_a.value() & 0xFFFFFFFFULL));
            write_le_at<uint16_t>(raw, 40 + sizeof(Ext4ExtentHeader) + 8,
                                  static_cast<uint16_t>(blk_a.value() >> 32));
            memset(raw.data() + 40 + sizeof(Ext4ExtentHeader) + 12, 0, 12);
            write_le_at<uint32_t>(raw, 40 + sizeof(Ext4ExtentHeader) + 12,
                                  sorted[half].block);
            write_le_at<uint32_t>(
                raw, 40 + sizeof(Ext4ExtentHeader) + 16,
                static_cast<uint32_t>(blk_b.value() & 0xFFFFFFFFULL));
            write_le_at<uint16_t>(raw, 40 + sizeof(Ext4ExtentHeader) + 20,
                                  static_cast<uint16_t>(blk_b.value() >> 32));
            return write_inode_raw(inode_id, raw);
        }

        const uint16_t idx_entries = read_le<uint16_t>(i_block + 2);
        auto *idxs                 = reinterpret_cast<Ext4ExtentIdx *>(
            i_block + sizeof(Ext4ExtentHeader));

        const Ext4ExtentIdx *target_idx = nullptr;
        for (uint16_t i = 0; i < idx_entries; ++i) {
            if (read_le<uint32_t>(&idxs[i].ei_block) > logical)
                break;
            target_idx = &idxs[i];
        }
        if (target_idx == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        const uint64_t leaf_blk =
            (static_cast<uint64_t>(read_le<uint16_t>(&target_idx->ei_leaf_hi))
             << 32) |
            read_le<uint32_t>(&target_idx->ei_leaf_lo);
        std::vector<byte> leaf(_block_size);
        auto read_lf = read_fs_block(leaf_blk, leaf.data(), leaf.size());
        propagate(read_lf);

        // collect intermediate nodes on the path (root→leaf) for split
        // propagation
        struct PathNode {
            uint64_t block;
            std::vector<byte> buf;
        };
        std::vector<PathNode> path;
        if (depth > 1) {
            path.push_back(PathNode{leaf_blk, leaf});
        }

        // descend intermediate nodes until we reach a leaf
        uint64_t cur_blk = leaf_blk;
        while (true) {
            auto *cur_eh = reinterpret_cast<Ext4ExtentHeader *>(leaf.data());
            uint16_t cur_depth = read_le<uint16_t>(&cur_eh->eh_depth);
            if (cur_depth == 0)
                break;

            uint16_t cur_entries = read_le<uint16_t>(&cur_eh->eh_entries);
            auto *cur_idxs       = reinterpret_cast<Ext4ExtentIdx *>(
                leaf.data() + sizeof(Ext4ExtentHeader));
            const Ext4ExtentIdx *chosen = nullptr;
            for (uint16_t j = 0; j < cur_entries; ++j) {
                if (read_le<uint32_t>(&cur_idxs[j].ei_block) > logical)
                    break;
                chosen = &cur_idxs[j];
            }
            if (chosen == nullptr) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            cur_blk =
                (static_cast<uint64_t>(read_le<uint16_t>(&chosen->ei_leaf_hi))
                 << 32) |
                read_le<uint32_t>(&chosen->ei_leaf_lo);
            auto rd = read_fs_block(cur_blk, leaf.data(), leaf.size());
            propagate(rd);
            if (read_le<uint16_t>(leaf.data() + 6) > 0) {
                path.push_back(PathNode{cur_blk, leaf});
            }
        }

        auto *lf_eh = reinterpret_cast<Ext4ExtentHeader *>(leaf.data());
        const uint16_t lf_entries = read_le<uint16_t>(&lf_eh->eh_entries);
        const uint16_t lf_max     = read_le<uint16_t>(&lf_eh->eh_max);
        auto *lf_exts             = reinterpret_cast<Ext4Extent *>(
            leaf.data() + sizeof(Ext4ExtentHeader));

        for (uint16_t i = 0; i < lf_entries; ++i) {
            const uint32_t first = read_le<uint32_t>(&lf_exts[i].ee_block);
            const uint16_t lr    = read_le<uint16_t>(&lf_exts[i].ee_len);
            const uint32_t elen  = extent_len(lr);
            const uint64_t start = (static_cast<uint64_t>(read_le<uint16_t>(
                                        &lf_exts[i].ee_start_hi))
                                    << 32) |
                                   read_le<uint32_t>(&lf_exts[i].ee_start_lo);

            if (logical + len == first &&
                physical + static_cast<uint64_t>(len) * _block_size == start)
            {
                write_le_at<uint32_t>(
                    leaf, sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent),
                    logical);
                write_le_at<uint16_t>(
                    leaf, sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent) + 4,
                    static_cast<uint16_t>(len + elen));
                return write_fs_block(cur_blk, leaf.data(), leaf.size());
            }
            if (first == logical + len &&
                start == physical + static_cast<uint64_t>(len) * _block_size)
            {
                write_le_at<uint16_t>(
                    leaf, sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent) + 4,
                    static_cast<uint16_t>(len + elen));
                return write_fs_block(cur_blk, leaf.data(), leaf.size());
            }
        }

        if (lf_entries < lf_max) {
            uint16_t ipos = lf_entries;
            for (uint16_t i = 0; i < lf_entries; ++i) {
                if (logical < read_le<uint32_t>(&lf_exts[i].ee_block)) {
                    ipos = i;
                    break;
                }
            }
            for (uint16_t i = lf_entries; i > ipos; --i) {
                const size_t d =
                    sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent);
                memcpy(leaf.data() + d, leaf.data() + d - sizeof(Ext4Extent),
                       sizeof(Ext4Extent));
            }
            const size_t off =
                sizeof(Ext4ExtentHeader) + ipos * sizeof(Ext4Extent);
            write_le_at<uint32_t>(leaf, off, logical);
            write_le_at<uint16_t>(leaf, off + 4, static_cast<uint16_t>(len));
            write_le_at<uint16_t>(leaf, off + 6,
                                  static_cast<uint16_t>(physical >> 32));
            write_le_at<uint32_t>(
                leaf, off + 8, static_cast<uint32_t>(physical & 0xFFFFFFFFULL));
            write_le_at<uint16_t>(leaf, 2,
                                  static_cast<uint16_t>(lf_entries + 1));
            return write_fs_block(cur_blk, leaf.data(), leaf.size());
        }

        // leaf full — prepare sorted extents and split
        const uint16_t total_lf = static_cast<uint16_t>(lf_entries + 1);
        const uint16_t half     = total_lf / 2;
        struct LfEntry {
            uint32_t block;
            uint32_t len;
            uint64_t phys;
        };
        std::vector<LfEntry> sorted;
        bool done = false;
        for (uint16_t i = 0; i < lf_entries; ++i) {
            const uint32_t eb = read_le<uint32_t>(&lf_exts[i].ee_block);
            if (!done && logical < eb) {
                sorted.push_back(LfEntry{logical, len, physical});
                done = true;
            }
            sorted.push_back(
                LfEntry{eb, extent_len(read_le<uint16_t>(&lf_exts[i].ee_len)),
                        (static_cast<uint64_t>(
                             read_le<uint16_t>(&lf_exts[i].ee_start_hi))
                         << 32) |
                            read_le<uint32_t>(&lf_exts[i].ee_start_lo)});
        }
        if (!done)
            sorted.push_back(LfEntry{logical, len, physical});

        auto new_block = alloc_block();
        propagate(new_block);
        std::vector<byte> new_leaf(_block_size, 0);
        write_le_at<uint16_t>(new_leaf, 0, EXT4_EXT_MAGIC);
        write_le_at<uint16_t>(new_leaf, 4, lf_max);
        for (uint16_t i = 0; i < half; ++i) {
            write_le_at<uint32_t>(
                leaf, sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent),
                sorted[i].block);
            write_le_at<uint16_t>(
                leaf, sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent) + 4,
                static_cast<uint16_t>(sorted[i].len));
            write_le_at<uint16_t>(
                leaf, sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent) + 6,
                static_cast<uint16_t>(sorted[i].phys >> 32));
            write_le_at<uint32_t>(
                leaf, sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent) + 8,
                static_cast<uint32_t>(sorted[i].phys & 0xFFFFFFFFULL));
        }
        write_le_at<uint16_t>(leaf, 2, half);
        for (uint16_t i = half; i < total_lf; ++i) {
            const uint16_t j = i - half;
            write_le_at<uint32_t>(
                new_leaf, sizeof(Ext4ExtentHeader) + j * sizeof(Ext4Extent),
                sorted[i].block);
            write_le_at<uint16_t>(
                new_leaf, sizeof(Ext4ExtentHeader) + j * sizeof(Ext4Extent) + 4,
                static_cast<uint16_t>(sorted[i].len));
            write_le_at<uint16_t>(
                new_leaf, sizeof(Ext4ExtentHeader) + j * sizeof(Ext4Extent) + 6,
                static_cast<uint16_t>(sorted[i].phys >> 32));
            write_le_at<uint32_t>(
                new_leaf, sizeof(Ext4ExtentHeader) + j * sizeof(Ext4Extent) + 8,
                static_cast<uint32_t>(sorted[i].phys & 0xFFFFFFFFULL));
        }
        write_le_at<uint16_t>(new_leaf, 2,
                              static_cast<uint16_t>(total_lf - half));

        auto w_old = write_fs_block(cur_blk, leaf.data(), leaf.size());
        propagate(w_old);
        auto w_new =
            write_fs_block(new_block.value(), new_leaf.data(), new_leaf.size());
        if (!w_new.has_value()) {
            free_block(new_block.value());
            propagate_return(w_new);
        }

        // propagate new leaf entry upward through the path
        uint32_t ins_first = sorted[half].block;
        uint64_t ins_phys  = new_block.value();

        while (!path.empty()) {
            PathNode pn = path.back();
            path.pop_back();
            uint64_t pn_blk = pn.block;

            auto *pn_eh = reinterpret_cast<Ext4ExtentHeader *>(pn.buf.data());
            uint16_t pn_entries = read_le<uint16_t>(&pn_eh->eh_entries);
            uint16_t pn_max     = read_le<uint16_t>(&pn_eh->eh_max);
            auto *pn_idxs       = reinterpret_cast<Ext4ExtentIdx *>(
                pn.buf.data() + sizeof(Ext4ExtentHeader));

            if (pn_entries < pn_max) {
                // room in this node — insert and done
                uint16_t ip = pn_entries;
                for (uint16_t i = 0; i < pn_entries; ++i) {
                    if (ins_first < read_le<uint32_t>(&pn_idxs[i].ei_block)) {
                        ip = i;
                        break;
                    }
                }
                for (uint16_t i = pn_entries; i > ip; --i) {
                    size_t d =
                        sizeof(Ext4ExtentHeader) + i * sizeof(Ext4ExtentIdx);
                    memcpy(pn.buf.data() + d,
                           pn.buf.data() + d - sizeof(Ext4ExtentIdx),
                           sizeof(Ext4ExtentIdx));
                }
                size_t o =
                    sizeof(Ext4ExtentHeader) + ip * sizeof(Ext4ExtentIdx);
                write_le_at<uint32_t>(pn.buf, o, ins_first);
                write_le_at<uint32_t>(
                    pn.buf, o + 4,
                    static_cast<uint32_t>(ins_phys & 0xFFFFFFFFULL));
                write_le_at<uint16_t>(pn.buf, o + 8,
                                      static_cast<uint16_t>(ins_phys >> 32));
                write_le_at<uint16_t>(pn.buf, 2,
                                      static_cast<uint16_t>(pn_entries + 1));
                return write_fs_block(pn_blk, pn.buf.data(), pn.buf.size());
            }

            // node full — split it
            struct IdxEntry {
                uint32_t first_block;
                uint64_t leaf_phys;
            };
            std::vector<IdxEntry> all;
            for (uint16_t i = 0; i < pn_entries; ++i) {
                all.push_back(
                    IdxEntry{read_le<uint32_t>(&pn_idxs[i].ei_block),
                             (static_cast<uint64_t>(
                                  read_le<uint16_t>(&pn_idxs[i].ei_leaf_hi))
                              << 32) |
                                 read_le<uint32_t>(&pn_idxs[i].ei_leaf_lo)});
            }
            {
                bool f = false;
                for (size_t i = 0; i < all.size(); ++i)
                    if (ins_first < all[i].first_block) {
                        all.insert(all.begin() + static_cast<long>(i),
                                   IdxEntry{ins_first, ins_phys});
                        f = true;
                        break;
                    }
                if (!f)
                    all.push_back(IdxEntry{ins_first, ins_phys});
            }
            size_t tot = all.size(), sp = (tot + 1) / 2;

            auto na = alloc_block();
            propagate(na);
            auto nb = alloc_block();
            if (!nb.has_value()) {
                free_block(na.value());
                propagate_return(nb);
            }

            auto write_ix_node = [&](uint64_t blk, size_t start, size_t cnt) {
                std::vector<byte> b(_block_size, 0);
                write_le_at<uint16_t>(b, 0, EXT4_EXT_MAGIC);
                write_le_at<uint16_t>(b, 2, static_cast<uint16_t>(cnt));
                write_le_at<uint16_t>(b, 4, blk_max_entries);
                write_le_at<uint16_t>(b, 6, 1);
                for (size_t i = 0; i < cnt; ++i) {
                    size_t o =
                        sizeof(Ext4ExtentHeader) + i * sizeof(Ext4ExtentIdx);
                    write_le_at<uint32_t>(b, o, all[start + i].first_block);
                    write_le_at<uint32_t>(
                        b, o + 4,
                        static_cast<uint32_t>(all[start + i].leaf_phys &
                                              0xFFFFFFFFULL));
                    write_le_at<uint16_t>(
                        b, o + 8,
                        static_cast<uint16_t>(all[start + i].leaf_phys >> 32));
                }
                auto wr = write_fs_block(blk, b.data(), b.size());
                propagate(wr);
            };
            write_ix_node(na.value(), 0, sp);
            write_ix_node(nb.value(), sp, tot - sp);

            // update parent pointer from old node → new node a
            if (path.empty()) {
                // parent is root i_block
                for (uint16_t i = 0; i < idx_entries; ++i) {
                    if (read_le<uint32_t>(&idxs[i].ei_leaf_lo) ==
                            (pn_blk & 0xFFFFFFFFULL) &&
                        read_le<uint16_t>(&idxs[i].ei_leaf_hi) ==
                            (static_cast<uint16_t>(pn_blk >> 32)))
                    {
                        write_le_at<uint32_t>(
                            raw,
                            40 + sizeof(Ext4ExtentHeader) +
                                i * sizeof(Ext4ExtentIdx) + 4,
                            static_cast<uint32_t>(na.value() & 0xFFFFFFFFULL));
                        write_le_at<uint16_t>(
                            raw,
                            40 + sizeof(Ext4ExtentHeader) +
                                i * sizeof(Ext4ExtentIdx) + 8,
                            static_cast<uint16_t>(na.value() >> 32));
                        break;
                    }
                }
            } else {
                // parent is another path node
                PathNode &pp  = path.back();
                auto *pp_idxs = reinterpret_cast<Ext4ExtentIdx *>(
                    pp.buf.data() + sizeof(Ext4ExtentHeader));
                uint16_t pp_entries = read_le<uint16_t>(pp.buf.data() + 2);
                for (uint16_t i = 0; i < pp_entries; ++i) {
                    if (read_le<uint32_t>(&pp_idxs[i].ei_leaf_lo) ==
                            (pn_blk & 0xFFFFFFFFULL) &&
                        read_le<uint16_t>(&pp_idxs[i].ei_leaf_hi) ==
                            (static_cast<uint16_t>(pn_blk >> 32)))
                    {
                        write_le_at<uint32_t>(
                            pp.buf,
                            sizeof(Ext4ExtentHeader) +
                                i * sizeof(Ext4ExtentIdx) + 4,
                            static_cast<uint32_t>(na.value() & 0xFFFFFFFFULL));
                        write_le_at<uint16_t>(
                            pp.buf,
                            sizeof(Ext4ExtentHeader) +
                                i * sizeof(Ext4ExtentIdx) + 8,
                            static_cast<uint16_t>(na.value() >> 32));
                        break;
                    }
                }
            }

            // push the second half's entry upward
            ins_first = all[sp].first_block;
            ins_phys  = nb.value();
        }

        if (idx_entries >= max_entries) {
            // root index full — promote to depth=2
            struct IdxEntry {
                uint32_t first_block;
                uint64_t leaf_phys;
            };
            std::vector<IdxEntry> idx_all;
            for (uint16_t i = 0; i < idx_entries; ++i) {
                idx_all.push_back(
                    IdxEntry{read_le<uint32_t>(&idxs[i].ei_block),
                             (static_cast<uint64_t>(
                                  read_le<uint16_t>(&idxs[i].ei_leaf_hi))
                              << 32) |
                                 read_le<uint32_t>(&idxs[i].ei_leaf_lo)});
            }
            {
                uint32_t nb = ins_first;
                bool ins    = false;
                for (size_t i = 0; i < idx_all.size(); ++i) {
                    if (nb < idx_all[i].first_block) {
                        idx_all.insert(idx_all.begin() + static_cast<long>(i),
                                       IdxEntry{nb, ins_phys});
                        ins = true;
                        break;
                    }
                }
                if (!ins)
                    idx_all.push_back(IdxEntry{nb, ins_phys});
            }

            const size_t total_idx = idx_all.size();
            const size_t idx_half  = (total_idx + 1) / 2;

            auto ix_a = alloc_block();
            propagate(ix_a);
            auto ix_b = alloc_block();
            if (!ix_b.has_value()) {
                free_block(ix_a.value());
                propagate_return(ix_b);
            }

            std::vector<byte> ix_a_buf(_block_size, 0);
            std::vector<byte> ix_b_buf(_block_size, 0);
            write_le_at<uint16_t>(ix_a_buf, 0, EXT4_EXT_MAGIC);
            write_le_at<uint16_t>(ix_a_buf, 2, static_cast<uint16_t>(idx_half));
            write_le_at<uint16_t>(ix_a_buf, 4, blk_max_entries);
            write_le_at<uint16_t>(ix_a_buf, 6, 1);
            for (size_t i = 0; i < idx_half; ++i) {
                size_t off =
                    sizeof(Ext4ExtentHeader) + i * sizeof(Ext4ExtentIdx);
                write_le_at<uint32_t>(ix_a_buf, off, idx_all[i].first_block);
                write_le_at<uint32_t>(
                    ix_a_buf, off + 4,
                    static_cast<uint32_t>(idx_all[i].leaf_phys &
                                          0xFFFFFFFFULL));
                write_le_at<uint16_t>(
                    ix_a_buf, off + 8,
                    static_cast<uint16_t>(idx_all[i].leaf_phys >> 32));
            }

            const size_t rem_b = total_idx - idx_half;
            write_le_at<uint16_t>(ix_b_buf, 0, EXT4_EXT_MAGIC);
            write_le_at<uint16_t>(ix_b_buf, 2, static_cast<uint16_t>(rem_b));
            write_le_at<uint16_t>(ix_b_buf, 4, blk_max_entries);
            write_le_at<uint16_t>(ix_b_buf, 6, 1);
            for (size_t i = 0; i < rem_b; ++i) {
                size_t off =
                    sizeof(Ext4ExtentHeader) + i * sizeof(Ext4ExtentIdx);
                write_le_at<uint32_t>(ix_b_buf, off,
                                      idx_all[idx_half + i].first_block);
                write_le_at<uint32_t>(
                    ix_b_buf, off + 4,
                    static_cast<uint32_t>(idx_all[idx_half + i].leaf_phys &
                                          0xFFFFFFFFULL));
                write_le_at<uint16_t>(
                    ix_b_buf, off + 8,
                    static_cast<uint16_t>(idx_all[idx_half + i].leaf_phys >>
                                          32));
            }

            auto w_ix_a =
                write_fs_block(ix_a.value(), ix_a_buf.data(), ix_a_buf.size());
            propagate(w_ix_a);
            auto w_ix_b =
                write_fs_block(ix_b.value(), ix_b_buf.data(), ix_b_buf.size());
            if (!w_ix_b.has_value()) {
                free_block(ix_b.value());
                propagate_return(w_ix_b);
            }

            write_le_at<uint16_t>(raw, 42, 2);
            write_le_at<uint16_t>(raw, 46, 2);
            size_t e0 = 40 + sizeof(Ext4ExtentHeader);
            write_le_at<uint32_t>(raw, e0, idx_all[0].first_block);
            write_le_at<uint32_t>(
                raw, e0 + 4,
                static_cast<uint32_t>(ix_a.value() & 0xFFFFFFFFULL));
            write_le_at<uint16_t>(raw, e0 + 8,
                                  static_cast<uint16_t>(ix_a.value() >> 32));
            size_t e1 = e0 + sizeof(Ext4ExtentIdx);
            write_le_at<uint32_t>(raw, e1, idx_all[idx_half].first_block);
            write_le_at<uint32_t>(
                raw, e1 + 4,
                static_cast<uint32_t>(ix_b.value() & 0xFFFFFFFFULL));
            write_le_at<uint16_t>(raw, e1 + 8,
                                  static_cast<uint16_t>(ix_b.value() >> 32));
            return write_inode_raw(inode_id, raw);
        }

        uint16_t ins_pos = idx_entries;
        for (uint16_t i = 0; i < idx_entries; ++i) {
            if (ins_first < read_le<uint32_t>(&idxs[i].ei_block)) {
                ins_pos = i;
                break;
            }
        }
        for (uint16_t i = idx_entries; i > ins_pos; --i) {
            const size_t d =
                40 + sizeof(Ext4ExtentHeader) + i * sizeof(Ext4ExtentIdx);
            memcpy(raw.data() + d, raw.data() + d - sizeof(Ext4ExtentIdx),
                   sizeof(Ext4ExtentIdx));
        }
        const size_t idx_off =
            40 + sizeof(Ext4ExtentHeader) + ins_pos * sizeof(Ext4ExtentIdx);
        write_le_at<uint32_t>(raw, idx_off, ins_first);
        write_le_at<uint32_t>(raw, idx_off + 4,
                              static_cast<uint32_t>(ins_phys & 0xFFFFFFFFULL));
        write_le_at<uint16_t>(raw, idx_off + 8,
                              static_cast<uint16_t>(ins_phys >> 32));
        write_le_at<uint16_t>(raw, 42, static_cast<uint16_t>(idx_entries + 1));
        return write_inode_raw(inode_id, raw);
    }

    Result<void> Ext4Superblock::update_inode_size(inode_t inode_id,
                                                   uint64_t new_size) {
        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        write_le_at<uint32_t>(raw_res.value(), 4,
                              static_cast<uint32_t>(new_size & 0xFFFFFFFFULL));
        write_le_at<uint32_t>(raw_res.value(), 108,
                              static_cast<uint32_t>(new_size >> 32));
        uint32_t now = _next_time();
        write_le_at<uint32_t>(raw_res.value(), 12, now);
        write_le_at<uint32_t>(raw_res.value(), 16, now);
        return write_inode_raw(inode_id, raw_res.value());
    }

    Result<void> Ext4Superblock::insert_dir_entry(inode_t parent_inode,
                                                  inode_t child_inode,
                                                  std::string_view name,
                                                  uint8_t file_type) {
        auto name_res = validate_entry_name(name);
        propagate(name_res);

        auto raw_res = read_inode_raw(parent_inode);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        const uint16_t mode = read_le_at<uint16_t>(raw_res.value(), 0);
        if ((mode & EXT4_S_IFMT) != EXT4_S_IFDIR) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        const uint32_t flags = read_le_at<uint32_t>(raw_res.value(), 32);
        if ((flags & EXT4_INDEX_FL) != 0) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }

        auto size_res = inode_size(parent_inode);
        propagate(size_res);
        const uint64_t block_count =
            (size_res.value() + _block_size - 1) / _block_size;
        const uint16_t need_len = min_dir_rec_len(name.size());
        std::vector<byte> block(_block_size);

        for (uint64_t logical = 0; logical < block_count; ++logical) {
            auto phys_res =
                extent_lookup(parent_inode, static_cast<uint32_t>(logical));
            propagate(phys_res);
            if (!phys_res.value().mapped || phys_res.value().unwritten) {
                continue;
            }
            auto read_res = read_fs_block(phys_res.value().physical_block,
                                          block.data(), block.size());
            propagate(read_res);

            size_t off = 0;
            while (off + sizeof(Ext4DirEntry2) <= block.size()) {
                auto *dirent =
                    reinterpret_cast<Ext4DirEntry2 *>(block.data() + off);
                const uint32_t ino     = read_le<uint32_t>(&dirent->inode);
                const uint16_t rec_len = read_le<uint16_t>(&dirent->rec_len);
                const uint8_t name_len = dirent->name_len;
                if (rec_len < sizeof(Ext4DirEntry2) ||
                    off + rec_len > block.size() ||
                    name_len > rec_len - sizeof(Ext4DirEntry2))
                {
                    break;
                }

                if (ino == 0) {
                    if (rec_len >= need_len) {
                        write_le_at<uint32_t>(block, off, child_inode);
                        write_le_at<uint16_t>(block, off + 4, rec_len);
                        block[off + 6] = static_cast<uint8_t>(name.size());
                        block[off + 7] = file_type;
                        memcpy(block.data() + off + sizeof(Ext4DirEntry2),
                               name.data(), name.size());
                        auto write_res =
                            write_fs_block(phys_res.value().physical_block,
                                           block.data(), block.size());
                        propagate(write_res);
                        void_return();
                    }
                    off += rec_len;
                    continue;
                }

                const uint16_t actual_len = min_dir_rec_len(name_len);
                if (rec_len >= actual_len + need_len) {
                    const size_t new_off   = off + actual_len;
                    const uint16_t new_len = rec_len - actual_len;
                    write_le_at<uint16_t>(block, off + 4, actual_len);
                    write_le_at<uint32_t>(block, new_off, child_inode);
                    write_le_at<uint16_t>(block, new_off + 4, new_len);
                    block[new_off + 6] = static_cast<uint8_t>(name.size());
                    block[new_off + 7] = file_type;
                    memcpy(block.data() + new_off + sizeof(Ext4DirEntry2),
                           name.data(), name.size());
                    auto write_res =
                        write_fs_block(phys_res.value().physical_block,
                                       block.data(), block.size());
                    propagate(write_res);
                    void_return();
                }
                off += rec_len;
            }
        }

        auto new_block = alloc_block();
        propagate(new_block);
        auto insert_ext =
            insert_extent(parent_inode, static_cast<uint32_t>(block_count),
                          new_block.value(), 1);
        if (!insert_ext.has_value()) {
            free_block(new_block.value());
            propagate_return(insert_ext);
        }
        auto update_sz =
            update_inode_size(parent_inode, size_res.value() + _block_size);
        if (!update_sz.has_value()) {
            free_block(new_block.value());
            propagate_return(update_sz);
        }

        std::vector<byte> new_dir_block(_block_size, 0);
        write_le_at<uint32_t>(new_dir_block, 0, child_inode);
        write_le_at<uint16_t>(new_dir_block, 4,
                              static_cast<uint16_t>(_block_size));
        new_dir_block[6] = static_cast<uint8_t>(name.size());
        new_dir_block[7] = file_type;
        memcpy(new_dir_block.data() + sizeof(Ext4DirEntry2), name.data(),
               name.size());
        auto write_res = write_fs_block(new_block.value(), new_dir_block.data(),
                                        new_dir_block.size());
        if (!write_res.has_value()) {
            free_block(new_block.value());
            propagate_return(write_res);
        }
        void_return();
    }

    Result<inode_t> Ext4Superblock::create_file(inode_t parent_inode,
                                                std::string_view name) {
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto name_res = validate_entry_name(name);
        propagate(name_res);

        auto entries_res = read_directory(parent_inode);
        propagate(entries_res);
        for (const auto &entry : entries_res.value()) {
            if (entry.name == name) {
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
        }

        auto inode_res = alloc_file_inode();
        propagate(inode_res);
        const inode_t inode_id = inode_res.value();

        auto insert_res =
            insert_dir_entry(parent_inode, inode_id, name, EXT4_FT_REG_FILE);
        if (!insert_res.has_value()) {
            auto release_res = release_file_inode(inode_id);
            if (!release_res.has_value()) {
                loggers::VFS::WARN(
                    "Ext4 创建文件回滚 inode 失败: inode=%u err=%s",
                    static_cast<unsigned>(inode_id),
                    to_cstring(release_res.error()));
            }
            propagate_return(insert_res);
        }
        return inode_id;
    }

    Result<void> Ext4Superblock::create_link(inode_t parent_inode,
                                             std::string_view name,
                                             inode_t target_inode) {
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto name_res = validate_entry_name(name);
        propagate(name_res);

        auto entries_res = read_directory(parent_inode);
        propagate(entries_res);
        for (const auto &entry : entries_res.value()) {
            if (entry.name == name) {
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
        }

        auto raw_res = read_inode_raw(target_inode);
        propagate(raw_res);
        auto mode = read_le_at<uint16_t>(raw_res.value(), 0);
        if ((mode & EXT4_S_IFMT) != EXT4_S_IFREG) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        uint16_t lc = read_le_at<uint16_t>(raw_res.value(), 26);
        if (lc == 0 || lc >= EXT4_LINK_MAX) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        write_le_at<uint16_t>(raw_res.value(), 26,
                              static_cast<uint16_t>(lc + 1));
        write_le_at<uint32_t>(raw_res.value(), 12, _next_time());
        auto w = write_inode_raw(target_inode, raw_res.value());
        if (!w.has_value())
            propagate_return(w);

        auto ins = insert_dir_entry(parent_inode, target_inode, name,
                                    EXT4_FT_REG_FILE);
        if (!ins.has_value()) {
            write_le_at<uint16_t>(raw_res.value(), 26, lc);
            write_inode_raw(target_inode, raw_res.value());
            propagate_return(ins);
        }
        void_return();
    }

    Result<inode_t> Ext4Superblock::create_directory(inode_t parent_inode,
                                                     std::string_view name) {
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto name_res = validate_entry_name(name);
        propagate(name_res);

        auto entries_res = read_directory(parent_inode);
        propagate(entries_res);
        for (const auto &entry : entries_res.value()) {
            if (entry.name == name) {
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
        }

        auto inode_res = alloc_file_inode();
        propagate(inode_res);
        const inode_t inode_id = inode_res.value();

        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        write_le_at<uint16_t>(raw_res.value(), 0, EXT4_S_IFDIR | 0755U);
        write_le_at<uint16_t>(raw_res.value(), 26, 2);
        auto write_inode_res = write_inode_raw(inode_id, raw_res.value());
        if (!write_inode_res.has_value()) {
            auto release_res = release_file_inode(inode_id);
            if (!release_res.has_value()) {
                loggers::VFS::WARN(
                    "Ext4 创建目录回滚 inode 失败: inode=%u err=%s",
                    static_cast<unsigned>(inode_id),
                    to_cstring(release_res.error()));
            }
            propagate_return(write_inode_res);
        }

        auto block_res = alloc_block();
        if (!block_res.has_value()) {
            auto release_res = release_file_inode(inode_id);
            if (!release_res.has_value()) {
                loggers::VFS::WARN(
                    "Ext4 创建目录回滚 inode 失败: inode=%u err=%s",
                    static_cast<unsigned>(inode_id),
                    to_cstring(release_res.error()));
            }
            propagate_return(block_res);
        }
        const uint64_t block_no = block_res.value();

        auto insert_ext_res = insert_extent(inode_id, 0, block_no, 1);
        if (!insert_ext_res.has_value()) {
            free_block(block_no);
            auto release_res = release_file_inode(inode_id);
            if (!release_res.has_value()) {
                loggers::VFS::WARN(
                    "Ext4 创建目录回滚 inode 失败: inode=%u err=%s",
                    static_cast<unsigned>(inode_id),
                    to_cstring(release_res.error()));
            }
            propagate_return(insert_ext_res);
        }
        auto update_res = update_inode_size(inode_id, _block_size);
        propagate(update_res);

        std::vector<byte> dir_block(_block_size, 0);
        write_le_at<uint32_t>(dir_block, 0, inode_id);
        write_le_at<uint16_t>(dir_block, 4, min_dir_rec_len(1));
        dir_block[6] = 1;
        dir_block[7] = EXT4_FT_DIR;
        memcpy(dir_block.data() + sizeof(Ext4DirEntry2), ".", 2);

        const uint16_t remaining =
            static_cast<uint16_t>(_block_size) - min_dir_rec_len(1);
        write_le_at<uint32_t>(dir_block, min_dir_rec_len(1), parent_inode);
        write_le_at<uint16_t>(dir_block, min_dir_rec_len(1) + 4, remaining);
        dir_block[min_dir_rec_len(1) + 6] = 2;
        dir_block[min_dir_rec_len(1) + 7] = EXT4_FT_DIR;
        memcpy(dir_block.data() + min_dir_rec_len(1) + sizeof(Ext4DirEntry2),
               "..", 3);

        auto write_dir_res =
            write_fs_block(block_no, dir_block.data(), dir_block.size());
        if (!write_dir_res.has_value()) {
            auto release_res = release_file_inode(inode_id);
            if (!release_res.has_value()) {
                loggers::VFS::WARN(
                    "Ext4 创建目录回滚 inode 失败: inode=%u err=%s",
                    static_cast<unsigned>(inode_id),
                    to_cstring(release_res.error()));
            }
            propagate_return(write_dir_res);
        }

        auto insert_res =
            insert_dir_entry(parent_inode, inode_id, name, EXT4_FT_DIR);
        if (!insert_res.has_value()) {
            auto release_res = release_file_inode(inode_id);
            if (!release_res.has_value()) {
                loggers::VFS::WARN(
                    "Ext4 创建目录回滚 inode 失败: inode=%u err=%s",
                    static_cast<unsigned>(inode_id),
                    to_cstring(release_res.error()));
            }
            propagate_return(insert_res);
        }
        return inode_id;
    }

    Result<inode_t> Ext4Superblock::create_symlink(inode_t parent_inode,
                                                   std::string_view name,
                                                   std::string_view target) {
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto name_res = validate_entry_name(name);
        propagate(name_res);
        if (target.empty() || target.size() > 255) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto entries_res = read_directory(parent_inode);
        propagate(entries_res);
        for (const auto &entry : entries_res.value()) {
            if (entry.name == name) {
                unexpect_return(ErrCode::KEY_DUPLICATED);
            }
        }

        auto inode_res = alloc_file_inode();
        propagate(inode_res);
        const inode_t inode_id = inode_res.value();

        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        write_le_at<uint16_t>(raw_res.value(), 0, EXT4_S_IFLNK | 0777U);
        write_le_at<uint32_t>(raw_res.value(), 4,
                              static_cast<uint32_t>(target.size()));

        if (target.size() <= 60) {
            memset(raw_res.value().data() + 40, 0, 60);
            memcpy(raw_res.value().data() + 40, target.data(), target.size());
            auto write_res = write_inode_raw(inode_id, raw_res.value());
            if (!write_res.has_value()) {
                auto release_res = release_file_inode(inode_id);
                if (!release_res.has_value()) {
                    loggers::VFS::WARN("Ext4 symlink rollback fail inode=%u",
                                       static_cast<unsigned>(inode_id));
                }
                propagate_return(write_res);
            }
        } else {
            auto write_res = write_inode_raw(inode_id, raw_res.value());
            if (!write_res.has_value()) {
                auto release_res = release_file_inode(inode_id);
                if (!release_res.has_value()) {
                    loggers::VFS::WARN("Ext4 symlink rollback fail inode=%u",
                                       static_cast<unsigned>(inode_id));
                }
                propagate_return(write_res);
            }

            auto block_res = alloc_block();
            if (!block_res.has_value()) {
                auto release_res = release_file_inode(inode_id);
                propagate_return(block_res);
            }
            const uint64_t block_no = block_res.value();

            auto insert_res = insert_extent(inode_id, 0, block_no, 1);
            if (!insert_res.has_value()) {
                free_block(block_no);
                auto release_res = release_file_inode(inode_id);
                propagate_return(insert_res);
            }

            std::vector<byte> blk(_block_size, 0);
            memcpy(blk.data(), target.data(), target.size());
            auto write_blk_res =
                write_fs_block(block_no, blk.data(), blk.size());
            if (!write_blk_res.has_value()) {
                free_block(block_no);
                auto release_res = release_file_inode(inode_id);
                propagate_return(write_blk_res);
            }
        }

        auto insert_res =
            insert_dir_entry(parent_inode, inode_id, name, EXT4_FT_SYMLINK);
        if (!insert_res.has_value()) {
            if (target.size() > 60) {
                auto phys_res = extent_lookup(inode_id, 0);
                if (phys_res.has_value() && phys_res.value().mapped) {
                    free_block(phys_res.value().physical_block);
                }
            }
            auto release_res = release_file_inode(inode_id);
            propagate_return(insert_res);
        }
        return inode_id;
    }

    Result<size_t> Ext4Superblock::read_inode_data(inode_t inode_id,
                                                   uint64_t offset, void *buf,
                                                   size_t len) {
        if (len == 0) {
            return 0;
        }
        if (buf == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto size_res = inode_size(inode_id);
        propagate(size_res);
        if (offset >= size_res.value()) {
            return 0;
        }

        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        const uint16_t mode = read_le_at<uint16_t>(raw_res.value(), 0);
        if ((mode & EXT4_S_IFMT) == EXT4_S_IFLNK && size_res.value() <= 60) {
            const size_t readable = static_cast<size_t>(
                std::min<uint64_t>(len, size_res.value() - offset));
            memcpy(buf, raw_res.value().data() + 40 + offset, readable);
            return readable;
        }

        const size_t readable = static_cast<size_t>(
            std::min<uint64_t>(len, size_res.value() - offset));
        auto *out   = static_cast<byte *>(buf);
        size_t done = 0;
        while (done < readable) {
            const uint64_t abs     = offset + done;
            const uint32_t logical = static_cast<uint32_t>(abs / _block_size);
            const size_t block_off = static_cast<size_t>(abs % _block_size);
            const size_t chunk     = std::min(
                readable - done, static_cast<size_t>(_block_size) - block_off);
            auto phys_res = extent_lookup(inode_id, logical);
            propagate(phys_res);
            if (!phys_res.value().mapped || phys_res.value().unwritten) {
                memset(out + done, 0, chunk);
            } else {
                auto read_res = read_device_bytes(
                    phys_res.value().physical_block * _block_size + block_off,
                    out + done, chunk);
                propagate(read_res);
            }
            done += chunk;
        }
        return done;
    }

    Result<size_t> Ext4Superblock::write_inode_data(inode_t inode_id,
                                                    uint64_t offset,
                                                    const void *buf,
                                                    size_t len) {
        if (len == 0) {
            return 0;
        }
        if (buf == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto mode_res = inode_mode(inode_id);
        propagate(mode_res);
        const uint16_t type = mode_res.value() & EXT4_S_IFMT;
        if (type != EXT4_S_IFREG && type != EXT4_S_IFLNK) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto size_res = inode_size(inode_id);
        propagate(size_res);
        uint64_t current_size = size_res.value();

        // fast symlink: write directly into i_block
        if (type == EXT4_S_IFLNK && current_size <= 60) {
            uint64_t wlen = std::min<uint64_t>(len, 60 - offset);
            auto raw_res  = read_inode_raw(inode_id);
            propagate(raw_res);
            memcpy(raw_res.value().data() + 40 + offset, buf,
                   static_cast<size_t>(wlen));
            if (offset + wlen > current_size) {
                write_le_at<uint32_t>(raw_res.value(), 4,
                                      static_cast<uint32_t>(offset + wlen));
            }
            auto w = write_inode_raw(inode_id, raw_res.value());
            propagate(w);
            return static_cast<size_t>(wlen);
        }

        const uint64_t end_offset = offset + len;
        if (end_offset > current_size) {
            const uint64_t grow_start =
                (current_size + _block_size - 1) / _block_size;
            const uint64_t grow_end =
                (end_offset + _block_size - 1) / _block_size;
            for (uint64_t logical = grow_start; logical < grow_end; ++logical) {
                auto phys_res =
                    extent_lookup(inode_id, static_cast<uint32_t>(logical));
                propagate(phys_res);
                if (phys_res.value().mapped) {
                    continue;
                }
                auto block_res = alloc_block();
                propagate(block_res);
                auto insert_res =
                    insert_extent(inode_id, static_cast<uint32_t>(logical),
                                  block_res.value(), 1);
                propagate(insert_res);
            }
            auto update_res = update_inode_size(inode_id, end_offset);
            propagate(update_res);
        }

        const auto *in = static_cast<const byte *>(buf);
        size_t done    = 0;
        while (done < len) {
            const uint64_t abs     = offset + done;
            const uint32_t logical = static_cast<uint32_t>(abs / _block_size);
            const size_t block_off = static_cast<size_t>(abs % _block_size);
            const size_t chunk     = std::min(
                len - done, static_cast<size_t>(_block_size) - block_off);
            auto phys_res = extent_lookup(inode_id, logical);
            propagate(phys_res);
            if (!phys_res.value().mapped || phys_res.value().unwritten) {
                unexpect_return(ErrCode::NOT_SUPPORTED);
            }
            auto write_res = write_device_bytes(
                phys_res.value().physical_block * _block_size + block_off,
                in + done, chunk);
            propagate(write_res);
            done += chunk;
        }
        return done;
    }

    Result<std::vector<Ext4DirEntry>> Ext4Superblock::read_directory(
        inode_t inode_id) {
        auto mode_res = inode_mode(inode_id);
        propagate(mode_res);
        if ((mode_res.value() & EXT4_S_IFMT) != EXT4_S_IFDIR) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto size_res = inode_size(inode_id);
        propagate(size_res);

        std::vector<Ext4DirEntry> entries{};
        std::vector<byte> block(_block_size);
        const uint64_t block_count =
            (size_res.value() + _block_size - 1) / _block_size;
        for (uint64_t logical = 0; logical < block_count; ++logical) {
            auto phys_res =
                extent_lookup(inode_id, static_cast<uint32_t>(logical));
            propagate(phys_res);
            if (!phys_res.value().mapped || phys_res.value().unwritten) {
                continue;
            }
            auto read_res = read_fs_block(phys_res.value().physical_block,
                                          block.data(), block.size());
            propagate(read_res);

            size_t off = 0;
            while (off + sizeof(Ext4DirEntry2) <= block.size()) {
                auto *dirent =
                    reinterpret_cast<const Ext4DirEntry2 *>(block.data() + off);
                const uint32_t ino      = read_le<uint32_t>(&dirent->inode);
                const uint16_t rec_len  = read_le<uint16_t>(&dirent->rec_len);
                const uint8_t name_len  = dirent->name_len;
                const uint8_t file_type = dirent->file_type;
                if (rec_len < sizeof(Ext4DirEntry2) ||
                    off + rec_len > block.size() ||
                    name_len > rec_len - sizeof(Ext4DirEntry2))
                {
                    break;
                }

                if (ino != 0 && name_len != 0) {
                    const char *name = reinterpret_cast<const char *>(
                        block.data() + off + sizeof(Ext4DirEntry2));
                    std::string entry_name(name, name_len);
                    if (entry_name != "." && entry_name != "..") {
                        entries.push_back(Ext4DirEntry{
                            .inode_id = static_cast<inode_t>(ino),
                            .name     = entry_name,
                        });
                    }
                }
                off += rec_len;
            }
        }
        return entries;
    }

    IFsDriver &Ext4Superblock::fs() {
        return *_fs;
    }

    Result<void> Ext4Superblock::delete_file(inode_t inode_id) {
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto size_res = inode_size(inode_id);
        propagate(size_res);
        auto mode_res = inode_mode(inode_id);
        propagate(mode_res);
        const uint16_t type = mode_res.value() & EXT4_S_IFMT;
        if (type == EXT4_S_IFDIR) {
            auto entries_res = read_directory(inode_id);
            propagate(entries_res);
            for (const auto &entry : entries_res.value()) {
                if (entry.name != "." && entry.name != "..") {
                    unexpect_return(ErrCode::OUT_OF_BOUNDARY);
                }
            }
        }

        const uint64_t block_count =
            (size_res.value() + _block_size - 1) / _block_size;
        for (uint64_t logical = 0; logical < block_count; ++logical) {
            auto phys_res =
                extent_lookup(inode_id, static_cast<uint32_t>(logical));
            if (!phys_res.has_value())
                continue;
            if (!phys_res.value().mapped)
                continue;
            auto free_res = free_block(phys_res.value().physical_block);
            if (!free_res.has_value()) {
                loggers::VFS::WARN(
                    "Ext4 delete_file free block failed: block=%lu",
                    (unsigned long)phys_res.value().physical_block);
            }
        }

        auto raw_res = read_inode_raw(inode_id);
        propagate(raw_res);
        auto &raw            = raw_res.value();
        const uint16_t depth = read_le<uint16_t>(raw.data() + 46);
        if (depth > 0) {
            const uint16_t idx_entries = read_le<uint16_t>(raw.data() + 42);
            const size_t idx_base      = 40 + sizeof(Ext4ExtentHeader);
            for (uint16_t i = 0; i < idx_entries; ++i) {
                const size_t off = idx_base + i * sizeof(Ext4ExtentIdx);
                const uint64_t child_block =
                    (static_cast<uint64_t>(
                         read_le<uint16_t>(raw.data() + off + 8))
                     << 32) |
                    read_le<uint32_t>(raw.data() + off + 4);
                if (child_block == 0)
                    continue;

                if (depth > 1) {
                    // child is intermediate index node; free its leaves first
                    std::vector<byte> cb(_block_size);
                    auto rd = read_fs_block(child_block, cb.data(), cb.size());
                    if (rd.has_value()) {
                        auto *ch = reinterpret_cast<const Ext4ExtentHeader *>(
                            cb.data());
                        uint16_t ce = read_le<uint16_t>(&ch->eh_entries);
                        auto *ci    = reinterpret_cast<const Ext4ExtentIdx *>(
                            cb.data() + sizeof(Ext4ExtentHeader));
                        for (uint16_t j = 0; j < ce; ++j) {
                            uint64_t leaf =
                                (static_cast<uint64_t>(
                                     read_le<uint16_t>(&ci[j].ei_leaf_hi))
                                 << 32) |
                                read_le<uint32_t>(&ci[j].ei_leaf_lo);
                            if (leaf != 0)
                                free_block(leaf);
                        }
                    }
                }
                free_block(child_block);
            }
        }
        memset(raw.data() + 40, 0, 60);
        write_le_at<uint16_t>(raw_res.value(), 40, EXT4_EXT_MAGIC);
        write_le_at<uint16_t>(raw_res.value(), 44, 4);
        write_le_at<uint32_t>(raw_res.value(), 4, 0);
        write_le_at<uint32_t>(raw_res.value(), 108, 0);
        auto write_res = write_inode_raw(inode_id, raw_res.value());
        propagate(write_res);

        auto release_res = release_file_inode(inode_id);
        propagate(release_res);
        void_return();
    }

    Result<void> Ext4Superblock::remove_dir_entry(inode_t parent_inode,
                                                  std::string_view name) {
        if (_read_only) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto raw_res = read_inode_raw(parent_inode);
        propagate(raw_res);
        auto valid_res = validate_inode_raw(raw_res.value());
        propagate(valid_res);
        const uint16_t mode = read_le_at<uint16_t>(raw_res.value(), 0);
        if ((mode & EXT4_S_IFMT) != EXT4_S_IFDIR) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto size_res = inode_size(parent_inode);
        propagate(size_res);
        const uint64_t block_count =
            (size_res.value() + _block_size - 1) / _block_size;
        std::vector<byte> block(_block_size);

        for (uint64_t logical = 0; logical < block_count; ++logical) {
            auto phys_res =
                extent_lookup(parent_inode, static_cast<uint32_t>(logical));
            propagate(phys_res);
            if (!phys_res.value().mapped)
                continue;

            auto read_res = read_fs_block(phys_res.value().physical_block,
                                          block.data(), block.size());
            propagate(read_res);

            size_t off                = 0;
            Ext4DirEntry2 *prev_entry = nullptr;
            while (off + sizeof(Ext4DirEntry2) <= block.size()) {
                auto *dirent =
                    reinterpret_cast<Ext4DirEntry2 *>(block.data() + off);
                const uint32_t ino     = read_le<uint32_t>(&dirent->inode);
                const uint16_t rec_len = read_le<uint16_t>(&dirent->rec_len);
                const uint8_t nlen     = dirent->name_len;
                if (rec_len < sizeof(Ext4DirEntry2) ||
                    off + rec_len > block.size() ||
                    nlen > rec_len - sizeof(Ext4DirEntry2))
                {
                    break;
                }

                if (ino != 0 && nlen == name.size() &&
                    memcmp(block.data() + off + sizeof(Ext4DirEntry2),
                           name.data(), nlen) == 0)
                {
                    write_le_at<uint32_t>(block, off, 0);
                    if (prev_entry != nullptr) {
                        const size_t prev_off =
                            reinterpret_cast<const byte *>(prev_entry) -
                            block.data();
                        const uint16_t prev_rec_len =
                            read_le<uint16_t>(block.data() + prev_off + 4);
                        write_le_at<uint16_t>(
                            block, prev_off + 4,
                            static_cast<uint16_t>(prev_rec_len + rec_len));
                    }
                    return write_fs_block(phys_res.value().physical_block,
                                          block.data(), block.size());
                }

                prev_entry  = dirent;
                off        += rec_len;
            }
        }
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }

    Result<void> Ext4Superblock::delete_directory(inode_t inode_id) {
        return delete_file(inode_id);
    }

    Result<void> Ext4Superblock::truncate(inode_t inode_id, uint64_t new_size) {
        auto mode_res = inode_mode(inode_id);
        propagate(mode_res);
        const uint16_t type = mode_res.value() & EXT4_S_IFMT;
        if (type != EXT4_S_IFREG && type != EXT4_S_IFLNK) {
            unexpect_return(ErrCode::NOT_SUPPORTED);
        }
        auto size_res = inode_size(inode_id);
        propagate(size_res);
        const uint64_t old_size   = size_res.value();
        const uint64_t old_blocks = (old_size + _block_size - 1) / _block_size;
        const uint64_t new_blocks = (new_size + _block_size - 1) / _block_size;

        if (new_blocks < old_blocks) {
            for (uint64_t logical = new_blocks; logical < old_blocks; ++logical)
            {
                auto phys_res =
                    extent_lookup(inode_id, static_cast<uint32_t>(logical));
                if (phys_res.has_value() && phys_res.value().mapped) {
                    free_block(phys_res.value().physical_block);
                }
            }
        } else if (new_blocks > old_blocks) {
            for (uint64_t logical = old_blocks; logical < new_blocks; ++logical)
            {
                auto phys_res =
                    extent_lookup(inode_id, static_cast<uint32_t>(logical));
                if (phys_res.has_value() && phys_res.value().mapped) {
                    continue;
                }
                auto block_res = alloc_block();
                propagate(block_res);
                std::vector<byte> zero(_block_size, 0);
                auto write_res =
                    write_fs_block(block_res.value(), zero.data(), zero.size());
                if (!write_res.has_value()) {
                    free_block(block_res.value());
                    propagate_return(write_res);
                }
                auto insert_res =
                    insert_extent(inode_id, static_cast<uint32_t>(logical),
                                  block_res.value(), 1);
                if (!insert_res.has_value()) {
                    free_block(block_res.value());
                    propagate_return(insert_res);
                }
            }
        }
        return update_inode_size(inode_id, new_size);
    }

    Result<void> Ext4Superblock::rename(inode_t old_parent,
                                        std::string_view old_name,
                                        inode_t new_parent,
                                        std::string_view new_name) {
        auto entries_res = read_directory(old_parent);
        propagate(entries_res);
        inode_t target = 0;
        bool found     = false;
        for (const auto &entry : entries_res.value()) {
            if (entry.name == old_name) {
                target = entry.inode_id;
                found  = true;
                break;
            }
        }
        if (!found) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        if (old_parent == new_parent && old_name == new_name) {
            void_return();
        }

        auto mode_res = inode_mode(target);
        propagate(mode_res);
        const bool is_file = (mode_res.value() & EXT4_S_IFMT) != EXT4_S_IFDIR;

        if (old_parent == new_parent) {
            auto sz = inode_size(old_parent);
            propagate(sz);
            const uint64_t blk_cnt =
                (sz.value() + _block_size - 1) / _block_size;
            std::vector<byte> blk(_block_size);
            for (uint64_t lg = 0; lg < blk_cnt; ++lg) {
                auto phys =
                    extent_lookup(old_parent, static_cast<uint32_t>(lg));
                propagate(phys);
                if (!phys.value().mapped)
                    continue;
                auto rd = read_fs_block(phys.value().physical_block, blk.data(),
                                        _block_size);
                propagate(rd);
                size_t off = 0;
                while (off + sizeof(Ext4DirEntry2) <= _block_size) {
                    auto *de =
                        reinterpret_cast<Ext4DirEntry2 *>(blk.data() + off);
                    const uint32_t ino = read_le<uint32_t>(&de->inode);
                    const uint16_t rl  = read_le<uint16_t>(&de->rec_len);
                    const uint8_t nl   = de->name_len;
                    if (rl < sizeof(Ext4DirEntry2) || off + rl > _block_size ||
                        nl > rl - sizeof(Ext4DirEntry2))
                        break;
                    if (ino != 0 && nl == old_name.size() &&
                        memcmp(blk.data() + off + sizeof(Ext4DirEntry2),
                               old_name.data(), nl) == 0)
                    {
                        const uint16_t need = min_dir_rec_len(new_name.size());
                        if (need > rl) {
                            unexpect_return(ErrCode::NOT_SUPPORTED);
                        }
                        blk[off + 6] = static_cast<uint8_t>(new_name.size());
                        memset(blk.data() + off + sizeof(Ext4DirEntry2), 0, nl);
                        memcpy(blk.data() + off + sizeof(Ext4DirEntry2),
                               new_name.data(), new_name.size());
                        auto wr = write_fs_block(phys.value().physical_block,
                                                 blk.data(), _block_size);
                        propagate(wr);
                        auto sync_res = sync();
                        propagate(sync_res);
                        void_return();
                    }
                    off += rl;
                }
            }
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto insert_res =
            insert_dir_entry(new_parent, target, new_name,
                             is_file ? EXT4_FT_REG_FILE : EXT4_FT_DIR);
        if (!insert_res.has_value()) {
            propagate_return(insert_res);
        }

        auto remove_res = remove_dir_entry(old_parent, old_name);
        propagate(remove_res);

        auto sync_res = sync();
        propagate(sync_res);
        void_return();
    }

    Result<void> Ext4Superblock::sync() {
        if (_cache == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto future = _cache->sync_all();
        auto res    = wait::blocking_wait_for(future);
        propagate(res);
        void_return();
    }

    Result<inode_t> Ext4Superblock::root() {
        return EXT4_ROOT_INO;
    }

    Result<util::owner<IINode *>> Ext4Superblock::get_inode(inode_t inode_id) {
        auto mode_res = inode_mode(inode_id);
        if (!mode_res.has_value()) {
            loggers::VFS::ERROR("Ext4 get_inode mode 失败: inode=%u err=%s",
                                static_cast<unsigned>(inode_id),
                                to_cstring(mode_res.error()));
            propagate_return(mode_res);
        }
        const uint16_t type = mode_res.value() & EXT4_S_IFMT;
        if (type == EXT4_S_IFDIR) {
            auto *dir = new Ext4Directory(*this, inode_id);
            if (dir == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
            return util::owner<IINode *>(dir);
        }
        if (type == EXT4_S_IFLNK) {
            auto *symlink = new Ext4Symlink(*this, inode_id);
            if (symlink == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
            return util::owner<IINode *>(symlink);
        }
        if (type == EXT4_S_IFREG) {
            auto *file = new Ext4File(*this, inode_id);
            if (file == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
            return util::owner<IINode *>(file);
        }
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<bool> Ext4Superblock::is_symlink(inode_t inode_id) {
        auto mode_res = inode_mode(inode_id);
        propagate(mode_res);
        return (mode_res.value() & EXT4_S_IFMT) == EXT4_S_IFLNK;
    }

    Result<std::string> Ext4Superblock::readlink(inode_t inode_id) {
        auto symlink_res = is_symlink(inode_id);
        propagate(symlink_res);
        if (!symlink_res.value()) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        auto size_res = inode_size(inode_id);
        propagate(size_res);
        std::string target;
        target.resize(static_cast<size_t>(size_res.value()));
        if (target.empty()) {
            return target;
        }

        auto read_res =
            read_inode_data(inode_id, 0, target.data(), target.size());
        propagate(read_res);
        target.resize(read_res.value());
        return target;
    }

    Result<inode_t> Ext4Superblock::alloc_inode(INodeType type) {
        (void)type;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> Ext4Superblock::free_inode(inode_t id) {
        (void)id;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    IMetadata &Ext4Superblock::metadata() {
        return _metadata;
    }

    size_t Ext4Superblock::sb_id() const {
        return _sb_id;
    }

    const char *Ext4Driver::name() const {
        return "ext4";
    }

    Result<void> Ext4Driver::probe(size_t devno, const char *options) {
        auto mount_res = mount(devno, options);
        propagate(mount_res);
        auto unmount_res = unmount(mount_res.value().get());
        propagate(unmount_res);
        delete mount_res.value().get();
        void_return();
    }

    Result<util::owner<ISuperblock *>> Ext4Driver::mount(size_t devno,
                                                         const char *options) {
        (void)options;
        auto device_res = blk::BlkManager::inst().lookup(devno);
        propagate(device_res);
        auto cache_res = blk::BlkManager::inst().lookup_cache(devno);
        propagate(cache_res);
        auto *device = device_res.value();
        auto *cache  = cache_res.value();
        if (device == nullptr || cache == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto block_size_res = device->block_sz();
        propagate(block_size_res);
        auto block_count_res = device->block_cnt();
        propagate(block_count_res);

        auto *sb = new Ext4Superblock(*this, *cache, block_size_res.value(),
                                      block_count_res.value(), _next_sb_id++);
        if (sb == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto load_res = sb->load();
        if (!load_res.has_value()) {
            delete sb;
            propagate_return(load_res);
        }
        return util::owner<ISuperblock *>(sb);
    }

    Result<void> Ext4Driver::unmount(ISuperblock *sb) {
        (void)sb;
        void_return();
    }
}  // namespace ext4
