/*!
 * EasyNextionMenus.cpp - Easy library for building menus on Nextion Displays
 * Copyright (c) 2025 Christophe Belmont
 * All rights reserved under the library's licence
 */

#include "EasyNextionMenus.h"

EasyNextionMenus::EasyNextionMenus(EasyNex* _nextion_display, int _max_number_of_items, int _menu_type)
{
    nextion_display = _nextion_display;
    menu_type = _menu_type;
    max_number_of_items = _max_number_of_items;
}

EasyNextionMenus::EasyNextionMenus(int _max_number_of_items, int _menu_type)
{
    menu_type = _menu_type;
    max_number_of_items = _max_number_of_items;
}


void EasyNextionMenus::SetNextionScreen(EasyNex* _nextion_display)
{
    nextion_display = _nextion_display;
}

void EasyNextionMenus::SetNxtObjectName(const char* _object_name)
{
    menu_nxt_obj_name = _object_name;
}

void EasyNextionMenus::SetNxtCheckObjectName(const char* _check_object_name)
{
    menu_nxt_check_name = _check_object_name;
}

int EasyNextionMenus::AddItem(void (*_CallBackFunction)(),bool (*_checkEnabledFunction)(),const char* _menu_item,const char* _icon_off,const char* _icon_on, int _menu_item_type)
{
    menu_items[number_of_recorded_items].mnu_item_name = _menu_item;
    menu_items[number_of_recorded_items].mnu_icon_off = _icon_off;
    menu_items[number_of_recorded_items].mnu_icon_on = _icon_on;
    menu_items[number_of_recorded_items].mnu_item_type = _menu_item_type;
    menu_items[number_of_recorded_items].callBackFunction = _CallBackFunction;
    menu_items[number_of_recorded_items].checkEnabledFunction = _checkEnabledFunction;
    number_of_recorded_items++;
    return (number_of_recorded_items-1);
}

int EasyNextionMenus::AddItem(void (*_CallBackFunction)(),bool (*_checkEnabledFunction)(),const char* _menu_item, const char* _icon_off,const char* _icon_on,int _menu_item_type,bool (*_checkStatusFunction)())
{
    menu_items[number_of_recorded_items].checkStatusFunction = _checkStatusFunction;
    return AddItem(_CallBackFunction,_checkEnabledFunction,_menu_item,_icon_off,_icon_on,_menu_item_type);
}
int EasyNextionMenus::AddItem(void (*_CallBackFunction)(),bool (*_checkEnabledFunction)(),const char* _menu_item, const char* _icon_off,const char* _icon_on,EasyNextionMenus* _easy_nextion_menu)
{
    menu_items[number_of_recorded_items].submenu = _easy_nextion_menu;
    return AddItem(_CallBackFunction,_checkEnabledFunction,_menu_item,_icon_off,_icon_on,ENM_SUBMENU);
}
int EasyNextionMenus::AddItem(void (*_CallBackFunction)(),bool (*_checkEnabledFunction)(),const char* _menu_item, const char* _icon_off,const char* _icon_on,int _menu_item_type, int _menu_item_action_ref, int _menu_item_overlay_index)
{
    menu_items[number_of_recorded_items].mnu_action_reference = _menu_item_action_ref;
    menu_items[number_of_recorded_items].mnu_overlay_index = _menu_item_overlay_index;
    return AddItem(_CallBackFunction,_checkEnabledFunction,_menu_item,_icon_off,_icon_on,_menu_item_type);
}
int EasyNextionMenus::AddItem(void (*_CallBackFunction)(),bool (*_checkEnabledFunction)(),const char* _menu_item, const char* _icon_off,const char* _icon_on,int _menu_item_type, const char* _menu_overlay_page_name)
{
    menu_items[number_of_recorded_items].mnu_overlay_page_name = _menu_overlay_page_name;
    return AddItem(_CallBackFunction,_checkEnabledFunction,_menu_item,_icon_off,_icon_on,_menu_item_type);
}

void EasyNextionMenus::Reinitialize()
{
    number_of_recorded_items=0;
}

Menu_Item *EasyNextionMenus::GetItemRef(int _item_number)
{
    return &menu_items[_item_number];
}
Menu_Item *EasyNextionMenus::GetItemRef(void)
{
    return &menu_items[selected_item];
}
void EasyNextionMenus::MenuDisplay(bool _full_menu_drawing)
{
    ///////////// MAIN MENU //////////////
    if(menu_type==ENM_MAIN)   // Main Menu
    {
        // Check menu configuration completeness
        if (menu_nxt_obj_name == nullptr) {
            menu_nxt_obj_name = MENU_MAIN_OBJECT_TXT;
        }        
        for(int i=0;i<number_of_recorded_items;i++)
        {
            // Check if menu type is different from ENM_NONE
            if(menu_items[i].mnu_item_type != ENM_NONE) 
            {
                if(_full_menu_drawing) { 
                    WriteNextionItemVisible(menu_nxt_obj_name,i,MNU_YES);            
                    WriteNextionItemStr(menu_nxt_obj_name,i,menu_items[i].mnu_item_name);
                }
            } else  // if type ENM_NONE hide menu
            {
                if(_full_menu_drawing) { 
                    WriteNextionItemVisible(menu_nxt_obj_name,i,MNU_NO);
                }
            }
        }
    }
    ///////////// SUB MENU //////////////
    if(menu_type==ENM_SUB)
    {
        // Check menu configuration completeness
        if (menu_nxt_obj_name == nullptr) {
            menu_nxt_obj_name = MENU_SUB_OBJECT_TXT;
        }    
        if (menu_nxt_check_name == nullptr) {
            menu_nxt_check_name = MENU_SUB_OBJECT_CHK;
        }    

        if (menu_nxt_sep_name == nullptr) {
            menu_nxt_sep_name = MENU_SUB_OBJECT_SEP;
        }    

        for(int i=0;i<max_number_of_items;i++) // SubMenu
        {
            // Menu Item Type instructs how to draw the menu item
            switch(menu_items[i].mnu_item_type){
                case ENM_NONE:
                    if(_full_menu_drawing) {    // Items that should change only when left item selected and first drawn (not at each loop, no runtime update)
                        WriteNextionItemVisible(menu_nxt_obj_name,i,MNU_NO);
                        WriteNextionItemVisible(menu_nxt_check_name,i,MNU_NO);
                        WriteNextionItemVisible(menu_nxt_sep_name,i,MNU_NO);
                    }
                break; 
                case ENM_BISTABLE:
                    if(_full_menu_drawing) {    // Items that should change only when left item selected and first drawn (not at each loop, no runtime update)
                        WriteNextionItemVisible(menu_nxt_obj_name,i,MNU_YES);
                        WriteNextionItemVisible(menu_nxt_check_name,i,MNU_YES);
                        WriteNextionItemVisible(menu_nxt_sep_name,i,MNU_YES);
                        WriteNextionItemStr(menu_nxt_obj_name,i,menu_items[i].mnu_item_name);
                    }

                    // CHECK THE STATUS OF THE BISTABLE MENU ELEMENT AND UPDATE STATUS
                    if(menu_items[i].checkStatusFunction != nullptr && menu_items[i].checkStatusFunction()) {
                            WriteNextionItemNum(menu_nxt_obj_name,i,MNU_CHECK);
                            WriteNextionItemNum(menu_nxt_check_name,i,MNU_CHECK);
                            WriteNextionItemStr(menu_nxt_check_name,i,menu_items[i].mnu_icon_on); // Checked
                    } else {
                            WriteNextionItemNum(menu_nxt_obj_name,i,MNU_UNCHECK);
                            WriteNextionItemNum(menu_nxt_check_name,i,MNU_UNCHECK);
                            WriteNextionItemStr(menu_nxt_check_name,i,menu_items[i].mnu_icon_off); // Unchecked
                    }
                break; 
                case ENM_ACTION:
                    if(_full_menu_drawing) {    // Items that should change only when selected (not at each loop)
                        WriteNextionItemVisible(menu_nxt_obj_name,i,MNU_YES);
                        WriteNextionItemVisible(menu_nxt_check_name,i,MNU_YES); 
                        WriteNextionItemVisible(menu_nxt_sep_name,i,MNU_YES); 
                        WriteNextionItemStr(menu_nxt_obj_name,i,menu_items[i].mnu_item_name);
                        WriteNextionItemNum(menu_nxt_obj_name,i,MNU_UNCHECK);
                        WriteNextionItemNum(menu_nxt_check_name,i,MNU_UNCHECK);
                        WriteNextionItemStr(menu_nxt_check_name,i,menu_items[i].mnu_icon_off); // Standard Menu Icon
                    }
                break;
                case ENM_OVERLAYPAGE:   // Load an overlay pge if this menu is selected
                    if(_full_menu_drawing) {    // Items that should change only when selected (not at each loop)
                        WriteNextionItemCmd(menu_items[i].mnu_overlay_page_name,-1,"page",-1);
                    }
                break;
            }

            // Check if menu should be greyed out
            if(menu_items[i].mnu_item_type != ENM_OVERLAYPAGE) {
                if(menu_items[i].checkEnabledFunction != nullptr && !menu_items[i].checkEnabledFunction()) {
                    // Grey Out if the function returns false (leave as this if it returns true)
                    WriteNextionItemEnabled(menu_nxt_obj_name,i,MNU_NO);
                    WriteNextionItemEnabled(menu_nxt_check_name,i,MNU_NO); 
                    WriteNextionItemAttribute(menu_nxt_obj_name,i,"pco",48599);
                    WriteNextionItemAttribute(menu_nxt_check_name,i,"pco",48599);
                } else {
                    // Grey Out if the function returns false (leave as this if it returns true)
                    WriteNextionItemEnabled(menu_nxt_obj_name,i,MNU_YES);
                    WriteNextionItemEnabled(menu_nxt_check_name,i,MNU_YES); 
                    WriteNextionItemAttribute(menu_nxt_obj_name,i,"pco",14890);
                    WriteNextionItemAttribute(menu_nxt_check_name,i,"pco",14890);
                }
            }
        }
    }
}

void EasyNextionMenus::Refresh()
{
    if(menu_items[selected_item].submenu != nullptr) {
        menu_items[selected_item].submenu->MenuDisplay(false);
    } 
}

void EasyNextionMenus::Select(uint8_t _menu_item_index)
{
    if(menu_items[_menu_item_index].mnu_item_type != ENM_NONE) {
        selected_item = _menu_item_index;
    }

    if (menu_type==ENM_MAIN) {
        if(menu_items[selected_item].submenu != nullptr) {
            menu_items[selected_item].submenu->MenuDisplay(true);
        }
    }
    if (menu_type==ENM_SUB) {
        switch(menu_items[selected_item].mnu_item_type){
            case ENM_ACTION:
                if(menu_items[selected_item].mnu_action_reference >= 0) {
                    WriteNextionItemNum(MENU_ACTION_OBJECT_CLICK,-1,menu_items[selected_item].mnu_action_reference);
                    WriteNextionItemNum(MENU_ACTION_NEXT_PAGE_OBJECT,-1,menu_items[selected_item].mnu_overlay_index);
                    WriteNextionExecute("MenuActions");  // Click on button Menu Action
                }
            break;
        }

        // If we have a callback action then execute it
        if(menu_items[selected_item].callBackFunction != nullptr) {
            menu_items[selected_item].callBackFunction();
        }
    }
}


/*****************************************************
 ********** HELPER FUNCTIONS *************************
 *****************************************************/
void EasyNextionMenus::WriteNextionItemVisible(const char * _nextion_object,int _object_index, int _visibility_status)
{
    WriteNextionItemCmd(_nextion_object,_object_index,"vis",_visibility_status);
}

void EasyNextionMenus::WriteNextionItemEnabled(const char * _nextion_object,int _object_index, int _enable_status)
{
    WriteNextionItemCmd(_nextion_object,_object_index,"tsw",_enable_status);
}

void EasyNextionMenus::WriteNextionItemStr(const char * _nextion_object,int _object_index, const char* _text_to_write)
{
    WriteNextionItemAttribute(_nextion_object,_object_index, "txt",_text_to_write);
}   

void EasyNextionMenus::WriteNextionItemNum(const char * _nextion_object,int _object_index, int _number_to_write)
{
    WriteNextionItemAttribute(_nextion_object,_object_index, "val",_number_to_write);
}

// Click on a button object (this is how function can be simulated with Nextion)
void EasyNextionMenus::WriteNextionExecute(const char * _nextion_object)
{
    WriteNextionItemCmd(_nextion_object,-1,"click",1);
}

// Write Nextion Command
// _nextion_object : Name of the objects part of the menu
// _object_index : if >=0, append "_index" with the actual menu item, do not append anything
// Ex: "MainMenu", 1, "vis", 1
//  ==> "vis MainMenu_1, 1"
void EasyNextionMenus::WriteNextionItemCmd(const char * _nextion_object,int _object_index,const char* _cmd_to_execute,int _value)
{
    char _menu__display_cmd[MAX_NEXT_CMD_LENGTH];
    if(_object_index>=0){
        if(_value>=0){
            snprintf_P(_menu__display_cmd,sizeof(_menu__display_cmd),PSTR("%s %s_%d,%d"),_cmd_to_execute,_nextion_object,_object_index,_value);
        }else{
            snprintf_P(_menu__display_cmd,sizeof(_menu__display_cmd),PSTR("%s %s_%d"),_cmd_to_execute,_nextion_object,_object_index);
        }
    } else {
        if(_value>=0){
            snprintf_P(_menu__display_cmd,sizeof(_menu__display_cmd),PSTR("%s %s,%d"),_cmd_to_execute, _nextion_object,_value);
        } else {
            snprintf_P(_menu__display_cmd,sizeof(_menu__display_cmd),PSTR("%s %s"),_cmd_to_execute, _nextion_object);
        }
    }
    //Serial.println(_menu__display_cmd);
    nextion_display->writeStr(_menu__display_cmd);
}   

// Write Nextion Attribute
// _nextion_object : Name of the objects part of the menu
// _object_index : if >=0, append "_index" with the actual menu item, if negative, do not append anything
// _attribute: Attribute of the object which should be modified
// _value: value to write which can be a char* or an int depending on the attribute
// Ex: "MainMenu", 4, "val", 1
//  ==> "MainMenu_4.val=1"
void EasyNextionMenus::WriteNextionItemAttribute(const char * _nextion_object,int _object_index,const char* _attribute,int _value)
{
    char _menu__display_cmd[MAX_NEXT_CMD_LENGTH];
    if(_object_index>=0){
        snprintf_P(_menu__display_cmd,sizeof(_menu__display_cmd),PSTR("%s_%d.%s"),_nextion_object,_object_index,_attribute);
    } else {
        snprintf_P(_menu__display_cmd,sizeof(_menu__display_cmd),PSTR("%s.%s"),_nextion_object,_attribute);
    }
    nextion_display->writeNum(_menu__display_cmd,_value);
}   
void EasyNextionMenus::WriteNextionItemAttribute(const char * _nextion_object,int _object_index,const char* _attribute,const char* _value)
{
    char _menu__display_cmd[MAX_NEXT_CMD_LENGTH];
    if(_object_index>=0){
        snprintf_P(_menu__display_cmd,sizeof(_menu__display_cmd),PSTR("%s_%d.%s"),_nextion_object,_object_index,_attribute);
    } else {
        snprintf_P(_menu__display_cmd,sizeof(_menu__display_cmd),PSTR("%s.%s"),_nextion_object,_attribute);
    }
    nextion_display->writeStr(_menu__display_cmd,_value);
}   

