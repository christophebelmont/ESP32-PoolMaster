/*
  NEXTION TFT related code, based on EasyNextion library by Seithan / Athanasios Seitanis (https://github.com/Seithan/EasyNextionLibrary)
  The trigger(s) functions at the end are called by the Nextion library on event (buttons, page change).

  Completely reworked and simplified. These functions only update Nextion variables. The display is then updated localy by the
  Nextion itself.

  Remove every usages of String in order to avoid duplication, fragmentation and random crashes.
  Note usage of snprintf_P which uses a fmt string that resides in program memory.
*/
#ifndef POOLMASTER_NEXTION_H
#define POOLMASTER_NEXTION_H

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include "EasyNextionMultiStream.h"
#include "Nextion_Menu.h"
#include "Nextion_Events.h"
#include "Nextion_Pages.h"
#include "translation.h"           // Include all translated strings into flash
#include "HistoryStats.h"
#include "uptime.h"


#define GLOBAL  "globals" // Name of Nextion page to store global variables

// Used for tasks
void stack_mon(UBaseType_t&);

// Functions prototypes
void ResetTFT(void);
void syncESP2RTC(uint32_t , uint32_t , uint32_t , uint32_t , uint32_t , uint32_t );
void syncRTC2ESP(void);
void UpdateTFT(void*);
void WriteSwitches(void);


#endif