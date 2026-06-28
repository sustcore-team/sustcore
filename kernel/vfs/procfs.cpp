/**
 * @file procfs.cpp
 * @author theflysong
 * @brief procfs 进程视图文件系统实现
 * @version alpha-1.0.0
 * @date 2026-06-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <logger.h>
#include <object/perm.h>
#include <task/scheduler.h>
#include <task/task.h>
#include <vfs/procfs.h>
#include <vfs/vfs.h>
#include <guard.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <env.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace procfs {
    namespace {
        constexpr uint16_t S_IFDIR = 0x4000;
        constexpr uint16_t S_IFLNK = 0xA000;
        bool _initialized = false;
        alignas(ProcFSDriver) byte _INSTANCE_STORAGE[sizeof(ProcFSDriver)];
        ProcFSDriver *_INSTANCE = nullptr;
    }  // namespace

    [[nodiscard]]
    std::string pid_to_string(pid_t pid) {
        char buf[32]{};
        snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(pid));
        return std::string(buf);
    }

    [[nodiscard]]
    std::vector<byte> make_nul_blob(const std::vector<std::string> &items) {
        std::vector<byte> out{};
        for (const auto &item : items) {
            out.insert(out.end(), item.begin(), item.end());
            out.push_back(0);
        }
        return out;
    }

    [[nodiscard]]
    std::vector<byte> make_u64_blob(const std::vector<uint64_t> &items) {
        std::vector<byte> out(items.size() * sizeof(uint64_t));
        if (!out.empty()) {
            memcpy(out.data(), items.data(), out.size());
        }
        return out;
    }

    [[nodiscard]]
    std::vector<byte> make_bsargv_blob(
        const std::vector<TaskSpec::BootstrapRecordData> &items) {
        std::vector<byte> out{};
        for (const auto &item : items) {
            out.insert(out.end(), item.bytes.begin(), item.bytes.end());
        }
        return out;
    }

    class BlobContainer final : public IPSContainer {
    public:
        static BlobContainer _INSTANCE;

        [[nodiscard]]
        Result<size_t> read(task::PCB &, task::ProcState &state,
                            std::string_view name, size_t offset, void *buf,
                            size_t len) const override {
            const std::vector<byte> *blob = nullptr;
            if (name == "cmdline") {
                blob = &state.cmdline_blob;
            } else if (name == "environ") {
                blob = &state.environ_blob;
            } else if (name == "bsargs") {
                blob = &state.bsargs_blob;
            } else if (name == "auxv") {
                blob = &state.auxv_blob;
            } else {
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            if (offset >= blob->size()) {
                return 0;
            }
            size_t actual = std::min(len, blob->size() - offset);
            if (actual != 0 && buf == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (actual != 0) {
                memcpy(buf, blob->data() + offset, actual);
            }
            return actual;
        }

        [[nodiscard]]
        Result<size_t> write(task::PCB &, task::ProcState &, std::string_view,
                             size_t, const void *, size_t) const override {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        [[nodiscard]]
        Result<void> redirect(task::PCB &, task::ProcState &, std::string_view,
                              std::string_view) const override {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
    };

    BlobContainer BlobContainer::_INSTANCE{};

    class CommContainer final : public IPSContainer {
    public:
        static CommContainer _INSTANCE;

        [[nodiscard]]
        Result<size_t> read(task::PCB &, task::ProcState &state,
                            std::string_view name, size_t offset, void *buf,
                            size_t len) const override {
            if (name != "comm") {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            if (offset >= state.comm.size()) {
                return 0;
            }
            size_t actual = std::min(len, state.comm.size() - offset);
            if (actual != 0 && buf == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (actual != 0) {
                memcpy(buf, state.comm.data() + offset, actual);
            }
            return actual;
        }

        [[nodiscard]]
        Result<size_t> write(task::PCB &, task::ProcState &state,
                             std::string_view name, size_t offset,
                             const void *buf, size_t len) const override {
            if (name != "comm" || offset != 0) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            if (len != 0 && buf == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            const auto *chars = static_cast<const char *>(buf);
            state.comm.assign(chars, chars + len);
            while (!state.comm.empty() &&
                   (state.comm.back() == '\0' || state.comm.back() == '\n'))
            {
                state.comm.pop_back();
            }
            return len;
        }

        [[nodiscard]]
        Result<void> redirect(task::PCB &, task::ProcState &, std::string_view,
                              std::string_view) const override {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
    };

    CommContainer CommContainer::_INSTANCE{};

    class LinkContainer final : public IPSContainer {
    public:
        static LinkContainer _INSTANCE;

        [[nodiscard]]
        Result<size_t> read(task::PCB &, task::ProcState &, std::string_view,
                            size_t, void *, size_t) const override {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        [[nodiscard]]
        Result<size_t> write(task::PCB &, task::ProcState &, std::string_view,
                             size_t, const void *, size_t) const override {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        [[nodiscard]]
        Result<void> redirect(task::PCB &, task::ProcState &state,
                              std::string_view name,
                              std::string_view target) const override {
            if (target.empty()) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            if (name == "exe") {
                state.exe_target.assign(target);
            } else if (name == "cwd") {
                state.cwd_target.assign(target);
            } else if (name == "root") {
                state.root_target.assign(target);
            } else {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            void_return();
        }
    };

    LinkContainer LinkContainer::_INSTANCE{};

    const ProcStateEntry PROC_STATE_ENTRIES[] = {
        ProcStateEntry{.name      = "comm",
                       .kind      = PSKind::WRITABLE,
                       .container = &CommContainer::_INSTANCE},
        ProcStateEntry{.name      = "cmdline",
                       .kind      = PSKind::READONLY,
                       .container = &BlobContainer::_INSTANCE},
        ProcStateEntry{.name      = "environ",
                       .kind      = PSKind::READONLY,
                       .container = &BlobContainer::_INSTANCE},
        ProcStateEntry{.name      = "bsargs",
                       .kind      = PSKind::READONLY,
                       .container = &BlobContainer::_INSTANCE},
        ProcStateEntry{.name      = "auxv",
                       .kind      = PSKind::READONLY,
                       .container = &BlobContainer::_INSTANCE},
        ProcStateEntry{.name      = "exe",
                       .kind      = PSKind::SYMLINK,
                       .container = &LinkContainer::_INSTANCE},
        ProcStateEntry{.name      = "cwd",
                       .kind      = PSKind::SYMLINK,
                       .container = &LinkContainer::_INSTANCE},
        ProcStateEntry{.name      = "root",
                       .kind      = PSKind::SYMLINK,
                       .container = &LinkContainer::_INSTANCE},
    };
    constexpr size_t PROC_STATE_ENTRIES_COUNT =
        sizeof(PROC_STATE_ENTRIES) / sizeof(PROC_STATE_ENTRIES[0]);

    const ProcStateEntry *lookup_entry(std::string_view name) noexcept {
        for (const auto &entry : PROC_STATE_ENTRIES) {
            if (name == entry.name) {
                return &entry;
            }
        }
        return nullptr;
    }

    Result<void> initialize_proc_state(
        task::ProcState &state, const std::string &comm,
        const std::vector<std::string> &argv,
        const std::vector<std::string> &envp, const std::vector<uint64_t> &auxv,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv) {
        state.comm         = comm;
        state.cmdline_blob = make_nul_blob(argv);
        state.environ_blob = make_nul_blob(envp);
        state.bsargs_blob  = make_bsargv_blob(bsargv);
        state.auxv_blob    = make_u64_blob(auxv);
        state.exe_target   = "/";
        state.cwd_target   = "/";
        state.root_target  = "/";
        void_return();
    }

    Result<void> clone_proc_state(const task::ProcState &src,
                                  task::ProcState &dst) {
        dst = src;
        void_return();
    }

    Result<void> init() {
        _initialized = true;
        void_return();
    }

    bool initialized() noexcept {
        return _initialized;
    }

    Result<void> register_process(task::PCB &pcb) {
        if (pcb.proc_state == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        void_return();
    }

    Result<void> unregister_process(task::PCB &) {
        void_return();
    }

    ProcFileNode::ProcFileNode(ProcFSSuperblock &sb, ProcNode &node) noexcept
        : _sb(&sb), _node(&node) {}

    MeminfoFile::MeminfoFile(ProcFSSuperblock &sb, ProcNode &node) noexcept
        : _sb(&sb), _node(&node) {}

    Result<void> MeminfoFile::getattr(AttrSet &out) const {
        out.mode    = 0444;
        out.uid     = 0;
        out.gid     = 0;
        auto size_res = const_cast<MeminfoFile *>(this)->size();
        if (!size_res.has_value()) { propagate_return(size_res); }
        out.size    = size_res.value();
        out.inode   = _node->inode_id;
        out.nlink   = 1;
        out.atime   = 0;
        out.mtime   = 0;
        out.ctime   = 0;
        out.blksize = 512;
        out.blocks  = (out.size + 511) / 512;
        void_return();
    }

    Result<void> MeminfoFile::setattr(AttrMask mask, const AttrSet &attrs) {
        (void)mask; (void)attrs;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> ProcFileNode::sync() {
        void_return();
    }

    IMetadata &ProcFileNode::metadata() {
        return _node->metadata;
    }

    inode_t ProcFileNode::inode_id() const {
        return _node->inode_id;
    }

    INodeCachePolicy ProcFileNode::inode_cache() const {
        return INodeCachePolicy::NONE;
    }

    Result<void> ProcFileNode::getattr(AttrSet &out) const {
        out.mode    = 0444;
        out.uid     = 0;
        out.gid     = 0;
        auto size_res = const_cast<ProcFileNode *>(this)->size();
        if (!size_res.has_value()) { propagate_return(size_res); }
        out.size    = size_res.value();
        out.inode   = _node->inode_id;
        out.nlink   = 1;
        out.atime   = 0;
        out.mtime   = 0;
        out.ctime   = 0;
        out.blksize = 512;
        out.blocks  = (out.size + 511) / 512;
        void_return();
    }

    Result<void> ProcFileNode::setattr(AttrMask mask, const AttrSet &attrs) {
        (void)mask; (void)attrs;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    ProcSymlinkNode::ProcSymlinkNode(ProcFSSuperblock &sb,
                                     ProcNode &node) noexcept
        : _sb(&sb), _node(&node) {}

    IMetadata &ProcSymlinkNode::metadata() {
        return _node->metadata;
    }

    inode_t ProcSymlinkNode::inode_id() const {
        return _node->inode_id;
    }

    INodeCachePolicy ProcSymlinkNode::inode_cache() const {
        return INodeCachePolicy::NONE;
    }

    Result<void> ProcSymlinkNode::getattr(AttrSet &out) const {
        out.mode    = S_IFLNK | 0777;
        out.uid     = 0;
        out.gid     = 0;
        auto target_res = const_cast<ProcSymlinkNode *>(this)->target();
        if (!target_res.has_value()) { propagate_return(target_res); }
        out.size    = target_res.value().size();
        out.inode   = _node->inode_id;
        out.nlink   = 1;
        out.atime   = 0;
        out.mtime   = 0;
        out.ctime   = 0;
        out.blksize = 512;
        out.blocks  = 0;
        void_return();
    }

    Result<void> ProcSymlinkNode::setattr(AttrMask mask, const AttrSet &attrs) {
        (void)mask; (void)attrs;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    ProcDirectoryNode::ProcDirectoryNode(ProcFSSuperblock &sb,
                                         ProcNode &node) noexcept
        : _sb(&sb), _node(&node) {}

    Result<inode_t> ProcDirectoryNode::mkfile(std::string_view, const char *) {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<inode_t> ProcDirectoryNode::mkdir(std::string_view, const char *) {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> ProcDirectoryNode::sync() {
        void_return();
    }

    IMetadata &ProcDirectoryNode::metadata() {
        return _node->metadata;
    }

    inode_t ProcDirectoryNode::inode_id() const {
        return _node->inode_id;
    }

    INodeCachePolicy ProcDirectoryNode::inode_cache() const {
        return INodeCachePolicy::NONE;
    }

    Result<void> ProcDirectoryNode::getattr(AttrSet &out) const {
        out.mode    = S_IFDIR | 0755;
        out.uid     = 0;
        out.gid     = 0;
        out.size    = 0;
        out.inode   = _node->inode_id;
        out.nlink   = 2;
        out.atime   = 0;
        out.mtime   = 0;
        out.ctime   = 0;
        out.blksize = 512;
        out.blocks  = 0;
        void_return();
    }

    Result<void> ProcDirectoryNode::setattr(AttrMask mask, const AttrSet &attrs) {
        (void)mask; (void)attrs;
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    ProcFSSuperblock::ProcFSSuperblock(ProcFSDriver &fs, size_t sb_id)
        : _fs(&fs), _sb_id(sb_id) {
        _nodes.insert_or_assign(0, ProcNode{
                                       .inode_id = 0,
                                       .kind     = NodeKind::ROOT_DIR,
                                       .pid      = 0,
                                       .entry    = nullptr,
                                       .metadata = {},
                                   });
        _nodes.insert_or_assign(1, ProcNode{
                                       .inode_id = 1,
                                       .kind     = NodeKind::SELF_LINK,
                                       .pid      = 0,
                                       .entry    = nullptr,
                                       .metadata = {},
                                   });
        _nodes.insert_or_assign(2, ProcNode{
                                       .inode_id = 2,
                                       .kind     = NodeKind::MEMINFO_FILE,
                                       .pid      = 0,
                                       .entry    = nullptr,
                                       .metadata = {},
                                   });
        _next_inode = 3;
    }

    Result<ProcNode *> ProcFSSuperblock::lookup_node(
        inode_t inode_id) noexcept {
        auto node_res = _nodes.at_nt(inode_id);
        if (!node_res.has_value()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return node_res.value();
    }

    Result<inode_t> ProcFSSuperblock::ensure_pid_node(pid_t pid) {
        for (const auto &[inode_id, node] : _nodes) {
            if (node.kind == NodeKind::PID_DIR && node.pid == pid) {
                return inode_id;
            }
        }
        auto pcb_res = task::TaskManager::inst().lookup_pcb_by_pid(pid);
        propagate(pcb_res);
        auto *pcb = pcb_res.value();
        if (pcb == nullptr || pcb->proc_state == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        inode_t inode_id = _next_inode++;
        _nodes.insert_or_assign(inode_id, ProcNode{
                                              .inode_id = inode_id,
                                              .kind     = NodeKind::PID_DIR,
                                              .pid      = pid,
                                              .entry    = nullptr,
                                              .metadata = {},
                                          });
        return inode_id;
    }

    Result<inode_t> ProcFSSuperblock::ensure_proc_node(
        pid_t pid, const ProcStateEntry &entry) {
        for (const auto &[inode_id, node] : _nodes) {
            if (node.pid == pid && node.entry == &entry) {
                return inode_id;
            }
        }
        auto pid_res = ensure_pid_node(pid);
        propagate(pid_res);
        inode_t inode_id = _next_inode++;
        _nodes.insert_or_assign(
            inode_id,
            ProcNode{
                .inode_id = inode_id,
                .kind     = entry.kind == PSKind::SYMLINK ? NodeKind::PROC_LINK
                                                          : NodeKind::PROC_FILE,
                .pid      = pid,
                .entry    = &entry,
                .metadata = {},
            });
        return inode_id;
    }

    IFsDriver &ProcFSSuperblock::fs() {
        return *_fs;
    }

    Result<void> ProcFSSuperblock::sync() {
        void_return();
    }

    Result<inode_t> ProcFSSuperblock::root() {
        return 0;
    }

    Result<inode_t> ProcFSSuperblock::alloc_inode(INodeType) {
        unexpect_return(ErrCode::NOT_SUPPORTED);
    }

    Result<void> ProcFSSuperblock::free_inode(inode_t id) {
        _nodes.erase(id);
        void_return();
    }

    IMetadata &ProcFSSuperblock::metadata() {
        return _metadata;
    }

    size_t ProcFSSuperblock::sb_id() const {
        return _sb_id;
    }

    Result<inode_t> ProcDirectoryNode::lookup(std::string_view name) {
        return _sb->lookup_child(_node->inode_id, name);
    }

    Result<size_t> ProcDirectoryNode::entry_count() {
        if (_node->kind == NodeKind::ROOT_DIR) {
            return task::TaskManager::inst().snapshot_pids().size() + 2;
        }
        if (_node->kind == NodeKind::PID_DIR) {
            return PROC_STATE_ENTRIES_COUNT;
        }
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }

    Result<DirectoryEntryInfo> ProcDirectoryNode::entry_at(size_t index) {
        if (_node->kind == NodeKind::ROOT_DIR) {
            if (index == 0) {
                return DirectoryEntryInfo{.name = "self"};
            }
            if (index == 1) {
                return DirectoryEntryInfo{.name = "meminfo"};
            }
            auto pids        = task::TaskManager::inst().snapshot_pids();
            size_t pid_index = index - 2;
            if (pid_index >= pids.size()) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return DirectoryEntryInfo{.name = pid_to_string(pids[pid_index])};
        }
        if (_node->kind == NodeKind::PID_DIR) {
            if (index >= PROC_STATE_ENTRIES_COUNT) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return DirectoryEntryInfo{.name = PROC_STATE_ENTRIES[index].name};
        }
        unexpect_return(ErrCode::TYPE_NOT_MATCHED);
    }

    Result<inode_t> ProcFSSuperblock::lookup_child(inode_t parent_inode,
                                                   std::string_view name) {
        auto parent_res = lookup_node(parent_inode);
        propagate(parent_res);
        auto *parent = parent_res.value();
        loggers::VFS::DEBUG(
            "procfs lookup_child: parent_inode=%lu kind=%u pid=%lu name=%s",
            static_cast<unsigned long>(parent_inode),
            static_cast<unsigned>(parent->kind),
            static_cast<unsigned long>(parent->pid), std::string(name).c_str());
        if (parent->kind == NodeKind::ROOT_DIR) {
            if (name == "self") {
                return inode_t(1);
            }
            if (name == "meminfo") {
                return inode_t(2);
            }
            pid_t pid = 0;
            if (name.empty()) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            for (char ch : name) {
                if (ch < '0' || ch > '9') {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                pid = pid * 10 + static_cast<pid_t>(ch - '0');
            }
            if (pid == 0) {
                unexpect_return(ErrCode::ENTRY_NOT_FOUND);
            }
            return ensure_pid_node(pid);
        }
        if (parent->kind != NodeKind::PID_DIR) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        auto entry = lookup_entry(name);
        if (entry == nullptr) {
            loggers::VFS::ERROR(
                "procfs lookup_child missing entry: pid=%lu name=%s",
                static_cast<unsigned long>(parent->pid),
                std::string(name).c_str());
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        loggers::VFS::DEBUG(
            "procfs lookup_child resolved entry: pid=%lu name=%s kind=%u",
            static_cast<unsigned long>(parent->pid), entry->name,
            static_cast<unsigned>(entry->kind));
        return ensure_proc_node(parent->pid, *entry);
    }

    Result<util::owner<IINode *>> ProcFSSuperblock::get_inode(
        inode_t inode_id) {
        auto node_res = lookup_node(inode_id);
        propagate(node_res);
        auto *node = node_res.value();
        switch (node->kind) {
            case NodeKind::ROOT_DIR:
            case NodeKind::PID_DIR:  {
                auto *dir = new ProcDirectoryNode(*this, *node);
                if (dir == nullptr) {
                    unexpect_return(ErrCode::OUT_OF_MEMORY);
                }
                return util::owner<IINode *>(dir);
            }
            case NodeKind::SELF_LINK:
            case NodeKind::PROC_LINK: {
                auto *symlink = new ProcSymlinkNode(*this, *node);
                if (symlink == nullptr) {
                    unexpect_return(ErrCode::OUT_OF_MEMORY);
                }
                return util::owner<IINode *>(symlink);
            }
            case NodeKind::MEMINFO_FILE: {
                auto *file = new MeminfoFile(*this, *node);
                if (file == nullptr) {
                    unexpect_return(ErrCode::OUT_OF_MEMORY);
                }
                return util::owner<IINode *>(file);
            }
            case NodeKind::PROC_FILE: {
                auto *file = new ProcFileNode(*this, *node);
                if (file == nullptr) {
                    unexpect_return(ErrCode::OUT_OF_MEMORY);
                }
                return util::owner<IINode *>(file);
            }
        }
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    Result<bool> ProcFSSuperblock::is_symlink(inode_t inode_id) {
        auto node_res = lookup_node(inode_id);
        propagate(node_res);
        auto kind = node_res.value()->kind;
        return kind == NodeKind::SELF_LINK || kind == NodeKind::PROC_LINK;
    }

    Result<std::string> ProcFSSuperblock::readlink(inode_t inode_id) {
        auto node_res = lookup_node(inode_id);
        propagate(node_res);
        auto *node = node_res.value();
        if (node->kind == NodeKind::SELF_LINK) {
            auto *current = schd::Scheduler::inst().current_pcb();
            if (current == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            return pid_to_string(current->pid);
        }
        if (node->kind != NodeKind::PROC_LINK || node->entry == nullptr) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        auto pcb_res = task::TaskManager::inst().lookup_pcb_by_pid(node->pid);
        propagate(pcb_res);
        auto *pcb = pcb_res.value();
        if (pcb == nullptr || pcb->proc_state == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        auto name = std::string_view(node->entry->name);
        if (name == "exe") {
            return pcb->proc_state->exe_target;
        }
        if (name == "cwd") {
            return pcb->proc_state->cwd_target;
        }
        if (name == "root") {
            return pcb->proc_state->root_target;
        }
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    Result<std::string> ProcSymlinkNode::target() {
        return _sb->readlink(_node->inode_id);
    }

    Result<size_t> ProcFileNode::read(off_t offset, void *buf, size_t len) {
        if (_node->entry == nullptr || offset < 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto pcb_res = task::TaskManager::inst().lookup_pcb_by_pid(_node->pid);
        propagate(pcb_res);
        auto *pcb = pcb_res.value();
        if (pcb == nullptr || pcb->proc_state == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return _node->entry->container->read(
            *pcb, *pcb->proc_state, _node->entry->name,
            static_cast<size_t>(offset), buf, len);
    }

    Result<size_t> ProcFileNode::write(off_t offset, const void *buf,
                                       size_t len) {
        if (_node->entry == nullptr || offset < 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto pcb_res = task::TaskManager::inst().lookup_pcb_by_pid(_node->pid);
        propagate(pcb_res);
        auto *pcb = pcb_res.value();
        if (pcb == nullptr || pcb->proc_state == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        return _node->entry->container->write(
            *pcb, *pcb->proc_state, _node->entry->name,
            static_cast<size_t>(offset), buf, len);
    }

    Result<size_t> ProcFileNode::size() {
        auto pcb_res = task::TaskManager::inst().lookup_pcb_by_pid(_node->pid);
        propagate(pcb_res);
        auto *pcb = pcb_res.value();
        if (pcb == nullptr || pcb->proc_state == nullptr ||
            _node->entry == nullptr)
        {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        auto name = std::string_view(_node->entry->name);
        if (name == "comm") {
            return pcb->proc_state->comm.size();
        }
        if (name == "cmdline") {
            return pcb->proc_state->cmdline_blob.size();
        }
        if (name == "environ") {
            return pcb->proc_state->environ_blob.size();
        }
        if (name == "bsargs") {
            return pcb->proc_state->bsargs_blob.size();
        }
        if (name == "auxv") {
            return pcb->proc_state->auxv_blob.size();
        }
        return 0;
    }

    std::string MeminfoFile::render() const {
        const auto &info = env::inst().system_memory_info();
        auto pages_to_kb = [](size_t pages) -> size_t {
            return (pages * PAGESIZE) / 1024;
        };
        constexpr size_t MEMINFO_BUF_SIZE = 4096;
        char *buf = new char[MEMINFO_BUF_SIZE];
        auto buf_guard = delete_guard(util::owner(buf));
        int len = snprintf(
            buf, MEMINFO_BUF_SIZE,
            "MemTotal: %12lu kB\n"
            "MemFree: %13lu kB\n"
            "MemAvailable: %8lu kB\n"
            "Buffers: %13lu kB\n"
            "Cached: %14lu kB\n"
            "SwapCached: %9u kB\n"
            "Active: %14lu kB\n"
            "Inactive: %12lu kB\n"
            "Active(anon): %8lu kB\n"
            "Inactive(anon): %6u kB\n"
            "Active(file): %8lu kB\n"
            "Inactive(file): %6lu kB\n"
            "Unevictable: %9u kB\n"
            "Mlocked: %14u kB\n"
            "SwapTotal: %13u kB\n"
            "SwapFree: %14u kB\n"
            "Dirty: %15lu kB\n"
            "Writeback: %11lu kB\n"
            "AnonPages: %11lu kB\n"
            "Mapped: %14lu kB\n"
            "KernelStack: %10lu kB\n"
            "PageTables: %11lu kB\n"
            "SecPageTables: %6u kB\n"
            "NFS_Unstable: %8u kB\n"
            "Bounce: %14u kB\n"
            "WritebackTmp: %8u kB\n"
            "CommitLimit: %11lu kB\n"
            "Committed_AS: %10lu kB\n"
            "HardwareCorrupted: %5u kB\n"
            "AnonHugePages: %8u kB\n"
            "ShmemHugePages: %7u kB\n"
            "ShmemPmdMapped: %6u kB\n"
            "FileHugePages: %8u kB\n"
            "FilePmdMapped: %8u kB\n"
            "Unaccepted: %10u kB\n"
            "HugePages_Total: %7u\n"
            "HugePages_Free: %8u\n"
            "HugePages_Rsvd: %8u\n"
            "HugePages_Surp: %8u\n"
            "Hugepagesize: %10u kB\n"
            "Hugetlb: %15u kB\n"
            "DirectMap4k: %10lu kB\n"
            "DirectMap2M: %10lu kB\n"
            "DirectMap1G: %10lu kB\n",
            static_cast<unsigned long>(pages_to_kb(info.mem_total_pages)),
            static_cast<unsigned long>(pages_to_kb(info.mem_free_pages)),
            static_cast<unsigned long>(
                pages_to_kb(info.mem_free_pages + info.inactive_file_pages)),
            static_cast<unsigned long>(pages_to_kb(info.buffer_pages)),
            static_cast<unsigned long>(pages_to_kb(info.page_cache_pages)),
            0U,
            static_cast<unsigned long>(
                pages_to_kb(info.anon_pages + info.active_file_pages)),
            static_cast<unsigned long>(pages_to_kb(info.inactive_file_pages)),
            static_cast<unsigned long>(pages_to_kb(info.anon_pages)),
            0U,
            static_cast<unsigned long>(pages_to_kb(info.active_file_pages)),
            static_cast<unsigned long>(pages_to_kb(info.inactive_file_pages)),
            0U, 0U, 0U, 0U,
            static_cast<unsigned long>(pages_to_kb(info.dirty_pages)),
            static_cast<unsigned long>(pages_to_kb(info.writeback_pages)),
            static_cast<unsigned long>(pages_to_kb(info.anon_pages)),
            static_cast<unsigned long>(pages_to_kb(info.mapped_pages)),
            static_cast<unsigned long>(pages_to_kb(info.kernel_stack_pages)),
            static_cast<unsigned long>(pages_to_kb(info.page_table_pages)),
            0U, 0U, 0U, 0U,
            static_cast<unsigned long>(pages_to_kb(info.mem_total_pages)),
            static_cast<unsigned long>(pages_to_kb(info.committed_pages)),
            0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
            0U, 2048U, 0U,
            static_cast<unsigned long>(pages_to_kb(info.directmap_4k_pages)),
            static_cast<unsigned long>(pages_to_kb(info.directmap_2m_pages)),
            static_cast<unsigned long>(pages_to_kb(info.directmap_1g_pages)));
        if (len < 0) {
            return {};
        }
        return std::string(buf, buf + len);
    }

    Result<size_t> MeminfoFile::read(off_t offset, void *buf, size_t len) {
        if (offset < 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        auto content = render();
        size_t off = static_cast<size_t>(offset);
        if (off >= content.size()) {
            return 0;
        }
        size_t actual = std::min(len, content.size() - off);
        if (actual != 0 && buf == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (actual != 0) {
            memcpy(buf, content.data() + off, actual);
        }
        return actual;
    }

    Result<size_t> MeminfoFile::write(off_t, const void *, size_t) {
        unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
    }

    Result<size_t> MeminfoFile::size() {
        return render().size();
    }

    Result<void> MeminfoFile::sync() {
        void_return();
    }

    IMetadata &MeminfoFile::metadata() {
        return _node->metadata;
    }

    inode_t MeminfoFile::inode_id() const {
        return _node->inode_id;
    }

    INodeCachePolicy MeminfoFile::inode_cache() const {
        return INodeCachePolicy::NONE;
    }

    ProcFSDriver &ProcFSDriver::inst() {
        if (_INSTANCE == nullptr) {
            _INSTANCE = new (&_INSTANCE_STORAGE[0]) ProcFSDriver();
        }
        return *_INSTANCE;
    }

    const char *ProcFSDriver::name() const {
        return "procfs";
    }

    Result<void> ProcFSDriver::probe(const char *name, const char *options) {
        (void)options;
        if (name == nullptr || std::string_view(name) != "procfs") {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        void_return();
    }

    Result<util::owner<ISuperblock *>> ProcFSDriver::mount(
        const char *name, const char *options) {
        (void)options;
        if (name == nullptr || std::string_view(name) != "procfs") {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (!initialized()) {
            auto init_res = init();
            propagate(init_res);
        }
        auto *sb = new ProcFSSuperblock(*this, _next_sb_id++);
        if (sb == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        return util::owner<ISuperblock *>(sb);
    }

    Result<void> ProcFSDriver::unmount(ISuperblock *sb) {
        delete sb;
        void_return();
    }

    Result<CapIdx> ProcFSDriver::get(pid_t pid, std::string_view name,
                                     cap::CHolder &holder) {
        auto pcb_res = task::TaskManager::inst().lookup_pcb_by_pid(pid);
        propagate(pcb_res);
        auto *pcb = pcb_res.value();
        if (pcb == nullptr || pcb->proc_state == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto base_path  = std::string("/proc/") + pid_to_string(pid);
        auto pseudo_res = VFS::inst().get_pseudo("procfs");
        propagate(pseudo_res);
        auto *sb = static_cast<ProcFSSuperblock *>(pseudo_res.value());
        loggers::VFS::DEBUG("procfs driver get: pid=%lu name=%s base=%s",
                           static_cast<unsigned long>(pid),
                           std::string(name).c_str(), base_path.c_str());
        if (name == "/") {
            return VFS::inst().open_dir(
                base_path.c_str(), holder,
                perm::vdir::READ | perm::vdir::EXEC | perm::basic::CLONE);
        }

        auto entry = lookup_entry(name);
        if (entry == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        if (entry->kind == PSKind::SYMLINK) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        b64 cap_perm = perm::vfile::READ | perm::basic::CLONE;
        if (entry->kind == PSKind::WRITABLE) {
            cap_perm |= perm::vfile::WRITE;
        }
        std::string full_path = base_path + "/" + std::string(name);
        return VFS::inst().__force_open(full_path.c_str(), cap_perm, holder);
    }

    Result<void> ProcFSDriver::redirect(pid_t pid, std::string_view name,
                                        std::string_view target) {
        auto pcb_res = task::TaskManager::inst().lookup_pcb_by_pid(pid);
        propagate(pcb_res);
        auto *pcb = pcb_res.value();
        if (pcb == nullptr || pcb->proc_state == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }
        auto entry = lookup_entry(name);
        if (entry == nullptr || entry->kind != PSKind::SYMLINK) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return entry->container->redirect(*pcb, *pcb->proc_state, name, target);
    }
}  // namespace procfs
