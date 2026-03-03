/**
 * @file path.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief path测试
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sus/path.h>
#include <test/path.h>

namespace test::path {

    class CasePathCombine : public TestCase {
    public:
        CasePathCombine() : TestCase("Path 连接与构建") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("使用 / 运算符连接路径组件");
            util::Path p = util::Path("home") / "user" / "docs" / "report.txt";
            ttest(p == "home/user/docs/report.txt");

            expect("连接空路径或带斜杠的路径");
            util::Path p2 = util::Path("") / "/usr" / "bin/";
            ttest(p2 == "/usr/bin/");
        }
    };

    class CasePathComponents : public TestCase {
    public:
        CasePathComponents() : TestCase("Path 组件提取 (Filename, Extension, etc.)") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            util::Path p("/home/flysong/Sustcore/report.tar.gz");

            check("提取文件名");
            ttest(p.filename() == "report.tar.gz");

            check("提取基础名 (Stem)");
            ttest(p.stem() == "report.tar");

            check("提取扩展名 (Extension)");
            ttest(p.extension() == ".gz");

            check("提取父目录");
            ttest(p.parent_path() == "/home/flysong/Sustcore");
        }
    };

    class CasePathNormalize : public TestCase {
    public:
        CasePathNormalize() : TestCase("Path 规范化测试 (Normalization)") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("处理冗余斜杠");
            ttest(util::Path("///usr//local/../bin").normalize() == "/usr/bin");

            expect("处理 . 和 .. 符号");
            ttest(util::Path("./a/b/../c/./d").normalize() == "a/c/d");

            expect("处理根目录往上的 .. (应保持在根目录)");
            ttest(util::Path("/../../etc").normalize() == "/etc");
        }
    };

    class CasePathIteration : public TestCase {
    public:
        CasePathIteration() : TestCase("Path 迭代器测试 (Walking)") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            util::Path p("/home/user/docs");
            const char* expected[] = {"/", "home", "user", "docs"};
            int i = 0;

            expect("遍历路径中的每个组件");
            for (auto it = p.begin(); it != p.end(); ++it) {
                util::string s((*it).data(), (*it).size());
                if (i < 4) {
                    ttest(s == expected[i]);
                }
                i++;
            }
            ttest(i == 4);
        }
    };

    class CasePathRelative : public TestCase {
    public:
        CasePathRelative() : TestCase("Path 相对路径计算 (relative_to)") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            util::Path target("/home/user/docs/report.txt");
            util::Path base("/home/user");

            check("计算 target 相对于 base 的路径");
            ttest(target.relative_to(base) == "docs/report.txt");

            expect("处理不相关的基路径 (预期返回原始路径或规范化后的路径)");
            util::Path other("/usr/bin");
            ttest(target.relative_to(other) == "../../home/user/docs/report.txt");
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CasePathCombine());
        cases.push_back(new CasePathComponents());
        cases.push_back(new CasePathNormalize());
        cases.push_back(new CasePathIteration());
        cases.push_back(new CasePathRelative());

        framework.add_category(new TestCategory("path", std::move(cases)));
    }
}  // namespace test::path