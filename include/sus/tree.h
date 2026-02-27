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
#include <cassert>

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
    namespace traits {
        template <typename Derived, typename Base>
        struct EmptyTrait;

        template <typename Derived, typename Base>
        struct DepthTrait;
    }  // namespace traits

    template <typename Derived, typename CustomTag = void,
              template <typename, typename> typename Trait = traits::EmptyTrait>
    class TreeBase;

    namespace traits {
        template <typename Derived, typename Base>
        struct EmptyTrait {
            static void on_link(Base &parent, Base &child) {}
            static void on_unlink(Base &parent, Base &child) {}
        };

        template <typename Derived, typename Base>
        struct DepthTrait {
            size_t depth = 0;

            static void on_link(Base &parent, Base &child) {
                auto *p_trait = as_trait(&parent);
                auto *c_trait = as_trait(&child);

                const size_t new_depth = p_trait->depth + 1;
                const int delta        = new_depth - c_trait->depth;

                c_trait->update_depth(delta);
            }

            static void on_unlink(Base &parent, Base &child) {
                auto *c_trait = as_trait(&child);

                c_trait->update_depth(-(int)c_trait->depth);
            }

            size_t get_depth(void) const {
                return depth;
            }

        private:
            static DepthTrait *as_trait(Base *d) {
                return static_cast<DepthTrait *>(d);
            }
            static Base *as_base(DepthTrait *t) {
                return static_cast<Base *>(t);
            }
            void update_depth(int delta) {
                depth         += delta;
                Base *derived  = as_base(this);
                for (auto &c : derived->get_children()) {
                    as_trait(c)->update_depth(delta);
                }
            }
        };

        template <typename Derived, typename Base>
        struct LazyDepthTrait {
            mutable size_t cached_depth     = 0;
            mutable size_t my_version     = 1;
            mutable size_t parent_version = 0;

            static void on_link(Base &parent, Base &child) {
                auto *c_trait = as_trait(&child);
                c_trait->parent_version = 0;
                c_trait->my_version++;
            }

            static void on_unlink(Base &parent, Base &child) {
                auto *c_trait = as_trait(&child);
                c_trait->parent_version = 0;
                c_trait->my_version++;
            }

            size_t get_lazy_depth(void) const {
                auto *par = as_base(this)->get_parent();
                if (par == nullptr) {
                    cached_depth = 0;
                    return 0;
                }

                auto *p_trait = as_trait(par);
                if (this->parent_version != p_trait->my_version) {
                    this->cached_depth   = p_trait->get_lazy_depth(par) + 1;
                    this->parent_version = p_trait->my_version;
                }
                return cached_depth;
            }

            size_t get_depth(void) const {
                return get_lazy_depth();
            }

        private:
            static Base *as_base(LazyDepthTrait *t) {
                return static_cast<Base *>(t);
            }
            static const Base *as_base(const LazyDepthTrait *t) {
                return static_cast<const Base *>(t);
            }
            static LazyDepthTrait *as_trait(Base *d) {
                return static_cast<LazyDepthTrait *>(d);
            }
            static const LazyDepthTrait *as_trait(const Base *d) {
                return static_cast<const LazyDepthTrait *>(d);
            }
        };

        template <typename Trait>
        concept IsDepthTrait = requires(Trait *t) {
            { t->get_depth() } -> std::convertible_to<size_t>;
        };
    }  // namespace traits

    // 子类注入的实现
    // 在这里不是用CustomTag来条件编译，是用来让编译器区分不同的树结构，避免多树无法区分

    template <typename Derived, typename CustomTag,
              template <typename, typename> typename Trait>
    class TreeBase
        : public Trait<Derived, TreeBase<Derived, CustomTag, Trait>> {
    protected:
        Derived *parent = nullptr;
        util::ArrayList<Derived *> children;

        using TreeType  = TreeBase<Derived, CustomTag, Trait>;
        using TraitType = Trait<Derived, TreeType>;

        constexpr Derived *as() {
            return static_cast<Derived *>(this);
        }
        constexpr const Derived *as() const {
            return static_cast<const Derived *>(this);
        }

        constexpr static Derived *as_derived(TreeBase *base) {
            return static_cast<Derived *>(base);
        }
        constexpr static const Derived *as_derived(const TreeBase *base) {
            return static_cast<const Derived *>(base);
        }

        constexpr static TreeBase *as_base(Derived *derived) {
            return static_cast<TreeBase *>(derived);
        }
        constexpr static const TreeBase *as_base(const Derived *derived) {
            return static_cast<const TreeBase *>(derived);
        }

        constexpr static TraitType *as_trait(Derived *derived) {
            return static_cast<TraitType *>(derived);
        }
        constexpr static const TraitType *as_trait(const Derived *derived) {
            return static_cast<const TraitType *>(derived);
        }

    public:
        // getter & setter
        const Derived *get_parent() const {
            return parent;
        }
        Derived *get_parent() {
            return parent;
        }
        void set_parent(Derived *par) {
            this->parent = par;
        }

        const util::ArrayList<Derived *> &get_children() const {
            return children;
        }
        util::ArrayList<Derived *> &get_children() {
            return children;
        }
        bool add_child(Derived *child) {
            if (children.contains(child)) {
                return false;
            }
            children.push_back(child);
            return true;
        }
        bool remove_child(Derived *child) {
            if (!children.contains(child)) {
                return false;
            }
            children.remove(child);
            return true;
        }

#ifndef NDEBUG
        void check_loop(const Derived *new_parent) const {
            const Derived *p = new_parent;
            while (p != nullptr) {
                if (p == as()) {
                    assert(false && "树结构中出现了循环链接");
                }
                p = as_base(p)->parent;
            }
        }
#endif

        bool link_parent(Derived *par) {
#ifndef NDEBUG
            check_loop(par);
#endif
            if (par != nullptr) {
                if (!as_base(par)->add_child(this->as())) {
                    return false;
                }
                TraitType::on_link(*as_base(par), *this);
            }
            this->set_parent(par);
            return true;
        }
        bool link_child(Derived *child) {
            return as_base(child)->link_parent(this->as());
        }

        bool unlink_parent() {
            if (parent != nullptr) {
                if (!as_base(parent)->remove_child(this->as())) {
                    return false;
                }
                TraitType::on_unlink(*as_base(parent), *this);
            }
            this->set_parent(nullptr);
            return true;
        }
        bool unlink_child(Derived *child) {
            return as_base(child)->unlink_parent();
        }

        TreeBase() : parent(nullptr), children() {}

        TreeBase(Derived *par) : parent(par), children() {
            if (par != nullptr)
                link_parent(par);
        }

        TreeBase(Derived *par, util::ArrayList<Derived *> &&children)
            : parent(par), children(std::move(children)) {
            if (par != nullptr)
                link_parent(par);
            for (auto &c : this->children) {
                as_base(c)->set_parent(this->as());
                TraitType::on_link(*this, *as_base(c));
            }
        }

        TreeBase(Derived *par, const util::ArrayList<Derived *> &children)
            : parent(par), children(children) {
            if (par != nullptr)
                link_parent(par);
            for (auto &c : this->children) {
                as_base(c)->set_parent(this->as());
                TraitType::on_link(*this, *as_base(c));
            }
        }

        bool is_root() const {
            return !parent;
        }

        void foreach_pre(auto func) {
            func(*as());
            for (auto p : children) {
                as_base(p)->foreach_pre(func);
            }
        }

        void foreach_post(auto func) {
            for (auto p : children) {
                as_base(p)->foreach_post(func);
            }
            func(*as());
        }

        void foreach_child(auto func) {
            for (auto p : children) func(*p);
        }

        // 设置父节点
        bool is_ancestor_of(const Derived &descendant) const {
            const Derived *p = &descendant;
            while (p != nullptr) {
                if (p == this) {
                    return true;
                }
                p = as_base(p)->parent;
            }
            return false;
        }

        constexpr size_t get_depth(void)
            requires traits::IsDepthTrait<TraitType>
        {
            return as_trait(as())->get_depth();
        }

        static Derived *lca(Derived *a, Derived *b)
            requires traits::IsDepthTrait<TraitType>
        {
            while (a != b) {
                assert(a != nullptr);
                assert(b != nullptr);
                if (as_base(a)->get_depth() > as_base(b)->get_depth()) {
                    a = as_base(a)->parent;
                } else {
                    b = as_base(b)->parent;
                }
            }
            return a;
        }
    };

    template <typename Derived, typename CustomTag = void>
    using LCATreeBase = TreeBase<Derived, CustomTag, traits::DepthTrait>;

    // template <typename node_t, typename CustomTag>
    // class TreeLCANode : public TreeBase<node_t, CustomTag> {
    // private:
    //     size_t depth;

    // public:
    //     void link_child(node_t &son) {
    //         TreeLCANode &son_node = static_cast<TreeLCANode &>(son);
    //         assert(son_node.parent == nullptr);
    //         this->children.push_back(&son);
    //         son_node.parent = this->as();
    //         son_node.depth  = this->depth + 1;
    //         assert(son_node.children.size() == 0 && "只能插入叶子节点");
    //     }

    //     static node_t *lca(node_t *a, node_t *b) {
    //         TreeLCANode *pa = static_cast<TreeLCANode *>(a);
    //         TreeLCANode *pb = static_cast<TreeLCANode *>(b);
    //         while (pa != pb) {
    //             assert(pa != nullptr);
    //             assert(pb != nullptr);
    //             if (pa->depth > pb->depth) {
    //                 pa = pa->parent;
    //             } else {
    //                 pb = pb->parent;
    //             }
    //         }
    //         return static_cast<node_t *>(pa);
    //     }
    // };

    // template <typename node_t>
    // using Tree = TreeNode<node_t, class TreeTag>;

    // template <typename node_t>
    // using TreeLCA = TreeLCANode<node_t, class TreeLCATag>;

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