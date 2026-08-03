#include <tay/effect.h>

#include <charconv>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct Unit final {};

// ---------- Compile-time effect nodes ----------
struct ReadOp final {
    template <typename Environment>
    auto run(Environment& environment) const {
        return environment.readLine();
    }
};

struct WriteOp final {
    std::string message;

    template <typename Environment>
    Unit run(Environment& environment) const {
        environment.printLine(message);
        return {};
    }
};

template <typename Operation>
using Effect = tay::effect::program<Operation>;

// ---------- Effect factories ----------
[[nodiscard]] auto readLine() {
    return Effect<ReadOp>{};
}

[[nodiscard]] auto printLine(std::string message) {
    return Effect<WriteOp>{std::move(message)};
}

// ---------- Pure business logic ----------
[[nodiscard]] std::optional<int> parseInt(const std::string& text) noexcept {
    int value         = 0;
    const auto* end   = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] constexpr int plus(const int left, const int right) noexcept {
    return left + right;
}

[[nodiscard]] std::optional<double> divide(const int numerator, const int denominator) noexcept {
    if (denominator == 0) {
        return std::nullopt;
    }
    return static_cast<double>(numerator) / denominator;
}

[[nodiscard]] std::optional<double> compute(const std::string& a, const std::string& b,
                                            const std::string& c) noexcept {
    const auto parsed_a = parseInt(a);
    const auto parsed_b = parseInt(b);
    const auto parsed_c = parseInt(c);
    if (!parsed_a || !parsed_b || !parsed_c) {
        return std::nullopt;
    }
    return divide(plus(*parsed_a, *parsed_b), *parsed_c);
}

// ---------- Runtime-injected environments ----------
struct ConsoleEnv final {
    std::string readLine() {
        std::string line;
        std::getline(std::cin, line);
        return line;
    }

    void printLine(const std::string& line) {
        std::cout << line << '\n';
    }
};

struct MockEnv final {
    std::vector<std::string> inputs{"10", "20", "5"};
    std::vector<std::string> outputs;
    std::size_t index = 0;

    std::string readLine() {
        return inputs[index++];
    }

    void printLine(const std::string& line) {
        outputs.push_back(line);
    }
};

int main() {
    // Every flatMap creates another concrete template node. No effect runs here.
    auto program = readLine().flatMap([](const std::string& a) {
        return readLine().flatMap([a](const std::string& b) {
            return readLine().flatMap([a, b](const std::string& c) {
                const auto result = compute(a, b, c);
                if (result) {
                    return printLine("Result: " + std::to_string(*result));
                }
                return printLine("Error: invalid input or division by zero");
            });
        });
    });

    using program_type = decltype(program);
    static_assert(!std::is_same_v<program_type, Effect<ReadOp>>);
    static_assert(!std::is_polymorphic_v<typename program_type::operation_type>);

    ConsoleEnv environment;
    program.run(environment);
}
