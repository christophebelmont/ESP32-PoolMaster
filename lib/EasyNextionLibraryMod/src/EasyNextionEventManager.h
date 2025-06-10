/*!
 * EasyNextionEventManager.h - Easy library for managing events on Nextion Displays
 * Copyright (c) 2025 Christophe Belmont
 * All rights reserved under the library's licence
 */

#ifndef EasyNextionEventManager_h
#define EasyNextionEventManager_h

// include this library's description file
//#ifndef EasyNextionLibrary_h
//#include "EasyNextionLibrary.h"
//#endif


#include <map>
#include <functional>

class EasyNextionEventManager {
public:
    using EventCallback = std::function<void(int)>;

    void registerEvent(int eventRef, EventCallback callback) {
        eventHandlers[eventRef] = callback;
    }

    void triggerEvent(int eventRef) {
        auto it = eventHandlers.find(eventRef);
        if (it != eventHandlers.end()) {
            it->second(eventRef);
        } else {
            //std::cerr << "Aucun gestionnaire trouvé pour l'événement: " << eventRef << std::endl;
        }
    }

private:
    std::map<int, EventCallback> eventHandlers;
};

#endif
