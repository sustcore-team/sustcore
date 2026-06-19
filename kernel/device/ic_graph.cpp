/**
 * @file ic_graph.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 中断控制器初始化依赖图实现
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/ic_graph.h>

namespace device {
    ICInitGraph::ICInitGraph() noexcept : _nodes(), _edges(), _ic_edges() {
        for (size_t i = 0; i < MAX_ICNODES; ++i) {
            _ic_edges[i] = -1;
        }
    }

    Result<void> ICInitGraph::add_node(driver::domain_t id, int node_id,
                                       DeviceNode *nd) noexcept {
        if (nd == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (_node_count >= MAX_ICNODES) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        if (_node_index.contains(id)) {
            unexpect_return(ErrCode::KEY_DUPLICATED);
        }

        _nodes[_node_count] = ICGraphNode{
            .id      = id,
            .node_id = node_id,
            .nd      = nd,
        };
        _node_index[id] = static_cast<int>(_node_count);
        _ic_edges[_node_count] = -1;
        ++_node_count;
        void_return();
    }

    Result<void> ICInitGraph::add_edge(driver::domain_t parent,
                                       driver::domain_t child) noexcept {
        if (_edge_count >= MAX_EDGES) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto parent_it = _node_index.find(parent);
        auto child_it  = _node_index.find(child);
        if (parent_it == _node_index.end() || child_it == _node_index.end()) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        int parent_idx = parent_it->second;
        _edges[_edge_count] = ICGraphEdge{
            .parent    = parent,
            .children  = child,
            .next_edge = _ic_edges[parent_idx],
        };
        _ic_edges[parent_idx] = static_cast<int>(_edge_count);
        ++_edge_count;
        void_return();
    }

    Result<std::vector<DeviceNode *>> ICInitGraph::topo_sort() const noexcept {
        int indegree[MAX_ICNODES]{};
        int queue[MAX_ICNODES]{};
        size_t head = 0;
        size_t tail = 0;

        for (size_t i = 0; i < _node_count; ++i) {
            for (int edge_idx = _ic_edges[i];
                 edge_idx != -1;
                 edge_idx = _edges[edge_idx].next_edge)
            {
                auto child_it = _node_index.find(_edges[edge_idx].children);
                if (child_it == _node_index.end()) {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                ++indegree[child_it->second];
            }
        }

        for (size_t i = 0; i < _node_count; ++i) {
            if (indegree[i] == 0) {
                queue[tail++] = static_cast<int>(i);
            }
        }

        std::vector<DeviceNode *> ordered;
        ordered.reserve(_node_count);

        while (head < tail) {
            int node_idx = queue[head++];
            ordered.push_back(_nodes[node_idx].nd);

            for (int edge_idx = _ic_edges[node_idx];
                 edge_idx != -1;
                 edge_idx = _edges[edge_idx].next_edge)
            {
                auto child_it = _node_index.find(_edges[edge_idx].children);
                if (child_it == _node_index.end()) {
                    unexpect_return(ErrCode::ENTRY_NOT_FOUND);
                }
                int child_idx = child_it->second;
                --indegree[child_idx];
                if (indegree[child_idx] == 0) {
                    queue[tail++] = child_idx;
                }
            }
        }

        if (ordered.size() != _node_count) {
            unexpect_return(ErrCode::UNKNOWN_ERROR);
        }
        return ordered;
    }
}  // namespace device
