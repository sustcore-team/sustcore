/**
 * @file path.h
 * @brief Allocator-aware POSIX lexical paths.
 */

#pragma once

#include <tay/allocator.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/fmt/core.h>
#include <tay/string.h>
#include <tay/string_view.h>

#include <compare>
#include <cstddef>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

namespace tay {
    template <class Allocator = allocator<char>>
    class path {
    public:
        using allocator_type = Allocator;
        using string_type    = string<allocator_type>;
        using size_type      = typename string_type::size_type;

    private:
        struct string_tag {};

        string_type path_;

        constexpr path(string_tag, string_type&& text) noexcept
            : path_(std::move(text)) {}

        [[nodiscard]] static constexpr bool is_dot(string_view component) {
            return component == ".";
        }

        [[nodiscard]] static constexpr bool is_dot_dot(string_view component) {
            return component == "..";
        }

        [[nodiscard]] static constexpr expected<void, error_code>
        append_component(string_type& output, string_view component) noexcept {
            if (!output.empty() && output[output.size() - 1] != '/') {
                auto separator = output.push_back('/');
                if (!separator) {
                    return separator;
                }
            }
            auto appended = output.append(component);
            if (!appended) {
                return expected<void, error_code>(unexpect, appended.error());
            }
            return {};
        }

        static constexpr void pop_component(string_type& output) noexcept {
            const size_type slash = output.rfind('/');
            if (slash == string_type::npos) {
                output.clear();
                return;
            }
            const size_type new_size = slash == 0 ? 1 : slash;
            (void)output.resize(new_size);
        }

        [[nodiscard]] constexpr expected<path, error_code> try_slice(
            size_type position, size_type count) const noexcept {
            auto text = string_type::try_create(path_.data() + position, count,
                                                path_.get_allocator());
            if (!text) {
                return expected<path, error_code>(unexpect, text.error());
            }
            return path(string_tag{}, std::move(*text));
        }

    public:
        class const_iterator {
        private:
            const path* owner_ = nullptr;
            size_type begin_   = 0;
            size_type end_     = 0;
            bool root_         = false;
            string_view current_{};

            constexpr const_iterator(const path* owner, size_type begin,
                                     size_type end, bool root) noexcept
                : owner_(owner), begin_(begin), end_(end), root_(root) {
                refresh();
            }

            constexpr void refresh() noexcept {
                if (owner_ == nullptr || begin_ == owner_->size()) {
                    current_ = {};
                } else if (root_) {
                    current_ = string_view("/", 1);
                } else {
                    current_ = string_view(owner_->path_.data() + begin_,
                                           end_ - begin_);
                }
            }

            constexpr void locate(size_type position) noexcept {
                const size_type length = owner_->size();
                while (position < length && owner_->path_[position] == '/') {
                    ++position;
                }
                begin_ = position;
                while (position < length && owner_->path_[position] != '/') {
                    ++position;
                }
                end_  = position;
                root_ = false;
                refresh();
            }

            friend class path;

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type        = string_view;
            using difference_type   = std::ptrdiff_t;
            using pointer           = const value_type*;
            using reference         = const value_type&;

            constexpr const_iterator() noexcept = default;

            [[nodiscard]] constexpr reference operator*() const noexcept {
                return current_;
            }

            [[nodiscard]] constexpr pointer operator->() const noexcept {
                return &current_;
            }

            constexpr const_iterator& operator++() noexcept {
                if (root_) {
                    locate(1);
                } else if (owner_ != nullptr && begin_ != owner_->size()) {
                    locate(end_);
                }
                return *this;
            }

            constexpr const_iterator operator++(int) noexcept {
                const_iterator copy = *this;
                ++*this;
                return copy;
            }

            [[nodiscard]] friend constexpr bool operator==(
                const const_iterator& left,
                const const_iterator& right) noexcept {
                return left.owner_ == right.owner_ &&
                       left.begin_ == right.begin_ && left.end_ == right.end_ &&
                       left.root_ == right.root_;
            }
        };

        using iterator = const_iterator;

        constexpr path() noexcept
            requires(std::is_nothrow_default_constructible_v<allocator_type>)
        = default;

        constexpr explicit path(const allocator_type& allocator) noexcept
            : path_(allocator) {}

        constexpr path(const path&) noexcept            = default;
        constexpr path(path&&) noexcept                 = default;
        constexpr path& operator=(const path&) noexcept = default;
        constexpr path& operator=(path&&) noexcept      = default;
        constexpr ~path() noexcept                      = default;

        [[nodiscard]] static constexpr expected<path, error_code> try_create(
            const allocator_type& allocator) noexcept {
            return path(allocator);
        }

        [[nodiscard]] static constexpr expected<path, error_code> try_create(
            string_view text, const allocator_type& allocator) noexcept {
            auto created = string_type::try_create(text, allocator);
            if (!created) {
                return expected<path, error_code>(unexpect, created.error());
            }
            return path(string_tag{}, std::move(*created));
        }

        [[nodiscard]] static constexpr expected<path, error_code> try_create(
            string_view text) noexcept
            requires(std::is_nothrow_default_constructible_v<allocator_type>)
        {
            return try_create(text, allocator_type{});
        }

        [[nodiscard]] static constexpr expected<path, error_code> try_create(
            const char* text, const allocator_type& allocator) noexcept {
            if (text == nullptr) {
                return expected<path, error_code>(unexpect,
                                                  error_code::NULLPTR);
            }
            return try_create(string_view(text), allocator);
        }

        [[nodiscard]] static constexpr expected<path, error_code> try_create(
            const char* text) noexcept
            requires(std::is_nothrow_default_constructible_v<allocator_type>)
        {
            return try_create(text, allocator_type{});
        }

        [[nodiscard]] constexpr const char* c_str() const noexcept {
            return path_.c_str();
        }

        [[nodiscard]] constexpr string_view view() const noexcept {
            return string_view(path_);
        }

        [[nodiscard]] constexpr size_type size() const noexcept {
            return path_.size();
        }

        [[nodiscard]] constexpr size_type length() const noexcept {
            return size();
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return path_.empty();
        }

        [[nodiscard]] constexpr bool is_absolute() const noexcept {
            return !empty() && path_[0] == '/';
        }

        [[nodiscard]] constexpr bool is_relative() const noexcept {
            return !is_absolute();
        }

        [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
            return path_.get_allocator();
        }

        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            if (empty()) {
                return end();
            }
            if (is_absolute()) {
                return const_iterator(this, 0, 1, true);
            }
            size_type component_end = 0;
            while (component_end < size() && path_[component_end] != '/') {
                ++component_end;
            }
            return const_iterator(this, 0, component_end, false);
        }

        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return const_iterator(this, size(), size(), false);
        }

        constexpr expected<void, error_code> try_concat(
            string_view suffix) noexcept {
            auto appended = path_.append(suffix);
            if (!appended) {
                return expected<void, error_code>(unexpect, appended.error());
            }
            return {};
        }

        constexpr expected<void, error_code> try_concat(
            const path& suffix) noexcept {
            return try_concat(suffix.view());
        }

        [[nodiscard]] constexpr expected<path, error_code> try_join(
            string_view other) const noexcept {
            if (!other.empty() && other[0] == '/') {
                return try_create(other, get_allocator());
            }

            auto result = try_create(view(), get_allocator());
            if (!result) {
                return result;
            }
            if (!result->empty() && !other.empty() &&
                result->path_[result->size() - 1] != '/')
            {
                auto separator = result->path_.push_back('/');
                if (!separator) {
                    return expected<path, error_code>(unexpect,
                                                      separator.error());
                }
            }
            auto appended = result->path_.append(other);
            if (!appended) {
                return expected<path, error_code>(unexpect, appended.error());
            }
            return result;
        }

        [[nodiscard]] constexpr expected<path, error_code> try_join(
            const path& other) const noexcept {
            return try_join(other.view());
        }

        [[nodiscard]] constexpr expected<path, error_code> try_parent_path()
            const noexcept {
            if (empty()) {
                return try_slice(0, 0);
            }

            size_type end_position = size();
            while (end_position > 1 && path_[end_position - 1] == '/') {
                --end_position;
            }
            if (end_position != size()) {
                return try_slice(0, end_position);
            }

            size_type slash = end_position;
            while (slash > 0 && path_[slash - 1] != '/') {
                --slash;
            }
            if (slash == 0) {
                return try_slice(0, 0);
            }
            while (slash > 1 && path_[slash - 1] == '/') {
                --slash;
            }
            return try_slice(0, slash);
        }

        [[nodiscard]] constexpr expected<path, error_code> try_filename()
            const noexcept {
            if (empty() || path_[size() - 1] == '/') {
                return try_slice(size(), 0);
            }
            size_type begin_position = size();
            while (begin_position > 0 && path_[begin_position - 1] != '/') {
                --begin_position;
            }
            return try_slice(begin_position, size() - begin_position);
        }

        [[nodiscard]] constexpr expected<path, error_code> try_stem()
            const noexcept {
            auto filename = try_filename();
            if (!filename) {
                return filename;
            }
            if (filename->view() == "." || filename->view() == "..") {
                return filename;
            }
            const size_type dot = filename->path_.rfind('.');
            if (dot == string_type::npos || dot == 0) {
                return filename;
            }
            return filename->try_slice(0, dot);
        }

        [[nodiscard]] constexpr expected<path, error_code> try_extension()
            const noexcept {
            auto filename = try_filename();
            if (!filename) {
                return filename;
            }
            if (filename->view() == "." || filename->view() == "..") {
                return try_slice(0, 0);
            }
            const size_type dot = filename->path_.rfind('.');
            if (dot == string_type::npos || dot == 0) {
                return try_slice(0, 0);
            }
            return filename->try_slice(dot, filename->size() - dot);
        }

        [[nodiscard]] constexpr expected<path, error_code> try_normalize()
            const noexcept {
            if (empty()) {
                return try_create(get_allocator());
            }

            auto output = string_type::try_create(get_allocator());
            if (!output) {
                return expected<path, error_code>(unexpect, output.error());
            }
            if (is_absolute()) {
                auto root = output->push_back('/');
                if (!root) {
                    return expected<path, error_code>(unexpect, root.error());
                }
            }

            for (auto component : *this) {
                if (component == "/" || is_dot(component)) {
                    continue;
                }
                if (is_dot_dot(component)) {
                    const size_type slash = output->rfind('/');
                    const size_type last_begin =
                        slash == string_type::npos ? 0 : slash + 1;
                    const string_view last(output->data() + last_begin,
                                           output->size() - last_begin);
                    if (!output->empty() && string_view(*output) != "/" &&
                        !is_dot_dot(last))
                    {
                        pop_component(*output);
                    } else if (!is_absolute()) {
                        auto appended = append_component(*output, component);
                        if (!appended) {
                            return expected<path, error_code>(unexpect,
                                                              appended.error());
                        }
                    }
                    continue;
                }
                auto appended = append_component(*output, component);
                if (!appended) {
                    return expected<path, error_code>(unexpect,
                                                      appended.error());
                }
            }

            if (output->empty() && is_relative()) {
                auto current = output->push_back('.');
                if (!current) {
                    return expected<path, error_code>(unexpect,
                                                      current.error());
                }
            }
            return path(string_tag{}, std::move(*output));
        }

        [[nodiscard]] constexpr expected<path, error_code> try_relative_to(
            const path& base) const noexcept {
            if (is_absolute() != base.is_absolute()) {
                return expected<path, error_code>(unexpect,
                                                  error_code::INVALID_ARGUMENT);
            }

            auto target_normal = try_normalize();
            if (!target_normal) {
                return target_normal;
            }
            auto base_normal = base.try_normalize();
            if (!base_normal) {
                return expected<path, error_code>(unexpect,
                                                  base_normal.error());
            }

            auto target_it = target_normal->begin();
            auto base_it   = base_normal->begin();
            while (target_it != target_normal->end() &&
                   base_it != base_normal->end() && *target_it == *base_it)
            {
                ++target_it;
                ++base_it;
            }

            std::ptrdiff_t parents = 0;
            for (auto it = base_it; it != base_normal->end(); ++it) {
                if (*it == "/" || is_dot(*it)) {
                    continue;
                }
                if (is_dot_dot(*it)) {
                    --parents;
                } else {
                    ++parents;
                }
            }
            if (parents < 0) {
                return try_create(get_allocator());
            }

            auto output = string_type::try_create(get_allocator());
            if (!output) {
                return expected<path, error_code>(unexpect, output.error());
            }
            while (parents-- > 0) {
                auto appended = append_component(*output, "..");
                if (!appended) {
                    return expected<path, error_code>(unexpect,
                                                      appended.error());
                }
            }
            for (; target_it != target_normal->end(); ++target_it) {
                if (*target_it == "/" || is_dot(*target_it)) {
                    continue;
                }
                auto appended = append_component(*output, *target_it);
                if (!appended) {
                    return expected<path, error_code>(unexpect,
                                                      appended.error());
                }
            }
            if (output->empty()) {
                auto current = output->push_back('.');
                if (!current) {
                    return expected<path, error_code>(unexpect,
                                                      current.error());
                }
            }
            return path(string_tag{}, std::move(*output));
        }

        [[nodiscard]] constexpr bool starts_with(
            string_view prefix) const noexcept {
            if (prefix.empty()) {
                return true;
            }
            if (size() < prefix.size()) {
                return false;
            }
            const string_view selected(path_.data(), prefix.size());
            if (selected != prefix) {
                return false;
            }
            return size() == prefix.size() ||
                   prefix[prefix.size() - 1] == '/' ||
                   path_[prefix.size()] == '/';
        }

        [[nodiscard]] constexpr bool starts_with(
            const path& prefix) const noexcept {
            return starts_with(prefix.view());
        }

        [[nodiscard]] constexpr bool ends_with(
            string_view suffix) const noexcept {
            if (suffix.empty()) {
                return true;
            }
            if (size() < suffix.size()) {
                return false;
            }
            const size_type start = size() - suffix.size();
            auto selected         = view().substr(start, suffix.size());
            if (!selected || *selected != suffix) {
                return false;
            }
            if (start == 0) {
                return true;
            }
            return suffix[0] != '/' && path_[start - 1] == '/';
        }

        [[nodiscard]] constexpr bool ends_with(
            const path& suffix) const noexcept {
            return ends_with(suffix.view());
        }

        [[nodiscard]] friend constexpr bool operator==(
            const path& left, const path& right) noexcept {
            return left.view() == right.view();
        }

        [[nodiscard]] friend constexpr std::strong_ordering operator<=>(
            const path& left, const path& right) noexcept {
            return left.view() <=> right.view();
        }

        [[nodiscard]] constexpr bool operator==(
            string_view other) const noexcept {
            return view() == other;
        }

        [[nodiscard]] constexpr std::strong_ordering operator<=>(
            string_view other) const noexcept {
            return view() <=> other;
        }
    };

    template <class Allocator>
    struct formatter<path<Allocator>> {
        constexpr format_parse_context::iterator parse(
            format_parse_context& context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const path<Allocator>& value,
                                                FormatContext& context) const {
            context.write(value.view());
            return context.out();
        }
    };
}  // namespace tay

namespace std {
    template <class Allocator>
    struct hash<tay::path<Allocator>> {
        [[nodiscard]] constexpr size_t operator()(
            const tay::path<Allocator>& value) const noexcept {
            return tay::string_view_hash{}(value.view());
        }
    };
}  // namespace std
