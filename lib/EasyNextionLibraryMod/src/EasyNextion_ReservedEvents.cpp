/*!
 * EasyNextion_ReservedEvents.cpp - Easy library for Nextion Displays - Internal events which does not need to be registered by the user (like page management)
 * Copyright (c) 2020 Christophe Belmonnt < christophe.belmont@gmail.com >. 
 * All rights reserved under the library's licence
 */

 #include "EasyNextionLibrary.h"
 
 void EasyNex::readReservedEvents(u_int8_t eventRef, u_int8_t paramLength, u_int8_t (&eventData)[10]) {

    switch(eventRef){
        case 'P': /*or <case 0x50:>  If 'P' matches, we have the command group "Page". 
                *The next byte is the page <Id> according to our protocol.
                *
                * We have write in every page's "preinitialize page" the command
                *  printh 23 02 50 xx (where xx is the page id in HEX, 00 for 0, 01 for 1, etc).
                * <<<<Every event written on Nextion's pages preinitialize page event will run every time the page is Loaded>>>>
                *  it is importand to let the Arduino "Know" when and which Page change.
                */
        lastCurrentPageId = currentPageId;
        currentPageId = eventData[0];   // First argument is the page id
        pageChangedAction = false;  // First action after page change is not yet done
        // Indicate that Nextion is not sleeping anymore
        if(Nextion_Sleeping) {
            Nextion_Sleeping = false;
        }
        Nextion_CanSleep = true;
        break;
      
    }
}