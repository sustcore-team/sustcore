/**
 * @file tree.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief tree测试
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sus/tree.h>
#include <test/tree.h>

#include <span>

namespace test::tree {
    struct TestTree {
        util::tree::TreeNode<TestTree, util::tree::tree_lca_tag> tree_node;
        int idx = 0;
    };

    class CaseTreeStructure : public TestCase {
    public:
        CaseTreeStructure() : TestCase("Tree 基本结构与遍历") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            constexpr size_t N = 10;
            TestTree _nodes[N];
            std::span<TestTree, N> nodes(_nodes, N);

            for (int i = 0; i < N; i++) {
                nodes[i] = {{}, i};
            }

            using Tree = util::tree::Tree<TestTree, &TestTree::tree_node,
                                          util::tree::tree_lca_tag>;

            expect("构建树结构: 0->(1,2), 1->(3,4)");
            Tree::link_child(nodes[0], nodes[1]);
            Tree::link_child(nodes[0], nodes[2]);
            Tree::link_child(nodes[1], nodes[3]);
            Tree::link_child(nodes[1], nodes[4]);

            check("验证层级关系");
            ttest(Tree::is_root(nodes[0]));
            ttest(Tree::get_parent(nodes[1]).idx == 0);
            ttest(Tree::get_children(nodes[1]).size() == 2);
            ttest(Tree::get_children(nodes[0]).size() == 2);

            expect("前序遍历校验顺序 (0 1 3 4 2)");
            int expected_order[] = {0, 1, 3, 4, 2};
            int visit_idx        = 0;
            Tree::foreach_pre(nodes[0], [&](TestTree &node) {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                if (visit_idx < 5) {
                    ttest(node.idx == expected_order[visit_idx]);
                }
                visit_idx++;
            });
            ttest(visit_idx == 5);
        }
    };

    class CaseTreeLCA : public TestCase {
    public:
        CaseTreeLCA() : TestCase("Tree 最近公共祖先 (LCA) 测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            constexpr size_t N = 16;
            TestTree _nodes[N];
            std::span<TestTree, N> nodes(_nodes, N);
            for (int i = 0; i < N; i++) {
                nodes[i] = {{}, i};
            }
            using Tree = util::tree::Tree<TestTree, &TestTree::tree_node,
                                          util::tree::tree_lca_tag>;

            // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

            // 构建稍复杂的树
            Tree::link_child(nodes[0], nodes[1]);
            Tree::link_child(nodes[0], nodes[2]);
            Tree::link_child(nodes[1], nodes[5]);
            Tree::link_child(nodes[5], nodes[12]);
            Tree::link_child(nodes[5], nodes[13]);
            Tree::link_child(nodes[13], nodes[15]);

            // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

            check("LCA(12, 15) 应为 5");
            ttest(Tree::lca(&nodes[12], &nodes[15])->idx == 5);

            check("LCA(12, 2) 应为 0");
            ttest(Tree::lca(&nodes[12], &nodes[2])->idx == 0);

            check("LCA(15, 15) 应为自身 15");
            ttest(Tree::lca(&nodes[15], &nodes[15])->idx == 15);
        }
    };

    using TreeA = util::tree_base::TreeBase<class Data, class TreeATag>;
    using TreeB = util::tree_base::LCATreeBase<class Data, class TreeBTag>;
    class Data : public TreeA, public TreeB {
    public:
        int data = 0;
    };

    class CaseTreeBaseMulti : public TestCase {
    public:
        CaseTreeBaseMulti() : TestCase("TreeBase 多态与多树共存测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            constexpr size_t N = 10;
            Data _nodes[N];
            std::span<Data, N> pool(_nodes, N);

            for (int i = 0; i < N; i++) {
                pool[i] = {.data = i};
            }

            expect("在同一组对象上构建两个不同的树结构 (TreeA 和 TreeB)");
            // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

            // TreeA: 0->1->2
            pool[0].TreeA::link_child(&pool[1]);
            pool[1].TreeA::link_child(&pool[2]);

            // TreeB: 0->5->6
            pool[0].TreeB::link_child(&pool[5]);
            pool[5].TreeB::link_child(&pool[6]);

            check("验证 TreeA 结构");
            ttest(pool[0].TreeA::get_children().size() == 1);
            ttest(pool[1].TreeA::get_parent()->data == 0);

            check("验证 TreeB 结构与 LCA (LCA(6, 5) in TreeB is 5)");
            ttest(pool[0].TreeB::get_children().size() == 1);
            ttest(TreeB::lca(&pool[6], &pool[5])->data == 5);

            // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
        }
    };

    void collect_tests(TestFramework &framework) {
        auto cases = util::ArrayList<TestCase *>();
        cases.push_back(new CaseTreeStructure());
        cases.push_back(new CaseTreeLCA());
        cases.push_back(new CaseTreeBaseMulti());

        framework.add_category(new TestCategory("tree", std::move(cases)));
    }
}  // namespace test::tree