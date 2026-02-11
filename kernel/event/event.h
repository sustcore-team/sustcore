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

// events

struct TimerTickEvent {
public:
    size_t gap_ticks;
    constexpr TimerTickEvent(size_t gap_ticks) : gap_ticks(gap_ticks) {}
};

// listeners

namespace schd {
    class TimerTickListener {
    public:
        static void handle(const TimerTickEvent &event);
    };
}

// Registries

template<>
class EventRegistry::EventInfo<TimerTickEvent> {
public:
    using Listeners = ListenerList<schd::TimerTickListener>;
    static constexpr size_t count = 1;
};

// dispatcher
template<typename EventType>
class EventDispatcher {
protected:
    using Info = typename EventRegistry::EventInfo<EventType>;
    using Listeners = typename Info::Listeners;

    template <typename Lst>
    struct DispatcherImpl;

    template<typename... _Ls>
    struct DispatcherImpl<ListenerList<_Ls...>> {
        static void dispatch(const EventType &event) {
            (_Ls::handle(event), ...);
        }
    };
public:
    static void dispatch(const EventType &event) {
        DispatcherImpl<Listeners>::dispatch(event);
    }

    static size_t listener_count() {
        return Info::count;
    }
};