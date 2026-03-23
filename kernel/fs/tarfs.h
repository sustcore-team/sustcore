/**
 * @file tarfs.h
 * @author hyj0824 (12510430@mail.sustech.edu.cn)
 * @brief Tar Based initramfs
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * 生命周期由 Dentry 树管理，INode 和 Dentry 保证一一对应；
 * 所以 TarNode 糅合了 Dentry 与 INode，
 * 负责析构 File/Directory 视图
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/block.h>
#include <sus/list.h>
#include <sus/path.h>
#include <vfs/ops.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace tarfs {
    static constexpr size_t BLOCK_SIZE = 512;

    template <typename T>
    inline size_t parse_octal(const T &field) {
        // 使用模板来适配不同长度的字符八进制字段
        size_t result = 0;
        for (size_t i = 0; i < sizeof(field); ++i) {
            char c = field[i];
            if (c == '\0') {
                break;
            }
            if (c < '0' || c > '7') {
                // 非法字符，返回当前结果
                break;
            }
            result = (result << 3) + (c - '0');
        }
        return result;
    }

    // NOLINTBEGIN
    union TarBlock {
        struct ustar_header {
            char name[100];
            char mode[8];
            char uid[8];
            char gid[8];
            char size[12];
            char mtime[12];
            char checksum[8];
            char typeflag[1];
            char linkname[100];
            char magic[6];
            char version[2];
            char uname[32];
            char gname[32];
            char devmajor[8];
            char devminor[8];
            char prefix[155];
            char pad[12];
        } header;
        uint8_t raw[512];

        inline bool is_header() const {
            return header.magic[0] == 'u' && header.magic[1] == 's' &&
                   header.magic[2] == 't' && header.magic[3] == 'a' &&
                   header.magic[4] == 'r';
        }

        inline bool is_empty() const {
            for (size_t i = 0; i < BLOCK_SIZE; ++i) {
                if (raw[i] != 0)
                    return false;
            }
            return true;
        }

        inline unsigned int calc_checksum() const {
            TarBlock temp           = *this;
            temp.header.checksum[0] = ' ';
            temp.header.checksum[1] = ' ';
            temp.header.checksum[2] = ' ';
            temp.header.checksum[3] = ' ';
            temp.header.checksum[4] = ' ';
            temp.header.checksum[5] = ' ';
            temp.header.checksum[6] = ' ';
            temp.header.checksum[7] = ' ';

            unsigned int sum = 0;
            for (size_t i = 0; i < BLOCK_SIZE; ++i) {
                sum += static_cast<unsigned int>(temp.raw[i]);
            }
            return sum;
        }

        inline util::Path get_entry_name() const {
            // header.name == "foo/bar/baz.txt" -> "baz.txt"
            // header.name == "foo/bar/" -> "bar"
            assert(is_header());
            return util::Path{header.name}.normalize().filename();
        }
    };
    // NOLINTEND

    class TarNode;

    class TarFile : public IFile {
    private:
        TarNode *node_;
        const uint8_t *ptr_;
        const uint8_t *data_;
        const uint8_t *end_;

    public:
        TarFile(TarNode *node);
        Result<size_t> read(void *buf, size_t len) override;
        Result<size_t> read(off_t offset, void *buf, size_t len) override;
        Result<off_t> seek(off_t offset, SeekWhence whence) override;
        Result<size_t> write(const void *buf, size_t len) override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }
        Result<size_t> write(off_t offset, const void *buf, size_t len) override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }
        Result<size_t> size() override {
            return end_ - data_;
        }
        Result<void> sync(void) override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }

        void *operator new(size_t sz) noexcept;
        void operator delete(void *ptr) noexcept;
    };

    class TarDirectory : public IDirectory {
    private:
        TarNode *node_;

    public:
        TarDirectory(TarNode *node) : node_(node) {}
        Result<IDentry *> lookup(const char *name) override;
        Result<IDentry *> create(const char *name, bool is_dir) override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }
        Result<void> sync(void) override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }

        void *operator new(size_t sz) noexcept;
        void operator delete(void *ptr) noexcept;
    };

    class TarNode : public IINode, public IDentry {
    private:
        // 理论上，多个 Dentry 可能指向同一个 INode
        // 但在 tarfs 中，每个 Dentry 都对应一个 INode
        // 这样可以简化生命期管理，增加缓存命中
        // 否则就得上引用计数了
        bool is_dir_;
        util::Path entry_;  // entry name, not full path
        const TarBlock *header_;

        union {
            TarDirectory *dir;
            TarFile *file;
        } view_{};
        util::ArrayList<TarNode *> children_{};

    public:
        TarNode(const uint8_t *header)
            : header_(reinterpret_cast<const TarBlock *>(header)) {
            is_dir_ = header_->header.typeflag[0] == '5';
            entry_  = header_->get_entry_name();
        }

        TarNode(const TarBlock *header) : header_(header) {
            is_dir_ = header_->header.typeflag[0] == '5';
            entry_  = header_->get_entry_name();
        }

        TarNode(const TarNode &)            = delete;
        TarNode &operator=(const TarNode &) = delete;
        TarNode(TarNode &&)                 = delete;
        TarNode &operator=(TarNode &&)      = delete;

        ~TarNode() noexcept {
            if (is_dir_)
                delete view_.dir;
            else
                delete view_.file;

            for (auto child : children_) {
                delete child;
            }
        }

        // IINode

        Result<IDirectory *> as_directory(void) override {
            if (!is_dir_)
                return {unexpect, ErrCode::INVALID_PARAM};
            if (!view_.dir) {
                view_.dir = new TarDirectory{this};
            }
            return view_.dir;
        }

        Result<IFile *> as_file(void) override {
            if (is_dir_)
                return {unexpect, ErrCode::INVALID_PARAM};
            if (!view_.file) {
                view_.file = new TarFile{this};
            }
            return view_.file;
        }

        Result<IMetadata *> metadata(void) override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }

        // IDentry

        Result<const char *> name(void) override {
            return entry_.c_str();
        }

        Result<void> remove(void) override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }

        Result<void> rename(const char *new_name) override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }

        Result<IINode *> inode(void) override {
            return static_cast<IINode *>(this);
        }

        friend class TarFile;
        friend class TarDirectory;
        friend class TarSuperblock;

        void *operator new(size_t sz) noexcept;
        void operator delete(void *ptr) noexcept;
    };

    // Like a SuperBlock factory
    class TarFSDriver : public IFsDriver {
    private:
        inline bool is_valid(size_t size_, const uint8_t *data_);

    public:
        inline const char *name() const override {
            return "tarfs";
        };

        Result<void> probe(IBlockDevice *device, const char *options) override;

        Result<ISuperblock *> mount(IBlockDevice *device,
                                    const char *options) override;

        Result<void> unmount(ISuperblock *&sb) override {
            delete sb;
            sb = nullptr;
            return {};
        }
    };

    class TarSuperblock : public ISuperblock {
    private:
        const uint8_t *data_;  // 只读数据段
        const size_t size_;
        TarFSDriver *fs_;
        IBlockDevice *device_;
        TarNode *root_{};

    public:
        TarSuperblock(const uint8_t *data, size_t size, TarFSDriver *fs,
                      IBlockDevice *device)
            : data_(data), size_(size), fs_(fs), device_(device) {}

        TarSuperblock(const TarSuperblock &)            = delete;
        TarSuperblock &operator=(const TarSuperblock &) = delete;
        TarSuperblock(TarSuperblock &&)                 = delete;
        TarSuperblock &operator=(TarSuperblock &&)      = delete;

        ~TarSuperblock() override {
            if (!device_->is<RamDiskDevice>())
                delete[] data_;
            delete root_;
            // 触发全部析构
        }

        IFsDriver *fs() override {
            return static_cast<IFsDriver *>(fs_);
        }

        Result<void> sync() override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }

        // tarfs 的根目录就是 tar 包中的第一个 header
        // ! unsafe 不能从外部 delete root_
        // 否则除了数据指针，tarfs 对象的所有指针都会失效
        Result<IINode *> root() override {
            // 返回根目录 inode
            if (!root_) {
                root_ = new TarNode(data_);
            }
            return root_;
        }

        Result<IMetadata *> metadata() override {
            return {unexpect, ErrCode::NOT_SUPPORTED};
        }
    };

};  // namespace tarfs