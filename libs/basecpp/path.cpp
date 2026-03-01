/**
 * @file path.cpp
 * @author hyj0824 (12510430@mail.sustech.edu.cn)
 * @brief Path 类实现
 * @version alpha-1.0.0
 * @date 2026-02-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sus/path.h>

#include <cassert>

namespace util {

    // 路径拼接
    Path Path::operator/(const Path &other) const {
        util::string_builder path{path_.length() + other.path_.length() + 2};
        // 额外空间用于斜杠和结尾的空字符
        if (path_.length()) {
            path.append(path_);
            path.append('/');
        }
        path.append(other.path_);
        return Path{path.build()};
    }

    Path Path::concat(const Path &other) const {
        util::string_builder path{path_.length() + other.path_.length() + 1};
        // 额外空间用于结尾的空字符
        path.append(path_);
        path.append(other.path_);
        return Path{path.build()};
    }

    // 其他可能的成员函数，如获取文件名、扩展名等
    bool Path::is_absolute() const {
        return path_[0] == '/';
    }

    bool Path::is_relative() const {
        return !is_absolute();
    }

    Path Path::parent_path() const {
        size_t last_slash = path_.length();
        while (last_slash && path_[--last_slash] != '/');
        if (path_[last_slash] == '/') {
            // 找到了最后一个斜杠
            if (last_slash == 0) {
                // 如果最后一个斜杠是第一个字符，说明父路径是根目录
                return Path{"/"};
            } else {
                return Path{path_.substr(0, last_slash)};
            }
        }
        return Path{""};
    }

    Path Path::filename() const {
        size_t last_slash = path_.length();
        while (last_slash && path_[--last_slash] != '/');
        if (path_[last_slash] == '/') {
            // 找到了最后一个斜杠，返回它之后的部分
            return Path{path_.substr(last_slash + 1)};
        }
        return *this;  // 没有斜杠，整个路径就是文件名
    }

    Path Path::stem() const {
        Path fname = filename();
        if (fname == "." || fname == "..") {
            // 特殊的，如果文件名是 "." 或 ".."，认为它没有 stem
            return fname;
        }
        size_t last_dot = fname.path_.length();
        while (last_dot && fname.path_[--last_dot] != '.');
        if (fname.path_[last_dot] == '.') {
            if (last_dot == 0)
                return fname;
            // 找到了最后一个点，返回它之前的部分
            return Path{fname.path_.substr(0, last_dot)};
        }
        return fname;  // 没有点，整个文件名就是 stem
    }

    Path Path::extension() const {
        Path fname = filename();
        if (fname == "." || fname == "..") {
            // 如果文件名是 "." 或 ".."，认为它没有扩展名
            return "";
        }
        size_t last_dot = fname.path_.length();
        while (last_dot && fname.path_[--last_dot] != '.');
        if (fname.path_[last_dot] == '.') {
            if (last_dot == 0)
                return "";  // 说明是 dotfile，没有扩展名
            // 找到了最后一个点，返回它以及之后的部分
            return Path{fname.path_.substr(last_dot)};
        }
        return "";  // 没有点，没有扩展名
    }

    bool Path::operator==(const Path &other) const {
        return path_ == other.path_;
    }

    bool Path::operator!=(const Path &other) const {
        return !(*this == other);
    }

    Path Path::normalize() const {
        if (path_.length() == 0)
            return "";
        util::ArrayList<Path::const_iterator> st;
        for (auto it = begin(); it != end(); ++it) {
            if (*it == ".") {
                continue;
            } else if (*it == "..") {
                if (st.empty() || *st.back() == "..") {
                    st.push_back(it);
                } else if (*st.back() == "/") {
                    continue;
                } else {
                    st.pop_back();
                }
            } else {
                st.push_back(it);
            }
        }

        util::string_builder path_builder;
        for (const auto &it : st) {
            path_builder.append(it->data(), it->size());
            if (*it == "/")
                continue;
            path_builder.append('/');
        }
        if (path_builder.length() == 0) {
            // 说明路径被规范化成了当前目录
            return ".";
        } else if (path_builder.length() > 1) {
            // 去掉末尾的斜杠，除非路径是根目录 "/"
            path_builder.revert(1);
        }
        return path_builder.build();
    }

    Path Path::relative_to(const Path &base) const {
        // 首先规范化路径
        Path norm      = this->normalize();
        Path norm_base = base.normalize();
        // 找相同前缀
        auto it1       = norm.begin();
        auto it2       = norm_base.begin();
        if (*it1 != *it2) {
            // 没有公共前缀
            return "";
        }
        while (it1 != norm.end() && it2 != norm_base.end() && *it1 == *it2) {
            ++it1;
            ++it2;
        }

        util::string_builder relative_path;
        // 从不匹配的位置开始构建相对路径
        while (it2 != norm_base.end()) {
            // 对于 base 中剩余的每个元素，添加一个 ".."
            relative_path.append("../");
            ++it2;
        }
        relative_path.append(norm.path_.c_str() + it1.begin_);
        return relative_path.build();
    }

    // 不允许迭代器修改 Path 的 entry
    void Path::const_iterator::locate_next() {
        assert(owner_ != nullptr);
        const size_t len = owner_->path_.length();
        // 这个算法对于第一个和后面的都适用，
        // 第一个时 end_ = 0，会跳过开头的斜杠
        size_t i         = end_;
        // skip the leading '/'
        while (i < len && owner_->path_[i] == '/') {
            ++i;
        }
        begin_ = i;
        while (i < len && owner_->path_[i] != '/') {
            ++i;
        }
        end_ = i;
        // 如果 begin_ == end_ == len，说明已经遍历完了

        // 更新 current_entry_
        assert(begin_ != npos && "没有 locate 到合法元素");
        if (begin_ != end_)
            current_entry_ = {owner_->c_str() + begin_, end_ - begin_};
        else
            current_entry_ = {};
    }

    Path::const_iterator::reference Path::const_iterator::operator*() const {
        return current_entry_;
    }

    Path::const_iterator::pointer Path::const_iterator::operator->() const {
        return &current_entry_;
    }

    Path::const_iterator &Path::const_iterator::operator++() {
        locate_next();
        return *this;
    }

    Path::const_iterator Path::const_iterator::operator++(int) {
        const_iterator temp = *this;
        ++(*this);
        return temp;
    }

    bool Path::const_iterator::operator==(const const_iterator &other) const {
        return owner_ == other.owner_ && begin_ == other.begin_ &&
               end_ == other.end_;
    }

    bool Path::const_iterator::operator!=(const const_iterator &other) const {
        return !(*this == other);
    }

    Path::const_iterator Path::begin() const {
        const_iterator it{this, const_iterator::npos, 0};
        if (is_relative())
            it.locate_next();
        else
            it.current_entry_ = {path_.c_str(), 1};
        return it;
    }

    Path::const_iterator Path::end() const {
        const size_t len = path_.length();
        return const_iterator{this, len, len};
    }

}  // namespace util