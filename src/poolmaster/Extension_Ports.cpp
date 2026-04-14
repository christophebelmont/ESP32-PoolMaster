
#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)

// *************************
// Addons on Extension Ports
// *************************
// Check GPIO IDs possibilities and limitations 
// https://www.upesy.fr/blogs/tutorials/esp32-pinout-reference-gpio-pins-ultimate-guide
//
// GPIOs Available :
//  GPIO O5   --> Input+Output + pullup/down
//  GPIO 12   --> Input+Output + pullup/down, must be a 0V at boot time
//  GPIO 14   --> Input+Output + pullup/down
//  GPIO 15   --> Input(pullup) + output 
//  GPIO 35   --> Input only, no pullup/pulldown
//  I2C       --> Address

#include "Extension_Ports.h"
#include "Extension_SuperVisor.h"
#include "Extension_TFAVenice_RF433T.h"
#include "Extension_WaterMeter_Pulse.h"
#include "Extension_BME680_0x77.h"
#include "Extension_BMP280_0x76.h" 
#include "Extension_SHT40_0x44.h"
#include "Extension_INA226_0x40.h"

#define _GPIO_TFA_RF433T_        0 // disable by default, suggest GPIO 5  - Water Temperature broadcasted to TFA 433Mhz receiver
#define _GPIO_WATERMETER_PULSE_  0 // disable by default, suggest GPIO 15 - Count WaterMeter pulse from external reader.
#define _I2C_                    0
#define _OTHER_                 -1
#define LOADSETTINGSFREQUENCY   2000 // check every xxx ms if SuperVisor has new settings (wifi, mqtt, watermeter, etc)

struct ListExtensions knownI2C[] = {
    {"SUPERVISOR",  0, SUPERVISOR_I2C_Address},  // 0x07
    {"PCF8574",     0, 0x20 },  // PoolMaster StatusLights
    {"PCF8574A",    0, 0x38 },  // PoolMaster StatusLights
    {"INA226",      0, 0x40 },  // PowerMeter
    {"SHT40",       0, 0x44 },  // ENV IV M5Stack = SHT40 + BMP280
    {"ADS1115",     0, INT_ADS1115_ADDR },  // PoolMaster AnalogPoll
    {"EXT_ADS1115", 0, EXT_ADS1115_ADDR },  // PoolMaster Loulou74 board
    {"BMP280",      0, 0x76 },  // ENV IV M5Stack = SHT40 + BMP280
    {"BME680",      0, 0x77 },  // ENV Pro M5Stack
};

// *******************************************
// * This is where we declare our extensions *
// * And all monitored Loops (functions)
// *******************************************
struct ListExtensions myListExtensions[] = {
    // NAME (max 15chars), INIT_FUNCTION,  PORT, SETTINGS STACK SIZE, TASK STACK SIZE
    {"SuperVisor",  SuperVisor_Init,        _I2C_   , 2250, 4096}, // Talk to SuperVisor via I2C
    {"SHT40",       SHT40_0x44_Init,        _I2C_   , 0, 2225},
    {"BMP280",      BMP280_0x76_Init,       _I2C_   , 0, 2250},
    {"BME680",      BME680_0x77_Init,       _I2C_   , 0, 2250},
    {"WaterMeter",  WaterMeterPulse_Init,   _GPIO_WATERMETER_PULSE_, 2250, 1800 },
    {"TFA_Venice",  TFAVenice_RF433T_Init,  _GPIO_TFA_RF433T_ ,      2250, 2048 },
    {"INA226",      INA226_0x40_Init,       _I2C_   , 0, 2250},
   // Futur extensions
//    {"MiLight",        MiLight_Init,           _I2C_ },
//    {"PoolCover",      PoolCover_Init,         _OTHER_ },
//    {"HeatPump",   PoolHeatPump_Init,      _OTHER_ },

    //  Monitor free stack (high watermark) of all PoolMaster Loops
    {"loopTask",        noInit,           0       , 0   , 0}, // the main loop
    {"AnalogPoll",      noInit,           0       , 0   , 0},
    {"ProcessCommand",  noInit,           0       , 0   , 0},
    {"PoolMaster",      noInit,           0       , 0   , 0},
    {"GetTemp",         noInit,           0       , 0   , 0},
    {"ORPRegulation",   noInit,           0       , 0   , 0},
    {"pHRegulation",    noInit,           0       , 0   , 0},
    {"StatusLights",    noInit,           0       , 0   , 0},
    {"MeasuresPublish", noInit,           0       , 0   , 0},
    {"SettingsPublish", noInit,           0       , 0   , 0},
    {"UpdateTFT",       noInit,           0       , 0   , 0},
    {"HistoryStats",    noInit,           0       , 0   , 0},
};
// *******************************************

void stack_mon(UBaseType_t &);
ExtensionStruct *myExtensions = 0;
int _NbExtensions = 0;

// Removes the duplicates "/" from the string provided as an argument
static void mqttRemoveDoubleSlash(char *str)
{
  int i,j,len,len1;
  /*calculating length*/
  for(len=0; str[len]!='\0'; len++);
  /*assign 0 to len1 - length of removed characters*/
  len1=0;
  /*Removing consecutive repeated characters from string*/
  for(i=0; i<(len-len1);) {
      if((str[i]==str[i+1]) && (str[i] == '/')) {
          /*shift all characters*/
          for(j=i;j<(len-len1);j++)
              str[j]=str[j+1];
          len1++;
      }
      else i++;
  }
}

char *ExtensionsCreateMQTTTopic(const char *t1, const char *t2)
{   
    const char *thetopic = PMConfig.get<const char*>(MQTT_TOPIC);
    char *topic = (char*)malloc(strlen(thetopic) + strlen(t1) + strlen(t2) + 1);
    sprintf(topic, "%s/%s%s", thetopic, t1, t2);
    return topic;
}


/* **********************************
 * Init All Extensions and run loops
 * **********************************
*/

void ExtensionsLoadSettings(void *pvParameters)
{
    ExtensionStruct *TheExtension = (ExtensionStruct *)pvParameters;

    if (!TheExtension->detected)     return;
    if (!TheExtension->LoadSettings) return;

    vTaskDelay(1000 / portTICK_PERIOD_MS); // Scheduling offset
    while (!startTasks) delay(200);

    for (;;) {
        TheExtension->LoadSettings(pvParameters);
        vTaskDelay(pdMS_TO_TICKS(LOADSETTINGSFREQUENCY));
    }
}

void ExtensionsLoop(void *pvParameters)
{
    ExtensionStruct *TheExtension = (ExtensionStruct *)pvParameters;

    if (!TheExtension->detected)      return;
    if (!TheExtension->Task)          return;
    if (TheExtension->frequency == 0) return;
    if (TheExtension->frequency < 0)  return;

    vTaskDelay(1500 / portTICK_PERIOD_MS); // Scheduling offset
    while (!startTasks) delay(200);

    for (;;) {
        TheExtension->Task(pvParameters);
        vTaskDelay(pdMS_TO_TICKS(TheExtension->frequency));
    }
}

const char *searchI2CName(int address)
{
    int nb = sizeof(knownI2C)/sizeof(knownI2C[0]);
    for (int i=0; i<nb; i++)
       if (address == knownI2C[i].port) return knownI2C[i].name;
    return "UNKNOWN";
}

void ExtensionsInit()
{
    // https://i2cdevices.org/addresses/
    Debug.print(DBG_INFO, "Scanning I2C bus...");
    byte error, address;
    for (address = 0x01; address < 0x7f; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) Debug.print(DBG_INFO,"\tI2C device %s found at address 0x%02X", searchI2CName(address), address); 
        else if (error == 4) Debug.print(DBG_INFO,"\tI2C device unknown, error at address 0x%02X", address);
    }
    Debug.print(DBG_INFO, "...done");
    if (sizeof(myListExtensions[0]))
        _NbExtensions = sizeof(myListExtensions)  / sizeof(myListExtensions[0]);
    Debug.print(DBG_INFO, "[Extensions Init] with %d entries", _NbExtensions);

    if (_NbExtensions) myExtensions = (ExtensionStruct*) malloc(_NbExtensions * sizeof(ExtensionStruct));

    for (int i = 0; i < _NbExtensions; i++) {
        myExtensions[i] = myListExtensions[i].init(myListExtensions[i].name, myListExtensions[i].port);
        if (myExtensions[i].detected) {
            char taskname[17]; // MAX 16
            if (myExtensions[i].LoadSettings) {
            sprintf(taskname, "%sS", myExtensions[i].name);
            xTaskCreatePinnedToCore(
                ExtensionsLoadSettings,
                taskname,
                myListExtensions[i].settingsStackHighWaterMark,
                &myExtensions[i],
                1,
                NULL, //&myListExtensions[i].settingsHandle,
                0); // run extentions from the other core
            }
            if (myExtensions[i].Task) {
            sprintf(taskname, "%s", myExtensions[i].name);
            xTaskCreatePinnedToCore(
                ExtensionsLoop,
                taskname,
                myListExtensions[i].taskStackHighWaterMark,
                &myExtensions[i],
                1,
                NULL, //&myListExtensions[i].taskHandle,
                0); // run extentions from the other core
            }
        }
    }
}
/*
void ExtensionsPublishSettings(void *pvParameters)
{
    for (int i = 0; i < _NbExtensions; i++)
    {
        if (myExtensions[i].SaveSettings) {
            myExtensions[i].SaveSettings(pvParameters);
            SANITYDELAY;
        }
    }
}

void ExtensionsPublishMeasures(void *pvParameters)
{
    for (int i = 0; i < _NbExtensions; i++) {
        if (myExtensions[i].SaveMeasures) {
            myExtensions[i].SaveMeasures(pvParameters);
            SANITYDELAY;
        }
    }
}

void ExtensionsHistoryStats(void *pvParameters)
{
    for (int i = 0; i < _NbExtensions; i++) {
        if (myExtensions[i].HistoryStats) {
            myExtensions[i].HistoryStats(pvParameters);
            SANITYDELAY;
        }
    }
}
*/
int ExtensionsNb()
{
    return _NbExtensions;
}

#endif