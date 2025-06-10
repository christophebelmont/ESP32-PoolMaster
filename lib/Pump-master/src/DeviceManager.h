/*
            Device Manager - a simple library to handle group of PIN/RELAY/PUMP/INPUTSENSOR devices
                 (c) Christophe <christophe.belmont@gmail.com> 2024
Features: 

- handle looping through all devices and global actions
- compatible with PIN base class
- handle access to specific devices 

NB: all timings are in milliseconds
*/
#include "Pin.h"
#include <map>
#include <Arduino.h>

#ifndef DEVICEMANAGER_h
#define DEVICEMANAGER_h

#define PREF_STORAGE_NAME "DeviceManager"
#define PREF_LOADSAVE_ALL_DEVICES 255

class DeviceManager {
  public:
    void AddDevice(uint8_t _index, PIN* _device) {
        devices[_index] = _device; // Ajout avec index explicite
    }

    PIN* GetDevice(uint8_t index) {
        return (devices.find(index) != devices.end()) ? devices[index] : nullptr;
    }

    uint8_t GetDeviceIndex(PIN* _device) {
        for (const auto& entry : devices) {
            if (entry.second == _device) {
                return entry.first;
            }
        }
        return UINT8_MAX; // Retourne une valeur impossible si l'appareil n'est pas trouvé
 }

    void InitDevicesInterlock(uint8_t _index = PREF_LOADSAVE_ALL_DEVICES)
    {
         if(_index != PREF_LOADSAVE_ALL_DEVICES) {
            if(devices[_index]->GetInterlockId() == NO_INTERLOCK) {
                devices[_index]->SetInterlock(nullptr); // Set interlock for the device
            } else {
                for (const auto& subentry : devices) {
                    if(devices[_index]->GetInterlockId() == subentry.second->GetPinId()) {
                        devices[_index]->SetInterlock(subentry.second); // Set interlock for the device
                    }
                }
            }
         } else {
            for (const auto& entry : devices) {
                if(entry.second->GetInterlockId() == NO_INTERLOCK) {
                    entry.second->SetInterlock(nullptr); // Set interlock for the device
                } else {
                    for (const auto& subentry : devices) {
                        if(entry.second->GetInterlockId() == subentry.second->GetPinId()) {
                            entry.second->SetInterlock(subentry.second); // Set interlock for the device
                        }
                    }
                }
            }
        }
    }

    void ResetUptimes()
    {
        double temp_tank_fill;
        for (const auto& entry : devices) {
            temp_tank_fill = entry.second->GetTankFill(); // Get the tank fill before resetting
            entry.second->ResetUpTime(); // Reset UpTime for all devices
            entry.second->SetTankFill(temp_tank_fill); // Set the tank fill back to its original value
        }
    }

    void SavePreferences(uint8_t _index = PREF_LOADSAVE_ALL_DEVICES) {
        preferences.begin(PREF_STORAGE_NAME, false);  // Démarrer la mémoire en read_write mode
        preferences.putUInt("device_count", devices.size()); // Enregistrer le nombre d'appareils

        if(_index != PREF_LOADSAVE_ALL_DEVICES) {
            devices[_index]->SavePreferences(preferences); // Sauvegarde individuelle
            return;
        } else {
            for (const auto& entry : devices) {
                entry.second->SavePreferences(preferences); // Sauvegarde de toutes les entrées
            }
        }
        preferences.end(); // Fermer la mémoire
    }

    void LoadPreferences(uint8_t _index = PREF_LOADSAVE_ALL_DEVICES) {
        preferences.begin(PREF_STORAGE_NAME, true);  // Démarrer la mémoire en read only mode
        uint8_t deviceCount = preferences.getUInt("device_count", 0);

        if(_index != PREF_LOADSAVE_ALL_DEVICES) {
            devices[_index]->LoadPreferences(preferences); // Sauvegarde individuelle
            return;
        } else {
            for (const auto& entry : devices) {
                entry.second->LoadPreferences(preferences); // Sauvegarde de toutes les entrées
            }
        }
        preferences.end(); // Fermer la mémoire
    }

    uint8_t GetSize() {
        return devices.size();
    }

    void Begin() {
        for (const auto& entry : devices) {
            entry.second->Begin(); // Appelle loop() sur chaque appareil enregistré
        }
    }


    void Loop() {
        for (const auto& entry : devices) {
            entry.second->loop(); // Appelle loop() sur chaque appareil enregistré
        }
    }


  private:
    std::map<uint8_t, PIN*> devices;
    Preferences preferences;
};

#endif
