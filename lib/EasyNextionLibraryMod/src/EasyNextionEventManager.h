/*!
 * EasyNextionEventManager.h - Easy library for managing events on Nextion Displays
 * Copyright (c) 2025 Christophe Belmont
 * All rights reserved under the library's licence
 *
 * Note that evenRef 50 is reserved for Page management.
 * 
 */

#ifndef EasyNextionEventManager_h
#define EasyNextionEventManager_h

#include "Arduino.h"
#include <map>
#include <functional>

class EasyNextionEventManager {
public:
    using EventCallback = std::function<void(u_int8_t ,u_int8_t (&)[10])>;

    void registerEvent(u_int8_t eventRef, EventCallback callback) {
        eventHandlers[eventRef] = callback;
    }

    void triggerEvent(u_int8_t eventRef, u_int8_t paramLength, u_int8_t (&eventData)[10]) {
        auto it = eventHandlers.find(eventRef);
        if (it != eventHandlers.end()) {
            it->second(paramLength,eventData);
        } else {
            // No handler found for the event
        }
    }

private:
    std::map<u_int8_t, EventCallback> eventHandlers;
};

#endif
