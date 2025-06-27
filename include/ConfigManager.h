#ifndef POOLMASTER_CONFIGMANAGER_H
#define POOLMASTER_CONFIGMANAGER_H

#include <Preferences.h>
#include <cstring>
#include <typeinfo>


#define MAX_PARAMS 70 // ou plus selon ton besoin

enum ParamType {
    TYPE_BOOL,
    TYPE_UINT8,
    TYPE_ULONG,
    TYPE_DOUBLE,
    TYPE_UINT32,
    TYPE_STRING
};

struct ParamValue {
    union {
        bool b;
        uint8_t u8;
        unsigned long ul;
        double d;
        uint32_t u32;
        char s[64];
    };
};

struct ParamEntry {
    ParamType type;
    const char* key;
    ParamValue value;
};


class ConfigManager {
public:
    ConfigManager(const char* ns = "config") {
        strncpy(namespaceName, ns, sizeof(namespaceName));
        namespaceName[sizeof(namespaceName) - 1] = '\0';
    }

    void SetNamespace(const char* ns) {
        strncpy(namespaceName, ns, sizeof(namespaceName));
        namespaceName[sizeof(namespaceName) - 1] = '\0';
    }

    // Initialisation
    void initParam(int id, const char* key, bool defaultValue) {
        entries[id].type = TYPE_BOOL;
        entries[id].key = key;
        Preferences prefs;
        prefs.begin(namespaceName, false);
        entries[id].value.b = prefs.getBool(key, defaultValue);
        prefs.end();
    }

    void initParam(int id, const char* key, uint8_t defaultValue) {
        entries[id].type = TYPE_UINT8;
        entries[id].key = key;
        Preferences prefs;
        prefs.begin(namespaceName, false);
        entries[id].value.u8 = prefs.getUChar(key, defaultValue);
        prefs.end();
    }

    void initParam(int id, const char* key, unsigned long defaultValue) {
        entries[id].type = TYPE_ULONG;
        entries[id].key = key;
        Preferences prefs;
        prefs.begin(namespaceName, false);
        entries[id].value.ul = prefs.getULong(key, defaultValue);
        prefs.end();
    }

    void initParam(int id, const char* key, double defaultValue) {
        entries[id].type = TYPE_DOUBLE;
        entries[id].key = key;
        Preferences prefs;
        prefs.begin(namespaceName, false);
        entries[id].value.d = prefs.getDouble(key, defaultValue);
        prefs.end();
    }

    void initParam(int id, const char* key, uint32_t defaultValue) {
        entries[id].type = TYPE_UINT32;
        entries[id].key = key;
        Preferences prefs;
        prefs.begin(namespaceName, false);
        entries[id].value.u32 = prefs.getUInt(key, defaultValue);
        prefs.end();
    }

    void initParam(int id, const char* key, const char* defaultValue) {
        entries[id].type = TYPE_STRING;
        entries[id].key = key;
        Preferences prefs;
        prefs.begin(namespaceName, false);
        prefs.getString(key, entries[id].value.s, sizeof(entries[id].value.s));
        if (strlen(entries[id].value.s) == 0) {
            strncpy(entries[id].value.s, defaultValue, sizeof(entries[id].value.s));
            entries[id].value.s[sizeof(entries[id].value.s) - 1] = '\0';
        }
        prefs.end();
    }

    // Méthodes génériques GET
    template<typename T>
    T get(int id);

    // Méthodes génériques PUT
    template<typename T>
    bool put(int id, const T& inValue);

    void resetAll() {
        Preferences prefs;
        prefs.begin(namespaceName, false);
        prefs.clear();
        prefs.end();
    }

    void printAllParams() {
        Serial.println("=== Configuration Parameters ===");
        for (int i = 0; i < MAX_PARAMS; ++i) {
            const ParamEntry& entry = entries[i];
            if (entry.key == nullptr) continue;

            Serial.printf("[%d] %s = ", i, entry.key);

            switch (entry.type) {
                case TYPE_BOOL:
                    Serial.println(entry.value.b ? "true" : "false");
                    break;
                case TYPE_UINT8:
                    Serial.println(entry.value.u8);
                    break;
                case TYPE_ULONG:
                    Serial.println(entry.value.ul);
                    break;
                case TYPE_DOUBLE:
                    Serial.println(entry.value.d, 10);
                    break;
                case TYPE_UINT32:
                    Serial.println(entry.value.u32);
                    break;
                case TYPE_STRING:
                    Serial.println(entry.value.s);
                    break;
                default:
                    Serial.println("(unknown type)");
                    break;
            }
        }
        Serial.println("================================");
    }


private:
    char namespaceName[32];
    ParamEntry entries[MAX_PARAMS]; // Assuming a maximum of 50 parameters

    void saveToFlash(int id) {
        Preferences prefs;
        prefs.begin(namespaceName, false);
        const char* key = entries[id].key;

        switch (entries[id].type) {
            case TYPE_BOOL:
                prefs.putBool(key, entries[id].value.b);
                break;
            case TYPE_UINT8:
                prefs.putUChar(key, entries[id].value.u8);
                break;
            case TYPE_ULONG:
                prefs.putULong(key, entries[id].value.ul);
                break;
            case TYPE_DOUBLE:
                prefs.putDouble(key, entries[id].value.d);
                break;
            case TYPE_UINT32:
                prefs.putUInt(key, entries[id].value.u32);
                break;
            case TYPE_STRING:
                prefs.putString(key, entries[id].value.s);
                break;
        }

        prefs.end();
    }
};

// Spécialisations GET
template<> inline bool ConfigManager::get<bool>(int id) {
    return entries[id].type == TYPE_BOOL ? entries[id].value.b : false;
}

template<> inline uint8_t ConfigManager::get<uint8_t>(int id) {
    return entries[id].type == TYPE_UINT8 ? entries[id].value.u8 : 0;
}

template<> inline unsigned long ConfigManager::get<unsigned long>(int id) {
    return entries[id].type == TYPE_ULONG ? entries[id].value.ul : 0;
}

template<> inline double ConfigManager::get<double>(int id) {
    return entries[id].type == TYPE_DOUBLE ? entries[id].value.d : 0.0;
}

template<> inline uint32_t ConfigManager::get<uint32_t>(int id) {
    return entries[id].type == TYPE_UINT32 ? entries[id].value.u32 : 0;
}

template<> inline const char* ConfigManager::get<const char*>(int id) {
    return entries[id].type == TYPE_STRING ? entries[id].value.s : "";
}

// Spécialisations PUT
template<> inline bool ConfigManager::put<bool>(int id, const bool& val) {
    if (entries[id].type == TYPE_BOOL && entries[id].value.b != val) {
        entries[id].value.b = val;
        saveToFlash(id);
        return true;
    }
    return false;
}

template<> inline bool ConfigManager::put<uint8_t>(int id, const uint8_t& val) {
    if (entries[id].type == TYPE_UINT8 && entries[id].value.u8 != val) {
        entries[id].value.u8 = val;
        saveToFlash(id);
        return true;
    }
    return false;
}

template<> inline bool ConfigManager::put<unsigned long>(int id, const unsigned long& val) {
    if (entries[id].type == TYPE_ULONG && entries[id].value.ul != val) {
        entries[id].value.ul = val;
        saveToFlash(id);
        return true;
    }
    return false;
}

template<> inline bool ConfigManager::put<double>(int id, const double& val) {
    if (entries[id].type == TYPE_DOUBLE && entries[id].value.d != val) {
        entries[id].value.d = val;
        saveToFlash(id);
        return true;
    }
    return false;
}

template<> inline bool ConfigManager::put<uint32_t>(int id, const uint32_t& val) {
    if (entries[id].type == TYPE_UINT32 && entries[id].value.u32 != val) {
        entries[id].value.u32 = val;
        saveToFlash(id);
        return true;
    }
    return false;
}

template<> inline bool ConfigManager::put<const char*>(int id, const char* const& val) {
    if (entries[id].type == TYPE_STRING && strncmp(entries[id].value.s, val, sizeof(entries[id].value.s)) != 0) {
        strncpy(entries[id].value.s, val, sizeof(entries[id].value.s));
        entries[id].value.s[sizeof(entries[id].value.s) - 1] = '\0';
        saveToFlash(id);
        return true;
    }
    return false;
}


#endif // POOLMASTER_CONFIGMANAGER_H