/**
 * @file string.cpp
 * @brief Demonstrate exception-free text manipulation with tay::string.
 * @version 0.1.0-dev.1
 * @date 2026-07-29
 */

#include <tay/allocator.h>
#include <tay/string.h>
#include <tay/string_view.h>

#include <cstdio>
#include <utility>

namespace {
    using string = tay::string<tay::allocator<char>>;

    template <class Result>
    bool succeeded(const Result& result, const char* operation) {
        if (result) {
            return true;
        }
        std::printf("%s failed with error code %u\n", operation,
                    static_cast<unsigned>(result.error()));
        return false;
    }

    void print(const char* label, tay::string_view text) {
        std::printf("%-12s %.*s\n", label, static_cast<int>(text.size()),
                    text.data());
    }
}  // namespace

int main() {
    string text("tay::string");
    print("created:", text);

    if (!succeeded(text.insert(0, "Using ", 6), "insert") ||
        !succeeded(text.append(" is pleasant"), "append"))
    {
        return 1;
    }
    print("expanded:", text);

    const auto adjective = text.find("pleasant");
    if (adjective == string::npos ||
        !succeeded(text.replace(adjective, 8, "exception-free", 14),
                   "replace") ||
        !succeeded(text.erase(0, 6), "erase"))
    {
        return 1;
    }
    print("edited:", text);

    std::printf("starts_with(\"tay\"): %s\n",
                text.starts_with("tay") ? "yes" : "no");
    std::printf("contains(\"exception\"): %s\n",
                text.contains("exception") ? "yes" : "no");
    std::printf("\"exception\" begins at: %zu\n", text.find("exception"));

    auto name = text.substr(0, 11);
    if (!succeeded(name, "substr")) {
        return 1;
    }
    print("substring:", *name);

    // Bounds errors are values too; no exception is thrown.
    auto outside = text.at(text.size());
    if (!outside) {
        std::printf("at(size) returned error code %u\n",
                    static_cast<unsigned>(outside.error()));
    }

    tay::string_view view = text;
    print("as view:", view);
    return 0;
}
