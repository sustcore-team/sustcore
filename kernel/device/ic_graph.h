/**
 * @file ic_graph.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 中断控制器初始化依赖图
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/device.h>
#include <sustcore/errcode.h>

#include <unordered_map>
#include <vector>

namespace device {
    struct ICGraphNode {
        driver::domain_t id;
        int node_id;
        DeviceNode *nd;
    };

    struct ICGraphEdge {
        driver::domain_t parent;
        driver::domain_t children;
        int next_edge;
    };

    class ICInitGraph {
    public:
        constexpr static size_t MAX_ICNODES = 32;
        constexpr static size_t MAX_EDGES   = 64;

        ICInitGraph() noexcept;

        Result<void> add_node(driver::domain_t id, int node_id,
                              DeviceNode *nd) noexcept;
        Result<void> add_edge(driver::domain_t parent,
                              driver::domain_t child) noexcept;
        Result<std::vector<DeviceNode *>> topo_sort() const noexcept;

    private:
        ICGraphNode _nodes[MAX_ICNODES]{};
        ICGraphEdge _edges[MAX_EDGES]{};
        int _ic_edges[MAX_ICNODES]{};
        std::unordered_map<driver::domain_t, int> _node_index;
        size_t _node_count = 0;
        size_t _edge_count = 0;
    };
}  // namespace device
