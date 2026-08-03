#include <tay/effect.h>

#include <cassert>
#include <type_traits>
#include <utility>

namespace {
    enum class error { denied, failed };
    struct read_effect {};
    struct write_effect {};

    struct move_only {
        int value;
        explicit move_only(int initial) : value(initial) {}
        move_only(const move_only&)            = delete;
        move_only& operator=(const move_only&) = delete;
        move_only(move_only&&)                 = default;
        move_only& operator=(move_only&&)      = default;
    };

    struct read_request {
        using effect_type = read_effect;
        using value_type  = move_only;
        int value;
    };

    struct write_request {
        using effect_type = write_effect;
        using value_type  = void;
        move_only value;
    };

    struct handler {
        int calls      = 0;
        int sum        = 0;
        bool fail_read = false;

        tay::expected<move_only, error> handle(read_request&& request) noexcept {
            ++calls;
            if (fail_read) {
                return tay::expected<move_only, error>(tay::unexpect, error::denied);
            }
            return move_only(request.value);
        }

        tay::expected<void, error> handle(write_request&& request) noexcept {
            ++calls;
            sum += request.value.value;
            return {};
        }
    };

    struct policy {
        using handled_effects = tay::effect::set<read_effect, write_effect>;
    };

    struct static_env {
        int input  = 4;
        int output = 0;

        int read() {
            return input;
        }

        void write(const int value) {
            output = value;
        }
    };

    struct static_read final {
        template <typename Environment>
        int run(Environment& environment) const {
            return environment.read();
        }
    };

    struct static_write final {
        int value;

        template <typename Environment>
        void run(Environment& environment) const {
            environment.write(value);
        }
    };
}  // namespace

int main() {
    using expected_effects = tay::effect::set<read_effect, write_effect>;
    using duplicate_union  = tay::effect::union_t<tay::effect::set<read_effect>,
                                                  tay::effect::set<read_effect, write_effect>>;
    static_assert(std::is_same_v<duplicate_union, expected_effects>);
    static_assert(tay::effect::subset_v<tay::effect::set<read_effect>, expected_effects>);
    static_assert(!tay::effect::subset_v<expected_effects, tay::effect::set<read_effect>>);

    handler effects;
    auto program =
        tay::effect::then(tay::effect::perform<error>(read_request{21}), [](move_only value) {
            return tay::effect::perform<error>(write_request{std::move(value)});
        });
    static_assert(std::is_same_v<typename decltype(program)::effects_type, expected_effects>);
    static_assert(!std::is_copy_constructible_v<decltype(program)>);
    assert(effects.calls == 0);

    const auto closed = tay::effect::endpoint<policy>::close(std::move(program), effects);
    assert(closed);
    assert(effects.calls == 2);
    assert(effects.sum == 21);

    auto transformed = tay::effect::transform(tay::effect::perform<error>(read_request{4}),
                                              [](move_only value) { return value.value * 3; });
    static_assert(std::is_same_v<typename decltype(transformed)::effects_type,
                                 tay::effect::set<read_effect>>);
    const auto transformed_result =
        tay::effect::endpoint<policy>::close(std::move(transformed), effects);
    assert(transformed_result && *transformed_result == 12);

    effects.fail_read = true;
    auto failed =
        tay::effect::then(tay::effect::perform<error>(read_request{1}), [](move_only value) {
            return tay::effect::perform<error>(write_request{std::move(value)});
        });
    const auto before  = effects.calls;
    const auto failure = tay::effect::endpoint<policy>::close(std::move(failed), effects);
    assert(!failure && failure.error() == error::denied);
    assert(effects.calls == before + 1);

    auto recovered = tay::effect::or_else(
        tay::effect::then(tay::effect::perform<error>(read_request{2}),
                          [](move_only value) {
                              return tay::effect::perform<error>(write_request{std::move(value)});
                          }),
        [](const error) { return tay::effect::perform<error>(write_request{move_only(99)}); });
    const auto recovered_result =
        tay::effect::endpoint<policy>::close(std::move(recovered), effects);
    assert(recovered_result && effects.calls == before + 3 && effects.sum == 120);

    auto explicit_failure = tay::effect::fail<int>(error::failed);
    const auto failed_without_effect =
        tay::effect::endpoint<policy>::close(std::move(explicit_failure), effects);
    assert(!failed_without_effect && failed_without_effect.error() == error::failed);

    auto pure_void =
        tay::effect::then(tay::effect::pure<error>(), [] { return tay::effect::pure<error>(9); });
    const auto pure_result = tay::effect::endpoint<policy>::close(std::move(pure_void), effects);
    assert(pure_result && *pure_result == 9);

    auto static_program = tay::effect::program<static_read>{static_read{}}.flatMap(
        [](int value) { return tay::effect::program<static_write>{static_write{value + 3}}; });
    static_assert(!std::is_polymorphic_v<decltype(static_program)>);
    static_env static_environment;
    static_program.run(static_environment);
    assert(static_environment.output == 7);
}
