/**
 * @file registries.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 事件/监听器注册
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <event/event.h>
#include <event/init_events.h>
#include <event/misc_events.h>
#include <mem/listeners.h>
#include <schd/listeners.h>
#include <task/listener.h>

template <>
class EventRegistry::EventInfo<TimerTickEvent> {
public:
    using Listeners               = ListenerList<schd::SchedulerListener>;
    static constexpr size_t count = 1;
};

template <>
class EventRegistry::EventInfo<PreGlobalObjectInitEvent> {
public:
    using Listeners               = ListenerList<BuddyListener>;
    static constexpr size_t count = 1;
};

template <>
class EventRegistry::EventInfo<PostGlobalObjectInitEvent> {
public:
    using Listeners               = ListenerList<TaskListener>;
    static constexpr size_t count = 1;
};

// dispatcher
template <typename EventType>
class EventDispatcher {
protected:
    using Info      = typename EventRegistry::EventInfo<EventType>;
    using Listeners = typename Info::Listeners;

    template <typename Lst>
    struct DispatcherImpl;

    template <typename... _Ls>
    struct DispatcherImpl<ListenerList<_Ls...>> {
        static void dispatch(EventType &event) {
            (_Ls::handle(event), ...);
        }
    };

public:
    static void dispatch(EventType &event) {
        DispatcherImpl<Listeners>::dispatch(event);
    }

    static size_t listener_count() {
        return Info::count;
    }
};