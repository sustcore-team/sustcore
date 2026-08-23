/**
 * @file vma.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief AddrSpace 中的虚拟内存区域及其 MemSeg backing capability。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <memory/virtual/flags.h>
#include <obj/kobject.h>
#include <obj/mem_seg.h>
#include <sustcore/addr.h>
#include <tay/list.h>

#include <utility>

namespace task {
    class VMA final {
    public:
        VMA(cap::CRef<memory::MemSeg> memory, VirArea area, size_t seg_offset,
            memory::PageFlags flags) noexcept
            : memory_(std::move(memory)), area_(area), segment_offset_(seg_offset), flags_(flags) {}

        VMA(const VMA &)            = delete;
        VMA &operator=(const VMA &) = delete;
        VMA(VMA &&)                 = delete;
        VMA &operator=(VMA &&)      = delete;

        [[nodiscard]] VirArea area() const noexcept {
            return area_;
        }
        [[nodiscard]] size_t seg_offset() const noexcept {
            return segment_offset_;
        }
        [[nodiscard]] const cap::CRef<memory::MemSeg> &memory() const noexcept {
            return memory_;
        }
        [[nodiscard]] memory::PageFlags flags() const noexcept {
            return flags_;
        }
        [[nodiscard]] bool linked() const noexcept {
            return hook_.in_list;
        }

    private:
        friend struct VMAHookLocator;
        cap::CRef<memory::MemSeg> memory_{};
        VirArea area_{};
        size_t segment_offset_ = 0;
        memory::PageFlags flags_{};
        tay::intrusive_list_hook<VMA *, VMA *> hook_{};
    };

    struct VMAHookLocator {
        using Hook = tay::intrusive_list_hook<VMA *, VMA *>;
        [[nodiscard]] Hook &operator()(VMA &vma) const noexcept {
            return vma.hook_;
        }
        [[nodiscard]] const Hook &operator()(const VMA &vma) const noexcept {
            return vma.hook_;
        }
    };
}  // namespace task
