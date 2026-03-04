/**
 * @file string.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief string 测试
 * @version alpha-1.1.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kio.h>
#include <test/string.h>

#include <cassert>
#include <cstdio>
#include <string>

namespace test::string {

    class CaseSSO : public TestCase {
    public:
        CaseSSO() : TestCase("String SSO 基础功能测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("构造短字符串 (SSO 模式)");
            std::string s1 = "Hello";
            ttest(s1.size() == 5);
            ttest(s1.capacity() >= 5);
            ttest(!s1.empty());
            ttest(std::string::traits_type::compare(s1.c_str(), "Hello", 5) == 0);

            action("测试 SSO 拷贝构造");
            std::string s2 = s1;
            ttest(s2 == s1);
            ttest(s2.size() == 5);

            action("测试 SSO 移动构造");
            std::string s3 = std::move(s1);
            ttest(s3 == s2);
            ttest(s1.empty());
            ttest(s1.size() == 0);

            action("测试 SSO 追加字符");
            s3.push_back('!');
            ttest(s3 == "Hello!");
            ttest(s3.size() == 6);

            action("测试 SSO 追加字符串");
            s3 += " SSO";
            ttest(s3 == "Hello! SSO");
        }
    };

    class CaseLongString : public TestCase {
    public:
        CaseLongString() : TestCase("String 动态分配 (Long) 模式测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("构造超过 SSO 阈值的长字符串");
            const char* long_str = "This is a very long string that should definitely exceed the SSO capacity of our implementation.";
            size_t len = 0;
            while (long_str[len]) len++;

            std::string s1(long_str);
            ttest(s1.size() == len);
            ttest(s1 == long_str);

            action("测试长字符串拷贝");
            std::string s2 = s1;
            ttest(s2 == s1);
            ttest(s2.data() != s1.data()); // 应该是不同的内存地址

            action("测试长字符串移动");
            const char* old_ptr = s1.data();
            std::string s3 = std::move(s1);
            ttest(s3 == long_str);
            ttest(s3.data() == old_ptr); // 移动应该偷走指针
            ttest(s1.empty());

            action("测试长字符串追加");
            s3 += " more text.";
            ttest(s3.size() == len + 11);
        }
    };

    class CaseGrowth : public TestCase {
    public:
        CaseGrowth() : TestCase("String 扩容与 SSO 转换测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("从小字符串逐渐增加到长字符串");
            std::string s = "Start";
            size_t initial_cap = s.capacity();

            action("连续 push_back 触发转换");
            for (int i = 0; i < 100; ++i) {
                s.push_back('a');
            }
            ttest(s.size() == 105);
            ttest(s.capacity() > initial_cap);
            ttest(s.capacity() >= 105);

            action("验证内容正确性");
            bool match = true;
            for (int i = 0; i < 5; ++i) if (s[i] != "Start"[i]) match = false;
            for (int i = 5; i < 105; ++i) if (s[i] != 'a') match = false;
            test(match, "字符串内容不正确");

            action("测试 reserve 提前扩容");
            std::string s2;
            s2.reserve(200);
            ttest(s2.capacity() >= 200);
            s2 = "Short";
            ttest(s2.capacity() >= 200); // 赋值后不应缩小容量
            ttest(s2 == "Short");
        }
    };

    class CaseComparison : public TestCase {
    public:
        CaseComparison() : TestCase("String 比较操作测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::string a = "apple";
            std::string b = "banana";
            std::string a2 = "apple";

            ttest(a == a2);
            ttest(a != b);
            ttest(a.compare(b) < 0);
            ttest(b.compare(a) > 0);
            ttest(a.compare(a2) == 0);

            action("不同模式下的比较 (SSO vs Long)");
            std::string long_a = "apple and more more more more more more more more";
            std::string long_a2 = long_a;
            ttest(long_a == long_a2);
            ttest(long_a != a);
        }
    };

    class CaseBoundary : public TestCase {
    public:
        CaseBoundary() : TestCase("String 空字串与边界稳定性测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("默认构造的空字符串状态");
            std::string s1;
            ttest(s1.empty());
            ttest(s1.size() == 0);
            ttest(s1.c_str() != nullptr); // 应指向一个有效地址（哪怕是内部缓冲区）
            ttest(s1.c_str()[0] == '\0');

            action("清空长字符串并验证状态");
            std::string s2 = "This is a long string that is definitely in dynamic mode.";
            s2.clear();
            ttest(s2.empty());
            ttest(s2.size() == 0);
            ttest(s2.c_str()[0] == '\0');
            ttest(s2.capacity() > 0); // 应该保留原有的缓冲区以备复用

            action("测试仅包含空字符的字符串构造");
            std::string s3("", 0);
            ttest(s3.size() == 0);
            ttest(s3.empty());
        }
    };

    class CaseSelfAssignment : public TestCase {
    public:
        CaseSelfAssignment() : TestCase("String 自赋值安全性测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            action("SSO 模式下的自赋值");
            std::string s1 = "small";
            s1 = s1;
            ttest(s1 == "small");

            action("Long 模式下的自赋值");
            std::string s2 = "This is a very long string used for testing self-assignment safety in dynamic allocation mode.";
            const char* old_ptr = s2.data();
            s2 = s2;
            ttest(s2.size() > 0);
            ttest(s2.data() == old_ptr); // 优秀的实现应当检测出自赋值并不进行重分配

            action("移动自赋值");
            s2 = std::move(s2);
            ttest(s2.size() > 0); // 移动自赋值后对象应当依然处于合法状态
        }
    };

    class CaseSSOThreshold : public TestCase {
    public:
        CaseSSOThreshold() : TestCase("String SSO 临界容量测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            // 在 64 位环境下，SSO 容量通常是 23
            std::string s;
            size_t sso_cap = s.capacity();
            
            expect("填充到 SSO 最大容量");
            for (size_t i = 0; i < sso_cap; ++i) {
                s.push_back('x');
            }
            ttest(s.size() == sso_cap);
            const char* sso_ptr = s.data();

            action("再追加一个字符触发临界转换");
            s.push_back('y');
            ttest(s.size() == sso_cap + 1);
            ttest(s.data() != sso_ptr); // 指针应当发生变化，转换到了长模式
            ttest(s[sso_cap] == 'y');
        }
    };

    class CaseStress : public TestCase {
    public:
        CaseStress() : TestCase("String 频繁分配与大规模追加压力测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::string s;
            action("执行 1000 次追加操作");
            for (int i = 0; i < 1000; ++i) {
                s += "word ";
            }
            ttest(s.size() == 5000);
            
            action("验证末尾内容");
            ttest(s[4999] == ' ');
            ttest(s[4995] == 'w');

            action("大字符串整体拷贝");
            std::string s_copy = s;
            ttest(s_copy.size() == s.size());
            ttest(s_copy[2500] == s[2500]);
        }
    };

    class CaseInsertErase : public TestCase {
    public:
        CaseInsertErase() : TestCase("String 插入与删除功能测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("在 SSO 字符串中间插入");
            std::string s = "abc";
            s.insert(1, "XYZ"); 
            ttest(s == "aXYZbc");
            ttest(s.size() == 6);

            action("在 SSO 字符串头部插入");
            s.insert(0, "123");
            ttest(s == "123aXYZbc");

            action("触发 SSO 到 Long 的插入");
            std::string long_str(50, 'A');
            s.insert(3, long_str);
            ttest(s.size() == 9 + 50);
            ttest(s[3] == 'A');
            ttest(s[52] == 'A');
            ttest(s[53] == 'a');

            action("删除操作测试");
            std::string s2 = "0123456789";
            s2.erase(3, 4); // 删除 "3456"
            ttest(s2 == "012789");
            ttest(s2.size() == 6);

            action("删除到末尾");
            s2.erase(4); // 删除 "89"
            ttest(s2 == "0127");

            action("清空式删除");
            s2.erase(0, 100);
            ttest(s2.empty());
        }
    };

    class CaseReplace : public TestCase {
    public:
        CaseReplace() : TestCase("String 替换功能测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            // 注意: basic_string::replace 的实现目前在代码中似乎是不完整的 (private __replace)
            // 但我们可以测试 operator= 或其他已公开的修改接口
            expect("通过赋值和追加进行替换模拟");
            std::string s = "hello world";
            s = "new string";
            ttest(s == "new string");

            action("使用 assign 覆盖");
            s.assign("replaced", 4);
            ttest(s == "repl");
        }
    };

    class CaseSearch : public TestCase {
    public:
        CaseSearch() : TestCase("String 搜索功能测试 (starts_with/ends_with/contains)") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::string s = "The quick brown fox jumps over the lazy dog";
            
            ttest(s.starts_with("The"));
            ttest(s.starts_with('T'));
            ttest(!s.starts_with("the")); // 大小写敏感

            ttest(s.ends_with("dog"));
            ttest(s.ends_with('g'));
            ttest(!s.ends_with("lazy"));

            ttest(s.contains("fox"));
            ttest(s.contains('j'));
            ttest(!s.contains("cat"));
        }
    };

    class CaseSubstr : public TestCase {
    public:
        CaseSubstr() : TestCase("String 子串提取测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::string s = "0123456789";
            
            action("提取中间子串");
            std::string sub1 = s.substr(3, 4);
            ttest(sub1 == "3456");

            action("提取到末尾");
            std::string sub2 = s.substr(7);
            ttest(sub2 == "789");

            action("超出范围处理");
            std::string sub3 = s.substr(10);
            ttest(sub3.empty());
        }
    };

    class CaseAliasing : public TestCase {
    public:
        CaseAliasing() : TestCase("String 别名冲突测试 (Self-referencing)") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("append 自身的一部分");
            std::string s = "abc";
            s.append(s.c_str()); 
            ttest(s == "abcabc");

            action("insert 自身的一部分");
            std::string s2 = "123";
            // insert自身
            comment("该行为实际上是一个UB, 因此如果测试失败也无需担心");
            s2.insert(1, s2.c_str()); 
            ttest(s2 == "112323");
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseSSO());
        cases.push_back(new CaseLongString());
        cases.push_back(new CaseGrowth());
        cases.push_back(new CaseComparison());
        cases.push_back(new CaseBoundary());
        cases.push_back(new CaseSelfAssignment());
        cases.push_back(new CaseSSOThreshold());
        cases.push_back(new CaseStress());
        cases.push_back(new CaseInsertErase());
        cases.push_back(new CaseSearch());
        cases.push_back(new CaseSubstr());
        cases.push_back(new CaseAliasing());

        framework.add_category(new TestCategory("string", std::move(cases)));
    }
}  // namespace test::string
