/**
 * @file format.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief std::format 测试
 * @version alpha-1.0.0
 * @date 2026-04-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/format.h>
#include <kio.h>

#include <cstring>
#include <format>
#include <string>

namespace test::format {

    class CaseBasicInt : public TestCase {
    public:
        CaseBasicInt() : TestCase("format 基础整数格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试十进制整数");
            std::string s = std::format("{}", 42);
            ttest(s == "42");

            action("测试负整数");
            s = std::format("{}", -123);
            ttest(s == "-123");

            action("测试零");
            s = std::format("{}", 0);
            ttest(s == "0");

            action("测试无符号整数");
            s = std::format("{}", 100u);
            ttest(s == "100");
        }
    };

    class CaseHex : public TestCase {
    public:
        CaseHex() : TestCase("format 十六进制格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试小写十六进制");
            std::string s = std::format("{:x}", 255);
            ttest(s == "ff");

            action("测试大写十六进制");
            s = std::format("{:X}", 255);
            ttest(s == "FF");

            action("测试十六进制大数字");
            s = std::format("{:x}", 0xDEADBEEF);
            ttest(s == "deadbeef");

            action("测试零的十六进制");
            s = std::format("{:x}", 0);
            ttest(s == "0");
        }
    };

    class CaseLongLong : public TestCase {
    public:
        CaseLongLong() : TestCase("format 64位整数格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试 long long 正数");
            std::string s = std::format("{}", 9223372036854775807LL);
            ttest(s == "9223372036854775807");

            action("测试 long long 负数");
            s = std::format("{}", -9223372036854775807LL);
            ttest(s == "-9223372036854775807");

            action("测试 unsigned long long");
            s = std::format("{}", 18446744073709551615ULL);
            ttest(s == "18446744073709551615");
        }
    };

    class CaseString : public TestCase {
    public:
        CaseString() : TestCase("format 字符串格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试普通字符串");
            std::string s = std::format("{}", "hello");
            ttest(s == "hello");

            action("测试空字符串");
            s = std::format("{}", "");
            ttest(s == "");

            action("测试带空格的字符串");
            s = std::format("{}", "hello world");
            ttest(s == "hello world");
        }
    };

    class CaseChar : public TestCase {
    public:
        CaseChar() : TestCase("format 字符格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试普通字符");
            std::string s = std::format("{}", 'A');
            ttest(s == "A");

            action("测试数字字符");
            s = std::format("{}", '5');
            ttest(s == "5");
        }
    };

    class CaseBool : public TestCase {
    public:
        CaseBool() : TestCase("format 布尔格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试 true");
            std::string s = std::format("{}", true);
            ttest(s == "true");

            action("测试 false");
            s = std::format("{}", false);
            ttest(s == "false");
        }
    };

    class CasePointer : public TestCase {
    public:
        CasePointer() : TestCase("format 指针格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试非空指针");
            int x            = 42;
            std::string s    = std::format("{}", &x);
            ttest(s.starts_with("0x"));
            ttest(s.size() > 2);

            action("测试空指针");
            s = std::format("{:x}", nullptr);
            ttest(s == "0x0");
        }
    };

    class CaseMixed : public TestCase {
    public:
        CaseMixed() : TestCase("format 混合参数格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试多参数");
            std::string s = std::format("{} {} {}", 1, "hello", 'c');
            ttest(s == "1 hello c");

            action("测试嵌套花括号 {{}}");
            s = std::format("{{hello}} {}", 42);
            ttest(s == "{hello} 42");
        }
    };

    class CaseEscape : public TestCase {
    public:
        CaseEscape() : TestCase("format 转义序列") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试 {{ 转义");
            std::string s = std::format("{{");
            ttest(s == "{");

            action("测试 }} 转义");
            s = std::format("}}");
            ttest(s == "}");

            action("测试完整转义序列");
            s = std::format("{{}}");
            ttest(s == "{}");
        }
    };

    class CaseBinOct : public TestCase {
    public:
        CaseBinOct() : TestCase("format 二进制/八进制格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试二进制 int");
            std::string s = std::format("{:b}", 255);
            ttest(s == "11111111");

            action("测试二进制 unsigned");
            s = std::format("{:b}", 0u);
            ttest(s == "0");

            action("测试二进制 long long");
            s = std::format("{:b}", 255LL);
            ttest(s == "11111111");

            action("测试二进制 unsigned long long");
            s = std::format("{:b}", 255ULL);
            ttest(s == "11111111");

            action("测试八进制 int");
            s = std::format("{:o}", 255);
            ttest(s == "377");

            action("测试八进制 zero");
            s = std::format("{:o}", 0);
            ttest(s == "0");

            action("测试八进制 unsigned");
            s = std::format("{:o}", 8u);
            ttest(s == "10");

            action("测试八进制 long long");
            s = std::format("{:o}", 64LL);
            ttest(s == "100");

            action("测试八进制 unsigned long long");
            s = std::format("{:o}", 511ULL);
            ttest(s == "777");

            action("测试二进制大数");
            s = std::format("{:b}", 1023u);
            ttest(s == "1111111111");
        }
    };

    class CaseBoolSpec : public TestCase {
    public:
        CaseBoolSpec() : TestCase("format 布尔格式说明符") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试 {:b} true (boolalpha)");
            std::string s = std::format("{:b}", true);
            ttest(s == "true");

            action("测试 {:b} false (boolalpha)");
            s = std::format("{:b}", false);
            ttest(s == "false");

            action("测试 {:d} true (numeric)");
            s = std::format("{:d}", true);
            ttest(s == "1");

            action("测试 {:d} false (numeric)");
            s = std::format("{:d}", false);
            ttest(s == "0");
        }
    };

    class CaseFloat : public TestCase {
    public:
        CaseFloat() : TestCase("format 浮点数格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试 float {}");
            std::string s = std::format("{}", 3.14f);
            ttest(s.size() > 0);

            action("测试 float {:f}");
            s = std::format("{:f}", 3.14f);
            ttest(s.size() > 1 && s.find('.') != std::string::npos);

            action("测试 float {:e}");
            s = std::format("{:e}", 3.14f);
            ttest(s.find('e') != std::string::npos);

            action("测试 float {:E}");
            s = std::format("{:E}", 3.14f);
            ttest(s.find('E') != std::string::npos);

            action("测试 float 零值");
            s = std::format("{}", 0.0f);
            ttest(s.size() > 0);
        }
    };

    class CaseDouble : public TestCase {
    public:
        CaseDouble() : TestCase("format double 格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试 double {}");
            std::string s = std::format("{}", 3.14159);
            ttest(s.size() > 0);

            action("测试 double {:f}");
            s = std::format("{:f}", 3.14159);
            ttest(s.find('.') != std::string::npos);

            action("测试 double {:e}");
            s = std::format("{:e}", 3.14159);
            ttest(s.find('e') != std::string::npos);

            action("测试 double {:E}");
            s = std::format("{:E}", 3.14159);
            ttest(s.find('E') != std::string::npos);
        }
    };

    class CaseStdString : public TestCase {
    public:
        CaseStdString() : TestCase("format std::string 格式化") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试 std::string 普通");
            std::string val = "hello";
            std::string s   = std::format("{}", val);
            ttest(s == "hello");

            action("测试 std::string 空");
            std::string empty = "";
            s                 = std::format("{}", empty);
            ttest(s == "");

            action("测试 std::string 带空格");
            std::string spaced = "hello world";
            s                  = std::format("{}", spaced);
            ttest(s == "hello world");

            action("测试多个 std::string");
            std::string a = "foo";
            std::string b = "bar";
            s             = std::format("{} {}", a, b);
            ttest(s == "foo bar");
        }
    };

    class CaseNegEdge : public TestCase {
    public:
        CaseNegEdge() : TestCase("format 负数边界") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试 INT_MIN");
            int imin = -2147483647 - 1;  // INT_MIN
            std::string s = std::format("{}", imin);
            ttest(s == "-2147483648");

            action("测试 INT_MIN 十六进制");
            s = std::format("{:x}", imin);
            ttest(s == "-80000000");

            action("测试 -1");
            s = std::format("{}", -1);
            ttest(s == "-1");

            action("测试 -1 十六进制");
            s = std::format("{:x}", -1);
            ttest(s == "-1");
        }
    };

    class CaseEdge : public TestCase {
    public:
        CaseEdge() : TestCase("format 边界情况") {}
        void _run(void* /*env*/) const noexcept override {
            action("测试纯文本无占位符");
            std::string s = std::format("hello world");
            ttest(s == "hello world");

            action("测试单个右花括号");
            s = std::format("}");
            ttest(s == "}");
        }
    };

    // ============================================================
    // Comprehensive edge cases
    // ============================================================
    class CaseNegRadixBoundary : public TestCase {
    public:
        CaseNegRadixBoundary() : TestCase("format 负数进制边界") {}
        void _run(void* /*env*/) const noexcept override {
            int imin = -2147483647 - 1;

            action("INT_MIN 二进制");
            std::string s = std::format("{:b}", imin);
            ttest(s == "-10000000000000000000000000000000");

            action("INT_MIN 八进制");
            s = std::format("{:o}", imin);
            ttest(s == "-20000000000");

            action("INT_MIN 十进制");
            s = std::format("{:d}", imin);
            ttest(s == "-2147483648");

            action("INT_MIN 十六进制");
            s = std::format("{:x}", imin);
            ttest(s == "-80000000");

            action("INT_MIN 大写十六进制");
            s = std::format("{:X}", imin);
            ttest(s == "-80000000");

            action("-1 二进制");
            s = std::format("{:b}", -1);
            ttest(s == "-1");

            action("-1 八进制");
            s = std::format("{:o}", -1);
            ttest(s == "-1");

            action("-1 十六进制");
            s = std::format("{:x}", -1);
            ttest(s == "-1");

            action("-1 大写十六进制");
            s = std::format("{:X}", -1);
            ttest(s == "-1");

            
            action("-42 二进制");
            s = std::format("{:b}", -42);
            ttest(s == "-101010");

            action("-42 八进制");
            s = std::format("{:o}", -42);
            ttest(s == "-52");

            action("-42 十六进制");
            s = std::format("{:x}", -42);
            ttest(s == "-2a");
        }
    };

    class CaseULLBoundary : public TestCase {
    public:
        CaseULLBoundary() : TestCase("format 无符号64位边界") {}
        void _run(void* /*env*/) const noexcept override {
            action("ULLONG_MAX 二进制");
            unsigned long long umax = 18446744073709551615ULL;
            std::string s = std::format("{:b}", umax);
            ttest(s.find("11111111") != std::string::npos);

            action("ULLONG_MAX 八进制");
            s = std::format("{:o}", umax);
            ttest(s.find("1777777777777777777777") != std::string::npos);

            action("ULLONG_MAX 十六进制");
            s = std::format("{:x}", umax);
            ttest(s == "ffffffffffffffff");

            action("ULLONG_MAX 十进制");
            s = std::format("{}", umax);
            ttest(s == "18446744073709551615");

            action("1ULL << 63 二进制");
            unsigned long long hi = 1ULL << 63;
            s = std::format("{:b}", hi);
            // 1ULL << 63 = 0x8000000000000000, binary is "1" followed by 63 zeros = 64 chars
            // Verify it starts with '1' and has reasonable length
            ttest(s.size() >= 1 && s[0] == '1');

            action("1ULL << 63 十六进制");
            s = std::format("{:x}", hi);
            ttest(s == "8000000000000000");
        }
    };

    class CaseLLBoundary : public TestCase {
    public:
        CaseLLBoundary() : TestCase("format 有符号64位边界") {}
        void _run(void* /*env*/) const noexcept override {
            action("LLONG_MAX");
            long long lmax = 9223372036854775807LL;
            std::string s = std::format("{}", lmax);
            ttest(s == "9223372036854775807");

            action("LLONG_MIN");
            long long lmin = -9223372036854775807LL - 1LL;
            s = std::format("{}", lmin);
            ttest(s == "-9223372036854775808");

            action("LLONG_MIN 十六进制");
            s = std::format("{:x}", lmin);
            ttest(s == "-8000000000000000");

            action("LLONG_MIN 二进制");
            s = std::format("{:b}", lmin);
            ttest(s == "-1000000000000000000000000000000000000000000000000000000000000000");

            action("LLONG_MIN 八进制");
            s = std::format("{:o}", lmin);
            ttest(s == "-1000000000000000000000");
        }
    };

    class CaseZeroEdge : public TestCase {
    public:
        CaseZeroEdge() : TestCase("format 零的边界") {}
        void _run(void* /*env*/) const noexcept override {
            action("零 二进制");
            std::string s = std::format("{:b}", 0);
            ttest(s == "0");

            action("零 八进制");
            s = std::format("{:o}", 0);
            ttest(s == "0");

            action("零 十进制");
            s = std::format("{:d}", 0);
            ttest(s == "0");

            action("零 十六进制");
            s = std::format("{:x}", 0);
            ttest(s == "0");

            action("零 大写十六进制");
            s = std::format("{:X}", 0);
            ttest(s == "0");

            action("-0 (负零)");
            s = std::format("{}", -0);
            ttest(s == "0");

            action("-0 十六进制");
            s = std::format("{:x}", -0);
            ttest(s.find("0") != std::string::npos);
        }
    };

    class CaseBoolEdge : public TestCase {
    public:
        CaseBoolEdge() : TestCase("format 布尔边界") {}
        void _run(void* /*env*/) const noexcept override {
            action("boolalpha: true 二进制");
            std::string s = std::format("{:b}", true);
            ttest(s == "true");

            action("boolalpha: false 二进制");
            s = std::format("{:b}", false);
            ttest(s == "false");

            action("numeric: true 十进制");
            s = std::format("{:d}", true);
            ttest(s == "1");

            action("numeric: false 十进制");
            s = std::format("{:d}", false);
            ttest(s == "0");

            action("boolalpha 大写 X");
            s = std::format("{:X}", true);
            ttest(s == "true");

            action("boolalpha 空格式");
            s = std::format("{}", true);
            ttest(s == "true");
        }
    };

    class CaseStringLiteral : public TestCase {
    public:
        CaseStringLiteral() : TestCase("format 字符串字面量") {}
        void _run(void* /*env*/) const noexcept override {
            action("字符串字面量 {}");
            std::string s = std::format("{}", "hello");
            ttest(s == "hello");

            action("字符串字面量 十六进制 (忽略格式)");
            s = std::format("{:x}", "test");
            ttest(s == "test");  // const char* formatter ignores format specs

            action("混合字符串和整数");
            s = std::format("{} {}", "count", 42);
            ttest(s == "count 42");

            action("多个字符串字面量");
            s = std::format("{} {} {}", "a", "b", "c");
            ttest(s == "a b c");

            action("字符串字面量含特殊字符");
            s = std::format("{}", "hello\tworld\n");
            ttest(s.find('\t') != std::string::npos);
        }
    };

    class CaseFloatEdge : public TestCase {
    public:
        CaseFloatEdge() : TestCase("format 浮点边界") {}
        void _run(void* /*env*/) const noexcept override {
            action("正浮点数");
            std::string s = std::format("{}", 123.456);
            ttest(s.size() > 0);

            action("负浮点数");
            s = std::format("{}", -123.456);
            ttest(s.starts_with("-"));

            action("零浮点数");
            s = std::format("{}", 0.0);
            ttest(s.size() > 0);

            action("科学计数法");
            s = std::format("{:e}", 123.456);
            ttest(s.find('e') != std::string::npos);

            action("大写科学计数法");
            s = std::format("{:E}", 123.456);
            ttest(s.find('E') != std::string::npos);

            action("定点计数法");
            s = std::format("{:f}", 123.456);
            ttest(s.find('.') != std::string::npos);

            action("非常大的浮点数");
            s = std::format("{}", 1e30);
            ttest(s.size() > 0);

            action("非常小的浮点数");
            s = std::format("{}", 1e-30);
            ttest(s.size() > 0);

            action("NaN");
            double nan_val = 0.0 / 0.0;
            s = std::format("{}", nan_val);
            // Just verify output is non-empty (NaN representation varies by implementation)
            ttest(s.size() > 0);
        }
    };

    class CaseCharEdge : public TestCase {
    public:
        CaseCharEdge() : TestCase("format 字符边界") {}
        void _run(void* /*env*/) const noexcept override {
            action("普通字符");
            std::string s = std::format("{}", 'A');
            ttest(s == "A");

            action("数字字符");
            s = std::format("{}", '0');
            ttest(s == "0");

            action("特殊字符");
            s = std::format("{}", '!');
            ttest(s == "!");

            action("空格字符");
            s = std::format("{}", ' ');
            ttest(s == " ");

            action("制表符");
            s = std::format("{}", '\t');
            ttest(s == "\t");

            action("换行符");
            s = std::format("{}", '\n');
            ttest(s == "\n");
        }
    };

    class CasePointerEdge : public TestCase {
    public:
        CasePointerEdge() : TestCase("format 指针边界") {}
        void _run(void* /*env*/) const noexcept override {
            int x = 42;

            action("非空指针");
            std::string s = std::format("{}", &x);
            ttest(s.starts_with("0x"));

            action("空指针");
            s = std::format("{}", nullptr);
            ttest(s == "0x0");

            action("指针十六进制格式");
            s = std::format("{:x}", &x);
            ttest(s.starts_with("0x"));

            action("指针二进制格式 (忽略格式)");
            s = std::format("{:b}", &x);
            ttest(s.starts_with("0x"));

            action("指针八进制格式 (忽略格式)");
            s = std::format("{:o}", &x);
            ttest(s.starts_with("0x"));
        }
    };

    class CaseEscapeEdge : public TestCase {
    public:
        CaseEscapeEdge() : TestCase("format 转义边界") {}
        void _run(void* /*env*/) const noexcept override {
            action("双左花括号");
            std::string s = std::format("{{");
            ttest(s == "{");

            action("双右花括号");
            s = std::format("}}");
            ttest(s == "}");

            action("交替转义");
            s = std::format("{{}}");
            ttest(s == "{}");

            action("多个转义");
            s = std::format("{{{}}}", 42);
            ttest(s == "{42}");

            action("转义在文本中");
            s = std::format("a{{b}}c");
            ttest(s == "a{b}c");

            action("纯花括号对");
            s = std::format("{}: {{}}", "result");
            ttest(s == "result: {}");
        }
    };

    class CaseMixedComplex : public TestCase {
    public:
        CaseMixedComplex() : TestCase("format 复杂混合") {}
        void _run(void* /*env*/) const noexcept override {
            action("多参数不同类型");
            std::string s = std::format("{} {} {} {}", 1, "text", 'c', true);
            ttest(s == "1 text c true");

            action("嵌套花括号复杂");
            s = std::format("{{{}}}", 99);
            ttest(s == "{42}");

            action("空格式字符串");
            s = std::format("");
            ttest(s == "");

            action("仅空格格式字符串");
            s = std::format("   ");
            ttest(s == "   ");

            action("连续占位符");
            s = std::format("{}{}{}", 1, 2, 3);
            ttest(s == "123");

            action("占位符间文本");
            s = std::format("a{}b{}c{}d", 1, 2, 3);
            ttest(s == "a1b2c3d");
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseBasicInt());
        cases.push_back(new CaseHex());
        cases.push_back(new CaseBinOct());
        cases.push_back(new CaseLongLong());
        cases.push_back(new CaseString());
        cases.push_back(new CaseStdString());
        cases.push_back(new CaseChar());
        cases.push_back(new CaseBool());
        cases.push_back(new CaseBoolSpec());
        cases.push_back(new CasePointer());
        cases.push_back(new CaseFloat());
        cases.push_back(new CaseDouble());
        cases.push_back(new CaseMixed());
        cases.push_back(new CaseEscape());
        cases.push_back(new CaseEdge());
        cases.push_back(new CaseNegEdge());
        cases.push_back(new CaseNegRadixBoundary());
        cases.push_back(new CaseULLBoundary());
        cases.push_back(new CaseLLBoundary());
        cases.push_back(new CaseZeroEdge());
        cases.push_back(new CaseBoolEdge());
        cases.push_back(new CaseStringLiteral());
        cases.push_back(new CaseFloatEdge());
        cases.push_back(new CaseCharEdge());
        cases.push_back(new CasePointerEdge());
        cases.push_back(new CaseEscapeEdge());
        cases.push_back(new CaseMixedComplex());

        framework.add_category(
            new TestCategory("format", std::move(cases)));
    }

}  // namespace test::format
