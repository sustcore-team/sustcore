/**
 * @file refc.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::refc 和 tay::refc_ptr 的引用计数语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/refcount.h>

#include <cassert>
#include <utility>

namespace {
    struct counted : tay::refc<counted> {
        int deaths = 0;

        void on_death() {
            ++deaths;
        }
    };
}  // namespace

int main() {
    counted object;
    {
        tay::refc_ptr<counted> first{&object};
        assert(object.ref_count() == 1);
        tay::refc_ptr<counted> second{first};
        assert(object.ref_count() == 2);
        tay::refc_ptr<counted> moved{std::move(second)};
        assert(object.ref_count() == 2 && second.get() == nullptr);
    }
    assert(object.deaths == 1);

    return 0;
}
