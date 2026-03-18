/**
 * @file intobj.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试对象, 用于测试能力系统
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <kio.h>
#include <mem/alloc.h>
#include <mem/slub.h>
#include <object/shared.h>
#include <perm/intobj.h>
#include <sustcore/capability.h>

class IntOp;
class SIntOp;

class IntObj : public _PayloadHelper<IntObj> {
public:
    static constexpr PayloadType IDENTIFIER = PayloadType::INTOBJ;
    friend class IntOp;
    using Operation = IntOp;

private:
    int value;

public:
    constexpr IntObj(int v) : value(v) {}
    ~IntObj() = default;
protected:
    int _read(void) const {
        return value;
    }
    void _write(int v) {
        value = v;
    }
    void _increase(void) {
        value++;
    }
    void _decrease(void) {
        value--;
    }
};

class SIntManager;

class SIntObj : public SharedObject<SIntObj> {
public:
    static constexpr PayloadType IDENTIFIER = PayloadType::SINTOBJ;
    friend class SIntOp;
    friend class SIntManager;
    using Operation = SIntOp;

private:
    int value;
    bool discarded = false;

public:
    constexpr SIntObj(int v) : value(v) {}
    virtual ~SIntObj() = default;

    virtual void on_zeroref(void) {
        discarded = true;
    }

protected:
    int _read(void) const {
        return value;
    }
    void _write(int v) {
        value = v;
    }
    void _increase(void) {
        value++;
    }
    void _decrease(void) {
        value--;
    }
};

class SIntManager {
protected:
    util::ArrayList<SIntObj *> objects;

public:
    constexpr SIntManager() : objects() {}
    ~SIntManager() {
        for (SIntObj *obj : objects) {
            assert(obj->discarded == false);
            delete obj;
        }
    }

    void GC() {
        for (auto &obj : objects) {
            if (obj->discarded) {
                delete obj;
                obj = nullptr;
            }
        }

        auto it = objects.find(nullptr);
        while (it != objects.end()) {
            objects.erase(it);
            it = objects.find(nullptr);
        }
    }

    template <typename... Args>
    SIntObj *create(Args... args) {
        SIntObj *obj = new SIntObj(args...);
        objects.push_back(obj);
        return obj;
    }

    constexpr size_t object_count() const {
        return objects.size();
    }
};

using SIntAcc = SharedObjectAccessor<SIntObj>;

class IntOp {
protected:
    Capability *_cap;
    IntObj *_obj;

    template <b64 perm>
    bool imply(void) const {
        return _cap->perm().basic_imply(perm);
    }

public:
    constexpr IntOp(Capability *cap)
        : _cap(cap), _obj(cap->payload<IntObj>()) {}
    ~IntOp() = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    CapOptional<int> read(void) const;
    void write(int v);
    void increase(void);
    void decrease(void);
};

class SIntOp {
protected:
    Capability *_cap;
    SIntAcc *_acc;
    SIntObj *_obj;
    template <b64 perm>
    bool imply(void) const {
        return _cap->perm().basic_imply(perm);
    }

public:
    constexpr SIntOp(Capability *cap)
        : _cap(cap), _acc(cap->payload<SIntAcc>()), _obj(_acc->obj()) {}
    ~SIntOp() = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    CapOptional<int> read(void) const;
    void write(int v);
    void increase(void);
    void decrease(void);

    CapErrCode split(Capability *dst_csa, CapIdx idx);
};