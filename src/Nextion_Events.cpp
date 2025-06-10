/*
  NEXTION EVENTS TFT related code, based on EasyNextion library by Seithan / Athanasios Seitanis.
  Used describe all the events and actions that are triggered by the Nextion display.
  The trigger(s) functions at the end are called by the Nextion library on event (buttons, page change).
    Completely reworked and simplified. These functions only update Nextion variables. The display is then updated localy by the
    Nextion itself.
*/
#include "Nextion_Events.h"

EasyNextionEventManager NexeventManager; // Create an instance of the event manager to handle events sent by Nextion

/**
 * @brief Initialize the Nextion events system.
 * @param _event_index The index of the event to .
 */
void NexEvents_Init(EasyNex& _myNex)
{
    //NexeventManager.registerEvent(ENMC_FILT_MODE, [](int eventRef) {
        // Handle the event for filtration mode
        // Add your code here
    //});

}

/* All Nextion page events handlers */
void page_ENMP_PH_REGULATION(EasyNex& _myNex) {

}


