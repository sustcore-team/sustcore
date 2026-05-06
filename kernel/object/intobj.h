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
#include <perm/intobj.h>
#include <sustcore/capability.h>

namespace cap {
    struct IntPayload : public _PayloadHelper<PayloadType::INTOBJ> {
        int value;
        explicit IntPayload(int v) : value(v) {}
    };

    class IntObj : public CapObj<IntPayload> {
    public:
        using CapObj<IntPayload>::CapObj;
        
        Result<int> read() {
            using namespace perm::intobj;
            if (!imply(READ)) {
                unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
            }
            return _obj->value;
        }

        Result<void> write(int new_value) {
            using namespace perm::intobj;
            if (!imply(WRITE)) {
                unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
            }
            _obj->value = new_value;
            void_return();
        }

        Result<void> increase() {
            using namespace perm::intobj;
            if (!imply(perm::sintobj::INCREASE)) {
                unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
            }
            _obj->value++;
            void_return();
        }

        Result<void> decrease() {
            using namespace perm::intobj;
            if (!imply(perm::sintobj::DECREASE)) {
                unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
            }
            _obj->value--;
            void_return();
        }
    };
}  // namespace cap