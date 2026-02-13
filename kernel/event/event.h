/**
 * @file event.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 事件系统
 * @version alpha-1.0.0
 * @date 2026-02-11
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <concepts>

// 事件分发器
template <typename EventType>
class EventDispatcher;

// 监听器Trait
template <typename Listener, typename Event>
concept ListenerTrait = requires(const Event &event) {
    {
        Listener::handle(event)
    } -> std::same_as<void>;
};

template <typename... _Ls>
struct ListenerList;

// event registry
class EventRegistry {
protected:
    template <typename Event>
    friend class EventDispatcher;

public:
    template <typename Event>
    class EventInfo {
        using Listeners = ListenerList<>;
        static constexpr size_t count = 0;
    };
};