/**
 * @file cap_type.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力类型
 * @version alpha-1.0.0
 * @date 2026-02-05
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/optional.h>
#include <sus/types.h>

// 载荷类型
enum class PayloadType { NONE = 0, CSPACE_ACCESSOR = 1, TEST_OBJECT = 2 };

constexpr const char *to_string(PayloadType type) {
    switch (type) {
        case PayloadType::NONE:            return "NONE";
        case PayloadType::CSPACE_ACCESSOR: return "CSPACE_ACCESSOR";
        case PayloadType::TEST_OBJECT:     return "TEST_OBJECT";
        default:                           return "UNKNOWN";
    }
}

// 常量定义

// CUniverse中的CSpace数量
// 每个进程都拥有一个CUniverse, 其中包含一个或多个CSpace
// 进程的所有线程都共享同一个CUniverse
// 每个线程都可以指定自己使用的两个CSpace, 称为Major Space与Minor Space
// 线程访问能力时, 需要说明是在Major Space还是Minor Space中寻找
// 其通过CapIdx指明
constexpr size_t CUNIVERSE_SIZE = 1024;

// CSpace中的CGroup数量
constexpr size_t CSPACE_SIZE = 1024;

// CGroup中的槽位数
// 每个槽位都存放着一个Capability
constexpr size_t CGROUP_SLOTS = 64;

// 每个CSpace中都有若干个CGroup, 每个CGroup又有若干个Capability
// 因此CSpace的容量为:
constexpr size_t CSPACE_CAPACITY = CSPACE_SIZE * CGROUP_SLOTS;

// SpaceType在CSpace中将被忽略
// 只有在未指明CSpace的情况下, 才会使用SpaceType来指明寻找能力的空间
namespace SpaceType {
    constexpr b64 NULLABLE = 0;
    constexpr b64 MAJOR = 1;
    constexpr b64 MINOR = 2;
    constexpr b64 ERROR = 3;
}

union CapIdx {
    // Note: 我们不考虑大端序机器
    struct {
        // 低位
        b16 slot : 16;
        // 高位
        b16 group : 16;
        // type
        b16 type : 16;
        // rsvf
        b16 rsvd : 16;
    } __attribute__((packed));
    static constexpr b64 MASK = 0x0000FFFFFFFFFFFF;
    b64 raw;

    // 然而, 构造函数中, 我们先指明group再指明slot,
    // 以符合我们平时习惯的Group在前, 槽位在后的表达方式
    constexpr CapIdx(b16 type, b16 group, b16 slot)
        : slot(slot), group(group), type(type){};
    // 默认major
    constexpr CapIdx(b16 group, b16 slot) : CapIdx(SpaceType::MAJOR, group, slot){};

    constexpr bool nullable(void) const noexcept {
        return type == SpaceType::NULLABLE;
    }

    static constexpr CapIdx from_raw(b64 raw) noexcept {
        return CapIdx(raw);
    }

protected:
    // 仅允许CapIdx类访问该方法
    constexpr CapIdx(b64 raw) : raw(raw){};

public:
    bool operator==(const CapIdx &other) const noexcept {
        if ((this->raw & MASK) == (other.raw & MASK)) return true;
        if (this->type == other.type) {
            if (this->type == SpaceType::NULLABLE || this->type == SpaceType::ERROR) {
                return true;
            }
            return false;
        }
        return this->group == other.group && this->slot == other.slot;
    }
    bool operator!=(const CapIdx &other) const noexcept {
        return !(*this == other);
    }
};

static_assert(sizeof(CapIdx) == sizeof(b64), "CapIdx must be 64 bits in size");

inline static CapIdx CapIdxNull       = CapIdx(SpaceType::NULLABLE, 0, 0);

enum class CapErrCode {
    SUCCESS                  = 0,
    INVALID_CAPABILITY       = -1,
    INVALID_INDEX            = -2,
    INSUFFICIENT_PERMISSIONS = -3,
    TYPE_NOT_MATCHED         = -4,
    PAYLOAD_ERROR            = -5,
    CREATION_FAILED          = -6,
    SLOT_BUSY                = -7,
    UNKNOWN_ERROR            = -255,
};

constexpr const char *to_string(CapErrCode err) {
    switch (err) {
        case CapErrCode::SUCCESS:                  return "SUCCESS";
        case CapErrCode::INVALID_CAPABILITY:       return "INVALID_CAPABILITY";
        case CapErrCode::INVALID_INDEX:            return "INVALID_INDEX";
        case CapErrCode::INSUFFICIENT_PERMISSIONS: return "INSUFFICIENT_PERMISSIONS";
        case CapErrCode::TYPE_NOT_MATCHED:         return "TYPE_NOT_MATCHED";
        case CapErrCode::PAYLOAD_ERROR:            return "PAYLOAD_ERROR";
        case CapErrCode::CREATION_FAILED:          return "CREATION_FAILED";
        case CapErrCode::SLOT_BUSY:                return "SLOT_BUSY";
        default:                                   return "UNKNOWN_ERROR";
    }
}

template <typename T>
using CapOptional = util::Optional<T, CapErrCode, CapErrCode::SUCCESS,
                                   CapErrCode::UNKNOWN_ERROR>;