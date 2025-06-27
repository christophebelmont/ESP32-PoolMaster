/*
  NEXTION EVENTS TFT related code, based on EasyNextion library by Seithan / Athanasios Seitanis.
  Used describe all the events and actions that are triggered by the Nextion display.
  The trigger(s) functions at the end are called by the Nextion library on event (buttons, page change).
    Completely reworked and simplified. These functions only update Nextion variables. The display is then updated localy by the
    Nextion itself.
*/
#include "Nextion.h"
#include "Nextion_Events.h"

EasyNextionEventManager NexeventManager; // Create an instance of the event manager to handle events sent by Nextion

// Initialize the Nextion events manager by registering even handlers
void NexEvents_Init()
{
    NexeventManager.registerEvent(ENCT_COMMAND,Nextion_Command_53);
    NexeventManager.registerEvent(ENCT_MENU,Nextion_Menu_4D);
    NexeventManager.registerEvent(ENCT_TXT_CMD,Nextion_Txt_Cmd_43);
    NexeventManager.registerEvent(ENCT_GRAPH,Nextion_Graph_47);
}

/* All Nextion page events handlers *
/***********************************/
// Menu command 43 or 'C'
void Nextion_Txt_Cmd_43(u_int8_t eventLength, u_int8_t (&eventData)[10])
{
    char Cmd[200] = "";
    strcpy(Cmd,myNex.readStr(F(GLOBAL".vaCommand.txt")).c_str());
    xQueueSendToBack(queueIn,&Cmd,0);
    Debug.print(DBG_INFO,"Nextion direct command 43: %s",Cmd);
}

// Menu command 47 or 'G'
void Nextion_Graph_47(u_int8_t eventLength, u_int8_t (&eventData)[10])
{
    int graphIndex = myNex.readByte();
    Debug.print(DBG_INFO,"Nextion graph command 47: %d",graphIndex);
    uint8_t _lang = PMConfig.get<uint8_t>(LANG_LOCALE);
    switch(graphIndex)
    {
    case 0x00:  // pH
        //Change Title
        myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_PH_GRAPH_TITLE),_lang));
        // Send samples to Nextion (default to graph object .id=2, channel=0)
        graphTable(pH_Samples,2,0);
    break;
    case 0x01:  // Orp
        //Change Title
        myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_ORP_GRAPH_TITLE),_lang));
        // Send samples to Nextion (default to graph object .id=2, channel=0)
        graphTable(Orp_Samples,2,0);
    break;
    case 0x02:  // Temperature
        //Change Title
        myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_TEMP_GRAPH_TITLE),_lang));
        // Send samples to Nextion (default to graph object .id=2, channel=0)
        graphTable(WTemp_Samples,2,0);
    break;
    }
}

// Menu command 4D or 'M'
void Nextion_Menu_4D(u_int8_t eventLength, u_int8_t (&eventData)[10])
{
    Debug.print(DBG_INFO,"Received Event 4D:");
    if(eventData[0] == 0x00) { // Main Menu
        MainMenu.Select(eventData[1]); // Select the item in the main menu
    } else if(eventData[0] == 0x01) { // Sub Menu
        MainMenu.GetItemRef()->submenu->Select(eventData[1]); // Select the item in the submenu
    } 
}

// Action command 53 or 'T'
void Nextion_Command_53(u_int8_t eventLength, u_int8_t (&eventData)[10])
{
    // from Nextion printh 23 03/02 53 00 00
    //                           |     |  |
    //                           |     |  | ---> Value (no value = toggle switch)
    //    If no value, toggle <--|     |----> Switch/Command Index
    // read the next byte that determines the position on the table
    // eventData[0] is the switch command index
    // eventData[1] is the value (no value = toggle switch)
    u_int8_t SwitchIndex = eventData[0];
    int value = 0;

    // read the next byte that keeps the value for the position
    if(eventLength==2) {
        value = eventData[1]; // Read the value from the event data
    } else if(eventLength==1) {
        value = SETVALUE_TOGGLE; // Instruct SetValue to Toggle the switch
    }

    // Set Value Usage
    Debug.print(DBG_INFO,"Nextion switch command 53: %d %d",SwitchIndex, eventLength, value);
    switch(SwitchIndex)
    {
    case ENMC_FILT_MODE:  // Filtration Mode
        SendCommand("Mode",value);
        break;
    case ENMC_PH_AUTOMODE:  // pH Auto Mode
        SendCommand("PhAutoMode",value);
        break;
    case ENMC_ORP_AUTOMODE:  // Orp Auto Mode
        SendCommand("OrpAutoMode",value);
        break;
    case ENMC_HEAT:  // Future Use for Heat Auto
        break;
    case ENMC_FILT_PUMP:  // Filtration Pump
        SendCommand("FiltPump",value);
        break;
    case ENMC_PH_PUMP:  // pH Pump
        SendCommand("PhPump",value);
        break;
    case ENMC_CHL_PUMP:  // Chl Pump
        SendCommand("ChlPump",value);
        break;
    case ENMC_HEATING:  // Heating
        break;
    case ENMC_WINTER_MODE:  // Winter Mode
        SendCommand("Winter",value);
    break;
    case ENMC_LANGUAGE:  // Change Language
        SendCommand("Lang_Locale",value);
    break;
    case ENMC_PH_PUMP_MENU:  // pH Pump
        if(value==BUTTON_AUTO) {
        SendCommand("PhAutoMode",1);
        } else {
        SendCommand("PhPump",value);
        }
    break;
    case ENMC_CHL_PUMP_MENU:  // Chl Pump
        if(value==BUTTON_AUTO) {
        SendCommand("OrpAutoMode",1);
        } else {
        SendCommand("ChlPump",value);
        }
    break;
    case ENMC_FILT_PUMP_MENU:  // Pump Menu Change
        if(value==BUTTON_AUTO) {
        SendCommand("Mode",1);
        } else {
        SendCommand("FiltPump",value);
        }
    break;
    case ENMC_HEAT_MENU:  // Heat Regulation
        if(value==BUTTON_AUTO) {
        // Not implemented
        } else {
        // Not implemented
        }
    break;
    case ENMC_SWG_MODE_MENU:  // SWG Regulation
        if(value==BUTTON_AUTO) {
        SendCommand("ElectrolyseMode",true);
        } else {
        SendCommand("Electrolyse",value);
        }
    break;
    case ENMC_TANK_STATUS:  // Tank Status
        if(value==BUTTON_OFF) {  //pH
        myNex.writeStr(F("pageFillTank.vaTankBut.txt"),"pH");
        myNex.writeStr(F("page pageFillTank"));
        } else if(value==BUTTON_ON) { //Chl
        myNex.writeStr(F("pageFillTank.vaTankBut.txt"),"Chl");
        myNex.writeStr(F("page pageFillTank"));
        }
    break;
    case ENMC_ROBOT:  // Robot
        SendCommand("RobotPump",value,SETVALUE_DIRECT,RobotPump.IsRunning());
    break;
    case ENMC_LIGHTS:  // Lights
        SendCommand("Relay",value,0,RELAYR0.IsActive());
    break;
    case ENMC_SPARE:  // Spare
        SendCommand("Relay",value,1,RELAYR1.IsActive());
    break;
    case ENMC_CLEAR_ALARMS:  // Clear Alarms
        SendCommand("Clear",1);
    break;
    case ENMC_FILLING_PUMP:  // Filling Pump
    SendCommand("FillingPump",value,SETVALUE_DIRECT,FillingPump.IsRunning());
    break;
    case ENMC_SWG_REGULATION:  // SWG Regulation Mode (fixed time or adjust)
    SendCommand("ElectroRunMode",value);
    break;
    }
}

/**
 * @brief  Function to send standard commands to PoolServer either toggle or force set a value
 * @param  _server_command: command to send to server (e.g. "Winter" for WinterMode)
 * @param  _force_state: -1 to toggle, 0 or 1 to force set the value
 * @param  _state_table: -1 to send direct command (e.g. "_server_command=_force_state"), or value to change in the table (e.g. "_server_command=[_state_table,_force_state]")
 * @param  _current_state: current state of the command (used when toggling value, _force_state=-1)
 * @retval None
 */
void SendCommand(const char* _server_command, int _force_state, int _state_table, int _current_state)
{
  // Do we need to toggle
  if (_force_state == -1) {
    _force_state = (_current_state)? false:true;
  }
  
  char Cmd[100];
  if(_state_table == -1) {
    sprintf(Cmd,"{\"%s\":%d}",_server_command,_force_state); // Build JSON Command
    Debug.print(DBG_INFO,"[NEXTION] Sending command to server %s = %d",_server_command,_force_state);
  } else {
    sprintf(Cmd,"{\"%s\":[%d,%d]}",_server_command,_state_table,_force_state); // Build JSON Command
    Debug.print(DBG_INFO,"[NEXTION] Sending command to server %s = [%d,%d]",_server_command,_state_table,_force_state);
  }
  xQueueSendToBack(queueIn,&Cmd,0);
}
