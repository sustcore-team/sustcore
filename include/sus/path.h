/**
 * @file path.h
 * @author hyj0824 (12510430@mail.sustech.edu.cn)
 * @brief 简单的路径处理
 * @version alpha-1.0.0
 * @date 2026-02-24
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/list.h>
#include <sus/mstring.h>

#include <cstddef>
#include <iterator>
#include <utility>

namespace util {
    class Path {
    private:
        util::string path_;

    public:
        // 构造函数
        Path() : path_("") {}
        Path(const char *path) : path_(path) {}
        Path(util::string path) : path_(std::move(path)) {}

        // 路径拼接
        Path operator/(const Path &other) const;
        Path concat(const Path &other) const;

        const char *c_str() const {
            return path_.c_str();
        }

        // 允许转换为 util::string，但有可能会发生拷贝
        operator util::string() const {
            return path_;
        }

        // 其他可能的成员函数，如获取文件名、扩展名等
        bool is_absolute() const;
        bool is_relative() const;
        Path parent_path() const;
        Path filename() const;
        Path stem() const;
        Path extension() const;

        bool operator==(const Path &other) const;
        bool operator!=(const Path &other) const;

        // 迭代器，返回每个以 / 分割的 entry
        // 注意，如果有根目录 / 也要作为作为元素
        class const_iterator {
        private:
            const Path *owner_{};
            size_t begin_{npos};
            size_t end_{};

            static constexpr size_t npos = static_cast<size_t>(-1);

            void locate_next();

            const_iterator(const Path *owner, size_t begin, size_t end)
                : owner_(owner), begin_(begin), end_(end) {}

            friend class Path;

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type        = Path;
            using difference_type   = void;
            using pointer           = const value_type *;
            using reference         = const value_type &;

            const_iterator() = default;

            value_type operator*() const;

            const_iterator &operator++();
            const_iterator operator++(int);

            bool operator==(const const_iterator &other) const;
            bool operator!=(const const_iterator &other) const;
        };
        using iterator = const_iterator;

        const_iterator begin() const;
        const_iterator end() const;

        Path normalize() const;
        Path relative_to(const Path &base) const;
    };
}  // namespace util