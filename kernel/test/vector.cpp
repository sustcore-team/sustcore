/**
 * @file vector.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief vector 测试
 * @version alpha-1.0.0
 * @date 2026-05-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/vector.h>

#include <nt/errors.h>
#include <vector>

namespace test::vector {
    constexpr bool constexpr_vector_smoke() {
        std::vector<int> v;
        if (!v.empty() || v.size() != 0) {
            return false;
        }
        if (!v.push_back_nt(1) || !v.emplace_back_nt(2)) {
            return false;
        }
        if (!v.insert_nt(v.begin() + 1, 9)) {
            return false;
        }
        if (v.size() != 3 || v[0] != 1 || v[1] != 9 || v[2] != 2) {
            return false;
        }
        if (!v.erase_nt(v.begin() + 1)) {
            return false;
        }
        if (!v.resize_nt(4, 7)) {
            return false;
        }
        if (v.size() != 4 || v[0] != 1 || v[1] != 2 || v.back() != 7) {
            return false;
        }
        v.pop_back();
        v.clear();
        return v.empty();
    }

#if __cplusplus > 202302L
    static_assert(constexpr_vector_smoke());
#endif

    class CaseBasicAccess : public TestCase {
    public:
        CaseBasicAccess() : TestCase("基础访问") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::vector<int> v;
            ttest(v.empty());
            ttest(v.size() == 0);
            ttest(v.begin() == v.end());

            ttest(v.push_back_nt(1).has_value());
            ttest(v.push_back_nt(2).has_value());
            ttest(v.emplace_back_nt(3).has_value());

            ttest(!v.empty());
            ttest(v.size() == 3);
            ttest(v.data() == v.begin());
            ttest(v[0] == 1);
            ttest(v.front() == 1);
            ttest(v.back() == 3);

            auto ok = v.at_nt(1);
            ttest(ok.has_value());
            ttest(*ok.value() == 2);

            auto fail = v.at_nt(99);
            ttest(!fail);
            ttest(fail.error() == std::error_type::OUT_OF_RANGE);
        }
    };

    class CaseCapacityResize : public TestCase {
    public:
        CaseCapacityResize() : TestCase("容量与 resize") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::vector<int> v;
            ttest(v.reserve_nt(8).has_value());
            ttest(v.capacity() >= 8);
            ttest(v.size() == 0);

            ttest(v.resize_nt(3, 7).has_value());
            ttest(v.size() == 3);
            ttest(v[0] == 7 && v[1] == 7 && v[2] == 7);

            ttest(v.resize_nt(1).has_value());
            ttest(v.size() == 1);
            ttest(v[0] == 7);

            v.clear();
            ttest(v.empty());
            ttest(v.capacity() >= 8);

            ttest(v.shrink_to_fit_nt().has_value());
            ttest(v.capacity() == 0);
        }
    };

    class CaseInsertErase : public TestCase {
    public:
        CaseInsertErase() : TestCase("插入与删除") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::vector<int> v;
            ttest(v.push_back_nt(1).has_value());
            ttest(v.push_back_nt(4).has_value());

            auto inserted = v.insert_nt(v.begin() + 1, 2);
            ttest(inserted.has_value());
            ttest(*inserted.value() == 2);

            int more[] = {3, 5};
            auto range_inserted = v.insert_nt(v.begin() + 2, more, more + 2);
            ttest(range_inserted.has_value());
            ttest(v.size() == 5);
            ttest(v[0] == 1 && v[1] == 2 && v[2] == 3);
            ttest(v[3] == 5 && v[4] == 4);

            auto erased = v.erase_nt(v.begin() + 3);
            ttest(erased.has_value());
            ttest(*erased.value() == 4);
            ttest(v.size() == 4);

            auto range_erased = v.erase_nt(v.begin() + 1, v.begin() + 3);
            ttest(range_erased.has_value());
            ttest(*range_erased.value() == 4);
            ttest(v.size() == 2);
            ttest(v[0] == 1 && v[1] == 4);
        }
    };

    class CaseCopyMoveAssign : public TestCase {
    public:
        CaseCopyMoveAssign() : TestCase("拷贝移动与 assign") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::vector<int> v;
            ttest(v.assign_nt({1, 2, 3}).has_value());

            std::vector<int> copy(v);
            ttest(copy == v);
            copy[1] = 9;
            ttest(copy != v);
            ttest(v[1] == 2);

            std::vector<int> moved(std::move(copy));
            ttest(moved.size() == 3);
            ttest(moved[0] == 1 && moved[1] == 9 && moved[2] == 3);
            ttest(copy.empty());

            std::vector<int> other;
            ttest(other.assign_nt(2, 6).has_value());
            v.swap(other);
            ttest(v.size() == 2);
            ttest(v[0] == 6 && v[1] == 6);
            ttest(other.size() == 3);
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
        bool operator==(const Tracked& other) const {
            return value == other.value;
        }
    };

    int Tracked::live_count = 0;

    class CaseObjectLifetime : public TestCase {
    public:
        CaseObjectLifetime() : TestCase("对象生命周期") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            Tracked::live_count = 0;
            {
                std::vector<Tracked> v;
                ttest(v.emplace_back_nt(1).has_value());
                ttest(v.emplace_back_nt(2).has_value());
                ttest(v.insert_nt(v.begin() + 1, Tracked(3)).has_value());
                ttest(v.size() == 3);
                ttest(Tracked::live_count == 3);
                ttest(v[0].value == 1);
                ttest(v[1].value == 3);
                ttest(v[2].value == 2);

                ttest(v.erase_nt(v.begin()).has_value());
                ttest(v.size() == 2);
                ttest(Tracked::live_count == 2);
            }
            ttest(Tracked::live_count == 0);
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseBasicAccess());
        cases.push_back(new CaseCapacityResize());
        cases.push_back(new CaseInsertErase());
        cases.push_back(new CaseCopyMoveAssign());
        cases.push_back(new CaseObjectLifetime());

        framework.add_category(new TestCategory("vector", std::move(cases)));
    }
}  // namespace test::vector
