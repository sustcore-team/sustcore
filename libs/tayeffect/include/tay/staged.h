/**
 * @file staged.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Staged Operations
 * @version 0.1.0-dev.1
 * @date 2026-08-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/effect.h>
#include <tay/expected.h>

#include <type_traits>
#include <utility>

namespace tay::staged {
    template <typename Derived>
    class operation {
        [[nodiscard]] constexpr const Derived& derived() const noexcept {
            return static_cast<const Derived&>(*this);
        }

        template <typename Receipt, typename Result, typename Plan, typename Error>
        [[nodiscard]] constexpr auto commit(const Plan& plan,
                                            tay::expected<Receipt, Error>&& performed) const {
            using error_type = Error;
            static_assert(std::is_same_v<error_type, typename Derived::error_type>);
            if (!performed) {
                return tay::expected<Result, error_type>(tay::unexpect,
                                                         std::move(performed).error());
            }

            if constexpr (std::is_void_v<Receipt>) {
                static_assert(noexcept(derived().commit(plan)), "staged commit must be noexcept");
                if constexpr (std::is_void_v<Result>) {
                    static_assert(std::is_void_v<decltype(derived().commit(plan))>,
                                  "staged commit must return result_type");
                    derived().commit(plan);
                    return tay::expected<Result, error_type>{};
                } else {
                    static_assert(std::is_same_v<decltype(derived().commit(plan)), Result>,
                                  "staged commit must return result_type exactly");
                    return tay::expected<Result, error_type>(derived().commit(plan));
                }
            } else {
                static_assert(noexcept(derived().commit(plan, static_cast<Receipt&&>(*performed))),
                              "staged commit must be noexcept");
                if constexpr (std::is_void_v<Result>) {
                    static_assert(std::is_void_v<decltype(derived().commit(
                                      plan, static_cast<Receipt&&>(*performed)))>,
                                  "staged commit must return result_type");
                    derived().commit(plan, static_cast<Receipt&&>(*performed));
                    return tay::expected<Result, error_type>{};
                } else {
                    static_assert(std::is_same_v<decltype(derived().commit(
                                                     plan, static_cast<Receipt&&>(*performed))),
                                                 Result>,
                                  "staged commit must return result_type exactly");
                    return tay::expected<Result, error_type>(
                        derived().commit(plan, static_cast<Receipt&&>(*performed)));
                }
            }
        }

    public:
        template <typename... Arguments>
        [[nodiscard]] constexpr auto execute(Arguments&&... arguments) const {
            using plan_type    = typename Derived::plan_type;
            using receipt_type = typename Derived::receipt_type;
            using result_type  = typename Derived::result_type;
            using error_type   = typename Derived::error_type;
            using planned_type = decltype(derived().plan(std::forward<Arguments>(arguments)...));
            static_assert(std::is_same_v<planned_type, tay::expected<plan_type, error_type>>,
                          "staged plan must return tay::expected<plan_type, error_type>");

            auto planned = derived().plan(std::forward<Arguments>(arguments)...);
            if (!planned) {
                return tay::expected<result_type, error_type>(tay::unexpect,
                                                              std::move(planned).error());
            }

            const plan_type& plan = *planned;
            auto performed        = derived().perform(plan);
            using performed_type  = std::remove_cvref_t<decltype(performed)>;

            if constexpr (effect::is_effectful_v<performed_type>) {
                static_assert(std::is_same_v<typename performed_type::value_type, receipt_type>,
                              "effectful perform must produce receipt_type");
                static_assert(std::is_same_v<typename performed_type::error_type, error_type>,
                              "effectful perform must preserve error_type");
                using endpoint_type = typename Derived::effect_endpoint;
                static_assert(noexcept(derived().effect_handler(plan)),
                              "staged effect_handler must be noexcept");
                auto closed =
                    endpoint_type::close(std::move(performed), derived().effect_handler(plan));
                return commit<receipt_type, result_type>(plan, std::move(closed));
            } else if constexpr (effect::is_program_v<performed_type>) {
                static_assert(noexcept(derived().effect_environment(plan)),
                              "staged effect_environment must be noexcept");
                auto&& environment   = derived().effect_environment(plan);
                using execution_type = decltype(performed.run(environment));
                static_assert(
                    std::is_same_v<execution_type, tay::expected<receipt_type, error_type>>,
                    "effect program must return expected<receipt_type, error_type>");
                auto executed = performed.run(environment);
                return commit<receipt_type, result_type>(plan, std::move(executed));
            } else {
                static_assert(
                    std::is_same_v<performed_type, tay::expected<receipt_type, error_type>>,
                    "perform must return expected, effectful, or an effect program");
                return commit<receipt_type, result_type>(plan, std::move(performed));
            }
        }
    };
}  // namespace tay::staged
