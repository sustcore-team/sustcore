#include <concepts>
#include <sus/logger.h>
// #include <list>
#include <sus/list.h>

struct tree_tag {};
struct tree_lca_tag : public tree_tag {};

template <typename node_t, typename Tag = tree_tag>
    requires std::is_base_of_v<tree_tag, Tag>
struct TreeNode {
    node_t *parent{nullptr};
    // std::list<node_t *> children{};
    util::IntrusiveList<node_t *> children{};
};

template <typename node_t>
struct TreeNode<node_t, tree_lca_tag> {
    size_t depth{0};
    node_t *parent{nullptr};
    // std::list<node_t *> children{};
    util::IntrusiveList<node_t *> children{};
};

template <typename node_t, auto tree_node_ptr, typename Tag = tree_tag>
    requires std::is_base_of_v<tree_tag, Tag>
struct Tree {
    using TNode = TreeNode<node_t, Tag>;
    static void link_child(node_t &r_now, node_t &r_son) {
        TNode &now = r_now.*tree_node_ptr;
        TNode &son = r_son.*tree_node_ptr;
        if (son.parent != nullptr){
            LOGGER::INFO("节点 %p 已经有父节点了, 将被重新链接到 %p", &r_son, &r_now);
        }
        now.children.push_back(&r_son);
        son.parent = &r_now;
        if constexpr (std::is_same_v<Tag, tree_lca_tag>) {
            son.depth = now.depth + 1;
            assert(son.children.size() == 0 && "只能插入叶子节点");
        }
    }

    static bool is_root(const node_t &node) {
        return (node.*tree_node_ptr).parent == nullptr;
    }

    static void foreach_pre(node_t &node, auto func) {
        func(node);
        for (auto p : (node.*tree_node_ptr).children) foreach_pre(*p, func);
    }

    static void foreach_pre(const node_t &node, auto func) {
        func(node);
        for (auto p : (node.*tree_node_ptr).children) foreach_pre(*p, func);
    }

    static void foreach_post(node_t &node, auto func) {
        for (auto p : (node.*tree_node_ptr).children) foreach_post(*p, func);
        func(node);
    }

    static void foreach_post(const node_t &node, auto func) {
        for (auto p : (node.*tree_node_ptr).children) foreach_post(*p, func);
        func(node);
    }

    static void foreach_child(node_t &node, auto func) {
        for (auto p : (node.*tree_node_ptr).children) func(*p);
    }

    static void foreach_child(const node_t &node, auto func) {
        for (auto p : (node.*tree_node_ptr).children) func(*p);
    }

    static auto &get_parent(node_t &node) {
        return *(node.*tree_node_ptr).parent;
    }

    static const auto &get_parent(const node_t &node) {
        return *(node.*tree_node_ptr).parent;
    }

    static auto &get_children(node_t &node) {
        return (node.*tree_node_ptr).children;
    }

    static const auto &get_children(const node_t &node) {
        return (node.*tree_node_ptr).children;
    }

    static bool is_ancestor(const node_t &ancestor, const node_t &descendant) {
        const node_t *p = &descendant;
        while (p != nullptr) {
            if (p == &ancestor) {
                return true;
            }
            p = (p->*tree_node_ptr).parent;
        }
        return false;
    }

    static node_t *lca(node_t *pa, node_t *pb) {
        if constexpr (!std::is_same_v<Tag, tree_lca_tag>) {
            return nullptr;
        } else {
            while (pa != pb) {
                if ((pa->*tree_node_ptr).depth > (pb->*tree_node_ptr).depth) {
                    pa = (pa->*tree_node_ptr).parent;
                } else {
                    pb = (pb->*tree_node_ptr).parent;
                }
            }
            return pa;
        }
    }
};