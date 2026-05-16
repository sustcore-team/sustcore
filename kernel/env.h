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
#include <mem/vma.h>
#include <sustcore/addr.h>

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
        struct trap_context : public tags {};
        struct pgd : public unmodifiable {};
        struct meminfo : public tags {};
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
        TaskMemoryManager *_tmm = nullptr;
        Context *_trap_context  = nullptr;
        MemInfo _meminfo;
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
        Context *trap_context() const {
            return _trap_context;
        }
        [[nodiscard]]
        Context *&trap_context(key::trap_context) {
            return _trap_context;
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

    };

    void construct();
    Environment &inst();
}  // namespace env
