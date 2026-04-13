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

        struct tm : public tags {};
        struct meminfo : public tags {};
        struct pgd : public unmodifiable {};
    }  // namespace key

    struct MemInfo {
        constexpr static size_t MAX_REGIONS = 128;
        MemRegion regions[MAX_REGIONS];
        size_t region_cnt   = 0;
        PhyAddr lowpm       = PhyAddr::null;
        PhyAddr uppm        = PhyAddr::null;
        VirAddr lowvm       = VirAddr::null;

        constexpr MemInfo()
        {
            memset(regions, 0, sizeof(regions));
        }
    };

    class Environment {
    protected:
        TM *_tm;
        MemInfo _meminfo;
    public:
        constexpr Environment()
            : _meminfo()
        {
        }

        // readers
        TM *tm() const;
        TM *&tm(key::tm);
        PhyAddr pgd() const;

        const MemInfo &meminfo() const;
        MemInfo &meminfo(key::meminfo);
    };

    void construct();
    Environment &inst();
}  // namespace env