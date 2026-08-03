/**
 * @file effect.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Static Effect System
 * @version 0.1.0-dev.1
 * @date 2026-08-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/expected.h>

#include <functional>
#include <type_traits>
#include <utility>

namespace tay::effect {
    template <typename... Tags>
    struct set final {};

    template <typename Tag, typename Set>
    inline constexpr bool contains_v = false;

    template <typename Tag, typename... Tags>
    inline constexpr bool contains_v<Tag, set<Tags...>> = (std::is_same_v<Tag, Tags> || ...);

    template <typename Actual, typename Allowed>
    inline constexpr bool subset_v = false;

    template <typename Allowed, typename... Actual>
    inline constexpr bool subset_v<set<Actual...>, Allowed> = (contains_v<Actual, Allowed> && ...);

    namespace detail {
        template <typename Set, typename Tag>
        struct append_unique;

        template <typename... Tags, typename Tag>
        struct append_unique<set<Tags...>, Tag> {
            using type =
                std::conditional_t<contains_v<Tag, set<Tags...>>, set<Tags...>, set<Tags..., Tag>>;
        };

        template <typename Left, typename Right>
        struct set_union;

        template <typename Left>
        struct set_union<Left, set<>> {
            using type = Left;
        };

        template <typename Left, typename Head, typename... Tail>
        struct set_union<Left, set<Head, Tail...>> {
            using next = typename append_unique<Left, Head>::type;
            using type = typename set_union<next, set<Tail...>>::type;
        };
    }  // namespace detail

    template <typename Left, typename Right>
    using union_t = typename detail::set_union<Left, Right>::type;

    template <typename PerformEffects, typename CommitEffects>
    struct manifest final {
        using perform_effects = PerformEffects;
        using commit_effects  = CommitEffects;
    };

    template <typename Value, typename Error, typename Effects, typename Program>
    class effectful;

    template <typename T>
    struct is_effectful : std::false_type {};

    template <typename Value, typename Error, typename Effects, typename Program>
    struct is_effectful<effectful<Value, Error, Effects, Program>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_effectful_v = is_effectful<std::remove_cvref_t<T>>::value;

    namespace detail {
        template <typename Value, typename Error>
        struct pure_node final {
            Value value;
        };

        template <typename Error>
        struct pure_node<void, Error> final {};

        template <typename Value, typename Error>
        struct fail_node final {
            Error error;
        };

        template <typename Request, typename Error>
        struct request_node final {
            Request request;
        };

        template <typename Value, typename Error, typename First, typename Continuation>
        struct bind_node final {
            First first;
            Continuation continuation;
        };

        template <typename Value, typename Error, typename First, typename Function>
        struct transform_node final {
            First first;
            Function function;
        };

        template <typename Value, typename Error, typename First, typename Recovery>
        struct recover_node final {
            First first;
            Recovery recovery;
        };

        template <typename Value, typename Function, bool = std::is_void_v<Value>>
        struct continuation_result;

        template <typename Value, typename Function>
        struct continuation_result<Value, Function, false> {
            using type = std::remove_cvref_t<std::invoke_result_t<Function&&, Value&&>>;
        };

        template <typename Value, typename Function>
        struct continuation_result<Value, Function, true> {
            using type = std::remove_cvref_t<std::invoke_result_t<Function&&>>;
        };

        template <typename Value, typename Function>
        using continuation_result_t = typename continuation_result<Value, Function>::type;

        template <typename Value, typename Function, bool = std::is_void_v<Value>>
        struct transform_result;

        template <typename Value, typename Function>
        struct transform_result<Value, Function, false> {
            using type = std::invoke_result_t<Function&&, Value&&>;
        };

        template <typename Value, typename Function>
        struct transform_result<Value, Function, true> {
            using type = std::invoke_result_t<Function&&>;
        };

        template <typename Value, typename Function>
        using transform_result_t = typename transform_result<Value, Function>::type;

        template <typename Error, typename Function>
        using recovery_result_t = std::remove_cvref_t<std::invoke_result_t<Function&&, Error&&>>;

        template <typename Value, typename Error, typename Effects, typename Program>
        struct effectful_access;

        template <typename Program>
        struct runner;

        template <typename Program, typename Handler>
        constexpr auto run(Program&& program, Handler& handler) {
            using program_type = std::remove_cvref_t<Program>;
            return runner<program_type>::run(std::forward<Program>(program), handler);
        }
    }  // namespace detail

    template <typename Value, typename Error, typename Effects, typename Program>
    class effectful final {
        Program program_;

        friend struct detail::effectful_access<Value, Error, Effects, Program>;

    public:
        using value_type   = Value;
        using error_type   = Error;
        using effects_type = Effects;
        using program_type = Program;

        constexpr explicit effectful(Program&& program) noexcept(
            std::is_nothrow_move_constructible_v<Program>)
            : program_(std::move(program)) {}

        effectful(const effectful&)                 = delete;
        effectful& operator=(const effectful&)      = delete;
        constexpr effectful(effectful&&)            = default;
        constexpr effectful& operator=(effectful&&) = default;
        constexpr ~effectful()                      = default;
    };

    namespace detail {
        template <typename Value, typename Error, typename Effects, typename Program>
        struct effectful_access final {
            static constexpr Program&& take(
                effectful<Value, Error, Effects, Program>&& value) noexcept {
                return std::move(value.program_);
            }
        };

        template <typename T>
        struct effectful_traits;

        template <typename Value, typename Error, typename Effects, typename Program>
        struct effectful_traits<effectful<Value, Error, Effects, Program>> {
            using value_type   = Value;
            using error_type   = Error;
            using effects_type = Effects;
            using program_type = Program;
        };

        template <typename Value, typename Error, typename Effects, typename Program>
        constexpr Program&& take_program(
            effectful<Value, Error, Effects, Program>&& value) noexcept {
            return effectful_access<Value, Error, Effects, Program>::take(std::move(value));
        }

        template <typename Value, typename Error>
        struct runner<pure_node<Value, Error>> final {
            template <typename Handler>
            static constexpr tay::expected<Value, Error> run(pure_node<Value, Error>&& node,
                                                             Handler&) {
                return tay::expected<Value, Error>(std::move(node.value));
            }
        };

        template <typename Error>
        struct runner<pure_node<void, Error>> final {
            template <typename Handler>
            static constexpr tay::expected<void, Error> run(pure_node<void, Error>&&, Handler&) {
                return {};
            }
        };

        template <typename Value, typename Error>
        struct runner<fail_node<Value, Error>> final {
            template <typename Handler>
            static constexpr tay::expected<Value, Error> run(fail_node<Value, Error>&& node,
                                                             Handler&) {
                return tay::expected<Value, Error>(tay::unexpect, std::move(node.error));
            }
        };

        template <typename Request, typename Error>
        struct runner<request_node<Request, Error>> final {
            template <typename Handler>
            static constexpr auto run(request_node<Request, Error>&& node, Handler& handler) {
                using value_type  = typename Request::value_type;
                using result_type = decltype(handler.handle(static_cast<Request&&>(node.request)));
                static_assert(noexcept(handler.handle(static_cast<Request&&>(node.request))),
                              "effect handler handle(Request&&) must be noexcept");
                static_assert(
                    std::is_same_v<result_type, tay::expected<value_type, Error>>,
                    "effect handler must return tay::expected<Request::value_type, Error>");
                return handler.handle(std::move(node.request));
            }
        };

        template <typename Value, typename Error, typename First, typename Continuation>
        struct runner<bind_node<Value, Error, First, Continuation>> final {
            template <typename Handler>
            static constexpr tay::expected<Value, Error> run(
                bind_node<Value, Error, First, Continuation>&& node, Handler& handler) {
                auto first = detail::run(std::move(node.first), handler);
                if (!first) {
                    return tay::expected<Value, Error>(tay::unexpect, std::move(first).error());
                }

                if constexpr (std::is_void_v<typename decltype(first)::value_type>) {
                    auto next = std::invoke(std::move(node.continuation));
                    return detail::run(take_program(std::move(next)), handler);
                } else {
                    auto next = std::invoke(std::move(node.continuation), *std::move(first));
                    return detail::run(take_program(std::move(next)), handler);
                }
            }
        };

        template <typename Value, typename Error, typename First, typename Function>
        struct runner<transform_node<Value, Error, First, Function>> final {
            template <typename Handler>
            static constexpr tay::expected<Value, Error> run(
                transform_node<Value, Error, First, Function>&& node, Handler& handler) {
                auto first = detail::run(std::move(node.first), handler);
                if (!first) {
                    return tay::expected<Value, Error>(tay::unexpect, std::move(first).error());
                }

                if constexpr (std::is_void_v<typename decltype(first)::value_type>) {
                    if constexpr (std::is_void_v<Value>) {
                        std::invoke(std::move(node.function));
                        return {};
                    } else {
                        return tay::expected<Value, Error>(std::invoke(std::move(node.function)));
                    }
                } else {
                    if constexpr (std::is_void_v<Value>) {
                        std::invoke(std::move(node.function), *std::move(first));
                        return {};
                    } else {
                        return tay::expected<Value, Error>(
                            std::invoke(std::move(node.function), *std::move(first)));
                    }
                }
            }
        };

        template <typename Value, typename Error, typename First, typename Recovery>
        struct runner<recover_node<Value, Error, First, Recovery>> final {
            template <typename Handler>
            static constexpr tay::expected<Value, Error> run(
                recover_node<Value, Error, First, Recovery>&& node, Handler& handler) {
                auto first = detail::run(std::move(node.first), handler);
                if (first) {
                    if constexpr (std::is_void_v<Value>) {
                        return {};
                    } else {
                        return tay::expected<Value, Error>(std::move(first).value());
                    }
                }

                auto next = std::invoke(std::move(node.recovery), std::move(first).error());
                return detail::run(take_program(std::move(next)), handler);
            }
        };

    }  // namespace detail

    template <typename Error, typename Request>
    [[nodiscard]] constexpr auto perform(Request&& request) {
        using request_type = std::remove_cvref_t<Request>;
        static_assert(
            requires {
                typename request_type::effect_type;
                typename request_type::value_type;
            }, "effect request must define effect_type and value_type");
        using node_type    = detail::request_node<request_type, Error>;
        using effects_type = set<typename request_type::effect_type>;
        return effectful<typename request_type::value_type, Error, effects_type, node_type>(
            node_type{std::forward<Request>(request)});
    }

    template <typename Error, typename Value>
        requires(!std::is_void_v<std::remove_cvref_t<Value>>)
    [[nodiscard]] constexpr auto pure(Value&& value) {
        using value_type = std::remove_cvref_t<Value>;
        using node_type  = detail::pure_node<value_type, Error>;
        return effectful<value_type, Error, set<>, node_type>(
            node_type{std::forward<Value>(value)});
    }

    template <typename Error>
    [[nodiscard]] constexpr auto pure() {
        using node_type = detail::pure_node<void, Error>;
        return effectful<void, Error, set<>, node_type>(node_type{});
    }

    template <typename Value, typename Error>
    [[nodiscard]] constexpr auto fail(Error&& error) {
        using error_type = std::remove_cvref_t<Error>;
        using node_type  = detail::fail_node<Value, error_type>;
        return effectful<Value, error_type, set<>, node_type>(
            node_type{std::forward<Error>(error)});
    }

    template <typename Value, typename Error, typename Effects, typename Program,
              typename Continuation>
    [[nodiscard]] constexpr auto then(effectful<Value, Error, Effects, Program>&& first,
                                      Continuation&& continuation) {
        using continuation_type = std::remove_cvref_t<Continuation>;
        using next_type         = detail::continuation_result_t<Value, continuation_type>;
        static_assert(is_effectful_v<next_type>,
                      "effect continuation must return tay::effect::effectful");
        using next_traits = detail::effectful_traits<next_type>;
        static_assert(std::is_same_v<Error, typename next_traits::error_type>,
                      "effect continuation must preserve the error type");
        using effects_type = union_t<Effects, typename next_traits::effects_type>;
        using node_type =
            detail::bind_node<typename next_traits::value_type, Error, Program, continuation_type>;
        return effectful<typename next_traits::value_type, Error, effects_type, node_type>(
            node_type{detail::take_program(std::move(first)),
                      std::forward<Continuation>(continuation)});
    }

    template <typename Value, typename Error, typename Effects, typename Program, typename Function>
    [[nodiscard]] constexpr auto transform(effectful<Value, Error, Effects, Program>&& first,
                                           Function&& function) {
        using function_type  = std::remove_cvref_t<Function>;
        using raw_value_type = detail::transform_result_t<Value, function_type>;
        static_assert(!std::is_reference_v<raw_value_type>,
                      "effect transform must return an object or void");
        using result_type = std::remove_cv_t<raw_value_type>;
        using node_type   = detail::transform_node<result_type, Error, Program, function_type>;
        return effectful<result_type, Error, Effects, node_type>(
            node_type{detail::take_program(std::move(first)), std::forward<Function>(function)});
    }

    template <typename Value, typename Error, typename Effects, typename Program, typename Recovery>
    [[nodiscard]] constexpr auto or_else(effectful<Value, Error, Effects, Program>&& first,
                                         Recovery&& recovery) {
        using recovery_type = std::remove_cvref_t<Recovery>;
        using next_type     = detail::recovery_result_t<Error, recovery_type>;
        static_assert(is_effectful_v<next_type>,
                      "effect recovery must return tay::effect::effectful");
        using next_traits = detail::effectful_traits<next_type>;
        static_assert(std::is_same_v<Value, typename next_traits::value_type>,
                      "effect recovery must preserve the value type");
        static_assert(std::is_same_v<Error, typename next_traits::error_type>,
                      "effect recovery must preserve the error type");
        using effects_type = union_t<Effects, typename next_traits::effects_type>;
        using node_type    = detail::recover_node<Value, Error, Program, recovery_type>;
        return effectful<Value, Error, effects_type, node_type>(
            node_type{detail::take_program(std::move(first)), std::forward<Recovery>(recovery)});
    }

    template <typename Policy>
    struct endpoint final {
        template <typename Value, typename Error, typename Effects, typename Program,
                  typename Handler>
        [[nodiscard]] static constexpr tay::expected<Value, Error> close(
            effectful<Value, Error, Effects, Program>&& program, Handler&& handler) {
            static_assert(subset_v<Effects, typename Policy::handled_effects>,
                          "effect program contains effects not handled by this endpoint");
            auto&& handler_ref = handler;
            return detail::run(detail::take_program(std::move(program)), handler_ref);
        }

        template <typename Value, typename Error, typename Effects, typename Program,
                  typename Handler>
        static void close(effectful<Value, Error, Effects, Program>&, Handler&&) = delete;
    };

    template <typename Previous, typename Function>
    struct flat_map_op final {
        Previous previous;
        Function function;

        template <typename Environment>
        constexpr decltype(auto) run(Environment& environment) const {
            using previous_result = decltype(previous.run(environment));
            if constexpr (std::is_void_v<previous_result>) {
                previous.run(environment);
                auto next = std::invoke(function);
                return next.run(environment);
            } else {
                decltype(auto) result = previous.run(environment);
                auto next             = std::invoke(function, result);
                return next.run(environment);
            }
        }
    };

    template <typename Operation>
    struct program final {
        using operation_type = Operation;

        Operation operation;

        template <typename... Arguments>
            requires std::is_constructible_v<Operation, Arguments&&...>
        constexpr explicit program(Arguments&&... arguments) noexcept(
            std::is_nothrow_constructible_v<Operation, Arguments&&...>)
            : operation(std::forward<Arguments>(arguments)...) {}

        template <typename Function>
        [[nodiscard]] constexpr auto flatMap(Function&& function) && {
            using function_type  = std::remove_cvref_t<Function>;
            using next_operation = flat_map_op<Operation, function_type>;
            return program<next_operation>{
                next_operation{std::move(operation), std::forward<Function>(function)}};
        }

        template <typename Environment>
        constexpr decltype(auto) run(Environment& environment) const {
            return operation.run(environment);
        }
    };

    template <typename T>
    struct is_program : std::false_type {};

    template <typename Operation>
    struct is_program<program<Operation>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_program_v = is_program<std::remove_cvref_t<T>>::value;

    template <typename Operation>
    [[nodiscard]] constexpr auto make_program(Operation&& operation) {
        using operation_type = std::remove_cvref_t<Operation>;
        return program<operation_type>{std::forward<Operation>(operation)};
    }
}  // namespace tay::effect
