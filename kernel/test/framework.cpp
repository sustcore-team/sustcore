/**
 * @file framework.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试框架实现
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <logger.h>
#include <sus/ansi.h>
#include <sus/list.h>
#include <sus/pair.h>
#include <test/array.h>
#include <test/buddy.h>
#include <test/cap.h>
#include <test/coroutine.h>
#include <test/expected.h>
#include <test/framework.h>
#include <test/functional.h>
#include <test/meta.h>
#include <test/optional.h>
#include <test/path.h>
#include <test/printf.h>
#include <test/raii.h>
#include <test/ranges.h>
#include <test/ringbuf.h>
#include <test/schd/fcfs.h>
#include <test/schd/rr.h>
#include <test/slub.h>
#include <test/source_location.h>
#include <test/string.h>
#include <test/string_view.h>
#include <test/tree.h>
#include <test/unordered_map.h>
#include <test/vector.h>
#include <test/wait.h>

void collect_tests(TestFramework& framework) {
    // test::array::collect_tests(framework);
    test::buddy::collect_tests(framework);
    test::slub::collect_tests(framework);
    // test::cap::collect_tests(framework);
    test::coroutine::collect_tests(framework);
    // test::expected::collect_tests(framework);
    // test::fs::collect_tests(framework);
    // test::functional::collect_tests(framework);
    // test::meta::collect_tests(framework);
    test::ringbuf::collect_tests(framework);
    test::wait::collect_tests(framework);
    // test::optional::collect_tests(framework);
    // test::path::collect_tests(framework);
    // test::printf::collect_tests(framework);
    // test::raii::collect_tests(framework);
    // test::ranges::collect_tests(framework);
    test::schd_test::fcfs::collect_tests(framework);
    test::schd_test::rr::collect_tests(framework);
    // test::slub::collect_tests(framework);
    // test::source_location::collect_tests(framework);
    // test::string::collect_tests(framework);
    // test::string_view::collect_tests(framework);
    // test::tree::collect_tests(framework);
    // test::unordered_map::collect_tests(framework);
    // test::vector::collect_tests(framework);
}

void TestFramework::run_all() const {
    struct FailedReason {
        const TestCategory* category         = nullptr;
        const TestCase* test_case            = nullptr;
        util::ArrayList<std::string> reasons = {0};

        FailedReason()                                     = default;
        ~FailedReason()                                    = default;
        FailedReason(const FailedReason& other)            = default;
        FailedReason& operator=(const FailedReason& other) = default;

        FailedReason(FailedReason&& other) noexcept
            : category(other.category),
              test_case(other.test_case),
              reasons(std::move(other.reasons)) {
            other.category  = nullptr;
            other.test_case = nullptr;
        }
        FailedReason& operator=(FailedReason&& other) noexcept {
            if (this != &other) {
                category        = other.category;
                test_case       = other.test_case;
                reasons         = std::move(other.reasons);
                other.category  = nullptr;
                other.test_case = nullptr;
            }
            return *this;
        }

        FailedReason(const TestCategory* cat, const TestCase* tc)
            : category(cat), test_case(tc), reasons(tc->fail_reasons_list()) {}
    };
    util::ArrayList<FailedReason> failed_cases;
    unsigned int category_count = 0;
    unsigned int case_count     = 0;
    unsigned int passed_count   = 0;

    for (auto* category : _categories) {
        ++category_count;
        kprintfln("运行测试范畴: %s", category->name());
        void* env = category->setup();
        for (const auto* test_case : category->cases()) {
            ++case_count;
            bool result = test_case->run(env);
            if (result) {
                ++passed_count;
                kprintfln(ANSI_GRAPHIC(ANSI_FG_GREEN) "  [PASS]" ANSI_GRAPHIC(
                              ANSI_GM_RESET) " %s",
                          test_case->name());
            } else {
                kprintfln(ANSI_GRAPHIC(ANSI_FG_RED) "  [FAIL]" ANSI_GRAPHIC(
                              ANSI_GM_RESET) " %s",
                          test_case->name());
                failed_cases.emplace_back(category, test_case);
            }
        }
        category->teardown();
        kprintfln("");
    }

    if (failed_cases.empty()) {
        kprintfln(ANSI_GRAPHIC(ANSI_FG_GREEN)
                      "所有测试通过! 范畴: %u, 案例: %u" ANSI_GRAPHIC(
                          ANSI_GM_RESET),
                  category_count, case_count);
    } else {
        kprintfln(ANSI_GRAPHIC(ANSI_FG_RED)
                      "测试完成: 范畴: %u, 案例: %u, 通过: %u, 失败: %u" ANSI_GRAPHIC(
                          ANSI_GM_RESET),
                  category_count, case_count, passed_count,
                  static_cast<unsigned int>(failed_cases.size()));
        for (const auto& [category, test_case, reasons] : failed_cases) {
            kprintfln("  - %s:%s 失败! 原因是:", category->name(),
                      test_case->name());
            for (const auto& reason : reasons) {
                kprintfln("    - %s", reason.c_str());
            }
        }
    }
}
