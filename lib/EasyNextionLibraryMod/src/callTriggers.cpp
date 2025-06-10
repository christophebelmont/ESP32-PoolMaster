/*!
 * callTriggers.cpp - Easy library for Nextion Displays
 * Copyright (c) 2020 Athanasios Seitanis < seithagta@gmail.com >. 
 * All rights reserved under the library's licence
 */

/*! We made a separate file < callTriggers.cpp > for the function callTriggerFunction(), 
 *  as it would be too long to fit in the EasyNextionLibrary.cpp, because there are
 *  50 predefined cases for the triggers and it is easier for someone to edit them
 */

#ifndef EasyNextionLibrary_h
#include "EasyNextionLibrary.h"
#endif                        
                                
#ifndef trigger_h
#include "trigger.h"
#endif

void EasyNex::callTriggerFunction(){
  uint8_t _tempRead = 0;
 // _tempRead = _serial->read();  // We read the next byte, which, according to our protocol, is the < Trigger ID >
                                // From Nextion we send: < printh 23 02 54 xx >
                                // (where xx is the trigger id in HEX, 01 for 1, 02 for 2, ... 0A for 10 etc)
  switch(_tempRead){
    
    case 0x00:
      trigger0();
      break;    
    
    case 0x01:
      trigger1();
      break;

    case 0x02:
      trigger2();
      break;
           
    case 0x03:
      trigger3();
      break;
                 
    case 0x04:
      trigger4();
      break;    
                 
    case 0x05:
      trigger5();
      break;   
    
    case 0x06:
      trigger6();
      break;

    case 0x07:
      trigger7();
      break;
           
    case 0x08:
      trigger8();
      break;
                 
    case 0x09:
      trigger9();
      break;    
                 
    case 0x0A:
      trigger10();
      break;
    
    case 0x0B:
      trigger11();
      break;

    case 0x0C:
      trigger12();
      break;
           
    case 0x0D:
      trigger13();
      break;
                 
    case 0x0E:
      trigger14();
      break;    
                 
    case 0x0F:
      trigger15();
      break;   
    
    case 0x10:
      trigger16();
      break;

    case 0x11:
      trigger17();
      break;
           
    case 0x12:
      trigger18();
      break;
                 
    case 0x13:
      trigger19();
      break;    
                 
    case 0x14:
      trigger20();
      break; 

    case 0x15:
      trigger21();
      break;

    case 0x16:
      trigger22();
      break;
           
    case 0x17:
      trigger23();
      break;
                 
    case 0x18:
      trigger24();
      break;    
                 
    case 0x19:
      trigger25();
      break;   
    
    case 0x1A:
      trigger26();
      break;

    case 0x1B:
      trigger27();
      break;
           
    case 0x1C:
      trigger28();
      break;
                 
    case 0x1D:
      trigger29();
      break;    
                 
    case 0x1E:
      trigger30();
      break;
    
    case 0x1F:
      trigger31();
      break;

    case 0x20:
      trigger32();
      break;
           
    case 0x21:
      trigger33();
      break;
                 
    case 0x22:
      trigger34();
      break;    
                 
    case 0x23:
      trigger35();
      break;   
    
    case 0x24:
      trigger36();
      break;

    case 0x25:
      trigger37();
      break;
           
    case 0x26:
      trigger38();
      break;
                 
    case 0x27:
      trigger39();
      break;    
                 
    case 0x28:
      trigger40();
      break; 
    
    case 0x29:
      trigger41();
      break;

    case 0x2A:
      trigger42();
      break;
           
    case 0x2B:
      trigger43();
      break;
                 
    case 0x2C:
      trigger44();
      break;    
                 
    case 0x2D:
      trigger45();
      break;   
    
    case 0x2E:
      trigger46();
      break;

    case 0x2F:
      trigger47();
      break;
           
    case 0x30:
      trigger48();
      break;
                 
    case 0x31:
      trigger49();
      break;    
                 
    case 0x32:
      trigger50();
      break;
  }
}

void EasyNex::calltriggermenuFunction(){
  uint8_t _tempRead1,_tempRead2;
  _tempRead1 = _serial->read();  // We read the next byte, main menu (00) or sub menu (01)
  switch(_tempRead1){
    case 0x00:
      _tempRead2 = _serial->read();  // We read the next byte, menu ID
      triggermainmenu(_tempRead2);  // MAIN MENU
      break;    
    
    case 0x01:
      _tempRead2 = _serial->read();  // We read the next byte, menu ID
      triggersubmenu(_tempRead2); // SUB MENU
      break;

    case 0x02:
      triggermenu2();
      break;
           
    case 0x03:
      triggermenu3();
      break;
                 
    case 0x04:
      triggermenu4();
      break;    
                 
    case 0x05:
      triggermenu5();
      break;   
    
    case 0x06:
      triggermenu6();
      break;

    case 0x07:
      triggermenu7();
      break;
           
    case 0x08:
      triggermenu8();
      break;
                 
    case 0x09:
      triggermenu9();
      break;    
                 
    case 0x0A:
      triggermenu10();
      break;
    
    case 0x0B:
      triggermenu11();
      break;

    case 0x0C:
      triggermenu12();
      break;
           
    case 0x0D:
      triggermenu13();
      break;
                 
    case 0x0E:
      triggermenu14();
      break;    
                 
    case 0x0F:
      triggermenu15();
      break;   
    
    case 0x10:
      triggermenu16();
      break;

    case 0x11:
      triggermenu17();
      break;
           
    case 0x12:
      triggermenu18();
      break;
                 
    case 0x13:
      triggermenu19();
      break;    
                 
    case 0x14:
      triggermenu20();
      break; 

    case 0x15:
      triggermenu21();
      break;

    case 0x16:
      triggermenu22();
      break;
           
    case 0x17:
      triggermenu23();
      break;
                 
    case 0x18:
      triggermenu24();
      break;    
                 
    case 0x19:
      triggermenu25();
      break;   
    
    case 0x1A:
      triggermenu26();
      break;

    case 0x1B:
      triggermenu27();
      break;
           
    case 0x1C:
      triggermenu28();
      break;
                 
    case 0x1D:
      triggermenu29();
      break;    
                 
    case 0x1E:
      triggermenu30();
      break;
    
    case 0x1F:
      triggermenu31();
      break;

    case 0x20:
      triggermenu32();
      break;
           
    case 0x21:
      triggermenu33();
      break;
                 
    case 0x22:
      triggermenu34();
      break;    
                 
    case 0x23:
      triggermenu35();
      break;   
    
    case 0x24:
      triggermenu36();
      break;

    case 0x25:
      triggermenu37();
      break;
           
    case 0x26:
      triggermenu38();
      break;
                 
    case 0x27:
      triggermenu39();
      break;    
                 
    case 0x28:
      triggermenu40();
      break; 
    
    case 0x29:
      triggermenu41();
      break;

    case 0x2A:
      triggermenu42();
      break;
           
    case 0x2B:
      triggermenu43();
      break;
                 
    case 0x2C:
      triggermenu44();
      break;    
                 
    case 0x2D:
      triggermenu45();
      break;   
    
    case 0x2E:
      triggermenu46();
      break;

    case 0x2F:
      triggermenu47();
      break;
           
    case 0x30:
      triggermenu48();
      break;
                 
    case 0x31:
      triggermenu49();
      break;    
                 
    case 0x32:
      triggermenu50();
      break;
  }
}
