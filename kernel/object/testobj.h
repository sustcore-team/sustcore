/**
 * @file testobj.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试对象, 用于测试能力系统
 * @version alpha-1.0.0
 * @date 2026-02-25
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cap/capdef.h>
#include <kio.h>
#include <perm/testobj.h>
#include <sustcore/capability.h>

class TestObjectOperation;

class TestObject : public _PayloadHelper<TestObject> {
public:
    static constexpr PayloadType IDENTIFIER = PayloadType::TEST_OBJECT;
    using Operation = TestObjectOperation;
protected:
    int value;
public:
    constexpr TestObject(int v) : value(v) {}
    ~TestObject() = default;

    inline void *operator new(size_t size) noexcept {
        assert(size == sizeof(TestObject));
        return KOP<TestObject>::instance().alloc();
    }

    inline void operator delete(void *ptr) noexcept {
        KOP<TestObject>::instance().free(static_cast<TestObject *>(ptr));
    }

    friend class TestObjectOperation;
protected:
    int _read(void) const {
        return value;
    }
    void _write(int v) {
        value = v;
    }
    void _increase(void) {
        value ++;
    }
    void _decrease(void) {
        value --;
    }
};

class TestObjectOperation {
protected:
    Capability *_cap;
    TestObject *_obj;

    template <b64 perm>
    bool imply(void) const {
        return _cap->perm().basic_imply(perm);
    }
public:
    constexpr TestObjectOperation(Capability *cap) : _cap(cap), _obj(cap->payload<TestObject>()) {}
    ~TestObjectOperation() = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    CapOptional<int> read(void) const;
    void write(int v);
    void increase(void);
    void decrease(void);
};