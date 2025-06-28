/*
  NEXTION TFT related code, based on EasyNextion library by Seithan / Athanasios Seitanis (https://github.com/Seithan/EasyNextionLibrary)
  The trigger(s) functions at the end are called by the Nextion library on event (buttons, page change).

  Completely reworked and simplified. These functions only update Nextion variables. The display is then updated localy by the
  Nextion itself.

  Remove every usages of String in order to avoid duplication, fragmentation and random crashes.
  Note usage of snprintf_P which uses a fmt string that resides in program memory.
*/
#include "Nextion.h"


// Nextion TFT objects. Choose which ever Serial port
WiFiServer telnetServer(23);
MultiStream MultiOutput(&Serial2);
EasyNex myNex(Serial2); // Create Nextion object using MultiStream

BaseType_t xWasDelayed;     // Return value for task delay

//Reset TFT at start of controller - Change transmission rate to 115200 bauds on both side (Nextion then ESP)
//could have been not in HMI file, but it is good to know that after reset the Nextion goes back to 9600 bauds
void ResetTFT()
{
  NexEvents_Init();
  myNex.SetEventManager(&NexeventManager); // Set the event manager to handle events sent by Nextion

  myNex.begin(115200);
  myNex.writeStr("sleep=0");
  myNex.writeStr(F("rest"));
  myNex.writeStr(F("wup=1")); // Exit from sleep on last page
  myNex.writeStr(F("usup=1")); // Authorize auto wake up on serial data
  myNex.writeStr("page pageSplash");
  delay(500);
  NexMenu_Init(myNex);
}

// Read and Write the boolean values stored in a 32 bits number
// to be sent to Nextion in one shot
void WriteSwitches()
{
  uint32_t switches_bitmap = 0;

  switches_bitmap |= (PoolMaster_FullyLoaded & 1)   << 31;     //2 147 483 648
  switches_bitmap |= (PoolMaster_BoardReady & 1)    << 30;     //1 073 741 824
  switches_bitmap |= (PoolMaster_WifiReady & 1)     << 29;     //  536 870 912
  switches_bitmap |= (PoolMaster_MQTTReady & 1)     << 28;     //  268 435 456
  switches_bitmap |= (PoolMaster_FullyLoaded & 1)   << 27;     //  134 217 728
  switches_bitmap |= (MQTTConnection & 1)           << 26;     //   67 108 864
  switches_bitmap |= (!PhPump.TankLevel() & 1)      << 25;     //   33 554 432
  switches_bitmap |= (!ChlPump.TankLevel() & 1)     << 24;     //   16 777 216
  switches_bitmap |= (PSIError & 1)                 << 23;     //    8 388 608
  switches_bitmap |= (PhPump.UpTimeError & 1)       << 22;     //    4 194 304
  switches_bitmap |= (ChlPump.UpTimeError & 1)      << 21;     //    2 097 152
  //switches_bitmap |= ((digitalRead(POOL_LEVEL)==HIGH) & 1) << 20; // 1 048 576
  switches_bitmap |= (PoolDeviceManager.GetDevice(DEVICE_POOL_LEVEL)->IsActive() & 1) << 20; // 1 048 576

  switches_bitmap |= (PMConfig.get<bool>(ELECTRORUNMODE) & 1)   << 16;     //       65 536
  switches_bitmap |= (FillingPump.UpTimeError & 1)  << 15;     //       32 768
  switches_bitmap |= (FillingPump.IsRunning() & 1)  << 14;     //       16 384
  switches_bitmap |= (PhPID.GetMode() & 1)          << 13;     //        8 192
  switches_bitmap |= (OrpPID.GetMode() & 1)         << 12;     //        4 096
  switches_bitmap |= (PhPump.IsRunning() & 1)       << 11;     //        2 048
  switches_bitmap |= (ChlPump.IsRunning() & 1)      << 10;     //        1 024
  switches_bitmap |= (PMConfig.get<bool>(AUTOMODE) & 1)         << 9;      //          512
  switches_bitmap |= (FiltrationPump.IsRunning() & 1) << 8;    //          256
  switches_bitmap |= (RobotPump.IsRunning() & 1)    << 7;      //          128
  switches_bitmap |= (RELAYR0.IsEnabled() & 1)      << 6;      //           64
  switches_bitmap |= (RELAYR1.IsEnabled() & 1)      << 5;      //           32
  switches_bitmap |= (PMConfig.get<bool>(WINTERMODE) & 1)       << 4;      //           16
  switches_bitmap |= (SWGPump.IsRunning() & 1)      << 3;      //            8
  switches_bitmap |= (PMConfig.get<bool>(ELECTROLYSEMODE) & 1)  << 2;      //            4
  switches_bitmap |= (PMConfig.get<bool>(PHAUTOMODE) & 1)       << 1;      //            2
  switches_bitmap |= (PMConfig.get<bool>(ORPAUTOMODE) & 1)      << 0;      //            1

  myNex.writeNum(F(GLOBAL".vaSwitches.val"),switches_bitmap);
}


//Function to update TFT display
//update the global variables of the TFT + the widgets of the active page
//call this function at least every second to ensure fluid display
void UpdateTFT(void *pvParameters)
{
  static UBaseType_t hwm=0;     // free stack size

  while(!startTasks);
  vTaskDelay(DT10);                                // Scheduling offset 

  esp_task_wdt_add(nullptr);
  TickType_t period = PT10;  
  TickType_t ticktime = xTaskGetTickCount(); 

  #ifdef CHRONO
  unsigned long td;
  int t_act=0,t_min=999,t_max=0;
  float t_mean=0.;
  int n=1;
  #endif

  for(;;)
  {
    // reset watchdog
    esp_task_wdt_reset();

    #ifdef CHRONO
    td = millis();
    #endif    

    // Has any button been touched? If yes, one of the trigger routines
    // will fire.
    // Return  0 if Nextion is sleeping 
    //         1 if Nextion Up
    if(myNex.NextionListen())
    {
       WriteSwitches(); // Write the switches bitmap every loop

      // Handle Pages related actions
      uint8_t _diviser = NexPages_Loop(myNex);
      period = (PT10 / _diviser); // Adjust the period to speed up the TFT refresh when needed

      // Handle Menu actions in the Loop
      NexMenu_Loop(myNex);
    } else {
      period = PT10;
    }
    #ifdef CHRONO
    t_act = millis() - td;
    if(t_act > t_max) t_max = t_act;
    if(t_act < t_min) t_min = t_act;
    t_mean += (t_act - t_mean)/n;
    ++n;
    Debug.print(DBG_INFO,"[PoolMaster] td: %d t_act: %d t_min: %d t_max: %d t_mean: %4.1f",td,t_act,t_min,t_max,t_mean);
    #endif 

    stack_mon(hwm);
    xWasDelayed = xTaskDelayUntil(&ticktime,period);

    // Check if the task was delayed normally. Error means that the 
    // next scheduled time is in the past and, thus, that the task
    // code took too long to execute.
    if (xWasDelayed == pdFALSE)
      Debug.print(DBG_ERROR,"Error delaying Nextion task. Possibly took too long to execute.");
  } 
}
/*******************************************************************
 * ******************** END OF TASK LOOP ***************************
 *******************************************************************/

void syncESP2RTC(uint32_t _second, uint32_t _minute, uint32_t _hour, uint32_t _day, uint32_t _month, uint32_t _year) {
  Debug.print(DBG_INFO,"[TIME] ESP/NTP -> RTC");

  myNex.writeNum(F("rtc5"),_second);
  myNex.writeNum(F("rtc4"),_minute);
  myNex.writeNum(F("rtc3"),_hour);
  myNex.writeNum(F("rtc2"),_day);
  myNex.writeNum(F("rtc1"),_month);
  myNex.writeNum(F("rtc0"),_year);
}

void syncRTC2ESP() {
  Debug.print(DBG_INFO,"[TIME] RTC Time -> ESP");

  setTime(
    (int)myNex.readNumber(F("rtc3")),
    (int)myNex.readNumber(F("rtc4")),
    (int)myNex.readNumber(F("rtc5")),
    (int)myNex.readNumber(F("rtc2")),
    (int)myNex.readNumber(F("rtc1")),
    (int)myNex.readNumber(F("rtc0"))
  );
}
