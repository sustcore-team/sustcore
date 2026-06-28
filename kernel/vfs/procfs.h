/**
 * @file procfs.h
 * @author theflysong
 * @brief procfs 进程视图文件系统
 * @version alpha-1.0.0
 * @date 2026-06-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <exe/task.h>
#include <sus/types.h>
#include <task/task_struct.h>
#include <vfs/ops.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace procfs {
    enum class PSKind : uint8_t {
        WRITABLE,
        READONLY,
        SYMLINK,
    };

    class IPSContainer {
    public:
        virtual ~IPSContainer() = default;

        [[nodiscard]]
        virtual Result<size_t> read(task::PCB &pcb, task::ProcState &state,
                                    std::string_view name, size_t offset,
                                    void *buf, size_t len) const = 0;
        [[nodiscard]]
        virtual Result<size_t> write(task::PCB &pcb, task::ProcState &state,
                                     std::string_view name, size_t offset,
                                     const void *buf, size_t len) const = 0;
        [[nodiscard]]
        virtual Result<void> redirect(task::PCB &pcb, task::ProcState &state,
                                      std::string_view name,
                                      std::string_view target) const = 0;
    };

    struct ProcStateEntry {
        const char *name;
        PSKind kind;
        const IPSContainer *container;
    };

    enum class NodeKind : uint8_t {
        ROOT_DIR,
        SELF_LINK,
        MEMINFO_FILE,
        PID_DIR,
        PROC_FILE,
        PROC_LINK,
    };

    struct ProcMetadata final : public IMetadata {};

    struct ProcNode {
        inode_t inode_id = 0;
        NodeKind kind = NodeKind::ROOT_DIR;
        pid_t pid = 0;
        const ProcStateEntry *entry = nullptr;
        ProcMetadata metadata{};
    };
}  // namespace procfs

namespace task {
    struct ProcState {
        std::string comm{};
        std::vector<byte> cmdline_blob{};
        std::vector<byte> environ_blob{};
        std::vector<byte> bsargs_blob{};
        std::vector<byte> auxv_blob{};
        std::string exe_target = "/";
        std::string cwd_target = "/";
        std::string root_target = "/";
    };
}  // namespace task

namespace procfs {
    class ProcFSDriver;
    class ProcFSSuperblock;
    class ProcFileNode final : public IFile {
    private:
        ProcFSSuperblock *_sb;
        ProcNode *_node;

    public:
        ProcFileNode(ProcFSSuperblock &sb, ProcNode &node) noexcept;
        ~ProcFileNode() final = default;

        [[nodiscard]]
        Result<size_t> read(off_t offset, void *buf, size_t len) override;
        [[nodiscard]]
        Result<size_t> write(off_t offset, const void *buf,
                             size_t len) override;
        [[nodiscard]]
        Result<size_t> size() override;
        [[nodiscard]]
        Result<void> sync() override;
        [[nodiscard]]
        IMetadata &metadata() override;
        [[nodiscard]]
        inode_t inode_id() const override;
        [[nodiscard]]
        INodeCachePolicy inode_cache() const override;
        [[nodiscard]]
        Result<void> getattr(AttrSet &out) const override;
        [[nodiscard]]
        Result<void> setattr(AttrMask mask, const AttrSet &attrs) override;
    };

    class ProcSymlinkNode final : public ISymlink {
    private:
        ProcFSSuperblock *_sb;
        ProcNode *_node;

    public:
        ProcSymlinkNode(ProcFSSuperblock &sb, ProcNode &node) noexcept;
        ~ProcSymlinkNode() final = default;

        [[nodiscard]]
        Result<std::string> target() override;
        [[nodiscard]]
        IMetadata &metadata() override;
        [[nodiscard]]
        inode_t inode_id() const override;
        [[nodiscard]]
        INodeCachePolicy inode_cache() const override;
        [[nodiscard]]
        Result<void> getattr(AttrSet &out) const override;
        [[nodiscard]]
        Result<void> setattr(AttrMask mask, const AttrSet &attrs) override;
    };

    class ProcDirectoryNode final : public IDirectory {
    private:
        ProcFSSuperblock *_sb;
        ProcNode *_node;

    public:
        ProcDirectoryNode(ProcFSSuperblock &sb, ProcNode &node) noexcept;
        ~ProcDirectoryNode() final = default;

        [[nodiscard]]
        Result<inode_t> lookup(std::string_view name) override;
        [[nodiscard]]
        Result<inode_t> mkfile(std::string_view name,
                               const char *options) override;
        [[nodiscard]]
        Result<inode_t> mkdir(std::string_view name,
                              const char *options) override;
        [[nodiscard]]
        Result<size_t> entry_count() override;
        [[nodiscard]]
        Result<DirectoryEntryInfo> entry_at(size_t index) override;
        [[nodiscard]]
        Result<void> sync() override;
        [[nodiscard]]
        IMetadata &metadata() override;
        [[nodiscard]]
        inode_t inode_id() const override;
        [[nodiscard]]
        INodeCachePolicy inode_cache() const override;
        [[nodiscard]]
        Result<void> getattr(AttrSet &out) const override;
        [[nodiscard]]
        Result<void> setattr(AttrMask mask, const AttrSet &attrs) override;
    };

    class MeminfoFile final : public IFile {
    private:
        ProcFSSuperblock *_sb;
        ProcNode *_node;

        [[nodiscard]]
        std::string render() const;

    public:
        MeminfoFile(ProcFSSuperblock &sb, ProcNode &node) noexcept;
        ~MeminfoFile() final = default;

        [[nodiscard]]
        Result<size_t> read(off_t offset, void *buf, size_t len) override;
        [[nodiscard]]
        Result<size_t> write(off_t offset, const void *buf,
                             size_t len) override;
        [[nodiscard]]
        Result<size_t> size() override;
        [[nodiscard]]
        Result<void> sync() override;
        [[nodiscard]]
        IMetadata &metadata() override;
        [[nodiscard]]
        inode_t inode_id() const override;
        [[nodiscard]]
        INodeCachePolicy inode_cache() const override;
        [[nodiscard]]
        Result<void> getattr(AttrSet &out) const override;
        [[nodiscard]]
        Result<void> setattr(AttrMask mask, const AttrSet &attrs) override;
    };

    class ProcFSSuperblock final : public ISuperblock {
    private:
        ProcFSDriver *_fs;
        size_t _sb_id;
        inode_t _next_inode = 2;
        std::unordered_map<inode_t, ProcNode> _nodes{};
        ProcMetadata _metadata{};

        [[nodiscard]]
        Result<ProcNode *> lookup_node(inode_t inode_id) noexcept;

    public:
        ProcFSSuperblock(ProcFSDriver &fs, size_t sb_id);
        ~ProcFSSuperblock() final = default;

        [[nodiscard]]
        Result<inode_t> ensure_pid_node(pid_t pid);
        [[nodiscard]]
        Result<inode_t> ensure_proc_node(pid_t pid,
                                         const ProcStateEntry &entry);
        [[nodiscard]]
        Result<inode_t> lookup_child(inode_t parent_inode,
                                     std::string_view name);
        [[nodiscard]]
        IFsDriver &fs() override;
        [[nodiscard]]
        Result<void> sync() override;
        [[nodiscard]]
        Result<inode_t> root() override;
        [[nodiscard]]
        Result<util::owner<IINode *>> get_inode(inode_t inode_id) override;
        [[nodiscard]]
        Result<bool> is_symlink(inode_t inode_id) override;
        [[nodiscard]]
        Result<std::string> readlink(inode_t inode_id) override;
        [[nodiscard]]
        Result<inode_t> alloc_inode(INodeType type) override;
        [[nodiscard]]
        Result<void> free_inode(inode_t id) override;
        [[nodiscard]]
        IMetadata &metadata() override;
        [[nodiscard]]
        size_t sb_id() const override;
    };

    [[nodiscard]]
    const ProcStateEntry *lookup_entry(std::string_view name) noexcept;

    [[nodiscard]]
    Result<void> initialize_proc_state(
        task::ProcState &state, const std::string &comm,
        const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<uint64_t> &auxv,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv);
    [[nodiscard]]
    Result<void> clone_proc_state(const task::ProcState &src,
                                  task::ProcState &dst);

    [[nodiscard]]
    Result<void> init();
    [[nodiscard]]
    bool initialized() noexcept;
    [[nodiscard]]
    Result<void> register_process(task::PCB &pcb);
    [[nodiscard]]
    Result<void> unregister_process(task::PCB &pcb);

    class ProcFSDriver final : public IPesudoFsDriver {
    private:
        size_t _next_sb_id = 1;

    public:
        static ProcFSDriver &inst();

        ~ProcFSDriver() final = default;

        [[nodiscard]]
        const char *name() const final;
        [[nodiscard]]
        Result<void> probe(const char *name, const char *options) final;
        [[nodiscard]]
        Result<util::owner<ISuperblock *>> mount(const char *name,
                                                 const char *options) final;
        [[nodiscard]]
        Result<void> unmount(ISuperblock *sb) final;
        [[nodiscard]]
        Result<CapIdx> get(pid_t pid, std::string_view name,
                           cap::CHolder &holder);
        [[nodiscard]]
        Result<void> redirect(pid_t pid, std::string_view name,
                              std::string_view target);
    };
}  // namespace procfs
