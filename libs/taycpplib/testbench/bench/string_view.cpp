#include <tay/string_view.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>

namespace {
    constexpr std::uint64_t iterations = 2000000;
    constexpr std::uint64_t long_search_iterations = 200000;

    constexpr std::size_t repetitive_text_length  = 512;
    constexpr std::size_t random_text_length      = 512;
    constexpr std::size_t random_pattern_length   = 32;
    constexpr std::size_t random_pattern_count    = 64;

    std::uint64_t next_random(std::uint64_t& state) noexcept {
        state ^= state << 7;
        state ^= state >> 9;
        state ^= state << 8;
        return state;
    }

    template <typename Operation>
    void benchmark(const char* name, std::uint64_t seed, Operation operation,
                   std::uint64_t operation_iterations = iterations) {
        volatile std::uint64_t checksum = 0;
        const auto start                = std::chrono::steady_clock::now();
        for (std::uint64_t index = 0; index < operation_iterations; ++index) {
            checksum = checksum + operation(index + seed);
        }
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start)
                .count();
        std::cout << std::format(
            "{:28} iterations={:7d} elapsed={:10d} ns ns/op={:8.3f} checksum={}\n",
            name, (unsigned long long)operation_iterations,
            (long long)elapsed,
            (double)elapsed / (double)operation_iterations,
            (unsigned long long)checksum);
    }
}  // namespace

int main() {
    constexpr tay::string_view text =
        "sustcore string_view benchmark: alpha beta gamma delta";
    constexpr tay::string_view words[] = {
        "alpha",
        "beta",
        "gamma",
        "delta",
    };
    constexpr char characters[] = {'a', 'e', 'm', 'z'};

    const auto seed = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    char repetitive_text_storage[repetitive_text_length];
    for (std::size_t index = 0; index + 1 < repetitive_text_length; ++index) {
        repetitive_text_storage[index] = 'A';
    }
    repetitive_text_storage[repetitive_text_length - 1] = 'B';
    const tay::string_view repetitive_text(repetitive_text_storage,
                                           repetitive_text_length);
    constexpr tay::string_view repetitive_patterns[] = {
        "AAAAAB",
        "AAAAAC",
        "AAAABB",
    };

    char random_text_storage[random_text_length];
    char random_pattern_storage[random_pattern_count][random_pattern_length];
    tay::string_view random_patterns[random_pattern_count];
    std::uint64_t random_state = seed | 1;
    for (char& character : random_text_storage) {
        character = static_cast<char>('a' + next_random(random_state) % 26);
    }
    for (std::size_t pattern_index = 0;
         pattern_index < random_pattern_count; ++pattern_index) {
        if ((pattern_index & 1) == 0) {
            const std::size_t start =
                next_random(random_state) %
                (random_text_length - random_pattern_length + 1);
            for (std::size_t index = 0; index < random_pattern_length;
                 ++index) {
                random_pattern_storage[pattern_index][index] =
                    random_text_storage[start + index];
            }
        } else {
            for (std::size_t index = 0; index < random_pattern_length;
                 ++index) {
                random_pattern_storage[pattern_index][index] =
                    static_cast<char>('a' + next_random(random_state) % 26);
            }
            random_pattern_storage[pattern_index][random_pattern_length / 2] =
                '\0';
        }
        random_patterns[pattern_index] = tay::string_view(
            random_pattern_storage[pattern_index], random_pattern_length);
    }
    const tay::string_view random_text(random_text_storage,
                                       random_text_length);

    std::printf(
        "taycpplib string_view benchmark (default=%llu, long-search=%llu "
        "iterations)\n",
        (unsigned long long)iterations,
        (unsigned long long)long_search_iterations);
    std::printf(
        "search datasets: repetitive=%zu bytes, random=%zu bytes/%zu-byte "
        "patterns (%zu total, 50%% hits)\n",
        repetitive_text_length, random_text_length, random_pattern_length,
        random_pattern_count);

    benchmark("baseline", seed,
              [](std::uint64_t token) { return token & 0xff; });

    benchmark("construct(c-string)", seed, [&](std::uint64_t token) {
        tay::string_view value = words[token & 3].data();
        return static_cast<std::uint64_t>(value.size());
    });

    benchmark("construct(pointer, size)", seed, [&](std::uint64_t token) {
        const auto length = words[token & 3].size();
        tay::string_view value(words[token & 3].data(), length);
        return static_cast<std::uint64_t>(value.size());
    });

    benchmark("begin/end", seed, [&](std::uint64_t token) {
        const auto position = token % text.size();
        return static_cast<std::uint64_t>(
                   static_cast<unsigned char>(*(text.begin() + position))) +
               static_cast<std::uint64_t>(text.end() - text.begin());
    });

    benchmark("operator[]", seed, [&](std::uint64_t token) {
        const auto position = token % text.size();
        return static_cast<unsigned char>(text[position]);
    });

    benchmark("at(valid)", seed, [&](std::uint64_t token) {
        const auto position = token % text.size();
        auto result         = text.at(position);
        return result ? static_cast<std::uint64_t>(
                            static_cast<unsigned char>(*result))
                      : 0;
    });

    benchmark("at(out-of-range)", seed, [&](std::uint64_t token) {
        auto result = text.at(text.size() + 1 + (token & 3));
        return result ? 1
                      : static_cast<std::uint64_t>(
                            result.error() == tay::error_code::OUT_OF_RANGE);
    });

    benchmark("front", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(
                   static_cast<unsigned char>(text.front())) +
               (token & 1);
    });

    benchmark("back", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(
                   static_cast<unsigned char>(text.back())) +
               (token & 1);
    });

    benchmark("data", seed, [&](std::uint64_t token) {
        return static_cast<unsigned char>(text.data()[token % text.size()]);
    });

    benchmark("size/length", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(text.size() + text.length()) +
               (token & 1);
    });

    benchmark("empty", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(text.empty()) + (token & 1);
    });

    benchmark("remove_prefix", seed, [&](std::uint64_t token) {
        tay::string_view value = text;
        auto result = value.remove_prefix(token % (value.size() + 1));
        return result ? static_cast<std::uint64_t>(value.size()) : 0;
    });

    benchmark("remove_suffix", seed, [&](std::uint64_t token) {
        tay::string_view value = text;
        auto result = value.remove_suffix(token % (value.size() + 1));
        return result ? static_cast<std::uint64_t>(value.size()) : 0;
    });

    benchmark("swap", seed, [&](std::uint64_t token) {
        tay::string_view left  = text;
        tay::string_view right = words[token & 3];
        left.swap(right);
        return static_cast<std::uint64_t>(left.size() + right.size());
    });

    benchmark("copy", seed, [&](std::uint64_t token) {
        char buffer[16]{};
        const auto position = token % (text.size() - 8);
        auto result         = text.copy(buffer, sizeof(buffer), position);
        return result ? static_cast<std::uint64_t>(*result) +
                            static_cast<unsigned char>(buffer[token & 7])
                      : 0;
    });

    benchmark("substr", seed, [&](std::uint64_t token) {
        const auto position = token % (text.size() + 1);
        auto result         = text.substr(position, 8);
        return result ? static_cast<std::uint64_t>((*result).size()) : 0;
    });

    benchmark("compare", seed, [&](std::uint64_t token) {
        const int result = text.compare(words[token & 3]);
        return static_cast<std::uint64_t>(result + 1);
    });

    benchmark("operator==", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(text == words[token & 3]);
    });

    benchmark("operator<=>", seed, [&](std::uint64_t token) {
        const auto result = text <=> words[token & 3];
        return static_cast<std::uint64_t>(result > 0) +
               static_cast<std::uint64_t>(result == 0);
    });

    benchmark("starts_with", seed, [&](std::uint64_t token) {
        constexpr tay::string_view prefixes[] = {"sustcore", "string", "s",
                                                 "x"};
        return static_cast<std::uint64_t>(
            text.starts_with(prefixes[token & 3]));
    });

    benchmark("ends_with", seed, [&](std::uint64_t token) {
        constexpr tay::string_view suffixes[] = {"delta", "gamma", "a", "x"};
        return static_cast<std::uint64_t>(text.ends_with(suffixes[token & 3]));
    });

    benchmark("contains(repetitive)", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(
            repetitive_text.contains(repetitive_patterns[token % 3]));
    }, long_search_iterations);

    benchmark("contains(random)", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(
            random_text.contains(random_patterns[token % random_pattern_count]));
    }, long_search_iterations);

    benchmark("find(repetitive)", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(
            repetitive_text.find(repetitive_patterns[token % 3]));
    }, long_search_iterations);

    benchmark("find(random)", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(
            random_text.find(random_patterns[token % random_pattern_count]));
    }, long_search_iterations);

    benchmark("rfind(repetitive)", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(
            repetitive_text.rfind(repetitive_patterns[token % 3]));
    }, long_search_iterations);

    benchmark("rfind(random)", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(
            random_text.rfind(random_patterns[token % random_pattern_count]));
    }, long_search_iterations);

    benchmark("find(char)", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(text.find(characters[token & 3]));
    });

    benchmark("rfind(char)", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(text.rfind(characters[token & 3]));
    });

    benchmark("find_first_of", seed, [&](std::uint64_t token) {
        constexpr tay::string_view sets[] = {"aeiou", "xyz", "bg", " :"};
        return static_cast<std::uint64_t>(text.find_first_of(sets[token & 3]));
    });

    benchmark("find_last_of", seed, [&](std::uint64_t token) {
        constexpr tay::string_view sets[] = {"aeiou", "xyz", "bg", " :"};
        return static_cast<std::uint64_t>(text.find_last_of(sets[token & 3]));
    });

    benchmark("find_first_not_of", seed, [&](std::uint64_t token) {
        constexpr tay::string_view sets[] = {"sustcore ", "alpha", " ", "xyz"};
        return static_cast<std::uint64_t>(
            text.find_first_not_of(sets[token & 3]));
    });

    benchmark("find_last_not_of", seed, [&](std::uint64_t token) {
        constexpr tay::string_view sets[] = {"delta", "aeiou", " ", "xyz"};
        return static_cast<std::uint64_t>(
            text.find_last_not_of(sets[token & 3]));
    });

    benchmark("string_view_hash", seed, [&](std::uint64_t token) {
        return static_cast<std::uint64_t>(
            tay::string_view_hash{}(words[token & 3]));
    });

    return 0;
}
