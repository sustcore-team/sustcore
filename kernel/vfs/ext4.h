/**
 * @file ext4.h
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief Ext4 block filesystem
 * @version alpha-1.0.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <bio/buffer.h>
#include <sus/types.h>
#include <vfs/ops.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ext4 {
    class Ext4Driver;
    class Ext4Superblock;

    struct Ext4Metadata final : public IMetadata {};

    struct Ext4DirEntry {
        inode_t inode_id = 0;
        std::string name {};
    };

    struct Ext4ExtentMapping {
        uint64_t physical_block = 0;
        bool mapped             = false;
        bool unwritten          = false;
    };

    class Ext4File final : public IFile {
    private:
        Ext4Superblock *_sb;
        inode_t _inode_id;
        Ext4Metadata _metadata {};

    public:
        Ext4File(Ext4Superblock &sb, inode_t inode_id) noexcept;
        ~Ext4File() final = default;

        [[nodiscard]]
        Result<size_t> read(off_t offset, void *buf, size_t len) final;
        [[nodiscard]]
        Result<size_t> write(off_t offset, const void *buf, size_t len) final;
        [[nodiscard]]
        Result<size_t> size() final;
        [[nodiscard]]
        Result<void> sync() final;
        [[nodiscard]]
        Result<void> truncate(size_t new_size) final;
        [[nodiscard]]
        IMetadata &metadata() final;
        [[nodiscard]]
        inode_t inode_id() const final;
        [[nodiscard]]
        INodeCachePolicy inode_cache() const final;
        [[nodiscard]]
        FileCachePolicy file_cache() const final;
        [[nodiscard]]
        Result<void> getattr(AttrSet &out) const final;
        [[nodiscard]]
        Result<void> setattr(AttrMask mask, const AttrSet &attrs) final;
    };

    class Ext4Symlink final : public ISymlink {
    private:
        Ext4Superblock *_sb;
        inode_t _inode_id;
        Ext4Metadata _metadata {};

    public:
        Ext4Symlink(Ext4Superblock &sb, inode_t inode_id) noexcept;
        ~Ext4Symlink() final = default;

        [[nodiscard]]
        Result<std::string> target() final;
        [[nodiscard]]
        IMetadata &metadata() final;
        [[nodiscard]]
        inode_t inode_id() const final;
        [[nodiscard]]
        INodeCachePolicy inode_cache() const final;
        [[nodiscard]]
        Result<void> getattr(AttrSet &out) const final;
        [[nodiscard]]
        Result<void> setattr(AttrMask mask, const AttrSet &attrs) final;
    };

    class Ext4Directory final : public IDirectory {
    private:
        Ext4Superblock *_sb;
        inode_t _inode_id;
        Ext4Metadata _metadata {};
        mutable bool _entries_cached = false;
        mutable std::vector<Ext4DirEntry> _cached_entries {};

        [[nodiscard]]
        Result<std::vector<Ext4DirEntry>> collect_entries();

    public:
        Ext4Directory(Ext4Superblock &sb, inode_t inode_id) noexcept;
        ~Ext4Directory() final = default;

        [[nodiscard]]
        Result<inode_t> lookup(std::string_view name) final;
        [[nodiscard]]
        Result<inode_t> mkfile(std::string_view name,
                               const char *options) final;
        [[nodiscard]]
        Result<inode_t> mkdir(std::string_view name,
                              const char *options) final;
        [[nodiscard]]
        Result<void> link(std::string_view name, inode_t target) final;
        [[nodiscard]]
        Result<size_t> entry_count() final;
        [[nodiscard]]
        Result<DirectoryEntryInfo> entry_at(size_t index) final;
        [[nodiscard]]
        Result<void> unlink(std::string_view name) final;
        [[nodiscard]]
        Result<void> rmdir(std::string_view name) final;
        [[nodiscard]]
        Result<inode_t> symlink(std::string_view name,
                                std::string_view target) final;
        [[nodiscard]]
        Result<void> rename(std::string_view old_name,
                            IDirectory &new_parent,
                            std::string_view new_name) final;
        [[nodiscard]]
        Result<void> sync() final;
        [[nodiscard]]
        IMetadata &metadata() final;
        [[nodiscard]]
        inode_t inode_id() const final;
        [[nodiscard]]
        INodeCachePolicy inode_cache() const final;
        [[nodiscard]]
        Result<void> getattr(AttrSet &out) const final;
        [[nodiscard]]
        Result<void> setattr(AttrMask mask, const AttrSet &attrs) final;
    };

    class Ext4Superblock final : public ISuperblock {
    private:
        Ext4Driver *_fs;
        blk::BufferCache *_cache;
        size_t _dev_block_size;
        size_t _dev_block_count;
        size_t _sb_id;
        Ext4Metadata _metadata {};

        std::vector<byte> _superblock;
        std::vector<byte> _group_desc;

        uint32_t _block_size        = 0;
        uint32_t _blocks_per_group  = 0;
        uint32_t _inodes_per_group  = 0;
        uint32_t _inode_count       = 0;
        uint16_t _inode_size        = 0;
        uint16_t _group_desc_size   = 0;
        uint32_t _feature_compat    = 0;
        uint32_t _feature_incompat  = 0;
        uint32_t _feature_ro_compat = 0;
        uint32_t _first_data_block  = 0;
        uint64_t _block_count       = 0;
        uint32_t _group_count       = 0;
        uint64_t _group_desc_offset = 0;
        bool _read_only             = false;
        uint32_t _time_counter      = 0;

        [[nodiscard]]
        uint32_t _next_time() { return ++_time_counter; }

        [[nodiscard]]
        Result<void> read_device_bytes(uint64_t offset, void *buf, size_t len);
        [[nodiscard]]
        Result<void> write_device_bytes(uint64_t offset, const void *buf,
                                        size_t len);
        [[nodiscard]]
        Result<void> read_fs_block(uint64_t block, void *buf, size_t len);
        [[nodiscard]]
        Result<void> write_fs_block(uint64_t block, const void *buf,
                                    size_t len);
        [[nodiscard]]
        Result<std::vector<byte>> read_inode_raw(inode_t inode_id);
        [[nodiscard]]
        Result<void> write_inode_raw(inode_t inode_id,
                                     const std::vector<byte> &raw);
        [[nodiscard]]
        Result<uint16_t> inode_mode(inode_t inode_id);
        [[nodiscard]]
        Result<uint64_t> inode_size(inode_t inode_id);
        [[nodiscard]]
        Result<void> validate_inode_raw(const std::vector<byte> &raw);
        [[nodiscard]]
        Result<uint64_t> block_bitmap_block(uint32_t group);
        [[nodiscard]]
        Result<uint32_t> group_free_blocks(uint32_t group);
        [[nodiscard]]
        Result<void> set_group_free_blocks(uint32_t group, uint32_t count);
        [[nodiscard]]
        Result<uint64_t> alloc_block();
        [[nodiscard]]
        Result<void> free_block(uint64_t block_no);
        [[nodiscard]]
        Result<uint64_t> inode_table_block(uint32_t group);
        [[nodiscard]]
        Result<uint64_t> inode_bitmap_block(uint32_t group);
        [[nodiscard]]
        Result<uint32_t> group_free_inodes(uint32_t group);
        [[nodiscard]]
        Result<void> set_group_free_inodes(uint32_t group, uint32_t count);
        [[nodiscard]]
        Result<void> sync_superblock_metadata();
        [[nodiscard]]
        Result<void> sync_group_descriptors();
        [[nodiscard]]
        Result<inode_t> alloc_file_inode();
        [[nodiscard]]
        Result<void> release_file_inode(inode_t inode_id);
        [[nodiscard]]
        Result<void> insert_dir_entry(inode_t parent_inode,
                                      inode_t child_inode,
                                      std::string_view name,
                                      uint8_t file_type);
        [[nodiscard]]
        Result<inode_t> create_file(inode_t parent_inode,
                                    std::string_view name);
        [[nodiscard]]
        Result<inode_t> create_directory(inode_t parent_inode,
                                         std::string_view name);
        [[nodiscard]]
        Result<void> create_link(inode_t parent_inode, std::string_view name,
                                 inode_t target_inode);
        [[nodiscard]]
        Result<inode_t> create_symlink(inode_t parent_inode,
                                       std::string_view name,
                                       std::string_view target);
        [[nodiscard]]
        Result<Ext4ExtentMapping> extent_lookup(inode_t inode_id,
                                                uint32_t logical);
        [[nodiscard]]
        Result<void> insert_extent(inode_t inode_id, uint32_t logical,
                                   uint64_t physical, uint32_t len);
        [[nodiscard]]
        Result<void> update_inode_size(inode_t inode_id, uint64_t new_size);
        [[nodiscard]]
        Result<void> delete_file(inode_t inode_id);
        [[nodiscard]]
        Result<void> delete_directory(inode_t inode_id);
        [[nodiscard]]
        Result<void> remove_dir_entry(inode_t parent_inode,
                                       std::string_view name);
        [[nodiscard]]
        Result<void> truncate(inode_t inode_id, uint64_t new_size);
        [[nodiscard]]
        Result<void> rename(inode_t old_parent, std::string_view old_name,
                            inode_t new_parent, std::string_view new_name);
        [[nodiscard]]
        Result<Ext4ExtentMapping> extent_lookup_from_node(const byte *node,
                                                          size_t node_size,
                                                          uint32_t logical);
        [[nodiscard]]
        Result<size_t> read_inode_data(inode_t inode_id, uint64_t offset,
                                       void *buf, size_t len);
        [[nodiscard]]
        Result<size_t> write_inode_data(inode_t inode_id, uint64_t offset,
                                        const void *buf, size_t len);
        [[nodiscard]]
        Result<std::vector<Ext4DirEntry>> read_directory(inode_t inode_id);

    public:
        Ext4Superblock(Ext4Driver &fs, blk::BufferCache &cache,
                       size_t dev_block_size, size_t dev_block_count,
                       size_t sb_id);
        ~Ext4Superblock() final = default;

        [[nodiscard]]
        Result<void> load();
        [[nodiscard]]
        IFsDriver &fs() final;
        [[nodiscard]]
        Result<void> sync() final;
        [[nodiscard]]
        Result<inode_t> root() final;
        [[nodiscard]]
        Result<util::owner<IINode *>> get_inode(inode_t inode_id) final;
        [[nodiscard]]
        Result<bool> is_symlink(inode_t inode_id) final;
        [[nodiscard]]
        Result<std::string> readlink(inode_t inode_id) final;
        [[nodiscard]]
        Result<inode_t> alloc_inode(INodeType type) final;
        [[nodiscard]]
        Result<void> free_inode(inode_t id) final;
        [[nodiscard]]
        IMetadata &metadata() final;
        [[nodiscard]]
        size_t sb_id() const final;
        [[nodiscard]]
        Result<void> fill_attr(inode_t inode_id, AttrSet &out);
        [[nodiscard]]
        Result<void> apply_attr(inode_t inode_id, AttrMask mask,
                                 const AttrSet &attrs);

        friend class Ext4File;
        friend class Ext4Symlink;
        friend class Ext4Directory;
    };

    class Ext4Driver final : public IFsDriver {
    private:
        size_t _next_sb_id = 1;

    public:
        ~Ext4Driver() final = default;

        [[nodiscard]]
        const char *name() const final;
        [[nodiscard]]
        Result<void> probe(size_t devno, const char *options) final;
        [[nodiscard]]
        Result<util::owner<ISuperblock *>> mount(size_t devno,
                                                 const char *options) final;
        [[nodiscard]]
        Result<void> unmount(ISuperblock *sb) final;
    };
}  // namespace ext4
