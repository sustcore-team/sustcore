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
#include <cstring>
#include <string_view>

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
		uint8_t raw[BLOCK_SIZE];

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
	};
	// NOLINTEND

	class TarSuperblock;

	class TarFile : public IFile {
	private:
		TarSuperblock *sb_;
		const TarBlock *header_;
		const uint8_t *data_;
		const uint8_t *end_;
		inode_t inode_id_;

		struct Meta : public IMetadata {} meta_;

	public:
		TarFile(TarSuperblock *sb, const TarBlock *header, inode_t id);

		// IINode
		IMetadata &metadata() override {
			return meta_;
		}

		inode_t inode_id() const override {
			return inode_id_;
		}

		// IFile
		Result<size_t> read(off_t offset, void *buf, size_t len) override;
		Result<size_t> write(off_t, const void *, size_t) override {
			unexpect_return(ErrCode::NOT_SUPPORTED);
		}
		Result<size_t> size() override {
			return static_cast<size_t>(end_ - data_);
		}
		Result<void> sync() override {
			unexpect_return(ErrCode::NOT_SUPPORTED);
		}

        void *operator new(size_t size);
        void operator delete(void *ptr);
	};

	class TarDirectory : public IDirectory {
	private:
		TarSuperblock *sb_;
		const TarBlock *header_;
		inode_t inode_id_;

		struct Meta : public IMetadata {} meta_;

	public:
		TarDirectory(TarSuperblock *sb, const TarBlock *header, inode_t id)
			: sb_(sb), header_(header), inode_id_(id) {}

		// IINode
		IMetadata &metadata() override {
			return meta_;
		}

		inode_t inode_id() const override {
			return inode_id_;
		}

		// IDirectory
		Result<inode_t> lookup(std::string_view name) override;
		Result<void> sync() override {
			unexpect_return(ErrCode::NOT_SUPPORTED);
		}

        void *operator new(size_t size);
        void operator delete(void *ptr);
	};

	// Like a SuperBlock factory
	class TarFSDriver : public IFsDriver {
	private:
		inline bool is_valid(size_t size_, const uint8_t *data_);

	public:
		const char *name() const override {
			return "tarfs";
		}

		Result<void> probe(IBlockDevice *device,
						   const char *options) override;

		Result<util::owner<ISuperblock *>> mount(IBlockDevice *device,
												 const char *options) override;

		Result<void> unmount(ISuperblock *sb) override;
	};

	class TarSuperblock : public ISuperblock {
	private:
		const uint8_t *data_;  // 只读数据段
		const size_t size_;
		TarFSDriver *fs_;
		IBlockDevice *device_;

		struct Meta : public IMetadata {} meta_;
		size_t sb_id_;

		const TarBlock *get_block(inode_t id) const;

	public:
		TarSuperblock(const uint8_t *data, size_t size, TarFSDriver *fs,
					  IBlockDevice *device, size_t sb_id)
			: data_(data), size_(size), fs_(fs), device_(device),
			  sb_id_(sb_id) {}

		TarSuperblock(const TarSuperblock &)            = delete;
		TarSuperblock &operator=(const TarSuperblock &) = delete;
		TarSuperblock(TarSuperblock &&)                 = delete;
		TarSuperblock &operator=(TarSuperblock &&)      = delete;

		~TarSuperblock() override;

		IFsDriver &fs() override {
			return *fs_;
		}

		Result<void> sync() override {
			unexpect_return(ErrCode::NOT_SUPPORTED);
		}

		Result<inode_t> root() override {
			return inode_t(0);
		}

		Result<util::owner<IINode *>> get_inode(inode_t inode_id) override;

		IMetadata &metadata() override {
			return meta_;
		}

		Result<size_t> sb_id() const override {
			return sb_id_;
		}

        friend class TarFile;
        friend class TarDirectory;
        friend class TarFSDriver;
	};

}  // namespace tarfs