/**
 * @file capdef.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力系统声明与常量定义
 * @version alpha-1.0.0
 * @date 2026-02-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/optional.h>
#include <sustcore/cap_type.h>

#include <concepts>

template <typename T>
using CapOptional = util::Optional<T, CapErrCode, CapErrCode::SUCCESS,
                                   CapErrCode::UNKNOWN_ERROR>;

template <typename Payload>
concept PayloadTrait = requires(Payload *p) {
    {
        p->retain()
    } -> std::same_as<void>;
    {
        p->release()
    } -> std::same_as<void>;
    {
        p->ref_count()
    } -> std::same_as<int>;
    {
        Payload::IDENTIFIER
    } -> std::same_as<const CapType &>;
    typename Payload::CCALL;
};

template <PayloadTrait Payload>
class Capability;

// CSpaces
class CSpaceBase;
template <PayloadTrait Payload, size_t SPACE_SIZE, size_t SPACE_COUNT>
class __CSpace;
template <PayloadTrait Payload, size_t SPACE_SIZE, size_t SPACE_COUNT>
class __CUniverse;
template <size_t SPACE_SIZE, size_t SPACE_COUNT, typename... Payloads>
class __CapHolder;

// CSpace  Size
constexpr size_t CAP_SPACE_SIZE  = 1024;
constexpr size_t CAP_SPACE_COUNT = 1024;

// CapHolder and Capability kinds
template <typename... Payloads>
using _CapHolder = __CapHolder<CAP_SPACE_SIZE, CAP_SPACE_COUNT, Payloads...>;

// 新的需要管理的内核对象应该追加到此处
using CapHolder = _CapHolder<CSpaceBase /*, other kernel objects*/>;