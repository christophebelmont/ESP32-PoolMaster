#ifndef POOLMASTER_NEXEVENTS_H
#define POOLMASTER_NEXEVENTS_H

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include "EasyNextionLibrary.h"
#include "EasyNextionMenus.h"
#include "EasyNextionEventManager.h"
#include "translation.h"           // Include all translated strings into flash

// MENU CUSTOM COMMANDS
// This is usefull for all the commands which dictates the action, the menu style and the configuration parameters
typedef enum EasyNextionCommandType { // IMPORTANT: Values not dynamic. Needs to be changed in Nextion Editor
    ENCT_PAGE         = 0x50,  // printh 23 02 50 XX YY (80)    // Already handled by EasyNextionEventManager but could be overloaded
    ENCT_MENU         = 0x4D,  // printh 23 02 4D XX (77)
    ENCT_COMMAND      = 0x53,  // printh 23 02 53 XX YY ZZ etc (83)
    ENCT_TXT_CMD      = 0x43,  // printh 23 02 43
    ENCT_GRAPH        = 0x47   // printh 23 02 47 XX
  } en_cmd_type_t;

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

typedef enum EasyNextionMenuCommands {  // IMPORTANT: Values not dynamic. Needs to be changed in Nextion Editor
    ENMC_FILT_MODE           = 0,    // printh 23 02 53 00 (toggle) | printh 23 03 53 00 00/01 (set)
    ENMC_PH_AUTOMODE         = 1,    // printh 23 02 53 01 (toggle) | printh 23 03 53 01 00/01 (set)
    ENMC_ORP_AUTOMODE        = 2,    // printh 23 02 53 02 (toggle) | printh 23 03 53 02 00/01 (set)
    ENMC_HEAT                = 3,
    ENMC_FILT_PUMP           = 4,
    ENMC_PH_PUMP             = 5,
    ENMC_CHL_PUMP            = 6,
    ENMC_HEATING             = 7,
    ENMC_WINTER_MODE         = 8,
    ENMC_LANGUAGE            = 9,
    ENMC_PH_PUMP_MENU        = 11,    // If value is BUTTON_AUTO=2 then change the automode if not 0 or 1 then set the pump state
    ENMC_CHL_PUMP_MENU       = 12,    // If value is BUTTON_AUTO=2 then change the automode if not 0 or 1 then set the pump state
    ENMC_FILT_PUMP_MENU      = 13,    // If value is BUTTON_AUTO=2 then change the automode if not 0 or 1 then set the pump state
    ENMC_HEAT_MENU           = 14,    // If value is BUTTON_AUTO=2 then change the automode if not 0 or 1 then set the pump state
    ENMC_SWG_MODE_MENU       = 15,    // If value is BUTTON_AUTO=2 then change the automode if not 0 or 1 then set the pump state
    ENMC_TANK_STATUS         = 16,    // Launch Fill page depending on which button user clicks
    ENMC_ROBOT               = 17,
    ENMC_LIGHTS              = 18,
    ENMC_SPARE               = 19,
    ENMC_CLEAR_ALARMS        = 20,    // No value
    ENMC_FILLING_PUMP        = 21,
    ENMC_SWG_REGULATION      = 22

  } enm_commands_t;

// FOR SETVALUE FUNCTION
#define SETVALUE_TOGGLE -1
#define SETVALUE_DIRECT -1

// NEXTION BUTTON RESULTS
#define BUTTON_OFF  0
#define BUTTON_ON   1
#define BUTTON_AUTO 2

// Controls Field first digit
#define ENM_CTRL_NONE     "-1"
#define ENM_CTRL_SLIDER    "0"
#define ENM_CTRL_ONOFF     "1"
#define ENM_CTRL_ONAUTOOFF "2"
#define ENM_CTRL_GAUGE     "3"

extern EasyNextionEventManager NexeventManager; // Create an instance of the event manager to handle events sent by Nextion
extern EasyNex myNex;

void NexEvents_Init();
//Event Handlers
void Nextion_Txt_Cmd_43(u_int8_t , u_int8_t (&)[10]);
void Nextion_Graph_47(u_int8_t , u_int8_t (&)[10]);
void Nextion_Menu_4D(u_int8_t , u_int8_t (&)[10]);
void Nextion_Command_53(u_int8_t , u_int8_t (&)[10]);


void SendCommand(const char* , int = -1, int = -1, int = -1);

#endif