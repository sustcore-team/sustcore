/**
 * @file expected.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief expected 测试实现
 * @version alpha-1.0.0
 * @date 2026-03-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/expected.h>

#include <expected>

namespace test::expected {

	class CaseUnexpected : public TestCase {
	public:
		CaseUnexpected() : TestCase("unexpected 基础行为") {}

		void _run(void* env [[maybe_unused]]) const noexcept override {
			std::unexpected<int> u0(10);
			std::unexpected<int> u1(20);

			ttest(u0.error() == 10);
			ttest(u1.error() == 20);

			action("交换两个 unexpected 对象");
			u0.swap(u1);
			ttest(u0.error() == 20);
			ttest(u1.error() == 10);

			action("验证 make_unexpected 工厂");
			auto u2 = std::make_unexpected(42);
			ttest(u2.error() == 42);
		}
	};

	class CaseExpectedValueAndError : public TestCase {
	public:
		CaseExpectedValueAndError() : TestCase("expected 值态与错态") {}

		void _run(void* env [[maybe_unused]]) const noexcept override {
			std::expected<int, int> ok(123);
			std::expected<int, int> err(std::unexpect, -5);

			expect("值态对象应可直接访问 value/operator*");
			tassert(ok.has_value());
			ttest(ok.value() == 123);
			ttest(*ok == 123);
			ttest(ok.value_or(0) == 123);

			expect("错态对象应可访问 error 且 value_or 返回默认值");
			tassert(!err.has_value());
			ttest(err.error() == -5);
			ttest(err.value_or(99) == 99);
		}
	};

	class CaseExpectedAssignAndEmplace : public TestCase {
	public:
		CaseExpectedAssignAndEmplace() : TestCase("expected 赋值/转态/emplace") {}

		void _run(void* env [[maybe_unused]]) const noexcept override {
			std::expected<int, int> e0(1);
			std::expected<int, int> e1(std::unexpect, 7);

			action("value -> error 转换");
			e0 = e1;
			tassert(!e0.has_value());
			ttest(e0.error() == 7);

			action("error -> value 转换");
			e0 = 88;
			tassert(e0.has_value());
			ttest(e0.value() == 88);

			action("赋值 unexpected");
			e0 = std::unexpected<int>(11);
			tassert(!e0.has_value());
			ttest(e0.error() == 11);

			action("emplace 直接构造 value");
			e0.emplace(256);
			tassert(e0.has_value());
			ttest(e0.value() == 256);
		}
	};

	class CaseExpectedSwap : public TestCase {
	public:
		CaseExpectedSwap() : TestCase("expected swap 行为") {}

		void _run(void* env [[maybe_unused]]) const noexcept override {
			std::expected<int, int> a(3);
			std::expected<int, int> b(std::unexpect, 9);

			action("交换 value/error 两种状态");
			a.swap(b);
			tassert(!a.has_value());
			tassert(b.has_value());
			ttest(a.error() == 9);
			ttest(b.value() == 3);
		}
	};

	class CaseExpectedVoid : public TestCase {
	public:
		CaseExpectedVoid() : TestCase("expected<void, E> 行为") {}

		void _run(void* env [[maybe_unused]]) const noexcept override {
			std::expected<void, int> ok;
			std::expected<void, int> err(std::unexpect, 5);

			expect("默认构造应处于 value 状态");
			tassert(ok.has_value());

			expect("错态应保存 error");
			tassert(!err.has_value());
			ttest(err.error() == 5);

			action("从 value 赋值为 error，再 emplace 回 value");
			ok = std::unexpected<int>(8);
			tassert(!ok.has_value());
			ttest(ok.error() == 8);

			ok.emplace();
			tassert(ok.has_value());

			action("void 特化下交换状态");
			ok.swap(err);
			tassert(!ok.has_value());
			tassert(err.has_value());
			ttest(ok.error() == 5);
		}
	};

	class CaseExpectedMonadicOps : public TestCase {
	public:
		CaseExpectedMonadicOps() : TestCase("expected 单子操作") {}

		void _run(void* env [[maybe_unused]]) const noexcept override {
			std::expected<int, int> ok(10);
			std::expected<int, int> err(std::unexpect, -3);

			action("and_then: 值态执行, 错态短路");
			auto a0 = ok.and_then([](int v) {
				return std::expected<int, int>(v + 5);
			});
			ttest(a0.has_value());
			ttest(a0.value() == 15);

			int called = 0;
			auto a1 = err.and_then([&called](int v [[maybe_unused]]) {
				++called;
				return std::expected<int, int>(0);
			});
			ttest(!a1.has_value());
			ttest(a1.error() == -3);
			ttest(called == 0);

			action("transform: 映射值而保留错误");
			auto t0 = ok.transform([](int v) {
				return v * 2;
			});
			ttest(t0.has_value());
			ttest(t0.value() == 20);

			auto t1 = err.transform([](int v) {
				return v * 2;
			});
			ttest(!t1.has_value());
			ttest(t1.error() == -3);

			action("or_else: 错态恢复, 值态透传");
			auto o0 = err.or_else([](int e) {
				return std::expected<int, int>(-e);
			});
			ttest(o0.has_value());
			ttest(o0.value() == 3);

			int recover_called = 0;
			auto o1 = ok.or_else([&recover_called](int e [[maybe_unused]]) {
				++recover_called;
				return std::expected<int, int>(0);
			});
			ttest(o1.has_value());
			ttest(o1.value() == 10);
			ttest(recover_called == 0);

			action("transform_error: 只映射错误类型");
			auto te0 = err.transform_error([](int e) {
				return e * 10L;
			});
			ttest(!te0.has_value());
			ttest(te0.error() == -30L);

			auto te1 = ok.transform_error([](int e) {
				return e * 10L;
			});
			ttest(te1.has_value());
			ttest(te1.value() == 10);

			action("void 特化: and_then/transform/or_else/transform_error");
			std::expected<void, int> vok;
			std::expected<void, int> verr(std::unexpect, 7);

			auto va0 = vok.and_then([]() {
				return std::expected<int, int>(42);
			});
			ttest(va0.has_value());
			ttest(va0.value() == 42);

			int void_called = 0;
			auto va1 = verr.and_then([&void_called]() {
				++void_called;
				return std::expected<int, int>(0);
			});
			ttest(!va1.has_value());
			ttest(va1.error() == 7);
			ttest(void_called == 0);

			auto vt0 = vok.transform([]() {
				return 9;
			});
			ttest(vt0.has_value());
			ttest(vt0.value() == 9);

			int seen_err = 0;
			auto vo0 = verr.or_else([&seen_err](int e) {
				seen_err = e;
				return std::expected<void, int>();
			});
			ttest(vo0.has_value());
			ttest(seen_err == 7);

			auto vte0 = verr.transform_error([](int e) {
				return e * 2L;
			});
			ttest(!vte0.has_value());
			ttest(vte0.error() == 14L);
		}
	};

	void collect_tests(TestFramework& framework) {
		auto cases = util::ArrayList<TestCase*>();
		cases.push_back(new CaseUnexpected());
		cases.push_back(new CaseExpectedValueAndError());
		cases.push_back(new CaseExpectedAssignAndEmplace());
		cases.push_back(new CaseExpectedSwap());
		cases.push_back(new CaseExpectedVoid());
		cases.push_back(new CaseExpectedMonadicOps());

		framework.add_category(new TestCategory("expected", std::move(cases)));
	}
}  // namespace test::expected
