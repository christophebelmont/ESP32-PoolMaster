#ifndef POOLMASTER_NEXPAGES_H
#define POOLMASTER_NEXPAGES_H

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include "EasyNextionLibrary.h"
#include "EasyNextionMenus.h"
#include "EasyNextionEventManager.h"
#include "translation.h"           // Include all translated strings into flash

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define MAX_SHOWN_NETWORKS  15  // Maximum number of scanned networks to display

static unsigned long LastWifiScan = 0; // Last Wifi Networks Scan

static uint8_t Current_Language = 0;
const char compile_date[] = __DATE__ " " __TIME__;

static unsigned long LastUpdatedHome = 0; // Last Time Home Page Updated

extern void DisconnectFromWiFi(bool);
extern void reconnectToWiFi(void);

typedef enum EasyNextionPages { // IMPORTANT: Values not dynamic. Needs to be changed in Nextion Editor
    ENP_SPLASH        = 1,  // printh 23 02 50 01
    ENP_HOME          = 2,  // printh 23 02 50 02
    ENP_MENU          = 3,  // printh 23 02 50 03
    ENP_CALIB         = 4,  // printh 23 02 50 04
    ENP_KEYPAD        = 5,  // printh 23 02 50 05
    ENP_WIFI_SCAN     = 6,  // printh 23 02 50 06
    ENP_ELECTROLYSE   = 7,  // printh 23 02 50 07
    ENP_NEWTANK       = 8,  // printh 23 02 50 08
    ENP_INFO          = 9,  // printh 23 02 50 09
    ENP_DATETIME      = 10, // printh 23 02 50 0A
    ENP_WIFI_CONFIG   = 11, // printh 23 02 50 0B
    ENP_GRAPH         = 13, // printh 23 02 50 0D
    ENP_HOME_SWITCH   = 14, // printh 23 02 50 0E
    ENP_LANGUAGE      = 15, // printh 23 02 50 0F
    ENP_MQTT_CONFIG   = 16, // printh 23 02 50 10
    ENP_ALERTS        = 17, // printh 23 02 50 11
    ENP_SMTP_CONFIG   = 18, // printh 23 02 50 12
    ENP_PINS_CONFIG   = 19, // printh 23 02 50 13
    ENP_HELP_POPUP    = 20, // printh 23 02 50 14
    ENP_HOME800       = 21, // printh 23 02 50 15
    ENP_H800_OV1      = 22  // printh 23 02 50 16
  } en_pages_t;

uint8_t NexPages_Loop(EasyNex&);
void ScanWiFiNetworks(void);
void printScannedNetworks(uint16_t);
void printLanguages(void);
double map(double, double, double, int, int);
char map(int, int, int, int, int);

#endif // POOLMASTER_NEXPAGES_H