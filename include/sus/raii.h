/**
 * @file raii.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief raii
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>

namespace util {
    template <typename T>
    concept NotVoid = !std::same_as<T, void>;

    template <NotVoid T>
    class DefaultDeleter {
    public:
        void operator()(T *resource) const noexcept {
            delete resource;
        }
    };

    template <NotVoid T>
    class DefaultDeleter<T[]> {
    public:
        void operator()(T *resource) const noexcept {
            delete[] resource;
        }
    };

    template <typename T, typename Deleter = DefaultDeleter<T>>
    class Raii;

    template <NotVoid T, typename Deleter>
    class Raii<T, Deleter> {
    public:
        using ValueType   = T;
        using DeleterType = Deleter;

    protected:
        ValueType *resource;
        DeleterType deleter;

    public:
        Raii(ValueType *resource, DeleterType deleter = DeleterType())
            : resource(resource), deleter(deleter) {}
        Raii(DeleterType deleter = DeleterType()) : Raii(nullptr, deleter) {}
        ~Raii() {
            if (resource != nullptr)
                deleter(resource);
        }
        ValueType *get() const noexcept {
            return resource;
        }
        // 释放所有权, 但不删除资源
        ValueType *release() noexcept {
            assert(resource != nullptr);
            ValueType *res = resource;
            resource       = nullptr;
            return res;
        }
        bool valid() const noexcept {
            return resource != nullptr;
        }

        Raii(const Raii<ValueType, DeleterType> &) = delete;
        Raii<ValueType, DeleterType> &operator=(
            const Raii<ValueType, DeleterType> &) = delete;

        Raii(Raii<ValueType, DeleterType> &&other) noexcept
            : resource(other.resource), deleter(other.deleter) {
            other.resource = nullptr;
        }

        Raii<ValueType, DeleterType> &operator=(
            Raii<ValueType, DeleterType> &&other) noexcept {
            if (this != &other) {
                deleter(resource);
                resource       = other.resource;
                deleter        = other.deleter;
                other.resource = nullptr;
            }
            return *this;
        }

        ValueType &operator*() const {
            assert(resource != nullptr);
            return *resource;
        }

        ValueType *operator->() const {
            assert(resource != nullptr);
            return resource;
        }

        operator bool() const noexcept {
            return valid();
        }
    };

    template <NotVoid T>
    class Raii<T[], DefaultDeleter<T[]>> {
    protected:
        using ValueType   = T;
        using DeleterType = DefaultDeleter<T[]>;

    protected:
        ValueType *resource;
        DeleterType deleter;

    public:
        Raii(ValueType *resource, DeleterType deleter = DeleterType())
            : resource(resource), deleter(deleter) {}
        Raii(DeleterType deleter = DeleterType()) : Raii(nullptr, deleter) {}
        ~Raii() {
            if (resource != nullptr)
                deleter(resource);
        }
        ValueType *get() const noexcept {
            return resource;
        }
        // 释放所有权, 但不删除资源
        ValueType *release() const noexcept {
            ValueType *res = resource;
            resource       = nullptr;
            return res;
        }
        bool valid() const noexcept {
            return resource != nullptr;
        }

        Raii(const Raii<ValueType, DeleterType> &) = delete;
        Raii<ValueType, DeleterType> &operator=(
            const Raii<ValueType, DeleterType> &) = delete;

        Raii(Raii<ValueType, DeleterType> &&other) noexcept
            : resource(other.resource), deleter(other.deleter) {
            other.resource = nullptr;
        }

        Raii<ValueType, DeleterType> &operator=(
            Raii<ValueType, DeleterType> &&other) noexcept {
            if (this != &other) {
                deleter(resource);
                resource       = other.resource;
                deleter        = other.deleter;
                other.resource = nullptr;
            }
            return *this;
        }

        const ValueType &operator[](size_t index) const {
            assert(resource != nullptr);
            return resource[index];
        }

        ValueType &operator[](size_t index) {
            assert(resource != nullptr);
            return resource[index];
        }

        operator bool() const noexcept {
            return valid();
        }
    };

    template <typename Deleter>
    class Raii<void, Deleter> {
    public:
        using ValueType   = void;
        using DeleterType = Deleter;

    protected:
        ValueType *resource;
        DeleterType deleter;

    public:
        Raii(ValueType *resource, DeleterType deleter = DeleterType())
            : resource(resource), deleter(deleter) {}
        Raii(DeleterType deleter = DeleterType()) : Raii(nullptr, deleter) {}
        ~Raii() {
            if (resource != nullptr)
                deleter(resource);
        }
        ValueType *get() const noexcept {
            return resource;
        }
        // 释放所有权, 但不删除资源
        ValueType *release() noexcept {
            assert(resource != nullptr);
            ValueType *res = resource;
            resource       = nullptr;
            return res;
        }
        bool valid() const noexcept {
            return resource != nullptr;
        }

        Raii(const Raii<ValueType, DeleterType> &) = delete;
        Raii<ValueType, DeleterType> &operator=(
            const Raii<ValueType, DeleterType> &) = delete;

        Raii(Raii<ValueType, DeleterType> &&other) noexcept
            : resource(other.resource), deleter(other.deleter) {
            other.resource = nullptr;
        }

        Raii<ValueType, DeleterType> &operator=(
            Raii<ValueType, DeleterType> &&other) noexcept {
            if (this != &other) {
                deleter(resource);
                resource       = other.resource;
                deleter        = other.deleter;
                other.resource = nullptr;
            }
            return *this;
        }

        operator bool() const noexcept {
            return valid();
        }
    };

    template <typename T>
    Raii<T> make_raii(T *resource) {
        return Raii<T>(resource);
    }

    template <typename T, typename... Args>
    Raii<T> make_raii(Args... args) {
        return Raii<T>(new T(args...));
    }

    template <typename T>
    Raii<T[]> make_array_raii(T *resource) {
        return Raii<T[]>(resource);
    }
}  // namespace util