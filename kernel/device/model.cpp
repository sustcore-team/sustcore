/**
 * @file model.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备模型实现
 * @version alpha-1.0.0
 * @date 2026-05-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/model.h>
#include <logger.h>
#include <symbols.h>

#include <algorithm>
#include <ranges>

namespace {
    [[nodiscard]]
    bool is_empty(MemRegion region) {
        return region.area.nullable();
    }

    [[nodiscard]]
    bool mergeable(MemRegion lhs, MemRegion rhs) {
        return lhs.status == rhs.status && rhs.area.begin <= lhs.area.end;
    }

    [[nodiscard]]
    bool region_less(MemRegion lhs, MemRegion rhs) {
        if (lhs.area.begin != rhs.area.begin) {
            return lhs.area.begin < rhs.area.begin;
        }
        if (lhs.area.end != rhs.area.end) {
            return lhs.area.end < rhs.area.end;
        }
        return static_cast<int>(lhs.status) < static_cast<int>(rhs.status);
    }

    [[nodiscard]]
    std::vector<MemRegion> merge_same_status_regions(
        std::vector<MemRegion> regions) {
        std::ranges::sort(regions, region_less);

        std::vector<MemRegion> merged;
        merged.reserve(regions.size());
        for (const auto &region : regions) {
            if (is_empty(region)) {
                continue;
            }
            if (!merged.empty() && mergeable(merged.back(), region)) {
                if (merged.back().area.end < region.area.end) {
                    merged.back().area.end = region.area.end;
                }
                continue;
            }
            merged.push_back(region);
        }
        return merged;
    }

    [[nodiscard]]
    std::vector<addr_t> sorted_unique_boundaries(
        const std::vector<MemRegion> &regions) {
        std::vector<addr_t> boundaries;
        boundaries.reserve(regions.size() * 2);
        for (const auto &region : regions) {
            if (is_empty(region)) {
                continue;
            }
            boundaries.push_back(region.area.begin.arith());
            boundaries.push_back(region.area.end.arith());
        }

        std::ranges::sort(boundaries,
                          [](addr_t lhs, addr_t rhs) { return lhs < rhs; });

        std::vector<addr_t> unique;
        unique.reserve(boundaries.size());
        for (addr_t boundary : boundaries) {
            if (unique.empty() || unique.back() != boundary) {
                unique.push_back(boundary);
            }
        }
        return unique;
    }
}  // namespace

namespace device {
    /**
     * @brief 登记一个统一设备节点并转移所有权给 DeviceModel.
     */
    Result<DeviceNode *> DeviceModel::register_device_node(
        util::owner<DeviceNode *> node) noexcept {
        if (node.get() == nullptr) {
            loggers::DEVICE::ERROR("登记 DeviceNode 失败: node 为空");
            unexpect_return(ErrCode::NULLPTR);
        }

        auto *registered = node.get();
        _devices.push_back(std::move(node));
        if (!driver::DriverModel::initialized()) {
            _non_irq_devices.push_back(registered);
        } else {
            auto *irq_factory =
                driver::DriverModel::inst().irq_factories().find(*registered);
            if (irq_factory == nullptr) {
                _non_irq_devices.push_back(registered);
            }
        }
        loggers::DEVICE::DEBUG("已登记 DeviceNode: platform=%s",
                               registered->platform());
        return registered;
    }

    /**
     * @brief 按 compatible 查询全部匹配的设备节点.
     */
    std::vector<DeviceNode *> DeviceModel::find_devices_by_compatible(
        std::string_view compatible) const noexcept {
        std::vector<DeviceNode *> matched;
        matched.reserve(_devices.size());
        for (const auto &node : _devices) {
            if (node.get() == nullptr) {
                continue;
            }
            if (node->is_compatible_with(compatible) >= 0) {
                matched.push_back(node.get());
            }
        }
        return matched;
    }

    /**
     * @brief 释放已登记的统一设备节点.
     */
    void DeviceModel::cleanup_device_nodes() noexcept {
        _non_irq_devices.clear();
        for (auto &node : _devices) {
            delete node.get();
        }
        _devices.clear();
    }

    [[nodiscard]]
    std::vector<MemRegion> DeviceModel::_normalize_memory_regions(
        const std::vector<MemRegion> &regions) const {
        // Step 1: merge overlapping or adjacent regions with the same status.
        std::vector<MemRegion> merged = merge_same_status_regions(regions);

        std::vector<MemRegion> free_regions;
        std::vector<MemRegion> non_free_regions;
        free_regions.reserve(merged.size());
        non_free_regions.reserve(merged.size());
        for (const auto &region : merged) {
            if (region.status == MemRegion::MemoryStatus::FREE) {
                free_regions.push_back(region);
            } else {
                non_free_regions.push_back(region);
            }
        }

        // Step 2: subtract non-FREE coverage from FREE regions.
        std::vector<MemRegion> trimmed_free_regions;
        for (const auto &free_region : free_regions) {
            std::vector<PhyArea> fragments = {free_region.area};
            for (const auto &used_region : non_free_regions) {
                std::vector<PhyArea> next_fragments;
                for (const auto &fragment : fragments) {
                    if (fragment.nullable() ||
                        !(fragment.begin < used_region.area.end &&
                          used_region.area.begin < fragment.end))
                    {
                        next_fragments.push_back(fragment);
                        continue;
                    }

                    if (fragment.begin < used_region.area.begin) {
                        next_fragments.push_back(
                            PhyArea(fragment.begin, used_region.area.begin));
                    }
                    if (used_region.area.end < fragment.end) {
                        next_fragments.push_back(
                            PhyArea(used_region.area.end, fragment.end));
                    }
                }
                fragments = std::move(next_fragments);
                if (fragments.empty()) {
                    break;
                }
            }

            for (const auto &fragment : fragments) {
                trimmed_free_regions.push_back(MemRegion{
                    .status = MemRegion::MemoryStatus::FREE,
                    .area   = fragment,
                });
            }
        }

        // Step 3: mark overlapping non-FREE conflicts as BAD_MEMORY and warn.
        std::vector<MemRegion> resolved_non_free_regions;
        auto boundaries = sorted_unique_boundaries(non_free_regions);
        if (boundaries.size() >= 2) {
            for (size_t i = 0; i + 1 < boundaries.size(); ++i) {
                MemRegion slice{
                    .status = MemRegion::MemoryStatus::BAD_MEMORY,
                    .area   = PhyArea(PhyAddr(boundaries[i]),
                                      PhyAddr(boundaries[i + 1])),
                };
                if (slice.area.nullable()) {
                    continue;
                }

                const MemRegion *first_cover  = nullptr;
                const MemRegion *second_cover = nullptr;
                size_t cover_count            = 0;
                for (const auto &region : non_free_regions) {
                    if (region.area.begin <= slice.area.begin &&
                        slice.area.end <= region.area.end)
                    {
                        if (cover_count == 0) {
                            first_cover = &region;
                        } else if (cover_count == 1) {
                            second_cover = &region;
                        }
                        ++cover_count;
                    }
                }

                if (cover_count == 0) {
                    continue;
                }
                if (cover_count == 1) {
                    slice.status = first_cover->status;
                } else {
                    loggers::DEVICE::WARN(
                        "non-free memory overlap detected: [%p, %p) status %d "
                        "vs %d",
                        slice.area.begin.addr(), slice.area.end.addr(),
                        static_cast<int>(first_cover->status),
                        static_cast<int>(second_cover->status));
                    slice.status = MemRegion::MemoryStatus::BAD_MEMORY;
                }
                resolved_non_free_regions.push_back(slice);
            }
        }

        // Step 4: remove empty regions.
        std::vector<MemRegion> normalized;
        normalized.reserve(trimmed_free_regions.size() +
                           resolved_non_free_regions.size());
        for (const auto &region : trimmed_free_regions) {
            if (!is_empty(region)) {
                normalized.push_back(region);
            }
        }
        for (const auto &region : resolved_non_free_regions) {
            if (!is_empty(region)) {
                normalized.push_back(region);
            }
        }

        // Step 5: sort by start address and do a final same-status merge.
        std::ranges::sort(normalized, region_less);
        return merge_same_status_regions(std::move(normalized));
    }

    void KernelProvider::register_device(DeviceModel &model) const {
        (void)model;
    }

    DeviceModel DeviceModel::_INSTANCE;
    bool DeviceModel::_initialized = false;

    DeviceModel &DeviceModel::inst() {
        assert(_initialized);
        return _INSTANCE;
    }

    void DeviceModel::init() {
        new (&_INSTANCE) DeviceModel();
        _initialized = true;
    }

    bool DeviceModel::initialized() {
        return _initialized;
    }
}  // namespace device
