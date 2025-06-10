/*
  NEXTION TFT related code, based on EasyNextion library by Seithan / Athanasios Seitanis (https://github.com/Seithan/EasyNextionLibrary)
  The trigger(s) functions at the end are called by the Nextion library on event (buttons, page change).

  Completely reworked and simplified. These functions only update Nextion variables. The display is then updated localy by the
  Nextion itself.

  Remove every usages of String in order to avoid duplication, fragmentation and random crashes.
  Note usage of snprintf_P which uses a fmt string that resides in program memory.
*/
#include "Nextion.h"

//Reset TFT at start of controller - Change transmission rate to 115200 bauds on both side (Nextion then ESP)
//could have been not in HMI file, but it is good to know that after reset the Nextion goes back to 9600 bauds
void ResetTFT()
{
  //myNex.begin(115200);
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
  
  //switches_bitmap |= (storage.ElectroRunMode & 1)   << 16;     //       65 536
  switches_bitmap |= (FillingPump.UpTimeError & 1)  << 15;     //       32 768
  switches_bitmap |= (FillingPump.IsRunning() & 1)  << 14;     //       16 384
  switches_bitmap |= (PhPID.GetMode() & 1)          << 13;     //        8 192
  switches_bitmap |= (OrpPID.GetMode() & 1)         << 12;     //        4 096
  switches_bitmap |= (PhPump.IsRunning() & 1)       << 11;     //        2 048
  switches_bitmap |= (ChlPump.IsRunning() & 1)      << 10;     //        1 024
  switches_bitmap |= (storage.AutoMode & 1)         << 9;      //          512
  switches_bitmap |= (FiltrationPump.IsRunning() & 1) << 8;    //          256
  switches_bitmap |= (RobotPump.IsRunning() & 1)    << 7;      //          128
  switches_bitmap |= (RELAYR0.IsEnabled() & 1)      << 6;      //           64
  switches_bitmap |= (RELAYR1.IsEnabled() & 1)      << 5;      //           32
  switches_bitmap |= (storage.WinterMode & 1)       << 4;      //           16
  switches_bitmap |= (SWGPump.IsRunning() & 1)      << 3;      //            8
  switches_bitmap |= (storage.ElectrolyseMode & 1)  << 2;      //            4
  switches_bitmap |= (storage.pHAutoMode & 1)       << 1;      //            2
  switches_bitmap |= (storage.OrpAutoMode & 1)      << 0;      //            1

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
      // Do not update switch status during 1s after a change of switch to allow the system to reflect it in real life
      // avoids switches to flicker on/off on the screen
      if (((unsigned long)(millis() - myNex.LastActionMillis) > 1000) || (myNex.currentPageId==1))
      {
        WriteSwitches();
      }

      // New Main Home Page Intelligent Series
      if (myNex.currentPageId==ENP_HOME800)
      {


      }


      if(myNex.currentPageId==ENP_SPLASH || myNex.currentPageId==ENP_HOME)    //Splash & Home
      {
        // Home page data is loaded during splash screen to avoid lag when Home page appears
        snprintf_P(temp,sizeof(temp),PSTR("%02d-%02dh"),storage.FiltrationStart,storage.FiltrationStop);
        myNex.writeStr(F(GLOBAL".vaStaSto.txt"),temp);
        sprintf(temp, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
        myNex.writeStr(F(GLOBAL".vaTime.txt"),temp);
        sprintf(temp, PSTR("%02d/%02d/%02d"), day(), month(), year()-2000);
        myNex.writeStr(F(GLOBAL".vaDate.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),storage.PhValue);
        myNex.writeStr(F(GLOBAL".vapH.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),storage.OrpValue);
        myNex.writeStr(F(GLOBAL".vaOrp.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%3.1f"),storage.Ph_SetPoint);
        myNex.writeStr(F(GLOBAL".vapHSP.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),storage.Orp_SetPoint);
        myNex.writeStr(F(GLOBAL".vaOrpSP.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),storage.WaterTemp_SetPoint);
        myNex.writeStr(F(GLOBAL".vaWTSP.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),storage.WaterTempLowThreshold);
        myNex.writeStr(F(GLOBAL".vaWTLT.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%3.1f"),storage.PSI_MedThreshold);
        myNex.writeStr(F(GLOBAL".vaPSIMin.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%3.1f"),storage.PSI_HighThreshold);
        myNex.writeStr(F(GLOBAL".vaPSIMax.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%4.1f"),storage.WaterTemp);
        myNex.writeStr(F("pageHome.vaWT.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%4.1f"),storage.AirTemp);
        myNex.writeStr(F("pageHome.vaAT.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),storage.PSIValue);
        myNex.writeStr(F("pageHome.vaPSI.txt"),temp);

        // Update all gauge values
        long temp_gauge; 
        temp_gauge = map(storage.PhValue, storage.Ph_SetPoint - 0.5, storage.Ph_SetPoint + 0.5, 0, 154);
        temp_gauge = constrain(temp_gauge, 0, 147);
        myNex.writeNum(F("pageHome.vaPercArrowPH.val"), temp_gauge);
        temp_gauge = map(storage.OrpValue, storage.Orp_SetPoint - 50, storage.Orp_SetPoint + 50, 0, 154);
        temp_gauge = constrain(temp_gauge, 0, 147);
        myNex.writeNum(F("pageHome.vaPercArrowORP.val"), temp_gauge);
        temp_gauge = map(storage.WaterTemp, storage.WaterTempLowThreshold, storage.WaterTemp_SetPoint, 37, 110);
        temp_gauge = constrain(temp_gauge, 0, 147);
        myNex.writeNum(F("pageHome.vaPercArrowT.val"), temp_gauge);
        if (storage.PSIValue <= storage.PSI_MedThreshold) {
          temp_gauge = map(storage.PSIValue, (double)0, storage.PSI_MedThreshold, 0, 24);
          temp_gauge = constrain(temp_gauge, 0, 78);        
          myNex.writeNum(F("pageHome.vaPercArrowP.val"), temp_gauge);
        } else {
          temp_gauge = map(storage.PSIValue, storage.PSI_MedThreshold, storage.PSI_HighThreshold, 25, 63);
          temp_gauge = constrain(temp_gauge, 0, 78);     
          myNex.writeNum(F("pageHome.vaPercArrowP.val"), temp_gauge);
        }

        // Send pH and ORP Errors
        // 0 if within +/- 0.1 of the setpoint  (20 for ORP)
        // 1 or -1 if between 0.1 and 0.2 away from setpoint (20 and 40 for ORP)
        // 2 or -2 if more than 0.2 from the setpoint (more than 40 for ORP)

        if(abs(storage.PhValue-storage.Ph_SetPoint) <= 0.1) 
          myNex.writeNum(F("pageHome.vapHErr.val"),0);
        if(abs(storage.PhValue-storage.Ph_SetPoint) > 0.1 && abs(storage.PhValue-storage.Ph_SetPoint) <= 0.2)  
          myNex.writeNum(F("pageHome.vapHErr.val"),1);
        if(abs(storage.PhValue-storage.Ph_SetPoint) > 0.2)  
          myNex.writeNum(F("pageHome.vapHErr.val"),2);
        if(abs(storage.OrpValue-storage.Orp_SetPoint) <= 20.) 
          myNex.writeNum(F("pageHome.vaOrpErr.val"),0);
        if(abs(storage.OrpValue-storage.Orp_SetPoint) > 20. && abs(storage.OrpValue-storage.Orp_SetPoint) <= 40.)  
          myNex.writeNum(F("pageHome.vaOrpErr.val"),1);
        if(abs(storage.OrpValue-storage.Orp_SetPoint) > 40.)  
          myNex.writeNum(F("pageHome.vaOrpErr.val"),2);
      }


      if (((unsigned long)(millis() - LastUpdatedHome) > 1000)) // Update home page values every second
      {
      /******************************************
       * Home Page Minimalist uses global variable (more memory waste but better look when page change)
       * Updated every second even when not shown so that values are always up to date (except when Nextion sleeps)
       * ****************************************/
        LastUpdatedHome = millis();
        // Date and Time
        sprintf(temp, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
        myNex.writeStr(F(GLOBAL".vaTime.txt"),temp);
        sprintf(temp, PSTR("%02d/%02d/%02d"), day(), month(), year()-2000);
        myNex.writeStr(F(GLOBAL".vaDate.txt"),temp);

        // PSI difference with Threshold
        if (storage.PSIValue <= storage.PSI_MedThreshold) {
          myNex.writeNum(F("pageMenu800.vaPSINiddle.val"), 0);
        } else if (storage.PSIValue > storage.PSI_HighThreshold){
          myNex.writeNum(F("pageMenu800.vaPSINiddle.val"), 4);
        } else {
          myNex.writeNum(F("pageMenu800.vaPSINiddle.val"), 2);
        }

        // pH & Orp niddle position
        if(abs(storage.PhValue-storage.Ph_SetPoint) <= 0.1) 
          myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),0);
        if((storage.PhValue-storage.Ph_SetPoint) > 0.1 && (storage.PhValue-storage.Ph_SetPoint) <= 0.3)  
          myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),1);
        if((storage.PhValue-storage.Ph_SetPoint) < -0.1 && (storage.PhValue-storage.Ph_SetPoint) >= -0.3)  
          myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),-1);
        if((storage.PhValue-storage.Ph_SetPoint) > 0.3)  
          myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),2);
        if((storage.PhValue-storage.Ph_SetPoint) < -0.3)  
          myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),-2);

        if(abs(storage.OrpValue-storage.Orp_SetPoint) <= 70.) 
          myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),0);
        if((storage.OrpValue-storage.Orp_SetPoint) > 70. && (storage.OrpValue-storage.Orp_SetPoint) <= 200.)  
          myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),1);
        if((storage.OrpValue-storage.Orp_SetPoint) < -70. && (storage.OrpValue-storage.Orp_SetPoint) >= -200.)  
          myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),-1);
        if((storage.OrpValue-storage.Orp_SetPoint) > 200.)  
          myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),2);    
        if((storage.OrpValue-storage.Orp_SetPoint) < -200.)  
          myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),-2); 

        // pH & Orp Values
        snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),storage.PhValue);
        myNex.writeStr(F(GLOBAL".vapH.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),storage.OrpValue);
        myNex.writeStr(F(GLOBAL".vaOrp.txt"),temp);
  
        // Water Temperature
        snprintf_P(temp,sizeof(temp),PSTR("%4.1f°C"),storage.WaterTemp);
        myNex.writeStr(F("pageMenu800.tTopRight.txt"),temp);
      }

      
      if(myNex.currentPageId == ENP_MENU)     //Settings Menu
      {
        period=PT10/2;  // Accelerate TFT refresh when browsing menu
        
        // Rebuild menu if language has changed
        if(storage.Lang_Locale != Current_Language)
        {
          Current_Language = storage.Lang_Locale;
          NexMenu_Init(myNex);
          MainMenu.MenuDisplay(true);
        }

        if ((unsigned long)(millis() - myNex.LastSentCommandMillis) > 1000)
        {
          // Redraw only the submenu currently selected for status change
          MainMenu.Refresh();
          myNex.LastSentCommandMillis = millis();
        }

        // Draw complete menu only once when page appears
        if(myNex.hasPageChanged()) {
          MainMenu.MenuDisplay(true);
        }

      } else {
        period=PT10/2;
      }

      if(myNex.currentPageId == ENP_CALIB)      //Calib
      {
        // Translations for page
        if(myNex.hasPageChanged()) {
          myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change

          myNex.writeStr(PSTR("tTitleCalib.txt"),Helpers::translated_word(FL_(NXT_CALIB_TITLE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tReference.txt"),Helpers::translated_word(FL_(NXT_CALIB_REFERENCE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tMeasured.txt"),Helpers::translated_word(FL_(NXT_CALIB_MEASURED),storage.Lang_Locale));
          myNex.writeStr(PSTR("b0.txt"),Helpers::translated_word(FL_(NXT_OK),storage.Lang_Locale));
          myNex.writeStr(PSTR("b1.txt"),Helpers::translated_word(FL_(NXT_OK),storage.Lang_Locale));
          myNex.writeStr(PSTR("b2.txt"),Helpers::translated_word(FL_(NXT_OK),storage.Lang_Locale));
          myNex.writeStr(PSTR("bApply.txt"),Helpers::translated_word(FL_(NXT_APPLY),storage.Lang_Locale));
          myNex.writeStr(PSTR("bAdd.txt"),Helpers::translated_word(FL_(NXT_ADD),storage.Lang_Locale));
          myNex.writeStr(PSTR("Calib_pH.txt"),Helpers::translated_word(FL_(NXT_CALIB_PHPROBE),storage.Lang_Locale));
          myNex.writeStr(PSTR("Calib_Orp.txt"),Helpers::translated_word(FL_(NXT_CALIB_ORPPROBE),storage.Lang_Locale));
        }
        snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),storage.PhValue);
        myNex.writeStr(F(GLOBAL".vapH.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),storage.OrpValue);
        myNex.writeStr(F(GLOBAL".vaOrp.txt"),temp);
      } 

      if(myNex.currentPageId == ENP_KEYPAD)      //Keypad & Keyboard
      {
        if(myNex.hasPageChanged()) {
          myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
        }
      }

      if(myNex.currentPageId == ENP_WIFI_SCAN)      //Wifi Scanner
      {
        if(myNex.hasPageChanged()) {
          myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
        }

        // Scan every 10 seconds
        if ((unsigned long)(millis() - LastWifiScan) >= WIFI_SCAN_INTERVAL) {
          ScanWiFiNetworks();
          LastWifiScan=millis();
        }
        // check WiFi Scan Async process
        int16_t WiFiScanStatus = WiFi.scanComplete();
        if (WiFiScanStatus < 0) {  // it is busy scanning or got an error
          if (WiFiScanStatus == WIFI_SCAN_FAILED) {
            Debug.print(DBG_INFO,"[WiFi] Scan has failed. Starting again.");
            ScanWiFiNetworks();
          }
          // other option is status WIFI_SCAN_RUNNING - just wait.
        } else {  // Found Zero or more Wireless Networks
          printScannedNetworks(WiFiScanStatus);
          //ScanWiFiNetworks(); // Start over
        }
      }

      if(myNex.currentPageId == ENP_ELECTROLYSE)      //Electrolyse
      {
      // Translations for page
        if(myNex.hasPageChanged()) {
          myNex.writeStr(PSTR("tTitleElectro.txt"),Helpers::translated_word(FL_(NXT_ELECTRO_TITLE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tTemp.txt"),Helpers::translated_word(FL_(NXT_ELECTRO_TEMP),storage.Lang_Locale));
          myNex.writeStr(PSTR("tDelay.txt"),Helpers::translated_word(FL_(NXT_ELECTRO_DELAY),storage.Lang_Locale));
          myNex.writeStr(PSTR("bApply.txt"),Helpers::translated_word(FL_(NXT_APPLY),storage.Lang_Locale));
        }
        myNex.writeNum(F(GLOBAL".vaElectroSec.val"), storage.SecureElectro);
        myNex.writeNum(F(GLOBAL".vaElectroDelay.val"), storage.DelayElectro);
      }

      if(myNex.currentPageId == ENP_NEWTANK)      //New Tank
      {
        // Translations for page
        if(myNex.hasPageChanged()) {
          myNex.writeStr(PSTR("tTitleNewTank.txt"),Helpers::translated_word(FL_(NXT_NEWTANK_TITLE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tVol.txt"),Helpers::translated_word(FL_(NXT_NEWTANK_VOL),storage.Lang_Locale));
          myNex.writeStr(PSTR("tFill.txt"),Helpers::translated_word(FL_(NXT_NEWTANK_FILL),storage.Lang_Locale));
          myNex.writeStr(PSTR("bApply.txt"),Helpers::translated_word(FL_(NXT_APPLY),storage.Lang_Locale));
        }
      }

      if(myNex.currentPageId == ENP_INFO)      //Info
      {
        // Translations for page
        if(myNex.hasPageChanged()) {
          uptime::calculateUptime();

          myNex.writeStr(PSTR("tInfoTitle.txt"),Helpers::translated_word(FL_(NXT_INFO_TITLE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tAlarms.txt"),Helpers::translated_word(FL_(NXT_INFO_ALARMS),storage.Lang_Locale));
          myNex.writeStr(PSTR("tCompiledTitle.txt"),Helpers::translated_word(FL_(NXT_INFO_COMPILE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tCompiledValue.txt"), compile_date); //FIRMW
          myNex.writeStr(PSTR("tUpTime.txt"),Helpers::translated_word(FL_(NXT_INFO_UPTIME),storage.Lang_Locale));
          snprintf_P(temp,sizeof(temp),PSTR("%d%s %dh %dmn"),uptime::getDays(),
                                                             Helpers::translated_word(FL_(NXT_INFO_UPTIME_DAYS),storage.Lang_Locale),
                                                             uptime::getHours(),
                                                             uptime::getMinutes());
          myNex.writeStr(PSTR("tUpTimeValue.txt"),temp);
          myNex.writeStr(PSTR("bReboot.txt"),Helpers::translated_word(FL_(NXT_INFO_REBOOT),storage.Lang_Locale));
          myNex.writeStr(PSTR("bReboot.txt"),Helpers::translated_word(FL_(NXT_INFO_REBOOT),storage.Lang_Locale));
          myNex.writeNum(PSTR("vaDelay.val"),REBOOT_DELAY);
          myNex.writeStr(F(GLOBAL".vaMCFW.txt"), FIRMW); 
        }
      }

      if(myNex.currentPageId == ENP_DATETIME)      //Date Time
      {
        // Translations for page
        if(myNex.hasPageChanged()) {
          myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
          myNex.writeStr(PSTR("tDateTitle.txt"),Helpers::translated_word(FL_(NXT_DATE_TITLE),storage.Lang_Locale));
          myNex.writeStr(PSTR("bApply.txt"),Helpers::translated_word(FL_(NXT_APPLY),storage.Lang_Locale));
          myNex.writeStr(PSTR("tStatus.txt"),Helpers::translated_word(FL_(NXT_MQTT_STATUS),storage.Lang_Locale));
        }
      }

      if(myNex.currentPageId == ENP_WIFI_CONFIG)      //Wifi Config
      {
        // Translations for page
        if(myNex.hasPageChanged()) {
          myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
          myNex.writeStr(PSTR("tWifiTitle.txt"),Helpers::translated_word(FL_(NXT_WIFI_TITLE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tNetwork.txt"),Helpers::translated_word(FL_(NXT_WIFI_NETWORK),storage.Lang_Locale));
          myNex.writeStr(PSTR("tPassword.txt"),Helpers::translated_word(FL_(NXT_WIFI_PASSWORD),storage.Lang_Locale));
          myNex.writeStr(PSTR("tWifi.txt"),Helpers::translated_word(FL_(NXT_WIFI_WIFI),storage.Lang_Locale));
          myNex.writeStr(PSTR("tIP.txt"),Helpers::translated_word(FL_(NXT_WIFI_IP),storage.Lang_Locale));
          myNex.writeStr(PSTR("tStatus.txt"),Helpers::translated_word(FL_(NXT_MQTT_STATUS),storage.Lang_Locale));
          myNex.writeStr(PSTR("bConnect.txt"),Helpers::translated_word(FL_(NXT_CONNECT),storage.Lang_Locale));
        }

        if(WiFi.status() != WL_CONNECTED) {
          myNex.writeStr(F(GLOBAL".vaSSID.txt"),Helpers::translated_word(FL_(NXT_WIFI_NOTCONNECTED),storage.Lang_Locale));
          myNex.writeStr(F(GLOBAL".vaIP.txt"),"");
        } else
        {
          snprintf_P(temp,sizeof(temp),PSTR("%s"),WiFi.SSID().c_str());
          myNex.writeStr(F(GLOBAL".vaSSID.txt"),temp);
          snprintf_P(temp,sizeof(temp),PSTR("%s"),WiFi.localIP().toString().c_str());
          myNex.writeStr(F(GLOBAL".vaIP.txt"),temp);
        } 
      }

      if(myNex.currentPageId == ENP_GRAPH)      //Graph card
      {
        // Stop sending other values not to interphere with graph data sending
        LastUpdatedHome = millis(); // Prevent home page values from being sent out 
        myNex.LastActionMillis = millis(); // Idem for switch states

        if(myNex.hasPageChanged()) {
          myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
        }
      }

      if(myNex.currentPageId == ENP_HOME_SWITCH)     // Switches minimalist home page
      {
        if(myNex.hasPageChanged()) {
          myNex.writeStr(PSTR("tLights.txt"),Helpers::translated_word(FL_(NXT_SWITCH_LIGHTS),storage.Lang_Locale));
          myNex.writeStr(PSTR("tSpare.txt"),Helpers::translated_word(FL_(NXT_SWITCH_SPARE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tRobot.txt"),Helpers::translated_word(FL_(NXT_SWITCH_ROBOT),storage.Lang_Locale));
        }
      }

      if(myNex.currentPageId == ENP_LANGUAGE)     //Page Language Selection
      {
        if(myNex.hasPageChanged()) {
          Debug.print(DBG_WARNING,"Language Requested");
          myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_LANG_TITLE),storage.Lang_Locale));
          printLanguages();
        }        
      }

      if(myNex.currentPageId == ENP_MQTT_CONFIG)     //MQTT Configuration
      {
        if(myNex.hasPageChanged()) {
          myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
          myNex.writeStr(PSTR("tMQTTTitle.txt"),Helpers::translated_word(FL_(NXT_MQTT_TITLE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tServer.txt"),Helpers::translated_word(FL_(NXT_MQTT_SERVER),storage.Lang_Locale));
          myNex.writeStr(PSTR("tLogin.txt"),Helpers::translated_word(FL_(NXT_MQTT_LOGIN),storage.Lang_Locale));
          myNex.writeStr(PSTR("tPassword.txt"),Helpers::translated_word(FL_(NXT_MQTT_PASSWORD),storage.Lang_Locale));
          myNex.writeStr(PSTR("tSrvID.txt"),Helpers::translated_word(FL_(NXT_MQTT_ID),storage.Lang_Locale));
          myNex.writeStr(PSTR("tTopic.txt"),Helpers::translated_word(FL_(NXT_MQTT_TOPIC),storage.Lang_Locale));
          myNex.writeStr(PSTR("tMQTTStatus.txt"),Helpers::translated_word(FL_(NXT_MQTT_STATUS),storage.Lang_Locale));
          myNex.writeStr(PSTR("bConnect.txt"),Helpers::translated_word(FL_(NXT_CONNECT),storage.Lang_Locale));

          // Update MQTT parameters
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.MQTT_IP.toString().c_str());
          myNex.writeStr(F("vaMQTTSERVER.txt"),temp);
          snprintf_P(temp,sizeof(temp),PSTR("%d"),storage.MQTT_PORT);
          myNex.writeStr(F("vaMQTTPORT.txt"),temp);
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.MQTT_LOGIN);
          myNex.writeStr(F("vaMQTTLOGIN.txt"),temp);   
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.MQTT_PASS);
          myNex.writeStr(F("vaMQTTPASSWORD.txt"),temp);   
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.MQTT_ID);
          myNex.writeStr(F("vaMQTTSRVID.txt"),temp);   
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.MQTT_TOPIC);
          myNex.writeStr(F("vaMQTTTopic.txt"),temp);   
        }
      }
      
      if(myNex.currentPageId == ENP_ALERTS)     //Alerts
      {
        if(myNex.hasPageChanged()) {
          myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_ALERTS_TITLE),storage.Lang_Locale));
          myNex.writeStr(PSTR("bClear.txt"),Helpers::translated_word(FL_(NXT_ALERTS_ACK),storage.Lang_Locale));
          myNex.writeStr(PSTR("tWater.txt"),Helpers::translated_word(FL_(NXT_ALERTS_WATER),storage.Lang_Locale));
        }
      }

      if(myNex.currentPageId == ENP_SMTP_CONFIG)     // SMTP Config
      {
        if(myNex.hasPageChanged()) {
          myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
          myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_SMTP_TITLE),storage.Lang_Locale));
          myNex.writeStr(PSTR("tServer.txt"),Helpers::translated_word(FL_(NXT_SMTP_SERVER),storage.Lang_Locale));
          myNex.writeStr(PSTR("tLogin.txt"),Helpers::translated_word(FL_(NXT_SMTP_LOGIN),storage.Lang_Locale));
          myNex.writeStr(PSTR("tPassword.txt"),Helpers::translated_word(FL_(NXT_SMTP_PASSWORD),storage.Lang_Locale));
          myNex.writeStr(PSTR("tSrcEmail.txt"),Helpers::translated_word(FL_(NXT_SMTP_SENDER),storage.Lang_Locale));
          myNex.writeStr(PSTR("tReciEmail.txt"),Helpers::translated_word(FL_(NXT_SMTP_RECIPIENT),storage.Lang_Locale));
          myNex.writeStr(PSTR("bSave.txt"),Helpers::translated_word(FL_(NXT_SMTP_SAVE),storage.Lang_Locale));

          // Update SMTP parameters
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.SMTP_SERVER);
          myNex.writeStr(F("vaSMTPServer.txt"),temp);
          snprintf_P(temp,sizeof(temp),PSTR("%d"),storage.SMTP_PORT);
          myNex.writeStr(F("vaSMTPPORT.txt"),temp);
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.SMTP_LOGIN);
          myNex.writeStr(F("vaSMTPLOGIN.txt"),temp);   
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.SMTP_PASS);
          myNex.writeStr(F("vaSMTPPASSWORD.txt"),temp);   
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.SMTP_SENDER);
          myNex.writeStr(F("vaSMTPEmail.txt"),temp);   
          snprintf_P(temp,sizeof(temp),PSTR("%s"),storage.SMTP_RECIPIENT);
          myNex.writeStr(F("vaSMTPRcvEmail.txt"),temp);   
        }
      }

      if(myNex.currentPageId == ENP_PINS_CONFIG)     // PINs Config
      {
        if(myNex.hasPageChanged()) {
          myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
          myNex.writeStr(PSTR("tTitle0.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME0),storage.Lang_Locale));
          myNex.writeStr(PSTR("tTitle1.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME1),storage.Lang_Locale));
          myNex.writeStr(PSTR("tTitle2.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME2),storage.Lang_Locale));
          myNex.writeStr(PSTR("tTitle3.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME3),storage.Lang_Locale));
          myNex.writeStr(PSTR("tTitle4.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME4),storage.Lang_Locale));
          myNex.writeStr(PSTR("tTitle5.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME5),storage.Lang_Locale));
          myNex.writeStr(PSTR("tTitle6.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME6),storage.Lang_Locale));
          myNex.writeStr(PSTR("tTitle7.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME7),storage.Lang_Locale));
          myNex.writeStr(PSTR("bClear.txt"),Helpers::translated_word(FL_(NXT_PIN_CLEAR),storage.Lang_Locale));
          myNex.writeStr(PSTR("vaAllPINS.txt"),ALL_PINS);
        }

        int i=0;
        char SrcPINs[30] = {0};
        char SrcLOCKs[35] = {0};
        uint32_t SrcACTIVE_Bitmap = 0;
        uint32_t SrcMOMENT_Bitmap = 0;
        uint32_t SrcISRELAY_Bitmap = 0;
        //for(auto equi: Pool_Equipment)
        for (uint8_t i = 0; i < DEVICE_POOL_LEVEL; i++)
        {
          PIN* equi = PoolDeviceManager.GetDevice(i);

          // Set PIN Numbers
          snprintf_P(temp,sizeof(temp),PSTR("%d"),equi->GetPinNumber());
          strcat(SrcPINs,temp);
          strcat(SrcPINs,"|");

          // Set LOCK Numbers
          int lock_id = equi->GetInterlockId();
          lock_id = ((lock_id == 255)?255:lock_id+1); // Nextion counts from 1 to 8 but GetInterlockId return from 0 to 7 (except NO_INTERLOCK which does not move)
          snprintf_P(temp,sizeof(temp),PSTR("%d"),lock_id);
          strcat(SrcLOCKs,temp);
          strcat(SrcLOCKs,"|");

          // Set ACTIVE Level
          SrcACTIVE_Bitmap |= (equi->GetActiveLevel() & 1) << i;

          // Set MOMENTARY Level  
          SrcMOMENT_Bitmap |= (equi->GetOperationMode() & 1) << i;

          // Set ISRELAY (Relays do not support Interlock so should be greyed out)  
          SrcISRELAY_Bitmap |= (equi->IsRelay() & 1) << i;

          i++;
        }
        snprintf_P(temp_command,sizeof(temp_command),PSTR("vaSrcACTIVE.val"),i);
        myNex.writeNum(temp_command,SrcACTIVE_Bitmap);

        snprintf_P(temp_command,sizeof(temp_command),PSTR("vaSrcMOMENT.val"),i);
        myNex.writeNum(temp_command,SrcMOMENT_Bitmap);

        snprintf_P(temp_command,sizeof(temp_command),PSTR("vaSrcRELAYS.val"),i);
        myNex.writeNum(temp_command,SrcISRELAY_Bitmap);

        snprintf_P(temp_command,sizeof(temp_command),PSTR("vaSrcPINs.txt"),i);
        myNex.writeStr(temp_command,SrcPINs);

        snprintf_P(temp_command,sizeof(temp_command),PSTR("vaSrcLOCKs.txt"),i);
        myNex.writeStr(temp_command,SrcLOCKs);
      }

      ///////// ACTIONS LINKED TO HELP
      if(myNex.currentPageId == ENP_HELP_POPUP)      //Help Popup
      {
        if(myNex.hasPageChanged()) {    // Trigger once when page changes
          uint32_t help_index = myNex.readNumber(F("pageHelpPopup.vaHelpIndex.val"));

          switch(help_index) {
            case 1: // Calibration Help
              myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_HELP_1_TITLE),storage.Lang_Locale));
              myNex.writeStr(PSTR("tContent.txt"),Helpers::translated_word(FL_(NXT_HELP_1_CONTENT),storage.Lang_Locale));
              break;
          }
        }
      }

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


/**************** CUSTOM COMMANDS *********************
 ***************************************************************/
// Custom Command used for quick actions triggered via
// printh 23 01/02/03 XX YY ZZ (XX=CmdGroup) then
//    for Switch change, YY=SwitchIndex ZZ=Value
//    for Graph request, YY=graphIndex
// Read Custom Commands used to force value of configuration parameter
void easyNexReadCustomCommand()
{
  int value;
 // LastAction = millis();
  switch(myNex.cmdGroup)
  {
    case 'C': // Or <case 0x43:>  If 'C' matches Direct Command formated in JSON
    {
      // from Nextion printh 23 01 43
      char Cmd[200] = "";
      strcpy(Cmd,myNex.readStr(F(GLOBAL".vaCommand.txt")).c_str());
      xQueueSendToBack(queueIn,&Cmd,0);
      Debug.print(DBG_INFO,"Nextion direct command 43: %s",Cmd);
      break;
    }

    case 'S': // Or <case 0x53:>  If 'S' matches Update Switches 
    {
      // from Nextion printh 23 03/02 53 00 00
      //                           |     |  |
      //                           |     |  | ---> Value (no value = toggle switch)
      //    If no value, toggle <--|     |----> Switch/Command Index
      // read the next byte that determines the position on the table
      int SwitchIndex;
      SwitchIndex = myNex.readByte();
      // read the next byte that keeps the value for the position
      if(myNex.cmdLength==3) {
        value = myNex.readByte();
      } else if(myNex.cmdLength==2) {
        value = SETVALUE_TOGGLE; // Instruct SetValue to Toggle the switch
      }

      // Set Value Usage
      Debug.print(DBG_VERBOSE,"Nextion switch command 53: %d %d",SwitchIndex,value);
      switch(SwitchIndex)
      {
        case ENMC_FILT_MODE:  // Filtration Mode
          SetValue("Mode",value);
          break;
        case ENMC_PH_AUTOMODE:  // pH Auto Mode
          SetValue("PhAutoMode",value);
          break;
        case ENMC_ORP_AUTOMODE:  // Orp Auto Mode
          SetValue("OrpAutoMode",value);
          break;
        case ENMC_HEAT:  // Future Use for Heat Auto
          break;
        case ENMC_FILT_PUMP:  // Filtration Pump
          SetValue("FiltPump",value);
          break;
        case ENMC_PH_PUMP:  // pH Pump
          SetValue("PhPump",value);
          break;
        case ENMC_CHL_PUMP:  // Chl Pump
          SetValue("ChlPump",value);
          break;
        case ENMC_HEATING:  // Heating
          break;
        case ENMC_WINTER_MODE:  // Winter Mode
          SetValue("Winter",value);
        break;
        case ENMC_LANGUAGE:  // Change Language
          SetValue("Lang_Locale",value);
        break;
        case ENMC_PH_PUMP_MENU:  // pH Pump
          if(value==BUTTON_AUTO) {
            SetValue("PhAutoMode",1);
          } else {
            SetValue("PhPump",value);
          }
        break;
        case ENMC_CHL_PUMP_MENU:  // Chl Pump
          if(value==BUTTON_AUTO) {
            SetValue("OrpAutoMode",1);
          } else {
            SetValue("ChlPump",value);
          }
        break;
        case ENMC_FILT_PUMP_MENU:  // Pump Menu Change
          if(value==BUTTON_AUTO) {
            SetValue("Mode",1);
          } else {
            SetValue("FiltPump",value);
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
            SetValue("ElectrolyseMode",true);
          } else {
            SetValue("Electrolyse",value);
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
          SetValue("RobotPump",value,SETVALUE_DIRECT,RobotPump.IsRunning());
        break;
        case ENMC_LIGHTS:  // Lights
          SetValue("Relay",value,0,RELAYR0.IsActive());
        break;
        case ENMC_SPARE:  // Spare
          SetValue("Relay",value,1,RELAYR1.IsActive());
        break;
        case ENMC_CLEAR_ALARMS:  // Clear Alarms
          SetValue("Clear",1);
        break;
        case ENMC_FILLING_PUMP:  // Filling Pump
        SetValue("FillingPump",value,SETVALUE_DIRECT,FillingPump.IsRunning());
        break;
        case ENMC_SWG_REGULATION:  // SWG Regulation Mode (fixed time or adjust)
        SetValue("ElectroRunMode",value);
        break;
      }
      break;
    }

    case 'G': // Or <case 0x47:> If 'G' send graph data
    {
      // from Nextion printh 23 02 47 XX (graph reference)
      // Read graph reference
      int graphIndex = myNex.readByte();
      Debug.print(DBG_INFO,"Nextion graph command 47: %d",graphIndex);
      switch(graphIndex)
      {
        case 0x00:  // pH
          //Change Title
          myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_PH_GRAPH_TITLE),storage.Lang_Locale));
          // Send samples to Nextion (default to graph object .id=2, channel=0)
          graphTable(pH_Samples,2,0);
        break;
        case 0x01:  // Orp
          //Change Title
          myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_ORP_GRAPH_TITLE),storage.Lang_Locale));
          // Send samples to Nextion (default to graph object .id=2, channel=0)
          graphTable(Orp_Samples,2,0);
        break;
        case 0x02:  // Temperature
          //Change Title
          myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_TEMP_GRAPH_TITLE),storage.Lang_Locale));
          // Send samples to Nextion (default to graph object .id=2, channel=0)
          graphTable(WTemp_Samples,2,0);
        break;
      }
    } // End of Case
  }  // End of Switch
} // End of Function

/********************* WIFI Scanning **********************
 **********************************************************/
// Functions used to scan and print WIfi Networks
void ScanWiFiNetworks(){
  // WiFi.scanNetworks will return the number of networks found.
  if(WiFi.status() != WL_CONNECTED)
    DisconnectFromWiFi(true); // Disconnect from Wifi and do not try to reconnect
  Debug.print(DBG_INFO,"Scanning Networks. Start");
  int n = WiFi.scanNetworks(true);  //Async mode
}

// Send  result of Wifi Scanning to Nextion screen
void printScannedNetworks(uint16_t networksFound) {
  if (networksFound == 0) {
    Debug.print(DBG_INFO,"[WiFi] No network found");
    snprintf_P(temp_command,sizeof(temp_command),PSTR("pageWifiPopup.tNetwork%d.txt"),1);
    snprintf_P(temp,sizeof(temp),PSTR("%s"),Helpers::translated_word(FL_(NXT_WIFI_NONETWORKFOUND),storage.Lang_Locale));
    myNex.writeStr(temp_command,temp);
  } else {
    // Print only first MAX_SHOWN_NETWORKS networks (depending on how Nextion page is designed)
    networksFound = (networksFound > MAX_SHOWN_NETWORKS? MAX_SHOWN_NETWORKS : networksFound);

    for (int i = 0; i < networksFound; ++i) {
      // Print SSID each network found
      snprintf_P(temp_command,sizeof(temp_command),PSTR("pageWifiPopup.tNetwork%d.txt"),i+1);
      snprintf_P(temp,sizeof(temp),PSTR("%s"),WiFi.SSID(i).c_str());
      myNex.writeStr(temp_command,temp);
      // Print RSSI Icons
      snprintf_P(temp_command,sizeof(temp_command),PSTR("pageWifiPopup.tNetSignal%d.txt"),i+1);
      if(WiFi.RSSI(i)>-70)
        snprintf_P(temp,sizeof(temp),PSTR("%s"),"┹"); // Highest signal
        else if(WiFi.RSSI(i)>-80)
          snprintf_P(temp,sizeof(temp),PSTR("%s"),"┶");
          else if(WiFi.RSSI(i)>-90)
            snprintf_P(temp,sizeof(temp),PSTR("%s"),"┳");
            else
              snprintf_P(temp,sizeof(temp),PSTR("%s"),"┰"); // Lowest Signal
      myNex.writeStr(temp_command,temp);
    }
    if(networksFound > 0){
      myNex.writeNum(F("pageWifiPopup.vaScanEnded.val"), 1);
    }
    // Delete the scan result to free memory for code.
    WiFi.scanDelete();
  }
  reconnectToWiFi();
}

/***************** LANGUAGE Scanning **********************
 **********************************************************/
void printLanguages()
{
  for (int i = 0; i < NUM_LANGUAGES; ++i) {
    // Print SSID each network found
    snprintf_P(temp_command,sizeof(temp_command),PSTR("Lang_%d.txt"),i);
    snprintf_P(temp,sizeof(temp),PSTR("%s"),languages[i]);
    myNex.writeStr(temp_command,temp);
  }
}

/********************* HELPER Functions **********************
 *************************************************************/
double map(double x, double in_min, double in_max, int out_min, int out_max) {
  if ((in_max - in_min)==0)
    return 0;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

char map(int x, int in_min, int in_max, int out_min, int out_max) {
  if ((in_max - in_min)==0)
    return 0;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

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

// Function to graph values. It computed best scale and send to Nextion
void graphTable(CircularBuffer<int,NUMBER_OF_HISTORY_SAMPLES> &_sample_table, int _graph_object_id, int _graph_channel)
{
  char buf[NUMBER_OF_HISTORY_SAMPLES] = { 0 };
  uint16_t sample_table_size = _sample_table.size();
  // ²
  // Calculate min and max values for automatic scaling
  if (!_sample_table.isEmpty()) {
    int minValue = 9999;
    int maxValue = -9999;

    // Find the maximum and minimum values
    for (int i = 0; i < sample_table_size; ++i) {
      if (_sample_table[i] < minValue) {
        minValue = _sample_table[i];
      }
      if (_sample_table[i] > maxValue) {
        maxValue = _sample_table[i];
    }
    }

    // Adjust baseline and scale values
    int graphMin = minValue - 10; // Add some padding
    int graphMax = maxValue + 10; // Add some padding
    int graphMid = (graphMin + graphMax) / 2;

    // Update graph scale on the Nextion display
    snprintf_P(temp, sizeof(temp), PSTR("%4.2f"), float(graphMax) / 100);
    myNex.writeStr(F("tMax.txt"), temp);
    snprintf_P(temp, sizeof(temp), PSTR("%4.2f"), float(graphMin) / 100);
    myNex.writeStr(F("tMin.txt"), temp);
    snprintf_P(temp, sizeof(temp), PSTR("%4.2f"), float(graphMid) / 100);
    myNex.writeStr(F("tMed.txt"), temp);


    // Initialize table
    for(int i=0;i<NUMBER_OF_HISTORY_SAMPLES;i++)
    {
      if(i<sample_table_size)
        // Get the pH Sample with baseline reference
        buf[i] = map(_sample_table[i],graphMin,graphMax,0,GRAPH_Y_SIZE);
      else
        buf[i] = 0;
    }
  }
  snprintf_P(temp_command,sizeof(temp_command),PSTR("addt %d,%d,%d"),_graph_object_id,_graph_channel,sample_table_size);
  myNex.writeStr(temp_command);
  myNex.writeAllowed=false;
  vTaskDelay(5 / portTICK_PERIOD_MS);
  Serial2.write(buf,sample_table_size);
  myNex.writeAllowed=true;
}

