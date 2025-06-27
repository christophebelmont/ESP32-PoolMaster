#ifndef POOLMASTER_NEXMENU_H
#define POOLMASTER_NEXMENU_H

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include "EasyNextionLibrary.h"
#include "EasyNextionMenus.h"
#include "EasyNextionEventManager.h"
#include "translation.h"           // Include all translated strings into flash

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

typedef enum EasyNextionMenuPages { // IMPORTANT: These values are dynamic. They are set in the code end send to Nextion
  ENMP_PH_REGULATION        = 60,   // printh 23 02 50 3C
  ENMP_ORP_REGULATION,      // 61
  ENMP_FILTRATION,          // 62
  ENMP_HEAT_REGULATION,     // 63
  ENMP_PSI_OPTIONS,         // 64
  ENMP_SWG_MODES,           // 65
  ENMP_TANK_STATUS,         // 66
  ENMP_RELAYS_CONTROL,      // 67
  ENMP_WATER_LEVEL,         // 68
  ENMP_SWG_REGULATION,      // 69

  ENMP_EMPTY_CONTROLS       = 90 // Not dynamic. Default value for empty controls page (before page is completed with actual values)
} enm_pages_t;

static bool MainMenuLoaded = false;
static bool SubMenuLoaded = false;

static char temp[64];
static char temp_command[64];

extern EasyNextionMenus  MainMenu;
extern EasyNextionMenus  SubMenu1;
extern EasyNextionMenus  SubMenu2;
extern EasyNextionMenus  SubMenu3;
extern EasyNextionMenus  SubMenu4;
extern EasyNextionMenus  SubMenu5;
extern EasyNextionMenus  SubMenu6;
extern EasyNextionMenus  SubMenu7;
extern EasyNextionMenus  SubMenu8;
extern EasyNextionMenus  SubMenu9;
extern EasyNextionMenus  SubMenu10;
extern EasyNextionMenus  SubMenu11;
extern EasyNextionMenus  SubMenu12;

void NexMenu_Loop(EasyNex&);
void NexMenu_Init(EasyNex&);
#endif