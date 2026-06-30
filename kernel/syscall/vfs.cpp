/**
 * @file vfs.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief VFS 相关系统调用
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <object/vdir.h>
#include <object/vfile.h>
#include <object/vmount.h>
#include <syscall/syscall.h>
#include <syscall/vfs.h>
#include <task/scheduler.h>
#include <vfs/vfs.h>

namespace syscall {
    namespace {
        [[nodiscard]]
        size_t dir_entry_record_size(const DirectoryEntryInfo &entry) {
            return sizeof(dir_entry_header) + entry.name.size() + 1;
        }

        [[nodiscard]]
        Result<size_t> encode_dir_entry(const DirectoryEntryInfo &entry,
                                        size_t startpos, void *buf,
                                        size_t buflen) {
            size_t record_size = dir_entry_record_size(entry);
            if ((startpos + record_size) > buflen) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            char *_buf = static_cast<char *>(buf);
            auto *header =
                reinterpret_cast<dir_entry_header *>(_buf + startpos);
            header->next_offset = 0;
            memcpy(_buf + startpos + sizeof(dir_entry_header),
                   entry.name.c_str(), entry.name.size() + 1);
            return record_size;
        }

        [[nodiscard]]
        Result<size_t> encode_dir_entries_to_buffer(
            const std::vector<DirectoryEntryInfo> &entries, void *buf,
            size_t buflen, size_t offset) {
            if (buf == nullptr && buflen != 0) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (offset >= entries.size() ||
                buflen < sizeof(dir_entry_header) + 1)
            {
                return 0;
            }

            size_t pos = 0;
            char *_buf = static_cast<char *>(buf);

            for (size_t i = offset; i < entries.size(); i++) {
                size_t record_size = dir_entry_record_size(entries[i]);
                if (pos + record_size > buflen) {
                    if (pos == 0) {
                        unexpect_return(ErrCode::INVALID_PARAM);
                    }
                    break;
                }
                auto encode_res =
                    encode_dir_entry(entries[i], pos, buf, buflen);
                propagate(encode_res);
                auto *header =
                    reinterpret_cast<dir_entry_header *>(_buf + pos);
                // 设置下一项的偏移
                header->next_offset = record_size;
                pos += record_size;
                if (pos == buflen) {
                    break;
                }
            }

            return pos;
        }

        [[nodiscard]]
        Result<cap::CHolder *> current_holder_for_vfs() {
            auto *current = schd::Scheduler::inst().current_tcb();
            if (current == nullptr || current->task == nullptr ||
                current->task->cholder == nullptr)
            {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            return current->task->cholder;
        }

        [[nodiscard]]
        Result<cap::Capability *> lookup_current_cap(CapIdx idx) {
            auto holder_res = current_holder_for_vfs();
            propagate(holder_res);
            return holder_res.value()->lookup(idx);
        }
    }  // namespace

    Result<CapIdx> vfs_open(CapIdx parent_dir_cap, const UString &relpath,
                            flags::oflg_t oflags) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().open(*parent_res.value(), relpath.kbuf(), oflags,
                                *holder_res.value());
    }

    Result<CapIdx> vfs_opendir(CapIdx parent_dir_cap, const UString &relpath,
                               flags::oflg_t oflags) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().opendir(*parent_res.value(), relpath.kbuf(), oflags,
                                   *holder_res.value());
    }

    Result<CapIdx> vfs_mkfile(CapIdx parent_dir_cap, const UString &relpath,
                              flags::oflg_t oflags) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().mkfile(*parent_res.value(), relpath.kbuf(), oflags,
                                  *holder_res.value());
    }

    Result<CapIdx> vfs_mkdir(CapIdx parent_dir_cap, const UString &relpath,
                             flags::oflg_t oflags) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().mkdir(*parent_res.value(), relpath.kbuf(), oflags,
                                 *holder_res.value());
    }

    Result<size_t> vfs_read(CapIdx file_cap, size_t offset, UBuffer &&buf,
                            size_t len) {
        auto cap_res = lookup_current_cap(file_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        cap::VFileObject obj(util::nnullforce(cap_res.value()));
        auto read_res = obj.read(offset, buf.kbuf(), len);
        propagate(read_res);
        auto commit_res = buf.commit_to_user(read_res.value());
        propagate(commit_res);
        return read_res.value();
    }

    Result<size_t> vfs_write(CapIdx file_cap, size_t offset, UBuffer &&buf,
                             size_t len) {
        auto cap_res = lookup_current_cap(file_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        cap::VFileObject obj(util::nnullforce(cap_res.value()));
        return obj.write(offset, buf.kbuf(), len);
    }

    Result<size_t> vfs_size(CapIdx file_cap) {
        auto cap_res = lookup_current_cap(file_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        cap::VFileObject obj(util::nnullforce(cap_res.value()));
        return obj.size();
    }

    Result<size_t> vfs_getdents(CapIdx dir_cap, UBuffer &&buf, size_t buflen,
                                size_t offset) {
        auto cap_res = lookup_current_cap(dir_cap);
        if (!cap_res.has_value()) {
            loggers::SYSCALL::ERROR(
                "getdents cap lookup failed: cap=0x%lx offset=%lu buflen=%lu err=%s",
                static_cast<unsigned long>(dir_cap),
                static_cast<unsigned long>(offset),
                static_cast<unsigned long>(buflen),
                to_cstring(cap_res.error()));
            propagate_return(cap_res);
        }
        auto *payload = cap_res.value()->payload();
        if (payload == nullptr || payload->type_id() != PayloadType::VDIR) {
            auto *current = schd::Scheduler::inst().current_tcb();
            loggers::SYSCALL::ERROR(
                "getdents cap type mismatch: pid=%lu cap=0x%lx type=%s offset=%lu buflen=%lu payload=%p",
                current != nullptr && current->task != nullptr
                    ? current->task->pid
                    : 0,
                static_cast<unsigned long>(dir_cap),
                payload != nullptr ? to_string(payload->type_id()) : "NULL",
                static_cast<unsigned long>(offset),
                static_cast<unsigned long>(buflen), payload);
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        cap::VDirectoryObject obj(util::nnullforce(cap_res.value()));
        auto entries_res = obj.getdents();
        propagate(entries_res);

        auto dents_res = encode_dir_entries_to_buffer(
            entries_res.value(), buf.kbuf(), buflen, offset);
        propagate(dents_res);

        auto commit_res = buf.commit_to_user(dents_res.value());
        propagate(commit_res);
        
        return dents_res.value();
    }

    Result<bool> vfs_sync(CapIdx capidx) {
        auto cap_res = lookup_current_cap(capidx);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() == PayloadType::VFILE) {
            cap::VFileObject obj(util::nnullforce(cap_res.value()));
            auto sync_res = obj.sync();
            propagate(sync_res);
            return true;
        }
        if (cap_res.value()->payload()->type_id() == PayloadType::VDIR) {
            cap::VDirectoryObject obj(util::nnullforce(cap_res.value()));
            auto sync_res = obj.sync();
            propagate(sync_res);
            return true;
        }
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }

    Result<void> vfs_unlink(CapIdx parent_dir_cap, const UString &relpath) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().unlink(*parent_res.value(), relpath.kbuf());
    }

    Result<void> vfs_rmdir(CapIdx parent_dir_cap, const UString &relpath) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().rmdir(*parent_res.value(), relpath.kbuf());
    }

    Result<void> vfs_truncate(CapIdx file_cap, size_t new_size) {
        auto cap_res = lookup_current_cap(file_cap);
        propagate(cap_res);
        return VFS::inst().truncate(*cap_res.value(), new_size);
    }

    Result<void> vfs_ioctl(CapIdx file_cap, size_t cmd, UBuffer &&arg) {
        auto cap_res = lookup_current_cap(file_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        cap::VFileObject obj(util::nnullforce(cap_res.value()));
        return obj.ioctl(cmd, std::move(arg));
    }

    Result<void> vfs_rename(CapIdx old_parent_cap, const UString &old_name,
                            CapIdx new_parent_cap, const UString &new_name) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto old_parent_res = lookup_current_cap(old_parent_cap);
        propagate(old_parent_res);
        auto new_parent_res = lookup_current_cap(new_parent_cap);
        propagate(new_parent_res);
        return VFS::inst().rename(*old_parent_res.value(), old_name.kbuf(),
                                   *new_parent_res.value(), new_name.kbuf());
    }

    Result<void> vfs_symlink(CapIdx parent_dir_cap, const UString &relpath,
                             const UString &target) {
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        return VFS::inst().symlink(*parent_res.value(), relpath.kbuf(),
                                   target.kbuf());
    }

    Result<void> vfs_link(CapIdx parent_dir_cap, const UString &relpath,
                          CapIdx target_file_cap) {
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        auto target_res = lookup_current_cap(target_file_cap);
        propagate(target_res);
        return VFS::inst().link(*parent_res.value(), relpath.kbuf(),
                                *target_res.value());
    }

    Result<void> vfs_stat(CapIdx parent_dir_cap, const UString &relpath,
                          UBuffer &&out) {
        if (out.kbuf() == nullptr || out.len() < sizeof(NodeMeta)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        NodeMeta st {};
        auto stat_res = VFS::inst().stat(*parent_res.value(), relpath.kbuf(), st);
        propagate(stat_res);
        memcpy(out.kbuf(), &st, sizeof(st));
        return out.commit_to_user(sizeof(st));
    }

    Result<void> vfs_lstat(CapIdx parent_dir_cap, const UString &relpath,
                           UBuffer &&out) {
        if (out.kbuf() == nullptr || out.len() < sizeof(NodeMeta)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        NodeMeta st {};
        auto stat_res =
            VFS::inst().lstat(*parent_res.value(), relpath.kbuf(), st);
        propagate(stat_res);
        memcpy(out.kbuf(), &st, sizeof(st));
        return out.commit_to_user(sizeof(st));
    }

    Result<void> vfs_fstat(CapIdx file_cap, UBuffer &&out) {
        if (out.kbuf() == nullptr || out.len() < sizeof(NodeMeta)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto file_res = lookup_current_cap(file_cap);
        propagate(file_res);
        NodeMeta st{};
        auto stat_res = VFS::inst().fstat(*file_res.value(), st);
        propagate(stat_res);
        memcpy(out.kbuf(), &st, sizeof(st));
        return out.commit_to_user(sizeof(st));
    }

    Result<void> vfs_statfs(CapIdx capidx, UBuffer &&out) {
        if (out.kbuf() == nullptr || out.len() < sizeof(VFSStatFS)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto cap_res = lookup_current_cap(capidx);
        propagate(cap_res);
        VFSStatFS st{};
        auto statfs_res = VFS::inst().statfs(*cap_res.value(), st);
        propagate(statfs_res);
        memcpy(out.kbuf(), &st, sizeof(st));
        return out.commit_to_user(sizeof(st));
    }

    Result<void> vfs_getattr(CapIdx capidx, UBuffer &&out) {
        if (out.kbuf() == nullptr || out.len() < sizeof(AttrSet)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto cap_res = lookup_current_cap(capidx);
        propagate(cap_res);
        AttrSet attrs{};
        auto getattr_res = VFS::inst().getattr(*cap_res.value(), attrs);
        propagate(getattr_res);
        memcpy(out.kbuf(), &attrs, sizeof(attrs));
        return out.commit_to_user(sizeof(attrs));
    }

    Result<void> vfs_getattr_at(CapIdx parent_dir_cap, const UString &relpath,
                                UBuffer &&out, uint32_t flags) {
        if (out.kbuf() == nullptr || out.len() < sizeof(AttrSet)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        AttrSet attrs{};
        auto getattr_res =
            VFS::inst().getattr_at(*parent_res.value(), relpath.kbuf(), attrs,
                                   flags);
        propagate(getattr_res);
        memcpy(out.kbuf(), &attrs, sizeof(attrs));
        return out.commit_to_user(sizeof(attrs));
    }

    Result<void> vfs_setattr(CapIdx capidx, UBuffer &&attrs_buf, uint32_t mask,
                             uint32_t flags) {
        (void)flags;
        if (attrs_buf.kbuf() == nullptr || attrs_buf.len() < sizeof(AttrSet)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto sync_res = attrs_buf.sync_from_user();
        propagate(sync_res);
        auto cap_res = lookup_current_cap(capidx);
        propagate(cap_res);
        auto *attrs = reinterpret_cast<const AttrSet *>(attrs_buf.kbuf());
        return VFS::inst().setattr(*cap_res.value(),
                                   static_cast<AttrMask>(mask), *attrs);
    }

    Result<void> vfs_setattr_at(CapIdx parent_dir_cap, const UString &relpath,
                                UBuffer &&attrs_buf, uint32_t mask,
                                uint32_t flags) {
        if (attrs_buf.kbuf() == nullptr || attrs_buf.len() < sizeof(AttrSet)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto sync_res = attrs_buf.sync_from_user();
        propagate(sync_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        auto *attrs = reinterpret_cast<const AttrSet *>(attrs_buf.kbuf());
        return VFS::inst().setattr_at(*parent_res.value(), relpath.kbuf(),
                                      static_cast<AttrMask>(mask), *attrs,
                                      flags);
    }

    Result<void> vfs_chown(CapIdx capidx, uint32_t uid, uint32_t gid,
                           uint32_t flags) {
        auto cap_res = lookup_current_cap(capidx);
        propagate(cap_res);
        return VFS::inst().chown(*cap_res.value(), uid, gid, flags);
    }

    Result<void> vfs_chown_at(CapIdx dirfd, const UString &relpath,
                              uint32_t uid, uint32_t gid, uint32_t flags) {
        auto parent_res = lookup_current_cap(dirfd);
        propagate(parent_res);
        return VFS::inst().chown_at(*parent_res.value(), relpath.kbuf(), uid,
                                    gid, flags);
    }

    Result<size_t> vfs_readlink(CapIdx parent_dir_cap, const UString &relpath,
                                 UBuffer &&buf, size_t bufsiz) {
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);
        auto readlink_res =
            VFS::inst().readlink(*parent_res.value(), relpath.kbuf(),
                                 buf.kbuf(), bufsiz);
        propagate(readlink_res);
        auto commit_res = buf.commit_to_user(readlink_res.value());
        propagate(commit_res);
        return readlink_res.value();
    }

    Result<void> vfs_page_cache_stats(UBuffer &&out, bool reset) {
        if (out.kbuf() == nullptr || out.len() < sizeof(VFSPageCacheStats)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        VFSPageCacheStats stats = VFS::page_cache_stats();
        if (reset) {
            VFS::reset_page_cache_stats();
        }
        memcpy(out.kbuf(), &stats, sizeof(stats));
        return out.commit_to_user(sizeof(stats));
    }

    Result<CapIdx> mnt_create(CapIdx devfile_cap, const UString &fs_name,
                              uint64_t superflags, const UString *options) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);

        bool has_device = devfile_cap != cap::null;
        size_t devno    = 0;
        if (has_device) {
            auto devfile_res = lookup_current_cap(devfile_cap);
            propagate(devfile_res);
            auto *vfile = devfile_res.value()->payload_as<VFile>();
            if (vfile == nullptr) {
                unexpect_return(ErrCode::TYPE_NOT_MATCHED);
            }
            auto *inode = vfile->vinode()->inode();
            auto *meta = static_cast<devfs::DevFileMetadata *>(&inode->metadata());
            if (meta == nullptr || !meta->is_blk) {
                unexpect_return(ErrCode::TYPE_NOT_MATCHED);
            }
            auto *blk_file = static_cast<devfs::BlockDevFile *>(inode);
            devno = blk_file->devno();
        }

        auto mount_res = VFS::inst().create_mount(
            fs_name.kbuf(), has_device, devno, superflags,
            options == nullptr ? nullptr : options->kbuf());
        propagate(mount_res);
        auto mount_owner = mount_res.value();
        auto *mount_ptr = mount_owner.get();
        auto idx_res =
            holder_res.value()->insert_to_free(mount_ptr, perm::allperm());
        if (!idx_res.has_value()) {
            delete mount_ptr;
            propagate_return(idx_res);
        }
        mount_owner = util::owner<VMount *>(nullptr);
        return idx_res.value();
    }

    Result<bool> mnt_mount(CapIdx mntcap, CapIdx parent_dir_cap,
                           const UString &mountpoint, uint64_t attachflags) {
        auto mnt_res = lookup_current_cap(mntcap);
        propagate(mnt_res);
        auto parent_res = lookup_current_cap(parent_dir_cap);
        propagate(parent_res);

        auto *mount_payload = mnt_res.value()->payload_as<VMount>();
        auto *parent_dir    = parent_res.value()->payload_as<VDirectory>();
        if (mount_payload == nullptr || parent_dir == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        cap::VMountObject mount_obj(util::nnullforce(mnt_res.value()));
        auto mount_res =
            mount_obj.mount(*parent_dir, mountpoint.kbuf(), attachflags);
        propagate(mount_res);
        return true;
    }

    Result<bool> mnt_umount(CapIdx mntcap, uint64_t flags) {
        auto mnt_res = lookup_current_cap(mntcap);
        propagate(mnt_res);

        auto *mount_payload = mnt_res.value()->payload_as<VMount>();
        if (mount_payload == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        cap::VMountObject mount_obj(util::nnullforce(mnt_res.value()));
        auto umount_res = mount_obj.umount(flags);
        propagate(umount_res);
        return true;
    }

    Result<CapIdx> mnt_root(CapIdx mntcap) {
        auto holder_res = current_holder_for_vfs();
        propagate(holder_res);
        auto mnt_res = lookup_current_cap(mntcap);
        propagate(mnt_res);

        auto *mount_payload = mnt_res.value()->payload_as<VMount>();
        if (mount_payload == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        cap::VMountObject mount_obj(util::nnullforce(mnt_res.value()));
        return mount_obj.root(*holder_res.value());
    }

    MountStatus mnt_state(CapIdx mntcap) {
        auto holder_res = current_holder_for_vfs();
        if (!holder_res.has_value()) {
            return MountStatus::INVALID;
        }
        auto mnt_res = holder_res.value()->lookup(mntcap);
        if (!mnt_res.has_value()) {
            return MountStatus::INVALID;
        }
        auto *mount_payload = mnt_res.value()->payload_as<VMount>();
        if (mount_payload == nullptr) {
            return MountStatus::INVALID;
        }

        cap::VMountObject mount_obj(util::nnullforce(mnt_res.value()));
        auto state_res = mount_obj.state();
        if (!state_res.has_value()) {
            return MountStatus::INVALID;
        }
        return state_res.value();
    }

}  // namespace syscall
