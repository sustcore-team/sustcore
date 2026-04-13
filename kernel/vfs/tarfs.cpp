/**
 * @file tarfs.cpp
 * @author hyj0824 (12510430@mail.sustech.edu.cn)
 * @brief tarfs 相关实现
 * @version alpha-1.0.0
 * @date 2026-02-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/block.h>
#include <vfs/tarfs.h>
#include <mem/alloc.h>
#include <sus/defer.h>

#include <algorithm>
#include <cstddef>

// TarFile / TarDirectory 使用 KOP 内存池
namespace kop {
	util::Defer<KOP<tarfs::TarFile>> TarFile;
	AutoDefer(TarFile);
	util::Defer<KOP<tarfs::TarDirectory>> TarDirectory;
	AutoDefer(TarDirectory);
}

namespace tarfs {
    // 用offset / BLOCK_SIZE作为inode号, 以保证每个文件/目录的inode号唯一且稳定
    // 且可以方便地从inode号计算出对应的TarBlock地址
    constexpr inode_t to_inode_id(size_t offset) {
        return inode_t(offset / BLOCK_SIZE);
    }

    constexpr size_t to_offset(inode_t id) {
        return id * BLOCK_SIZE;
    }

	// TarFile

    void *TarFile::operator new(size_t size) {
		assert(size == sizeof(TarFile));
		return kop::TarFile->alloc();
	}

	void TarFile::operator delete(void *ptr) {
		kop::TarFile->free(static_cast<TarFile *>(ptr));
	}

	TarFile::TarFile(TarSuperblock *sb, const TarBlock *header, inode_t id)
		: sb_(sb), header_(header), inode_id_(id) {
		(void)sb_;
		const uint8_t *raw = reinterpret_cast<const uint8_t *>(header_);
		data_              = raw + BLOCK_SIZE;
		size_t fsize       = parse_octal(header_->header.size);
		end_               = data_ + fsize;
	}

	Result<size_t> TarFile::read(off_t offset, void *buf, size_t len) {
		if (offset < 0) {
			unexpect_return(ErrCode::INVALID_PARAM);
		}
		const uint8_t *ptr = data_ + static_cast<size_t>(offset);
		if (ptr < data_ || ptr > end_) {
			unexpect_return(ErrCode::INVALID_PARAM);
		}
		size_t to_read = std::min(len, static_cast<size_t>(end_ - ptr));
		memcpy(buf, ptr, to_read);
		return to_read;
	}

	// TarDirectory

	void *TarDirectory::operator new(size_t size) {
		assert(size == sizeof(TarDirectory));
		return kop::TarDirectory->alloc();
	}

	void TarDirectory::operator delete(void *ptr) {
		kop::TarDirectory->free(static_cast<TarDirectory *>(ptr));
	}

	Result<inode_t> TarDirectory::lookup(std::string_view name) {
		if (name.empty()) {
			return inode_id_;
		}

		// 目标路径: 当前目录 header.name 与 name 拼接后规范化
		util::Path path{header_->header.name};
		path = (path / name).normalize();

		const uint8_t *base = sb_->data_;
		const uint8_t *end  = base + sb_->size_;

		for (const TarBlock *p = header_ + 1;
			 reinterpret_cast<const uint8_t *>(p) + BLOCK_SIZE <= end;) {
			if (!p->is_header()) {
				break;
			}

			util::Path p_path{p->header.name};
			p_path = p_path.normalize();

			if (path == p_path) {
				size_t offset = reinterpret_cast<const uint8_t *>(p) - base;
				return to_inode_id(offset);
			}

			size_t file_size  = parse_octal(p->header.size);
			size_t file_block = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
			p += file_block + 1;
		}

		unexpect_return(ErrCode::ENTRY_NOT_FOUND);
	}

	// TarFSDriver

	bool TarFSDriver::is_valid(size_t size_, const uint8_t *data_) {
		// 检查文件大小是否为 BLOCK_SIZE 的整数倍
		if (size_ % BLOCK_SIZE != 0)
			return false;

		// 检查 checksum
		for (size_t offset = 0; offset < size_;) {
			const TarBlock *block =
				reinterpret_cast<const TarBlock *>(data_ + offset);
			auto stored = parse_octal(block->header.checksum);
			if (stored == block->calc_checksum()) {
				size_t file_size  = parse_octal(block->header.size);
				size_t file_block =
					(file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
				offset += BLOCK_SIZE * (file_block + 1);
				// offset 跳到下一个 header 块
			} else {
				if (block->is_empty()) {
					offset += BLOCK_SIZE;
					// 允许有空块
				} else {
					return false;
				}
			}
		}
		return true;
	}

	Result<void> TarFSDriver::probe(IBlockDevice *device,
									const char *options) {
		(void)options;
		size_t size   = device->block_sz() * device->block_cnt();
		uint8_t *data = new uint8_t[size];
		if (!data) {
			unexpect_return(ErrCode::OUT_OF_MEMORY);
		}
		size_t read_blocks = device->read_blocks(0, data, device->block_cnt());
		if (read_blocks != device->block_cnt()) {
			delete[] data;
			unexpect_return(ErrCode::IO_ERROR);
		}
		bool ok = is_valid(size, data);
		delete[] data;
		if (!ok) {
			unexpect_return(ErrCode::INVALID_PARAM);
		}
		void_return();
	}

	static size_t g_next_sb_id = 1;

	Result<util::owner<ISuperblock *>> TarFSDriver::mount(
		IBlockDevice *device, const char *options) {
		(void)options;
		size_t size   = device->block_sz() * device->block_cnt();
		uint8_t *data = nullptr;
		if (device->is<RamDiskDevice>()) {
			// 直接使用其内存作为数据源，减少复制
			data = static_cast<uint8_t *>(device->as<RamDiskDevice>()->base());
		} else {
			data = new uint8_t[size];
			if (!data) {
				unexpect_return(ErrCode::OUT_OF_MEMORY);
			}
			size_t blocks = device->read_blocks(0, data, device->block_cnt());
			if (blocks != device->block_cnt()) {
				delete[] data;
				unexpect_return(ErrCode::IO_ERROR);
			}
		}

		size_t sbid = g_next_sb_id++;
		TarSuperblock *sb_impl =
			new TarSuperblock(data, size, this, device, sbid);
		if (!sb_impl) {
			if (!device->is<RamDiskDevice>()) {
				delete[] data;
			}
			unexpect_return(ErrCode::OUT_OF_MEMORY);
		}
		return util::owner<ISuperblock *>(sb_impl);
	}

	Result<void> TarFSDriver::unmount(ISuperblock *sb) {
		void_return();
	}

	// TarSuperblock

	TarSuperblock::~TarSuperblock() {
		if (!device_->is<RamDiskDevice>()) {
			delete[] data_;
		}
	}

	const TarBlock *TarSuperblock::get_block(inode_t id) const {
		size_t offset = to_offset(id);
		if (offset + BLOCK_SIZE > size_) {
			return nullptr;
		}
		auto *blk =
			reinterpret_cast<const TarBlock *>(data_ + offset);
		if (!blk->is_header()) {
			return nullptr;
		}
		return blk;
	}

	Result<util::owner<IINode *>> TarSuperblock::get_inode(inode_t inode_id) {
		const TarBlock *blk = get_block(inode_id);
		if (!blk) {
			unexpect_return(ErrCode::INVALID_PARAM);
		}
		bool is_dir = blk->header.typeflag[0] == '5';
		if (is_dir) {
			TarDirectory *dir = new TarDirectory(this, blk, inode_id);
			if (!dir) {
				unexpect_return(ErrCode::OUT_OF_MEMORY);
			}
			return util::owner<IINode *>(static_cast<IINode *>(dir));
		} else {
			TarFile *file = new TarFile(this, blk, inode_id);
			if (!file) {
				unexpect_return(ErrCode::OUT_OF_MEMORY);
			}
			return util::owner<IINode *>(static_cast<IINode *>(file));
		}
	}

}  // namespace tarfs
