/**
 * @file path.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::path 的路径解析和词法操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/format.h>
#include <tay/path.h>

#include <cassert>
#include <cstddef>
#include <functional>
#include <new>

namespace {
    struct allocation_state {
        bool fail            = false;
        size_t allocations   = 0;
        size_t deallocations = 0;
    };

    template <class T>
    struct checked_allocator {
        using value_type      = T;
        using is_always_equal = std::false_type;

        allocation_state* state = nullptr;

        constexpr explicit checked_allocator(allocation_state& value) noexcept : state(&value) {}

        template <class U>
        constexpr checked_allocator(const checked_allocator<U>& other) noexcept
            : state(other.state) {}

        tay::expected<T*, tay::error_code> try_allocate(size_t count) noexcept {
            if (state->fail) {
                return tay::expected<T*, tay::error_code>(tay::unexpect,
                                                          tay::error_code::OUT_OF_MEMORY);
            }
            auto* memory = static_cast<T*>(::operator new(count * sizeof(T), std::nothrow));
            if (memory == nullptr) {
                return tay::expected<T*, tay::error_code>(tay::unexpect,
                                                          tay::error_code::OUT_OF_MEMORY);
            }
            ++state->allocations;
            return memory;
        }

        void deallocate(T* memory, size_t) noexcept {
            ++state->deallocations;
            ::operator delete(memory);
        }

        template <class U>
        struct rebind {
            using other = checked_allocator<U>;
        };

        template <class>
        friend struct checked_allocator;
    };

    template <class T, class U>
    constexpr bool operator==(const checked_allocator<T>& left,
                              const checked_allocator<U>& right) noexcept {
        return left.state == right.state;
    }

    template <class T, class U>
    constexpr bool operator!=(const checked_allocator<T>& left,
                              const checked_allocator<U>& right) noexcept {
        return !(left == right);
    }

    tay::path<> make_path(const char* text) {
        auto created = tay::path<>::try_create(text);
        assert(created);
        return std::move(*created);
    }
}  // namespace

static_assert(std::is_same_v<typename tay::path<>::allocator_type, tay::allocator<char>>);

int main() {
    auto empty = tay::path<>::try_create("");
    assert(empty && empty->empty() && empty->is_relative());
    assert(empty->begin() == empty->end());

    auto root = make_path("/");
    assert(root.is_absolute());
    auto root_it = root.begin();
    assert(root_it != root.end() && *root_it == "/");
    assert(++root_it == root.end());

    auto components        = make_path("///home//user/docs/");
    const char* expected[] = {"/", "home", "user", "docs"};
    size_t index           = 0;
    for (auto component : components) {
        assert(index < 4);
        assert(component == expected[index++]);
    }
    assert(index == 4);

    auto file      = make_path("/home/user/report.tar.gz");
    auto parent    = file.try_parent_path();
    auto filename  = file.try_filename();
    auto stem      = file.try_stem();
    auto extension = file.try_extension();
    assert(parent && *parent == "/home/user");
    assert(filename && *filename == "report.tar.gz");
    assert(stem && *stem == "report.tar");
    assert(extension && *extension == ".gz");

    auto root_parent     = make_path("/foo").try_parent_path();
    auto relative_parent = make_path("foo/bar").try_parent_path();
    auto trailing_parent = make_path("foo/").try_parent_path();
    assert(root_parent && *root_parent == "/");
    assert(relative_parent && *relative_parent == "foo");
    assert(trailing_parent && *trailing_parent == "foo");
    auto trailing_filename = make_path("foo/").try_filename();
    assert(trailing_filename && trailing_filename->empty());

    auto dot_stem            = make_path(".profile").try_stem();
    auto dot_extension       = make_path(".profile").try_extension();
    auto final_dot_extension = make_path("name.").try_extension();
    assert(dot_stem && *dot_stem == ".profile");
    assert(dot_extension && dot_extension->empty());
    assert(final_dot_extension && *final_dot_extension == ".");

    auto normalized          = make_path("///usr//local/../bin/.").try_normalize();
    auto relative_normalized = make_path("./a/b/../c/./d").try_normalize();
    auto above_root          = make_path("/../../etc").try_normalize();
    auto cancelled           = make_path("a/..").try_normalize();
    auto unresolved          = make_path("../../a").try_normalize();
    auto normalized_empty    = make_path("").try_normalize();
    assert(normalized && *normalized == "/usr/bin");
    assert(relative_normalized && *relative_normalized == "a/c/d");
    assert(above_root && *above_root == "/etc");
    assert(cancelled && *cancelled == ".");
    assert(unresolved && *unresolved == "../../a");
    assert(normalized_empty && normalized_empty->empty());

    auto joined   = make_path("home").try_join("user/docs");
    auto replaced = make_path("home/user").try_join("/etc");
    assert(joined && *joined == "home/user/docs");
    assert(replaced && *replaced == "/etc");
    assert(joined->try_concat("/file"));
    assert(*joined == "home/user/docs/file");

    auto target     = make_path("/home/user/docs/report.txt");
    auto relative   = target.try_relative_to(make_path("/home/user"));
    auto unrelated  = target.try_relative_to(make_path("/usr/bin"));
    auto same       = target.try_relative_to(target);
    auto mismatch   = target.try_relative_to(make_path("home/user"));
    auto impossible = make_path("a").try_relative_to(make_path(".."));
    assert(relative && *relative == "docs/report.txt");
    assert(unrelated && *unrelated == "../../home/user/docs/report.txt");
    assert(same && *same == ".");
    assert(!mismatch && mismatch.error() == tay::error_code::INVALID_ARGUMENT);
    assert(impossible && impossible->empty());

    assert(target.starts_with(make_path("/home")));
    assert(!target.starts_with(make_path("/home/us")));
    assert(target.ends_with(make_path("docs/report.txt")));
    assert(!target.ends_with(make_path("port.txt")));
    assert(!target.ends_with(make_path("/docs/report.txt")));
    assert(target < make_path("/z"));
    assert(std::hash<tay::path<>>{}(target) == std::hash<tay::path<>>{}(make_path(target.c_str())));

    auto formatted = tay::format("path={}", target);
    assert(formatted && *formatted == "path=/home/user/docs/report.txt");

    allocation_state state;
    using checked_path = tay::path<checked_allocator<char>>;
    checked_allocator<char> allocator(state);
    auto atomic = checked_path::try_create("123456789012345", allocator);
    assert(atomic);
    state.fail          = true;
    auto concat_failure = atomic->try_concat("x");
    assert(!concat_failure);
    assert(concat_failure.error() == tay::error_code::OUT_OF_MEMORY);
    assert(*atomic == "123456789012345");
    auto join_failure = atomic->try_join("a/path/which/requires/allocation");
    assert(!join_failure);
    assert(join_failure.error() == tay::error_code::OUT_OF_MEMORY);
    assert(*atomic == "123456789012345");

    state.fail = false;
    auto long_path =
        checked_path::try_create("/a/long/path/which/needs/dynamic/storage/../file", allocator);
    assert(long_path);
    state.fail             = true;
    auto normalize_failure = long_path->try_normalize();
    assert(!normalize_failure);
    assert(normalize_failure.error() == tay::error_code::OUT_OF_MEMORY);
    assert(*long_path == "/a/long/path/which/needs/dynamic/storage/../file");
    return 0;
}
