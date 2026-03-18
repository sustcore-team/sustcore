/**
 * @file shared.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 共享对象
 * @version alpha-1.0.0
 * @date 2026-03-18
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <sustcore/capability.h>

#include <concepts>

// 共享对象基类
template <typename PayloadType>
class SharedObject {
public:
    constexpr PayloadType *as(void) {
        return static_cast<PayloadType>(this);
    }

private:
    int refcount;

protected:
    constexpr SharedObject() : refcount(0) {}
    virtual void on_zeroref(void) = 0;

public:
    constexpr void keep(void) {
        refcount++;
    }
    constexpr void release(void) {
        if (refcount > 0) {
            refcount--;
        } else {
            // throw an error here
        }
        if (refcount == 0) {
            on_zeroref();
        }
    }
};

/**
 * @brief 共享对象访问器
 *
 * Capability并不直接持有共享对象, 而是持有Accessor
 * 当Accessor的生命周期结束时, 会使SharedObject的引用计数减一
 * 从而在保证Capability对持有的Payload具有完全所有权的同时
 * 保证SharedObject能够被多个对象持有
 *
 * @tparam SharedObj 共享对象类型
 */
template <typename SharedObj>
    requires std::derived_from<SharedObj, SharedObject<SharedObj>>
class SharedObjectAccessor : public Payload {
public:
    using Payload = SharedObj;
    static constexpr PayloadType IDENTIFIER = PayloadType::SINTOBJ;
    virtual PayloadType type_id(void) const override {
        return SharedObj::IDENTIFIER;
    }

private:
    SharedObj *_obj;

public:
    constexpr SharedObjectAccessor(SharedObj *obj) : _obj(obj) {
        _obj->keep();
    }
    virtual ~SharedObjectAccessor() {
        _obj->release();
    }
    constexpr SharedObj *obj() const {
        return _obj;
    }
};