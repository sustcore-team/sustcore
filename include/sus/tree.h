/**
 * @file tree.h
 * @author theflysong (song_of_the_fly@163.com), hyj0
 * @brief 树的实现
 * @version alpha-1.0.0
 * @date 2026-02-11
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sus/list.h>
#include <sus/logger.h>

#include <concepts>

namespace util {
    struct tree_tag {};
    struct tree_lca_tag : public tree_tag {};

    template <typename T, typename node_t>
    concept is_node_t =
        std::same_as<T, node_t> || std::same_as<T, const node_t>;

    template <typename node_t, typename Tag = tree_tag>
        requires std::is_base_of_v<tree_tag, Tag>
    struct TreeNode {
        node_t *parent = nullptr;
        util::ArrayList<node_t *> children;
    };

    template <typename node_t>
    struct TreeNode<node_t, tree_lca_tag> {
        size_t depth = 0;
        node_t *parent = nullptr;
        util::ArrayList<node_t *> children;
    };

    template <typename node_t, auto tree_node_ptr = &node_t::tree_node,
              typename Tag = tree_tag>
        requires std::is_base_of_v<tree_tag, Tag>
    struct Tree {
        using TNode = TreeNode<node_t, Tag>;

    protected:
        constexpr static TNode &U_node(node_t &n) {
            return n.*tree_node_ptr;
        }
        constexpr static TNode &U_node(node_t *n) {
            return n->*tree_node_ptr;
        }
        constexpr static const TNode &U_node(const node_t &n) {
            return n.*tree_node_ptr;
        }
        constexpr static const TNode &U_node(const node_t *n) {
            return n->*tree_node_ptr;
        }

    public:
        static void link_child(node_t &r_par, node_t &r_son) {
            TNode &par = U_node(r_par);
            TNode &son = U_node(r_son);
            assert(son.parent == nullptr);
            par.children.push_back(&r_son);
            son.parent = &r_par;
            if constexpr (std::is_same_v<Tag, tree_lca_tag>) {
                son.depth = par.depth + 1;
                assert(son.children.size() == 0 && "只能插入叶子节点");
            }
        }

        static bool is_root(const node_t &node) {
            return U_node(node).parent == nullptr;
        }

        template <is_node_t<node_t> T>
        static void foreach_pre(T &node, auto func) {
            func(node);
            for (auto p : U_node(node).children) foreach_pre(*p, func);
        }

        template <is_node_t<node_t> T>
        static void foreach_post(T &node, auto func) {
            for (auto p : U_node(node).children) foreach_post(*p, func);
            func(node);
        }

        template <is_node_t<node_t> T>
        static void foreach_child(T &node, auto func) {
            for (auto p : U_node(node).children) func(*p);
        }

        template <is_node_t<node_t> T>
        static T &get_parent(T &node) {
            return * (U_node(node).parent);
        }

        template <is_node_t<node_t> T>
        static auto &get_children(T &node) {
            return U_node(node).children;
        }

        static bool is_ancestor(const node_t &ancestor,
                                const node_t &descendant) {
            const node_t *p = &descendant;
            while (p != nullptr) {
                if (p == &ancestor) {
                    return true;
                }
                p = U_node(p).parent;
            }
            return false;
        }

        static node_t *lca(node_t *pa, node_t *pb) {
            if constexpr (!std::is_same_v<Tag, tree_lca_tag>) {
                return nullptr;
            } else {
                while (pa != pb) {
                    assert(pa != nullptr);
                    assert(pb != nullptr);
                    if (U_node(pa).depth > U_node(pb).depth) {
                        pa = U_node(pa).parent;
                    } else {
                        pb = U_node(pb).parent;
                    }
                }
                return pa;
            }
        }
    };

    /*
    Usage:

    struct MyNode {
        int value;
        TreeNode<MyNode, tree_lca_tag> tree_node;
    };

    using MyTree = Tree<MyNode, &MyNode::tree_node>;

    MyNode root{.value = 1};
    MyNode child1{.value = 2};
    MyNode child2{.value = 3};

    MyTree::link_child(root, child1);
    MyTree::link_child(root, child2);

    MyTree::foreach_pre(root, [](const MyNode &node) {
        std::cout << node.value << std::endl;
    });
    */
}  // namespace util