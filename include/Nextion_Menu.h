#ifndef POOLMASTER_NEXMENU_H
#define POOLMASTER_NEXMENU_H

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include "EasyNextionLibrary.h"
#include "EasyNextionMenus.h"
#include "EasyNextionEventManager.h"
#include "translation.h"           // Include all translated strings into flash


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
void SetValue(const char* , int = -1, int = -1, int = -1);
#endif