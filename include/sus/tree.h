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

#pragma once

#include <sus/list.h>
#include <sus/logger.h>

#include <concepts>

namespace util::tree {
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
        size_t depth   = 0;
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
            return *(U_node(node).parent);
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
}  // namespace util::tree

namespace util::tree_base {
    // 子类注入的实现
    // 在这里不是用CustomTag来条件编译，是用来让编译器区分不同的树结构，避免多树无法区分

    template <typename node_t, typename CustomTag>
    class TreeBase {
    protected:
        node_t *parent;
        util::ArrayList<node_t *> children;

        inline node_t *as() {
            return static_cast<node_t *>(this);
        }

        inline const node_t *as() const {
            return static_cast<const node_t *>(this);
        }

    public:
        constexpr TreeBase()
            : parent(nullptr), children()
        {
        }

        TreeBase(node_t *par)
            : parent(par), children()
        {
        }

        TreeBase(node_t *par, util::ArrayList<node_t *> &&children)
            : parent(par), children(std::move(children))
        {
        }

        TreeBase(node_t *par, const util::ArrayList<node_t *> &children)
            : parent(par), children(children)
        {
        }

        bool is_root() const {
            return !parent;
        }

        void foreach_pre(auto func) {
            func(*as());
            for (auto p : children) {
                static_cast<TreeBase<node_t, CustomTag> *>(p)->foreach_pre(
                    func);
            }
        }

        void foreach_post(auto func) {
            for (auto p : children) {
                static_cast<TreeBase<node_t, CustomTag> *>(p)->foreach_post(
                    func);
            }
            func(*as());
        }

        void foreach_child(auto func) {
            for (auto p : children) func(*p);
        }

        const node_t &get_parent() const {
            return *parent;
        }

        node_t &get_parent() {
            return *parent;
        }

        const util::ArrayList<node_t *> &get_children() const {
            return children;
        }

        util::ArrayList<node_t *> &get_children() {
            return children;
        }

        // 添加子节点
        inline bool add_child(node_t *child) {
            if (children.contains(child)) {
                return false;
            }
            children.push_back(child);
            return true;
        }

        // 移除子节点
        inline bool remove_child(node_t *child) {
            if (! children.contains(child)) {
                return false;
            }
            children.remove(child);
            return true;
        }

        // 设置父节点
        inline void set_parent(node_t *par) {
            this->parent = par;
        }

        bool is_ancestor_of(const node_t &descendant) const {
            const node_t *p = &descendant;
            while (p != nullptr) {
                if (p == this) {
                    return true;
                }
                p = static_cast<TreeBase<node_t, CustomTag> *>(p)->parent;
            }
            return false;
        }
    };

    template <typename node_t, typename CustomTag>
    class TreeNode : public TreeBase<node_t, CustomTag> {
    public:
        using Base = TreeBase<node_t, CustomTag>;

        constexpr TreeNode()
            : Base()
        {
        }

        inline TreeNode(node_t *par)
            : Base(par)
        {
            if (par != nullptr)
                par->add_child(this->as());
        }

        inline TreeNode(node_t *par, util::ArrayList<node_t *> &&children)
            : Base(par, std::move(children))
        {
            if (par != nullptr)
                par->add_child(this->as());
            for (auto &nd : children) {
                nd->set_parent(this->as());
            }
        }

        inline TreeNode(node_t *par, const util::ArrayList<node_t *> &children)
            : Base(par, children)
        {
            if (par != nullptr)
                par->add_child(this->as());
            for (auto &nd : children) {
                nd->set_parent(this->as());
            }
        }

        void link_child(node_t &son) {
            TreeNode &son_node = static_cast<TreeNode &>(son);
            assert(son_node.parent == nullptr);
            this->children.push_back(&son);
            son_node.parent = this->as();
        }

        // 设置父节点, 并将自己添加到父节点的子节点列表中
        bool link_parent(node_t *par) {
            if (! par->add_child(this->as()))
                return false;
            this->set_parent(par);
            return true;
        }

        // 移除子节点
        bool unlink_child(node_t *child) {
            if (! this->remove_child(child)) {
                return false;
            }
            child->parent = nullptr;
            return true;
        }
    };

    template <typename node_t, typename CustomTag>
    class TreeLCANode : public TreeBase<node_t, CustomTag> {
    private:
        size_t depth;

    public:
        void link_child(node_t &son) {
            TreeLCANode &son_node = static_cast<TreeLCANode &>(son);
            assert(son_node.parent == nullptr);
            this->children.push_back(&son);
            son_node.parent = this->as();
            son_node.depth  = this->depth + 1;
            assert(son_node.children.size() == 0 && "只能插入叶子节点");
        }

        static node_t *lca(node_t *a, node_t *b) {
            TreeLCANode *pa = static_cast<TreeLCANode *>(a);
            TreeLCANode *pb = static_cast<TreeLCANode *>(b);
            while (pa != pb) {
                assert(pa != nullptr);
                assert(pb != nullptr);
                if (pa->depth > pb->depth) {
                    pa = pa->parent;
                } else {
                    pb = pb->parent;
                }
            }
            return static_cast<node_t *>(pa);
        }
    };

    template <typename node_t>
    using Tree = TreeNode<node_t, class TreeTag>;

    template <typename node_t>
    using TreeLCA = TreeLCANode<node_t, class TreeLCATag>;

    /**
    Usage:

    using TreeA = TreeNode<class Data, class TreeATag>;
    using TreeB = TreeLCANode<class Data, class TreeBTag>;

    class Data : public TreeA, public TreeB {
    public:
        int data;
        int key() const {
            return data;
        }
    } pool[1000];

    void test() {
        for (int i = 0; i < 1000; i++) {
            pool[i].data = i;
        }
        auto &rt = pool[0];
        rt.TreeA::link_child(pool[1]);
        pool[1].TreeA::link_child(pool[2]);
        pool[1].TreeA::link_child(pool[3]);
        pool[3].TreeA::link_child(pool[4]);
        rt.TreeB::link_child(pool[5]);
        pool[5].TreeB::link_child(pool[6]);
        pool[6].TreeB::link_child(pool[7]);
        pool[6].TreeB::link_child(pool[8]);
        pool[5].TreeB::link_child(pool[9]);
    }
    */
}  // namespace util::tree_base