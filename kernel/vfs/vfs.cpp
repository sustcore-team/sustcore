/**
 * @file vfs.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief VFS
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <bio/blk.h>
#include <cap/cholder.h>
#include <mem/gfp.h>
#include <sus/path.h>
#include <sustcore/errcode.h>
#include <sustcore/files.h>
#include <task/scheduler.h>
#include <task/wait.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <utility>

namespace {
    constexpr size_t kMaxPageCachePages = 1024;

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static VFS inst_vfs;
    static bool inst_vfs_initialized = false;
    static VFSPageCacheStats page_cache_stats{
        .hits          = 0,
        .misses        = 0,
        .invalidations = 0,
        .writebacks    = 0,
        .evictions     = 0,
        .cached_pages  = 0,
        .max_pages     = kMaxPageCachePages,
        .backing_reads = 0,
        .backing_writes = 0,
    };
    static SpinLocker page_cache_lock;
    static std::atomic<size_t> page_cache_readers{0};
    static VINode::CachedFilePage *inactive_head = nullptr;
    static VINode::CachedFilePage *inactive_tail = nullptr;
    static VINode::CachedFilePage *active_head = nullptr;
    static VINode::CachedFilePage *active_tail = nullptr;

    class PageCacheReadGuard {
    public:
        PageCacheReadGuard() {
            page_cache_readers.fetch_add(1, std::memory_order_acquire);
        }

        ~PageCacheReadGuard() {
            page_cache_readers.fetch_sub(1, std::memory_order_release);
        }
    };

    void synchronize_page_cache_rcu() noexcept {
        while (page_cache_readers.load(std::memory_order_acquire) != 0) {
            if (schd::Scheduler::initialized()) {
                schd::Scheduler::inst().yield();
            }
        }
    }

    void lru_refs(bool active, VINode::CachedFilePage *&head,
                  VINode::CachedFilePage *&tail) noexcept {
        if (active) {
            head = active_head;
            tail = active_tail;
        } else {
            head = inactive_head;
            tail = inactive_tail;
        }
    }

    void lru_store_refs(bool active, VINode::CachedFilePage *head,
                        VINode::CachedFilePage *tail) noexcept {
        if (active) {
            active_head = head;
            active_tail = tail;
        } else {
            inactive_head = head;
            inactive_tail = tail;
        }
    }

    void lru_remove(VINode::CachedFilePage &page) noexcept {
        VINode::CachedFilePage *head = nullptr;
        VINode::CachedFilePage *tail = nullptr;
        lru_refs(page.active, head, tail);

        if (page.prev != nullptr) {
            page.prev->next = page.next;
        } else if (head == &page) {
            head = page.next;
        }
        if (page.next != nullptr) {
            page.next->prev = page.prev;
        } else if (tail == &page) {
            tail = page.prev;
        }
        page.prev = nullptr;
        page.next = nullptr;
        lru_store_refs(page.active, head, tail);
    }

    void lru_push_tail(VINode::CachedFilePage &page, bool active) noexcept {
        page.active = active;
        page.prev = nullptr;
        page.next = nullptr;
        VINode::CachedFilePage *head = nullptr;
        VINode::CachedFilePage *tail = nullptr;
        lru_refs(active, head, tail);
        if (tail == nullptr) {
            head = &page;
            tail = &page;
        } else {
            tail->next = &page;
            page.prev = tail;
            tail = &page;
        }
        lru_store_refs(active, head, tail);
    }

    void lru_touch(VINode::CachedFilePage &page) noexcept {
        lru_remove(page);
        lru_push_tail(page, true);
    }

    void lru_demote_one_active_page() noexcept {
        if (active_head == nullptr) {
            return;
        }
        auto *page = active_head;
        lru_remove(*page);
        lru_push_tail(*page, false);
    }

    VINode::CachedFilePage *lru_victim() noexcept {
        if (inactive_head == nullptr) {
            lru_demote_one_active_page();
        }
        return inactive_head;
    }

    Result<bool> evict_lru_page();

    Result<void> ensure_page_cache_capacity() {
        while (page_cache_stats.cached_pages >= kMaxPageCachePages) {
            auto evict_res = evict_lru_page();
            propagate(evict_res);
            if (!evict_res.value()) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
        }
        void_return();
    }

    Result<bool> evict_lru_page() {
        VINode *owner = nullptr;
        {
            GuardedLock cache_guard(page_cache_lock);
            auto *victim = lru_victim();
            if (victim == nullptr || victim->owner == nullptr) {
                return false;
            }
            owner = victim->owner;
            owner->keep();
        }
        auto evict_res = owner->evict_file_page();
        owner->release();
        return evict_res;
    }
}  // namespace

VSuperblock::~VSuperblock() {
    auto flush_res = flush_file_pages();
    if (!flush_res.has_value()) {
        loggers::VFS::ERROR("VSuperblock 析构回写页缓存失败: err=%s",
                            to_cstring(flush_res.error()));
    }
    for (auto &entry : _inode_cache) {
        if (entry.second != nullptr) {
            entry.second->release();
        }
    }
    _inode_cache.clear();
    delete _sb.get();
    _sb = util::owner<ISuperblock *>(nullptr);
}

Result<util::refc_ptr<VINode>> VSuperblock::get_vnode(inode_t inode_id) {
    auto cache_res = _inode_cache.at_nt(inode_id);
    if (cache_res.has_value()) {
        VINode *cached = *cache_res.value();
        if (cached != nullptr) {
            return util::refc_ptr(cached);
        }
        _inode_cache.erase(inode_id);
    }

    auto inode_res = sb()->get_inode(inode_id);
    if (!inode_res.has_value()) {
        loggers::VFS::ERROR("VSuperblock get_vnode get_inode failed: inode=%u err=%s",
                            static_cast<unsigned>(inode_id),
                            to_cstring(inode_res.error()));
        propagate_return(inode_res);
    }

    auto *vnode = new VINode(inode_res.value(), vfsd(), *this);
    if (vnode == nullptr) {
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }

    auto policy = vnode->inode()->inode_cache();
    if (policy != INodeCachePolicy::NONE) {
        vnode->keep();
        _inode_cache.insert_or_assign(inode_id, vnode);
    }

    return util::refc_ptr(vnode);
}

Result<void> VSuperblock::invalidate_inode(inode_t inode_id) {
    auto cache_res = _inode_cache.at_nt(inode_id);
    if (!cache_res.has_value()) {
        void_return();
    }
    VINode *cached = *cache_res.value();
    if (cached == nullptr) {
        _inode_cache.erase(inode_id);
        void_return();
    }
    return cached->invalidate();
}

Result<void> VSuperblock::evict_inode(inode_t inode_id) {
    auto cache_res = _inode_cache.at_nt(inode_id);
    if (cache_res.has_value()) {
        VINode *cached = *cache_res.value();
        if (cached != nullptr) {
            auto flush_res = cached->flush_file_pages();
            propagate(flush_res);
            cached->invalidate_file_pages();
            cached->release();
        }
    }
    _inode_cache.erase(inode_id);
    void_return();
}

Result<void> VSuperblock::flush_file_pages() {
    for (auto &entry : _inode_cache) {
        if (entry.second == nullptr) {
            continue;
        }
        auto flush_res = entry.second->flush_file_pages();
        propagate(flush_res);
    }
    void_return();
}

void VSuperblock::on_death() {
    // MountRecord owns mounted superblocks; zero vnode refs must not unmount.
}

Result<void> VINode::invalidate() {
    if (_inode.get() == nullptr) {
        unexpect_return(ErrCode::NULLPTR);
    }

    auto flush_res = flush_file_pages();
    propagate(flush_res);
    invalidate_file_pages();

    const inode_t inode_id = _inode->inode_id();
    auto inode_res = superblock().sb()->get_inode(inode_id);
    if (!inode_res.has_value()) {
        loggers::VFS::ERROR("VINode invalidate get_inode failed: inode=%u err=%s",
                            static_cast<unsigned>(inode_id),
                            to_cstring(inode_res.error()));
        propagate_return(inode_res);
    }

    IINode *old_inode = _inode.get();
    _inode            = inode_res.value();
    delete old_inode;
    void_return();
}

Result<PhyAddr> VINode::cached_file_page(IFile &file, size_t page_index,
                                          size_t *valid_len) {
    {
        GuardedLock cache_guard(page_cache_lock);
        auto cached = _file_pages.find(page_index);
        if (cached != _file_pages.end() && !cached->second.evicting) {
            lru_touch(cached->second);
            page_cache_stats.hits++;
            loggers::VFS::DEBUG("page cache hit: inode=%u page=%lu paddr=%p",
                                static_cast<unsigned>(_inode->inode_id()),
                                page_index, cached->second.paddr.addr());
            if (valid_len != nullptr) {
                *valid_len = cached->second.valid;
            }
            return cached->second.paddr;
        }
    }

    {
        GuardedLock cache_guard(page_cache_lock);
        page_cache_stats.misses++;
    }
    loggers::VFS::DEBUG("page cache miss: inode=%u page=%lu",
                        static_cast<unsigned>(_inode->inode_id()), page_index);

    auto capacity_res = ensure_page_cache_capacity();
    propagate(capacity_res);

    auto page_res = GFP::get_free_page(1);
    propagate(page_res);
    PhyAddr paddr = page_res.value();
    auto *page     = convert<KpaAddr>(paddr).addr();
    memset(page, 0, PAGESIZE);

    auto read_res = file.read(static_cast<off_t>(page_index * PAGESIZE), page,
                              PAGESIZE);
    if (!read_res.has_value()) {
        GFP::put_page(paddr, 1);
        propagate_return(read_res);
    }

    size_t valid = read_res.value();
    {
        GuardedLock cache_guard(page_cache_lock);
        page_cache_stats.backing_reads++;
        auto existing = _file_pages.find(page_index);
        if (existing != _file_pages.end() && !existing->second.evicting) {
            lru_touch(existing->second);
            GFP::put_page(paddr, 1);
            if (valid_len != nullptr) {
                *valid_len = existing->second.valid;
            }
            return existing->second.paddr;
        }
        auto &cached = _file_pages[page_index];
        cached.paddr = paddr;
        cached.valid = valid;
        cached.dirty = false;
        cached.evicting = false;
        cached.owner = this;
        cached.page_index = page_index;
        lru_push_tail(cached, false);
        page_cache_stats.cached_pages++;
    }
    if (valid_len != nullptr) {
        *valid_len = valid;
    }
    return paddr;
}

Result<size_t> VINode::read_cached_file(IFile &file, size_t offset, void *buf,
                                        size_t len) {
    auto *dst        = static_cast<char *>(buf);
    size_t completed = 0;
    while (completed < len) {
        size_t cur_offset = offset + completed;
        if (cur_offset < offset) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        size_t page_index = cur_offset / PAGESIZE;
        size_t in_page    = cur_offset % PAGESIZE;
        size_t valid      = 0;
        auto page_res     = cached_file_page(file, page_index, &valid);
        propagate(page_res);

        if (in_page >= valid) {
            break;
        }

        CachedFilePage *page = nullptr;
        {
            PageCacheReadGuard rcu_guard;
            GuardedLock cache_guard(page_cache_lock);
            auto page_it = _file_pages.find(page_index);
            if (page_it == _file_pages.end() || page_it->second.evicting) {
                unexpect_return(ErrCode::PAGE_NOT_PRESENT);
            }
            page = &page_it->second;
            GuardedLock page_guard(page->lock);
            size_t chunk = std::min(len - completed, page->valid - in_page);
            memcpy(dst + completed,
                   static_cast<char *>(convert<KpaAddr>(page->paddr).addr()) +
                       in_page,
                   chunk);
            completed += chunk;
            if (page->valid < PAGESIZE && in_page + chunk >= page->valid) {
                break;
            }
        }
    }
    return completed;
}

Result<size_t> VINode::write_cached_file(IFile &file, size_t offset,
                                         const void *buf, size_t len) {
    auto *src       = static_cast<const char *>(buf);
    size_t written  = 0;
    while (written < len) {
        size_t cur_offset = offset + written;
        if (cur_offset < offset) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        size_t page_index = cur_offset / PAGESIZE;
        size_t in_page    = cur_offset % PAGESIZE;
        size_t chunk      = std::min(len - written, PAGESIZE - in_page);
        auto page_res = cached_file_page(file, page_index, nullptr);
        propagate(page_res);

        {
            PageCacheReadGuard rcu_guard;
            GuardedLock cache_guard(page_cache_lock);
            auto page_it = _file_pages.find(page_index);
            if (page_it == _file_pages.end() || page_it->second.evicting) {
                unexpect_return(ErrCode::UNKNOWN_ERROR);
            }
            GuardedLock page_guard(page_it->second.lock);
            memcpy(static_cast<char *>(convert<KpaAddr>(page_it->second.paddr).addr()) +
                       in_page,
                   src + written, chunk);
            page_it->second.valid = std::max(page_it->second.valid,
                                             in_page + chunk);
            page_it->second.dirty = true;
            lru_touch(page_it->second);
        }
        written += chunk;
    }
    return written;
}

Result<void> VINode::flush_file_pages() {
    if (_file_pages.empty()) {
        void_return();
    }

    auto file_res = _inode->as_file();
    propagate(file_res);
    IFile *file = file_res.value();

    for (auto &[page_index, page] : _file_pages) {
        bool wrote_back = false;
        {
            GuardedLock page_guard(page.lock);
            if (!page.dirty) {
                continue;
            }
            auto write_res = file->write(static_cast<off_t>(page_index * PAGESIZE),
                                         convert<KpaAddr>(page.paddr).addr(),
                                         page.valid);
            propagate(write_res);
            if (write_res.value() != page.valid) {
                unexpect_return(ErrCode::IO_ERROR);
            }
            page.dirty = false;
            wrote_back = true;
            loggers::VFS::DEBUG("page cache writeback: inode=%u page=%lu len=%lu",
                                static_cast<unsigned>(_inode->inode_id()),
                                page_index, page.valid);
        }
        if (wrote_back) {
            GuardedLock cache_guard(page_cache_lock);
            page_cache_stats.writebacks++;
            page_cache_stats.backing_writes++;
        }
    }
    void_return();
}

Result<bool> VINode::evict_file_page() {
    CachedFilePage *page = nullptr;
    size_t page_index = 0;
    {
        GuardedLock cache_guard(page_cache_lock);
        page = lru_victim();
        if (page == nullptr || page->owner != this || page->evicting) {
            return false;
        }
        page->evicting = true;
        page_index = page->page_index;
        lru_remove(*page);
    }

    {
        bool wrote_back = false;
        {
            GuardedLock page_guard(page->lock);
            if (page->dirty) {
                auto file_res = _inode->as_file();
                propagate(file_res);
                auto write_res = file_res.value()->write(
                    static_cast<off_t>(page_index * PAGESIZE),
                    convert<KpaAddr>(page->paddr).addr(), page->valid);
                propagate(write_res);
                if (write_res.value() != page->valid) {
                    unexpect_return(ErrCode::IO_ERROR);
                }
                wrote_back = true;
            }
        }
        if (wrote_back) {
            GuardedLock cache_guard(page_cache_lock);
            page_cache_stats.writebacks++;
            page_cache_stats.backing_writes++;
        }
    }

    PhyAddr paddr = page->paddr;
    synchronize_page_cache_rcu();
    {
        GuardedLock cache_guard(page_cache_lock);
        auto it = _file_pages.find(page_index);
        if (it != _file_pages.end() && &it->second == page) {
            _file_pages.erase(it);
            page_cache_stats.cached_pages--;
            page_cache_stats.evictions++;
        }
    }
    GFP::put_page(paddr, 1);
    return true;
}

bool VINode::has_file_pages() const noexcept {
    return !_file_pages.empty();
}

void VINode::invalidate_file_pages() noexcept {
    std::vector<size_t> pages_to_free{};
    {
        GuardedLock cache_guard(page_cache_lock);
        if (!_file_pages.empty()) {
            page_cache_stats.invalidations++;
            loggers::VFS::DEBUG("page cache invalidate: inode=%u pages=%lu",
                                _inode.get() == nullptr ? 0U
                                                        : static_cast<unsigned>(_inode->inode_id()),
                                _file_pages.size());
        }
        for (auto &[page_index, page] : _file_pages) {
            if (page.evicting) {
                continue;
            }
            page.evicting = true;
            page.invalidating = true;
            lru_remove(page);
            pages_to_free.push_back(page_index);
        }
    }
    synchronize_page_cache_rcu();

    for (size_t page_index : pages_to_free) {
        PhyAddr paddr = PhyAddr::null;
        {
            GuardedLock cache_guard(page_cache_lock);
            auto it = _file_pages.find(page_index);
            if (it == _file_pages.end() || !it->second.invalidating) {
                continue;
            }
            GuardedLock page_guard(it->second.lock);
            paddr = it->second.paddr;
            _file_pages.erase(it);
            if (page_cache_stats.cached_pages > 0) {
                page_cache_stats.cached_pages--;
            }
        }
        GFP::put_page(paddr, 1);
    }
}

VFile::VFile(VINode &vind, const util::Path &mount_path, VFS &vfs)
    : _vind(&vind), _mount_path(mount_path), _vfs(&vfs) {}

void VFile::destruct() {
    if (_vfs != nullptr) {
        _vfs->_on_vfile_destroy(_mount_path);
        _vfs = nullptr;
    }
    delete this;
}

VDirectory::VDirectory(VINode &vind, const util::Path &mount_path,
                       const util::Path &global_path, VFS &vfs)
    : _vind(&vind),
      _mount_path(mount_path),
      _global_path(global_path),
      _vfs(&vfs) {}

void VDirectory::destruct() {
    if (_vfs != nullptr) {
        _vfs->_on_vfile_destroy(_mount_path);
        _vfs = nullptr;
    }
    delete this;
}

void VFS::init() {
    // call the constructor explicitly to ensure the instance is initialized
    // before use
    new (&inst_vfs) VFS();
    inst_vfs_initialized = true;
}

bool VFS::initialized() {
    return inst_vfs_initialized;
}

VFS &VFS::inst() {
    if (!initialized()) {
        panic("VFS 未初始化!");
    }
    return inst_vfs;
}

VFSPageCacheStats VFS::page_cache_stats() noexcept {
    return ::page_cache_stats;
}

void VFS::reset_page_cache_stats() noexcept {
    size_t cached_pages = ::page_cache_stats.cached_pages;
    ::page_cache_stats = VFSPageCacheStats{
        .hits          = 0,
        .misses        = 0,
        .invalidations = 0,
        .writebacks    = 0,
        .evictions     = 0,
        .cached_pages  = cached_pages,
        .max_pages     = kMaxPageCachePages,
        .backing_reads = 0,
        .backing_writes = 0,
    };
}

Result<IDirectory *> IINode::as_directory() {
    IDirectory *dir = this->as<IDirectory>();
    if (dir) {
        return dir;
    } else {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
}

Result<IFile *> IINode::as_file() {
    IFile *file = this->as<IFile>();
    if (file) {
        return file;
    } else {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
}

Result<ISymlink *> IINode::as_symlink() {
    ISymlink *symlink = this->as<ISymlink>();
    if (symlink) {
        return symlink;
    } else {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
}

VFS::~VFS() = default;

Result<const char *> VFS::__register_fs(util::owner<IFsDriver *> &&driver) {
    const char *fs_name = driver->name();
    if (fs_table.contains(fs_name)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    fs_table.insert_or_assign(fs_name,
                              util::owner(new VFsDriver(std::move(driver))));
    return fs_name;
}

Result<void> VFS::unregister_fs(const char *fs_name) {
    if (!fs_table.contains(fs_name)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto get_res = fs_table.at_nt(fs_name);
    if (!get_res.has_value()) {
        unexpect_return(ErrCode::UNKNOWN_ERROR);
    }

    util::owner<VFsDriver *> driver = *get_res.value();
    if (!driver->closable()) {
        unexpect_return(ErrCode::BUSY);
    }

    fs_table.erase(fs_name);
    delete driver;
    void_return();
}

Result<void> VFS::mount(const char *fs_name, size_t devno,
                        const char *mountpoint, MountFlags flags,
                        const char *options) {
    (void)flags;
    auto dev_res = blk::BlkManager::inst().lookup(devno);
    if (!dev_res.has_value()) {
        propagate_return(dev_res);
    }
    util::Path mnt_path = util::Path::normalize(mountpoint);
    auto ensure_res     = _ensure_mountpoint_path(mnt_path);
    propagate(ensure_res);
    auto key_res = _build_mount_key(mnt_path);
    propagate(key_res);
    MountKey mount_key    = std::move(key_res.value().first);
    util::Path mount_path = std::move(key_res.value().second);
    if (mount_table.contains(mount_key)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto lookup_result = fs_table.at_nt(fs_name);
    if (!lookup_result.has_value()) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    VFsDriver *fsd = *lookup_result.value();
    if (fsd->fsd()->is_pseudo()) {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    IFsDriver *driver = fsd->fsd();
    auto mount_result = driver->mount(devno, options);
    return mount_result.transform(
        [this, mount_key = std::move(mount_key),
         mount_path = std::move(mount_path), fsd, devno](
            util::owner<ISuperblock *> isb) {
            MountRecord record{
                .parent_vinode  = mount_key.parent,
                .entry_name     = mount_key.entry,
                .mount_path     = mount_path,
                .superblock     = util::owner(new VSuperblock(isb, *fsd)),
                .devno          = devno,
                .is_block_mount = true,
                .active_files   = 0,
            };
            if (record.parent_vinode != nullptr) {
                record.parent_vinode->keep();
            }
            this->mount_table.insert_or_assign(mount_key, record);
        });
}

Result<void> VFS::mount(const char *fs_name, const char *mountpoint,
                        const char *options) {
    util::Path mnt_path = util::Path::normalize(mountpoint);
    auto ensure_res     = _ensure_mountpoint_path(mnt_path);
    propagate(ensure_res);
    auto key_res = _build_mount_key(mnt_path);
    propagate(key_res);
    MountKey mount_key    = std::move(key_res.value().first);
    util::Path mount_path = std::move(key_res.value().second);
    if (mount_table.contains(mount_key)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto lookup_result = fs_table.at_nt(fs_name);
    if (!lookup_result.has_value()) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    VFsDriver *fsd = *lookup_result.value();
    if (!fsd->fsd()->is_pseudo()) {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    auto *pseudo      = static_cast<IPesudoFsDriver *>(fsd->fsd());
    auto mount_result = pseudo->mount(fs_name, options);
    return mount_result.transform(
        [this, mount_key = std::move(mount_key),
         mount_path = std::move(mount_path), fsd, fs_name](
            util::owner<ISuperblock *> isb) {
            MountRecord record{
                .parent_vinode  = mount_key.parent,
                .entry_name     = mount_key.entry,
                .mount_path     = mount_path,
                .superblock     = util::owner(new VSuperblock(isb, *fsd)),
                .devno          = 0,
                .is_block_mount = false,
                .active_files   = 0,
            };
            auto *vsb = record.superblock.get();
            if (record.parent_vinode != nullptr) {
                record.parent_vinode->keep();
            }
            this->mount_table.insert_or_assign(mount_key, record);
            this->pseudo_mounts.insert_or_assign(fs_name, vsb);
        });
}

Result<void> VFS::umount(const char *mountpoint) {
    util::Path mnt_path = util::Path::normalize(mountpoint);
    auto key_res        = _build_mount_key(mnt_path);
    propagate(key_res);
    auto lookup_result  = mount_table.at_nt(key_res.value().first);
    if (!lookup_result.has_value()) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    MountRecord &record = *lookup_result.value();
    if (record.active_files != 0) {
        unexpect_return(ErrCode::BUSY);
    }

    auto page_cache_flush_res = record.superblock->flush_file_pages();
    propagate(page_cache_flush_res);

    auto super_sync_res = record.superblock->sb()->sync();
    if (!super_sync_res.has_value() &&
        super_sync_res.error() != ErrCode::NOT_SUPPORTED)
    {
        propagate_return(super_sync_res);
    }
    if (record.is_block_mount) {
        auto cache_res = blk::BlkManager::inst().lookup_cache(record.devno);
        propagate(cache_res);
        auto cache_sync_future = cache_res.value()->sync_all();
        auto cache_sync_res =
            wait::blocking_wait_for(cache_sync_future);
        propagate(cache_sync_res);
    }

    util::owner<VSuperblock *> vsb = record.superblock;
    if (!record.is_block_mount) {
        this->pseudo_mounts.erase(vsb->vfsd().fsd()->name());
    }
    VINode *parent_vinode = record.parent_vinode;
    this->mount_table.erase(key_res.value().first);
    if (parent_vinode != nullptr) {
        parent_vinode->release();
    }

    Result<void> ret = vsb->vfsd().fsd()->unmount(vsb->sb());
    delete vsb.get();
    return ret;
}

namespace {
    constexpr size_t kMaxSymlinkDepth = 16;

    [[nodiscard]]
    Result<void> validate_relpath(const char *relpath) {
        if (relpath == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (*relpath == '\0') {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (util::Path::from(relpath).is_absolute()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        void_return();
    }

    [[nodiscard]]
    Result<void> validate_file_oflags(flags::oflg_t oflags) {
        using namespace flags;
        constexpr oflg_t valid_mask = O_READ | O_WRITE | O_EXECUTE | O_CREAT;
        if ((oflags & ~valid_mask) != 0 || oflags == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if ((oflags & O_EXECUTE) != 0 && oflags != O_EXECUTE) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        void_return();
    }

    [[nodiscard]]
    Result<void> validate_dir_oflags(flags::oflg_t oflags) {
        using namespace flags;
        constexpr oflg_t valid_mask = O_READ | O_WRITE | O_EXECUTE;
        if ((oflags & ~valid_mask) != 0 || oflags == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        void_return();
    }

    struct CreateTarget {
        util::Path parent_path;
        std::string name;
    };

    [[nodiscard]]
    Result<CreateTarget> parse_create_target(const char *relpath) {
        auto valid_res = validate_relpath(relpath);
        propagate(valid_res);

        auto norm_path = util::Path::from(relpath).normalize();
        if (norm_path == "." || norm_path == "..") {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto name_path = norm_path.filename();
        auto name_view = name_path.view();
        if (name_view.empty() || name_view == "." || name_view == "..") {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto parent_path = norm_path.parent_path();
        if (parent_path.view().empty()) {
            parent_path = ".";
        }

        return CreateTarget{
            .parent_path = parent_path.normalize(),
            .name        = std::string(name_view),
        };
    }

    [[nodiscard]]
    bool is_lookup_missing(const Result<inode_t> &lookup_res) {
        return !lookup_res.has_value() &&
               lookup_res.error() == ErrCode::ENTRY_NOT_FOUND;
    }

    [[nodiscard]]
    b64 file_perm_from_oflags(flags::oflg_t oflags) {
        b64 perm = perm::basic::CLONE;
        if ((oflags & flags::O_READ) != 0) {
            perm |= perm::vfile::READ;
        }
        if ((oflags & flags::O_WRITE) != 0) {
            perm |= perm::vfile::WRITE;
        }
        if ((oflags & flags::O_EXECUTE) != 0) {
            perm |= perm::vfile::EXEC;
        }
        return perm;
    }

    [[nodiscard]]
    b64 dir_perm_from_oflags(flags::oflg_t oflags) {
        b64 perm = perm::basic::CLONE;
        if ((oflags & flags::O_READ) != 0) {
            perm |= perm::vdir::READ;
        }
        if ((oflags & flags::O_WRITE) != 0) {
            perm |= perm::vdir::WRITE;
        }
        if ((oflags & flags::O_EXECUTE) != 0) {
            perm |= perm::vdir::EXEC;
        }
        return perm;
    }
}  // namespace

Result<util::refc_ptr<VINode>> VFS::_resolve_from(util::refc_ptr<VINode> base,
                                                  const util::Path &base_path,
                                                  const util::Path &path,
                                                  VSuperblock *vsb) const {
    util::Path current_mount_path = base_path;
    return _resolve_path(base, current_mount_path, base_path, path, vsb,
                         kMaxSymlinkDepth, true);
}

Result<util::refc_ptr<VINode>> VFS::_follow_symlink(
    util::refc_ptr<VINode> symlink_vnode, const util::Path &mount_path,
    const util::Path &symlink_path, const util::Path &remaining_path,
    size_t symlink_budget, bool follow_final_symlink) const {
    if (symlink_budget == 0) {
        unexpect_return(ErrCode::OUT_OF_BOUNDARY);
    }

    auto target_res =
        symlink_vnode->superblock().sb()->readlink(symlink_vnode->inode()->inode_id());
    propagate(target_res);

    const util::Path target_path = util::Path::from(target_res.value()).normalize();
    const util::Path combined =
        remaining_path.view().empty() ? target_path
                                      : (target_path / remaining_path).normalize();

    if (target_path.is_absolute()) {
        util::Path next_mount_path;
        if (follow_final_symlink) {
            return _resolve_inode(combined, next_mount_path);
        }
        return _resolve_inode_no_follow(combined, next_mount_path);
    }

    const util::Path parent_path = symlink_path.parent_path().normalize();
    util::Path parent_mount_path;
    auto parent_res = _resolve_inode(parent_path, parent_mount_path);
    if (!parent_res.has_value()) {
        propagate_return(parent_res);
    }
    return _resolve_path(parent_res.value(), parent_mount_path, parent_path, combined,
                         &parent_res.value()->superblock(), symlink_budget - 1,
                         follow_final_symlink);
}

Result<util::refc_ptr<VINode>> VFS::_resolve_path(
    util::refc_ptr<VINode> base, util::Path &mount_path,
    const util::Path &base_path, const util::Path &path, VSuperblock *vsb,
    size_t symlink_budget, bool follow_final_symlink) const {
    auto current = base;
    if (current.get() == nullptr || vsb == nullptr) {
        unexpect_return(ErrCode::NULLPTR);
    }
    const util::Path relpath = path.normalize();
    if (relpath == ".") {
        return base;
    }
    util::Path current_path = base_path.normalize();
    std::vector<std::string> entries;
    for (const auto &entry : relpath) {
        entries.emplace_back(entry);
    }

    for (size_t idx = 0; idx < entries.size(); ++idx) {
        const std::string_view entry(entries[idx]);
        util::Path next_path = (current_path / util::Path(entry)).normalize();
        auto key_res = _entry_mount_key(current.get(), entry);
        propagate(key_res);
        auto mount_res = mount_table.at_nt(key_res.value());
        if (mount_res.has_value()) {
            auto *mount_record = mount_res.value();
            auto root_res      = mount_record->superblock->sb()->root();
            propagate(root_res);
            auto mounted_vnode =
                mount_record->superblock->get_vnode(root_res.value());
            propagate(mounted_vnode);
            mount_path   = mount_record->mount_path;
            current      = mounted_vnode.value();
            current_path = next_path;
            vsb          = mount_record->superblock.get();
            continue;
        }

        auto lookup_res = current->inode()->as_directory().and_then(
            [entry](IDirectory *dir) { return dir->lookup(entry); });
        propagate(lookup_res);

        auto next_vind = vsb->get_vnode(lookup_res.value());
        propagate(next_vind);
        auto symlink_res =
            next_vind.value()->superblock().sb()->is_symlink(lookup_res.value());
        propagate(symlink_res);
        const bool is_final_component = idx + 1 == entries.size();
        if (symlink_res.value() &&
            (follow_final_symlink || !is_final_component))
        {
            util::Path remaining(".");
            if (idx + 1 < entries.size()) {
                remaining = util::Path(entries[idx + 1]);
                for (size_t rem = idx + 2; rem < entries.size(); ++rem) {
                    remaining = (remaining / util::Path(entries[rem])).normalize();
                }
            }
            return _follow_symlink(next_vind.value(), mount_path, next_path,
                                   remaining, symlink_budget,
                                   follow_final_symlink);
        }
        current      = next_vind.value();
        current_path = next_path;
    }
    return current;
}

Result<VFile *> VFS::_open_file_at(VINode &parent, const util::Path &mount_path,
                                   const char *relpath) {
    auto valid_res = validate_relpath(relpath);
    propagate(valid_res);

    auto parent_dir_res = parent.inode()->as_directory();
    propagate(parent_dir_res);

    auto target_res =
        _resolve_from(util::refc_ptr(&parent), mount_path,
                      util::Path::from(relpath).normalize(),
                      &parent.superblock());
    propagate(target_res);
    auto file_res = target_res.value()->inode()->as_file();
    propagate(file_res);

    auto *file = new VFile(*target_res.value().get(), mount_path, *this);
    if (file == nullptr) {
        unexpect_return(ErrCode::ALLOCATION_FAILED);
    }

    auto key_res = _build_mount_key(mount_path);
    propagate(key_res);
    auto mount_res = _lookup_mount_record(key_res.value().first);
    if (!mount_res.has_value()) {
        delete file;
        unexpect_return(ErrCode::FS_ERROR);
    }
    mount_res.value()->active_files++;
    return file;
}

Result<VDirectory *> VFS::_open_dir_at(VINode &parent,
                                       const util::Path &mount_path,
                                       const util::Path &base_path,
                                       const char *relpath) {
    (void)mount_path;
    (void)parent;
    auto valid_res = validate_relpath(relpath);
    propagate(valid_res);

    const util::Path normalized_base = base_path.normalize();
    const util::Path absolute_base =
        normalized_base.view().empty() || !normalized_base.is_absolute()
            ? util::Path("/")
            : normalized_base;
    const util::Path global_path =
        (absolute_base / util::Path::from(relpath)).normalize();

    util::Path target_mount_path;
    auto target_res = _resolve_inode(global_path, target_mount_path);
    if (!target_res.has_value()) {
        loggers::VFS::ERROR("VFS opendir_at resolve failed: global=%s err=%s",
                            global_path.c_str(),
                            to_cstring(target_res.error()));
        propagate_return(target_res);
    }
    auto dir_res = target_res.value()->inode()->as_directory();
    if (!dir_res.has_value()) {
        loggers::VFS::ERROR(
            "VFS opendir_at target is not directory: global=%s mount=%s inode=%u err=%s",
            global_path.c_str(), target_mount_path.c_str(),
            static_cast<unsigned>(target_res.value()->inode()->inode_id()),
            to_cstring(dir_res.error()));
        propagate_return(dir_res);
    }

    auto *dir =
        new VDirectory(*target_res.value().get(), target_mount_path,
                       global_path, *this);
    if (dir == nullptr) {
        unexpect_return(ErrCode::ALLOCATION_FAILED);
    }

    auto key_res = _build_mount_key(target_mount_path);
    propagate(key_res);
    auto mount_res = _lookup_mount_record(key_res.value().first);
    if (!mount_res.has_value()) {
        delete dir;
        unexpect_return(ErrCode::FS_ERROR);
    }
    mount_res.value()->active_files++;
    return dir;
}

Result<VFile *> VFS::_open_file(const char *filepath) {
    if (filepath == nullptr) {
        unexpect_return(ErrCode::NULLPTR);
    }
    if (*filepath == '\0') {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    util::Path mount_path;
    auto vind_res =
        _resolve_inode(util::Path::from(filepath).normalize(), mount_path);
    propagate(vind_res);

    auto *file = new VFile(*vind_res.value().get(), mount_path, *this);
    if (file == nullptr) {
        unexpect_return(ErrCode::ALLOCATION_FAILED);
    }

    auto key_res = _build_mount_key(mount_path);
    propagate(key_res);
    auto mount_res = _lookup_mount_record(key_res.value().first);
    if (!mount_res.has_value()) {
        delete file;
        unexpect_return(ErrCode::FS_ERROR);
    }
    mount_res.value()->active_files++;
    return file;
}

Result<VDirectory *> VFS::_open_dir(const char *filepath) {
    if (filepath == nullptr) {
        unexpect_return(ErrCode::NULLPTR);
    }
    if (*filepath == '\0') {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    util::Path mount_path;
    auto vind_res =
        _resolve_inode(util::Path::from(filepath).normalize(), mount_path);
    propagate(vind_res);
    auto dir_res = vind_res.value()->inode()->as_directory();
    if (!dir_res.has_value()) {
        // stale VINode cache after inode reuse
        inode_t stale_id = vind_res.value()->inode()->inode_id();
        loggers::VFS::INFO("_open_dir: invalidate stale inode=%u and retry",
                            static_cast<unsigned>(stale_id));
        auto invalidate_res =
            vind_res.value()->superblock().invalidate_inode(stale_id);
        propagate(invalidate_res);
        vind_res =
            _resolve_inode(util::Path::from(filepath).normalize(), mount_path);
        propagate(vind_res);
        dir_res = vind_res.value()->inode()->as_directory();
        propagate(dir_res);
    }

    auto *dir =
        new VDirectory(*vind_res.value().get(), mount_path,
                       util::Path::from(filepath).normalize(), *this);
    if (dir == nullptr) {
        unexpect_return(ErrCode::ALLOCATION_FAILED);
    }

    auto key_res = _build_mount_key(mount_path);
    propagate(key_res);
    auto mount_res = _lookup_mount_record(key_res.value().first);
    if (!mount_res.has_value()) {
        delete dir;
        unexpect_return(ErrCode::FS_ERROR);
    }
    mount_res.value()->active_files++;
    return dir;
}

Result<CapIdx> VFS::open(const char *filepath, cap::CHolder &holder) {
    auto file_res = _open_file(filepath);
    propagate(file_res);

    auto insert_res = holder.insert_to_free(file_res.value());
    if (!insert_res.has_value()) {
        file_res.value()->destruct();
        propagate_return(insert_res);
    }
    return insert_res.value();
}

Result<CapIdx> VFS::open(cap::Capability &parent_dir_cap, const char *relpath,
                         flags::oflg_t oflags, cap::CHolder &holder) {
    auto oflag_res = validate_file_oflags(oflags);
    propagate(oflag_res);

    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }

    if ((oflags & flags::O_EXECUTE) == 0) {
        auto ensure_res = _ensure_parent_directory(*parent, relpath);
        propagate(ensure_res);
    }

    auto global_res = _global_target_path(*parent, relpath);
    propagate(global_res);
    auto file_res = _open_file_at(*parent->vinode(), parent->mount_path(),
                                  relpath);
    if (!file_res.has_value() && file_res.error() == ErrCode::ENTRY_NOT_FOUND &&
        (oflags & flags::O_EXECUTE) == 0 &&
        (oflags & flags::O_CREAT) != 0)
    {
        auto create_res = mkfile(parent_dir_cap, relpath, oflags, holder);
        propagate(create_res);
        return create_res.value();
    }
    propagate(file_res);

    auto insert_res =
        holder.insert_to_free(file_res.value(), file_perm_from_oflags(oflags));
    if (!insert_res.has_value()) {
        file_res.value()->destruct();
        propagate_return(insert_res);
    }
    return insert_res.value();
}

Result<CapIdx> VFS::opendir(cap::Capability &parent_dir_cap, const char *relpath,
                            flags::oflg_t oflags, cap::CHolder &holder) {
    auto oflag_res = validate_dir_oflags(oflags);
    propagate(oflag_res);

    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }

    auto dir_res = _open_dir_at(*parent->vinode(), parent->mount_path(),
                                parent->global_path(), relpath);
    propagate(dir_res);

    auto insert_res =
        holder.insert_to_free(dir_res.value(), dir_perm_from_oflags(oflags));
    if (!insert_res.has_value()) {
        dir_res.value()->destruct();
        propagate_return(insert_res);
    }
    return insert_res.value();
}

Result<CapIdx> VFS::mkfile(cap::Capability &parent_dir_cap, const char *relpath,
                           flags::oflg_t oflags, cap::CHolder &holder) {
    auto oflag_res = validate_file_oflags(oflags);
    propagate(oflag_res);

    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }

    auto create_target_res = parse_create_target(relpath);
    propagate(create_target_res);
    const auto &target = create_target_res.value();

    auto create_parent_res = _ensure_parent_directory(*parent, relpath);
    propagate(create_parent_res);

    auto target_dir_res = create_parent_res.value()->inode()->as_directory();
    propagate(target_dir_res);

    auto inode_res = target_dir_res.value()->mkfile(target.name, nullptr);
    propagate(inode_res);
    (void)inode_res;

    // refresh parent dir VINode so subsequent lookup sees the new entry
    auto invalidate_res = create_parent_res.value()->superblock().invalidate_inode(
        create_parent_res.value()->inode()->inode_id());
    propagate(invalidate_res);

    auto global_res = _global_target_path(*parent, relpath);
    propagate(global_res);
    auto file_res = _open_file(global_res.value().second.c_str());
    propagate(file_res);

    auto insert_res =
        holder.insert_to_free(file_res.value(), file_perm_from_oflags(oflags));
    if (!insert_res.has_value()) {
        file_res.value()->destruct();
        propagate_return(insert_res);
    }
    return insert_res.value();
}

Result<CapIdx> VFS::mkdir(cap::Capability &parent_dir_cap, const char *relpath,
                          flags::oflg_t oflags, cap::CHolder &holder) {
    auto oflag_res = validate_dir_oflags(oflags);
    propagate(oflag_res);

    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }

    auto create_target_res = parse_create_target(relpath);
    propagate(create_target_res);
    const auto &target = create_target_res.value();

    auto create_parent_res = _ensure_parent_directory(*parent, relpath);
    propagate(create_parent_res);

    auto target_dir_res = create_parent_res.value()->inode()->as_directory();
    propagate(target_dir_res);

    auto inode_res = target_dir_res.value()->mkdir(target.name, nullptr);
    propagate(inode_res);
    (void)inode_res;

    auto global_res = _global_target_path(*parent, relpath);
    propagate(global_res);
    auto dir_res = _open_dir(global_res.value().second.c_str());
    propagate(dir_res);

    auto insert_res =
        holder.insert_to_free(dir_res.value(), dir_perm_from_oflags(oflags));
    if (!insert_res.has_value()) {
        dir_res.value()->destruct();
        propagate_return(insert_res);
    }
    return insert_res.value();
}

Result<void> VFS::unlink(cap::Capability &parent_dir_cap,
                        const char *relpath) {
    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    auto target_res = parse_create_target(relpath);
    propagate(target_res);
    auto create_parent_res = _ensure_parent_directory(*parent, relpath);
    propagate(create_parent_res);
    auto target_dir_res = create_parent_res.value()->inode()->as_directory();
    propagate(target_dir_res);

    // lookup target inode before delete so we can evict its VINode cache
    auto lookup_res = target_dir_res.value()->lookup(target_res.value().name);
    propagate(lookup_res);
    auto vnode_res = create_parent_res.value()->superblock().get_vnode(
        lookup_res.value());
    propagate(vnode_res);
    auto flush_res = vnode_res.value()->flush_file_pages();
    propagate(flush_res);

    auto unlink_res = target_dir_res.value()->unlink(target_res.value().name);
    propagate(unlink_res);
    // evict the freed inode's VINode from cache
    auto evict_res = create_parent_res.value()->superblock().evict_inode(
        lookup_res.value());
    propagate(evict_res);
    void_return();
}

Result<void> VFS::rmdir(cap::Capability &parent_dir_cap,
                       const char *relpath) {
    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    auto target_res = parse_create_target(relpath);
    propagate(target_res);
    auto create_parent_res = _ensure_parent_directory(*parent, relpath);
    propagate(create_parent_res);
    auto target_dir_res = create_parent_res.value()->inode()->as_directory();
    propagate(target_dir_res);

    auto lookup_res = target_dir_res.value()->lookup(target_res.value().name);
    propagate(lookup_res);
    auto rmdir_res = target_dir_res.value()->rmdir(target_res.value().name);
    propagate(rmdir_res);
    auto evict_res = create_parent_res.value()->superblock().evict_inode(
        lookup_res.value());
    propagate(evict_res);
    void_return();
}

Result<void> VFS::truncate(cap::Capability &file_cap, size_t new_size) {
    auto *vfile = file_cap.payload_as<VFile>();
    if (vfile == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    auto file_res = vfile->vinode()->inode()->as_file();
    propagate(file_res);
    auto flush_res = vfile->vinode()->flush_file_pages();
    propagate(flush_res);
    auto truncate_res = file_res.value()->truncate(new_size);
    propagate(truncate_res);
    vfile->vinode()->invalidate_file_pages();
    void_return();
}

Result<void> VFS::link(cap::Capability &parent_dir_cap,
                      const char *relpath,
                      cap::Capability &target_inode_cap) {
    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    inode_t target = 0;
    if (auto *vfile = target_inode_cap.payload_as<VFile>(); vfile != nullptr) {
        target = vfile->vinode()->inode()->inode_id();
    } else if (auto *vdir = target_inode_cap.payload_as<VDirectory>();
               vdir != nullptr)
    {
        target = vdir->vinode()->inode()->inode_id();
    } else {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    auto target_res = parse_create_target(relpath);
    propagate(target_res);
    auto create_parent_res = _ensure_parent_directory(*parent, relpath);
    propagate(create_parent_res);
    auto target_dir_res = create_parent_res.value()->inode()->as_directory();
    propagate(target_dir_res);
    return target_dir_res.value()->link(target_res.value().name, target);
}

Result<void> VFS::rename(cap::Capability &old_parent_cap,
                          const char *old_name,
                          cap::Capability &new_parent_cap,
                          const char *new_name) {
    auto *old_parent = old_parent_cap.payload_as<VDirectory>();
    auto *new_parent = new_parent_cap.payload_as<VDirectory>();
    if (old_parent == nullptr || new_parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    auto old_target_res = parse_create_target(old_name);
    propagate(old_target_res);
    auto new_target_res = parse_create_target(new_name);
    propagate(new_target_res);

    auto old_dir_parent = _ensure_parent_directory(*old_parent, old_name);
    propagate(old_dir_parent);
    auto new_dir_parent = _ensure_parent_directory(*new_parent, new_name);
    propagate(new_dir_parent);

    auto old_dir_res = old_dir_parent.value()->inode()->as_directory();
    propagate(old_dir_res);
    auto new_dir_res = new_dir_parent.value()->inode()->as_directory();
    propagate(new_dir_res);

    return old_dir_res.value()->rename(old_target_res.value().name,
                                        *new_dir_res.value(),
                                        new_target_res.value().name);
}

Result<void> VFS::symlink(cap::Capability &parent_dir_cap,
                          const char *relpath, const char *target) {
    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    if (target == nullptr || target[0] == '\0') {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto create_target_res = parse_create_target(relpath);
    propagate(create_target_res);
    const auto &ctgt = create_target_res.value();

    auto create_parent_res = _ensure_parent_directory(*parent, relpath);
    propagate(create_parent_res);

    auto target_dir_res =
        create_parent_res.value()->inode()->as_directory();
    propagate(target_dir_res);

    auto inode_res =
        target_dir_res.value()->symlink(ctgt.name, target);
    propagate(inode_res);
    auto invalidate_res = create_parent_res.value()->superblock().invalidate_inode(
        create_parent_res.value()->inode()->inode_id());
    propagate(invalidate_res);
    void_return();
}

Result<CapIdx> VFS::open_dir(const char *filepath, cap::CHolder &holder,
                             b64 perm) {
    auto dir_res = _open_dir(filepath);
    propagate(dir_res);
    auto insert_res = holder.insert_to_free(dir_res.value(), perm);
    if (!insert_res.has_value()) {
        dir_res.value()->destruct();
        propagate_return(insert_res);
    }
    return insert_res.value();
}

Result<ISuperblock *> VFS::get_pseudo(const char *pseudo_fs_id) {
    if (pseudo_fs_id == nullptr) {
        unexpect_return(ErrCode::NULLPTR);
    }
    auto lookup_res = pseudo_mounts.at_nt(pseudo_fs_id);
    if (!lookup_res.has_value() || *lookup_res.value() == nullptr) {
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }
    return (*lookup_res.value())->sb();
}

Result<devfs::DevFSSuperblock *> VFS::devfs() {
    auto pseudo_res = get_pseudo("devfs");
    propagate(pseudo_res);
    return static_cast<devfs::DevFSSuperblock *>(pseudo_res.value());
}

Result<VFile *> VFS::__debug_open(const char *filepath) {
    return _open_file(filepath);
}

Result<VFS::MountRecord *> VFS::_lookup_mount_record(const MountKey &key) const {
    auto record_res = mount_table.at_nt(key);
    if (!record_res.has_value()) {
        unexpect_return(ErrCode::ENTRY_NOT_FOUND);
    }
    return const_cast<MountRecord *>(record_res.value());
}

Result<std::pair<VFS::MountKey, util::Path>> VFS::_build_mount_key(
    const util::Path &mount_path) {
    const util::Path normalized = mount_path.normalize();
    if (normalized == "/") {
        return std::make_pair(MountKey{.parent = nullptr, .entry = "/"},
                              normalized);
    }

    util::Path parent_path = normalized.parent_path().normalize();
    auto entry             = normalized.filename();
    auto parent_vinode_res =
        _resolve_inode(parent_path, parent_path).and_then([](auto vnode) {
            return Result<VINode *>(vnode.get());
        });
    propagate(parent_vinode_res);
    return std::make_pair(
        MountKey{
            .parent = parent_vinode_res.value(),
            .entry  = std::string(entry.view()),
        },
        normalized);
}

Result<VFS::MountKey> VFS::_entry_mount_key(VINode *parent,
                                            std::string_view entry) const {
    return MountKey{
        .parent = parent,
        .entry  = std::string(entry),
    };
}

Result<std::pair<util::Path, util::Path>> VFS::_global_target_path(
    const VDirectory &base, const char *relpath) const {
    auto valid_res = validate_relpath(relpath);
    propagate(valid_res);
    const util::Path normalized_base = base.global_path().normalize();
    const util::Path base_path =
        normalized_base.view().empty() || !normalized_base.is_absolute()
            ? util::Path("/")
            : normalized_base;
    auto normalized =
        (base_path / util::Path::from(relpath)).normalize();
    return std::make_pair(base_path, normalized);
}

Result<util::refc_ptr<VINode>> VFS::_ensure_directory_path(
    util::refc_ptr<VINode> base, const util::Path &mount_path,
    const util::Path &base_path, const util::Path &dir_path) {
    (void)mount_path;
    auto current = base;
    if (current.get() == nullptr) {
        unexpect_return(ErrCode::NULLPTR);
    }

    const util::Path normalized = dir_path.normalize();
    if (normalized == "." || normalized.view().empty()) {
        return current;
    }

    util::Path current_path = base_path.normalize();
    for (const auto &entry : normalized) {
        util::Path next_path = (current_path / util::Path(entry)).normalize();
        auto key_res         = _entry_mount_key(current.get(), entry);
        propagate(key_res);
        auto mount_res = mount_table.at_nt(key_res.value());
        if (mount_res.has_value()) {
            auto root_res = mount_res.value()->superblock->sb()->root();
            propagate(root_res);
            auto next_res =
                mount_res.value()->superblock->get_vnode(root_res.value());
            propagate(next_res);
            current      = next_res.value();
            current_path = next_path;
            continue;
        }

        auto dir_res = current->inode()->as_directory();
        propagate(dir_res);
        auto lookup_res = dir_res.value()->lookup(entry);
        if (is_lookup_missing(lookup_res)) {
            auto mkdir_res = dir_res.value()->mkdir(entry, nullptr);
            propagate(mkdir_res);
            lookup_res = mkdir_res;
        } else {
            propagate(lookup_res);
        }

        auto next_res = current->superblock().get_vnode(lookup_res.value());
        propagate(next_res);
        auto next_dir_res = next_res.value()->inode()->as_directory();
        propagate(next_dir_res);
        current      = next_res.value();
        current_path = next_path;
    }

    return current;
}

Result<util::refc_ptr<VINode>> VFS::_ensure_parent_directory(
    const VDirectory &base, const char *relpath) {
    auto target_res = parse_create_target(relpath);
    propagate(target_res);
    return _ensure_directory_path(util::refc_ptr(base.vinode().get()),
                                  base.mount_path(), base.global_path(),
                                  target_res.value().parent_path);
}

Result<void> VFS::_ensure_mountpoint_path(const util::Path &mount_path) {
    const util::Path normalized = mount_path.normalize();
    if (normalized == "/") {
        void_return();
    }

    auto root_res = _lookup_mount_record(MountKey{.parent = nullptr,
                                                  .entry = "/"});
    propagate(root_res);
    auto root_inode_res = root_res.value()->superblock->sb()->root();
    propagate(root_inode_res);
    auto root_vnode_res =
        root_res.value()->superblock->get_vnode(root_inode_res.value());
    propagate(root_vnode_res);

    auto ensure_res =
        _ensure_directory_path(root_vnode_res.value(),
                               root_res.value()->mount_path, "/",
                               normalized.relative_to("/"));
    propagate(ensure_res);
    void_return();
}

Result<util::refc_ptr<VINode>> VFS::_resolve_inode(const util::Path &path,
                                                   util::Path &mount_path) const {
    const util::Path normalized = path.normalize();
    auto root_key_res = _lookup_mount_record(MountKey{.parent = nullptr,
                                                      .entry = "/"});
    propagate(root_key_res);
    mount_path       = root_key_res.value()->mount_path;
    VSuperblock *vsb = root_key_res.value()->superblock.get();

    auto root_res = vsb->sb()->root();
    propagate(root_res);

    auto current_res = vsb->get_vnode(root_res.value());
    propagate(current_res);
    auto current = current_res.value();

    if (normalized == "/") {
        return current;
    }

    return _resolve_path(current, mount_path, "/", normalized.relative_to("/"),
                         vsb, kMaxSymlinkDepth, true);
}

Result<util::refc_ptr<VINode>> VFS::_resolve_inode_no_follow(
    const util::Path &path, util::Path &mount_path) const {
    const util::Path normalized = path.normalize();
    auto root_key_res = _lookup_mount_record(MountKey{.parent = nullptr,
                                                      .entry = "/"});
    propagate(root_key_res);
    mount_path       = root_key_res.value()->mount_path;
    VSuperblock *vsb = root_key_res.value()->superblock.get();

    auto root_res = vsb->sb()->root();
    propagate(root_res);

    auto current_res = vsb->get_vnode(root_res.value());
    propagate(current_res);
    auto current = current_res.value();

    if (normalized == "/") {
        return current;
    }

    return _resolve_path(current, mount_path, "/", normalized.relative_to("/"),
                         vsb, kMaxSymlinkDepth, false);
}

Result<void> VFS::_stat_from_vinode(VINode &vnode, NodeMeta &out) const {
    out.inode = vnode.inode()->inode_id();

    auto symlink_res = vnode.superblock().sb()->is_symlink(out.inode);
    propagate(symlink_res);
    if (symlink_res.value()) {
        out.type = EntryType::SYMLINK;
        auto target_res = vnode.superblock().sb()->readlink(out.inode);
        propagate(target_res);
        out.size = target_res.value().size();
        void_return();
    }

    auto file_res = vnode.inode()->as_file();
    if (file_res.has_value()) {
        out.type = EntryType::FILE;
        auto flush_res = vnode.flush_file_pages();
        propagate(flush_res);
        auto size_res = file_res.value()->size();
        propagate(size_res);
        out.size = size_res.value();
        void_return();
    }

    auto dir_res = vnode.inode()->as_directory();
    if (dir_res.has_value()) {
        out.type = EntryType::DIR;
        out.size = 0;
        void_return();
    }

    unexpect_return(ErrCode::TYPE_NOT_MATCHED);
}

std::vector<DirectoryEntryInfo> VFS::_append_mount_entries(
    const VDirectory &vdir, std::vector<DirectoryEntryInfo> entries) const {
    VINode *parent = vdir.vinode().get();
    for (const auto &[key, _] : mount_table) {
        if (key.parent != parent) {
            continue;
        }
        const std::string &name = key.entry;
        bool duplicated = false;
        for (const auto &entry : entries) {
            if (entry.name == name) {
                duplicated = true;
                break;
            }
        }
        if (!duplicated) {
            entries.push_back(DirectoryEntryInfo{
                .name = name,
            });
        }
    }
    return entries;
}

void VFS::_on_vfile_destroy(const util::Path &mount_path) noexcept {
    auto key_res = _build_mount_key(mount_path);
    if (!key_res.has_value()) {
        loggers::VFS::WARN("VFile 销毁时挂载点 key 解析失败: %s",
                           mount_path.c_str());
        return;
    }
    auto active_res = mount_table.at_nt(key_res.value().first);
    if (!active_res.has_value()) {
        loggers::VFS::WARN("VFile 销毁时找不到挂载点: %s", mount_path.c_str());
        return;
    }
    MountRecord &record = *active_res.value();
    if (record.active_files == 0) {
        loggers::VFS::WARN("VFile 活跃计数已经为 0: %s", mount_path.c_str());
        return;
    }
    record.active_files--;
}

Result<size_t> VFS::read(VFile &vfile, off_t offset, void *buf,
                         size_t len) const {
    if (offset < 0 || (len != 0 && buf == nullptr)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto file_res = vfile.vinode()->inode()->as_file();
    propagate(file_res);
    IFile *file = file_res.value();

    if (file->file_cache() != FileCachePolicy::NONE) {
        return vfile.vinode()->read_cached_file(
            *file, static_cast<size_t>(offset), buf, len);
    }

    auto read_res = file->read(offset, buf, len);
    propagate(read_res);
    return read_res.value();
}

Result<size_t> VFS::write(VFile &vfile, off_t offset, const void *buf,
                          size_t len) const {
    if (offset < 0 || (len != 0 && buf == nullptr)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto file_res = vfile.vinode()->inode()->as_file();
    propagate(file_res);
    IFile *file = file_res.value();

    if (file->file_cache() != FileCachePolicy::NONE) {
        return vfile.vinode()->write_cached_file(
            *file, static_cast<size_t>(offset), buf, len);
    }

    auto write_res = file->write(offset, buf, len);
    propagate(write_res);

    if (file->file_cache() == FileCachePolicy::NONE) {
        auto sync_res = file->sync();
        if (!sync_res.has_value() && sync_res.error() != ErrCode::NOT_SUPPORTED)
        {
            propagate_return(sync_res);
        }
    }
    return write_res.value();
}

Result<size_t> VFS::size(VFile &vfile) const {
    auto file_res = vfile.vinode()->inode()->as_file();
    propagate(file_res);
    auto flush_res = vfile.vinode()->flush_file_pages();
    propagate(flush_res);
    return file_res.value()->size();
}

Result<std::vector<DirectoryEntryInfo>> VFS::getdents(VDirectory &vdir) const {
    auto dir_res = vdir.vinode()->inode()->as_directory();
    propagate(dir_res);
    auto count_res = dir_res.value()->entry_count();
    propagate(count_res);
    std::vector<DirectoryEntryInfo> entries{};
    entries.reserve(count_res.value());
    for (size_t i = 0; i < count_res.value(); ++i) {
        auto entry_res = dir_res.value()->entry_at(i);
        propagate(entry_res);
        entries.push_back(entry_res.value());
    }
    return _append_mount_entries(vdir, std::move(entries));
}

Result<void> VFS::stat(cap::Capability &parent_dir_cap, const char *relpath,
                       NodeMeta &out) const {
    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    auto global_res = _global_target_path(*parent, relpath);
    propagate(global_res);
    util::Path mount_path;
    auto vnode_res = _resolve_inode(global_res.value().second, mount_path);
    propagate(vnode_res);
    return _stat_from_vinode(*vnode_res.value().get(), out);
}

Result<void> VFS::lstat(cap::Capability &parent_dir_cap, const char *relpath,
                        NodeMeta &out) const {
    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    auto global_res = _global_target_path(*parent, relpath);
    propagate(global_res);
    util::Path mount_path;
    auto vnode_res =
        _resolve_inode_no_follow(global_res.value().second, mount_path);
    propagate(vnode_res);
    return _stat_from_vinode(*vnode_res.value().get(), out);
}

Result<size_t> VFS::readlink(cap::Capability &parent_dir_cap,
                             const char *relpath, char *buf,
                             size_t bufsiz) const {
    auto *parent = parent_dir_cap.payload_as<VDirectory>();
    if (parent == nullptr) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }
    if (buf == nullptr && bufsiz != 0) {
        unexpect_return(ErrCode::NULLPTR);
    }
    auto global_res = _global_target_path(*parent, relpath);
    propagate(global_res);
    util::Path mount_path;
    auto vnode_res =
        _resolve_inode_no_follow(global_res.value().second, mount_path);
    propagate(vnode_res);

    auto symlink_res =
        vnode_res.value()->superblock().sb()->is_symlink(vnode_res.value()->inode()->inode_id());
    propagate(symlink_res);
    if (!symlink_res.value()) {
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }

    auto target_res =
        vnode_res.value()->superblock().sb()->readlink(vnode_res.value()->inode()->inode_id());
    propagate(target_res);
    const std::string &target = target_res.value();
    const size_t copied       = std::min(bufsiz, target.size());
    if (copied != 0) {
        memcpy(buf, target.data(), copied);
    }
    return copied;
}

Result<void> VFS::sync(VDirectory &vdir) const {
    auto dir_res = vdir.vinode()->inode()->as_directory();
    propagate(dir_res);
    return dir_res.value()->sync();
}

Result<void> VFS::sync(VFile &vfile) const {
    auto file_res = vfile.vinode()->inode()->as_file();
    propagate(file_res);
    auto flush_res = vfile.vinode()->flush_file_pages();
    propagate(flush_res);
    return file_res.value()->sync();
}
