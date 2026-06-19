/**
 * @file optional.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief optional 测试
 * @version alpha-1.0.0
 * @date 2026-05-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/optional.h>

#include <nt/errors.h>
#include <optional>
#include <type_traits>
#include <utility>

namespace test::optional {
    constexpr bool constexpr_optional_smoke() {
        std::optional<int> value;
        if (value.has_value()) {
            return false;
        }
        value.emplace(4);
        if (!value || *value != 4) {
            return false;
        }
        value = 8;
        auto doubled = value.transform([](int v) {
            return v * 2;
        });
        value.reset();
        return !value && doubled.has_value() && *doubled == 16;
    }

#if __cplusplus > 202302L
    static_assert(constexpr_optional_smoke());
#endif

    class CaseBasicState : public TestCase {
    public:
        CaseBasicState() : TestCase("基础状态") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::optional<int> empty;
            ttest(!empty.has_value());
            ttest(!static_cast<bool>(empty));
            ttest(empty == std::nullopt);

            std::optional<int> value(42);
            ttest(value.has_value());
            ttest(static_cast<bool>(value));
            ttest(*value == 42);
            ttest(value.value_nt().has_value());
            ttest(value.value_nt().value().get() == 42);

            value = std::nullopt;
            ttest(!value.has_value());
            auto fail = value.value_nt();
            ttest(!fail);
            ttest(fail.error() == std::error_type::NULLPTR);
        }
    };

    class PairLike {
    public:
        int first;
        int second;

        constexpr PairLike(int a, int b) : first(a), second(b) {}
    };

    class CaseConstruction : public TestCase {
    public:
        CaseConstruction() : TestCase("构造与访问") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::optional<PairLike> p(std::in_place, 1, 2);
            ttest(p.has_value());
            ttest(p->first == 1);
            ttest((*p).second == 2);

            std::optional deduced(9);
            ttest((std::is_same_v<decltype(deduced), std::optional<int>>));
            ttest(deduced.value_nt().value().get() == 9);

            const std::optional<int> cvalue(7);
            auto ok = cvalue.value_nt();
            ttest(ok.has_value());
            ttest(ok.value().get() == 7);
        }
    };

    class CaseAssignResetEmplace : public TestCase {
    public:
        CaseAssignResetEmplace() : TestCase("赋值 reset emplace") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::optional<int> a;
            a = 3;
            ttest(a.has_value());
            ttest(*a == 3);

            std::optional<int> b(a);
            ttest(b.has_value());
            ttest(*b == 3);

            a.reset();
            ttest(!a.has_value());

            a = b;
            ttest(a.has_value());
            ttest(*a == 3);

            a.emplace(8);
            ttest(*a == 8);

            std::optional<int> c(std::nullopt);
            a.swap(c);
            ttest(!a.has_value());
            ttest(c.has_value());
            ttest(*c == 8);
        }
    };

    class CaseValueOrAndCompare : public TestCase {
    public:
        CaseValueOrAndCompare() : TestCase("value_or 与比较") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::optional<int> empty;
            std::optional<int> a(1);
            std::optional<int> b(2);
            std::optional<int> a2(1);

            ttest(empty.value_or(9) == 9);
            ttest(a.value_or(9) == 1);

            ttest(a == a2);
            ttest(a != b);
            ttest(empty < a);
            ttest(a < b);
            ttest(b > a);
            ttest(a == 1);
            ttest(1 == a);
            ttest(empty != 1);
            ttest(empty < 1);
            ttest(1 > empty);
            ttest(a > std::nullopt);
        }
    };

    class Tracked {
    public:
        static int live_count;
        int value = 0;

        Tracked() : value(0) {
            ++live_count;
        }
        explicit Tracked(int v) : value(v) {
            ++live_count;
        }
        Tracked(const Tracked& other) : value(other.value) {
            ++live_count;
        }
        Tracked(Tracked&& other) noexcept : value(other.value) {
            other.value = -1;
            ++live_count;
        }
        Tracked& operator=(const Tracked& other) {
            value = other.value;
            return *this;
        }
        Tracked& operator=(Tracked&& other) noexcept {
            value       = other.value;
            other.value = -1;
            return *this;
        }
        ~Tracked() {
            --live_count;
        }
    };

    int Tracked::live_count = 0;

    class CaseObjectLifetime : public TestCase {
    public:
        CaseObjectLifetime() : TestCase("对象生命周期") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            Tracked::live_count = 0;
            {
                std::optional<Tracked> opt;
                ttest(Tracked::live_count == 0);

                opt.emplace(5);
                ttest(Tracked::live_count == 1);
                ttest(opt->value == 5);

                opt.emplace(6);
                ttest(Tracked::live_count == 1);
                ttest(opt->value == 6);

                opt.reset();
                ttest(Tracked::live_count == 0);
            }
            ttest(Tracked::live_count == 0);
        }
    };

    class CaseMonadicOps : public TestCase {
    public:
        CaseMonadicOps() : TestCase("单子操作") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::optional<int> ok(10);
            std::optional<int> empty;

            auto t0 = ok.transform([](int v) {
                return v * 2;
            });
            ttest(t0.has_value());
            ttest(*t0 == 20);

            auto t1 = empty.transform([](int v) {
                return v * 2;
            });
            ttest(!t1.has_value());

            auto a0 = ok.and_then([](int v) {
                return std::optional<int>(v + 1);
            });
            ttest(a0.has_value());
            ttest(*a0 == 11);

            int called = 0;
            auto a1 = empty.and_then([&called](int v [[maybe_unused]]) {
                ++called;
                return std::optional<int>(0);
            });
            ttest(!a1.has_value());
            ttest(called == 0);

            auto o0 = empty.or_else([]() {
                return std::optional<int>(33);
            });
            ttest(o0.has_value());
            ttest(*o0 == 33);

            int recover_called = 0;
            auto o1 = ok.or_else([&recover_called]() {
                ++recover_called;
                return std::optional<int>(0);
            });
            ttest(o1.has_value());
            ttest(*o1 == 10);
            ttest(recover_called == 0);
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseBasicState());
        cases.push_back(new CaseConstruction());
        cases.push_back(new CaseAssignResetEmplace());
        cases.push_back(new CaseValueOrAndCompare());
        cases.push_back(new CaseObjectLifetime());
        cases.push_back(new CaseMonadicOps());

        framework.add_category(new TestCategory("optional", std::move(cases)));
    }
}  // namespace test::optional
