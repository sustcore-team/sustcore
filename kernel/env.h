/**
 * @file env.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief environment
 * @version alpha-1.0.0
 * @date 2026-04-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/riscv64/description.h>
#include <cap/cholder.h>
#include <mem/vma.h>
#include <sustcore/addr.h>
#include <task/scheduler.h>
#include <task/task.h>
#include <task/task_struct.h>
#include <vfs/vfs.h>

namespace env {
    // passkey
    namespace key {
        struct tags {
        protected:
            tags() = default;
        };

        struct unmodifiable : public tags {
        private:
            unmodifiable() = default;
        };

        struct tmm : public tags {};
        struct pgd : public unmodifiable {};
        struct meminfo : public tags {};
        struct vfs : public tags {};
        struct chman : public tags {};
        struct scheduler : public tags {};
        struct tm : public tags {};
    }  // namespace key

    struct MemInfo {
        constexpr static size_t MAX_REGIONS = 128;
        MemRegion regions[MAX_REGIONS];
        size_t region_cnt = 0;
        PhyAddr lowpm     = PhyAddr::null;
        PhyAddr uppm      = PhyAddr::null;
        VirAddr lowvm     = VirAddr::null;

        constexpr MemInfo() {
            memset((void *)regions, 0, sizeof(regions));
        }
    };

    class Environment {
    private:
        TaskMemoryManager *_tmm;
        VFS *_vfs;
        CHolderManager *_chman;
        MemInfo _meminfo;
        schd::Scheduler *_scheduler;
        TaskManager *_tm;
    public:
        constexpr Environment() : _meminfo() {}

        // readers
        [[nodiscard]]
        TaskMemoryManager *tmm() const {
            return _tmm;
        }
        [[nodiscard]]
        TaskMemoryManager *&tmm(key::tmm) {
            return _tmm;
        }

        [[nodiscard]]
        PhyAddr pgd() const {
            return PageMan::read_root();
        }

        [[nodiscard]]
        const MemInfo &meminfo() const {
            return _meminfo;
        }
        [[nodiscard]]
        MemInfo &meminfo(key::meminfo) {
            return _meminfo;
        }

        [[nodiscard]]
        VFS *vfs() const {
            return _vfs;
        }
        [[nodiscard]]
        VFS *&vfs(key::vfs) {
            return _vfs;
        }

        [[nodiscard]]
        CHolderManager *chman() const {
            return _chman;
        }
        [[nodiscard]]
        CHolderManager *&chman(key::chman) {
            return _chman;
        }

        [[nodiscard]]
        schd::Scheduler *scheduler() const {
            return _scheduler;
        }
        [[nodiscard]]
        schd::Scheduler *&scheduler(key::scheduler) {
            return _scheduler;
        }

        [[nodiscard]]
        TaskManager *tm() const {
            return _tm;
        }
        [[nodiscard]]
        TaskManager *&tm(key::tm) {
            return _tm;
        }
    };

    void construct();
    Environment &inst();
}  // namespace env