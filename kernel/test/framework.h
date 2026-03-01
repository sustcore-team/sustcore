/**
 * @file tester.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试框架
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <kio.h>
#include <sus/list.h>
#include <sus/macros.h>
#include <sus/mstring.h>

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class TestCase {
private:
    const char* _name;

    mutable bool passflag = false;
    mutable util::ArrayList<util::string> fail_reasons;

public:
    TestCase(const char* name) : _name(name) {}
    virtual ~TestCase() = default;

    [[nodiscard]]
    const char* name() const {
        return _name;
    }

    virtual void _run() const noexcept = 0;

protected:
    bool test(bool condition, util::string&& reason) const {
        if (!condition) {
            kprintfln(ANSI_GRAPHIC(ANSI_FG_RED) "      - 检查失败:" ANSI_GRAPHIC(
                ANSI_GM_RESET)" %s" , reason.c_str());
            fail_reasons.emplace_back(std::move(reason));
        } else {
            kprintfln(ANSI_GRAPHIC(ANSI_FG_GREEN) "    - 检查通过:" ANSI_GRAPHIC(
                ANSI_GM_RESET)" %s" ANSI_GRAPHIC(
                ANSI_GM_RESET), reason.c_str());
        }
        passflag &= condition;
        return condition;
    }
    bool test(bool condition, const char* reason) const {
        if (!condition) {
            kprintfln(ANSI_GRAPHIC(ANSI_FG_RED) "      - 检查失败:" ANSI_GRAPHIC(
                ANSI_GM_RESET)" %s" , reason);
            fail_reasons.emplace_back(reason);
        } else {
            kprintfln(ANSI_GRAPHIC(ANSI_FG_GREEN) "    - 检查通过:" ANSI_GRAPHIC(
                ANSI_GM_RESET)" %s" ANSI_GRAPHIC(
                ANSI_GM_RESET), reason);
        }
        passflag &= condition;
        return condition;
    }

    void expect(const char* reason) const {
        kprintfln("    - [预期行为] %s", reason);
    }
    void check(const char* reason) const {
        kprintfln("    - [状态检查] %s", reason);
    }
    void action(const char* reason) const {
        kprintfln("    - [执行动作] %s", reason);
    }

    void expect(const util::string& reason) const {
        expect(reason.c_str());
    }
    void check(const util::string& reason) const {
        check(reason.c_str());
    }
    void action(const util::string& reason) const {
        action(reason.c_str());
    }

public:
    [[nodiscard]]
    bool run() const noexcept {
        passflag = true;  // reset flag before running
        fail_reasons.clear();
        _run();
        return passflag;
    }

    [[nodiscard]]
    const util::ArrayList<util::string>& fail_reasons_list() const {
        return fail_reasons;
    }
};

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define ttest(cond) test((cond), #cond)
#define __tassert_1(cond)                \
    do {                                 \
        bool flag = test((cond), #cond); \
        if (!flag) {                     \
            return;                      \
        }                                \
    } while (0)
#define __tassert_2(cond, info)         \
    do {                                \
        bool flag = test((cond), info); \
        if (!flag) {                    \
            return;                     \
        }                               \
    } while (0)
#define _tassert(...) \
    SCCAT(__tassert, SCMACRO_ARGNUMBER(__VA_ARGS__))(__VA_ARGS__)
#define tassert(...) SCEXP(_tassert(__VA_ARGS__))
// NOLINTEND(cppcoreguidelines-macro-usage)

class TestCategory {
private:
    const char* _name;
    util::ArrayList<TestCase*> _cases;

public:
    TestCategory(const char* name, util::ArrayList<TestCase*> cases)
        : _name(name), _cases(std::move(cases)) {}
    ~TestCategory() {
        for (auto* testCase : _cases) {
            delete testCase;
        }
    }

    TestCategory(const TestCategory&)            = delete;
    TestCategory& operator=(const TestCategory&) = delete;
    TestCategory(TestCategory&&)                 = delete;
    TestCategory& operator=(TestCategory&&)      = delete;

    [[nodiscard]]
    const char* name() const {
        return _name;
    }

    [[nodiscard]]
    const util::ArrayList<TestCase*>& cases() const {
        return _cases;
    }
};

class TestFramework {
private:
    util::ArrayList<TestCategory*> _categories;

public:
    TestFramework() : _categories() {}
    ~TestFramework() {
        for (auto* category : _categories) {
            delete category;
        }
    }

    TestFramework(const TestFramework&)            = delete;
    TestFramework& operator=(const TestFramework&) = delete;
    TestFramework(TestFramework&&)                 = delete;
    TestFramework& operator=(TestFramework&&)      = delete;

    void add_category(TestCategory* category) {
        _categories.push_back(category);
    }

    [[nodiscard]]
    const util::ArrayList<TestCategory*>& categories() const {
        return _categories;
    }

    void run_all() const;
};

void collect_tests(TestFramework& framework);