/*
  NEXTION PAGES TFT related code, based on EasyNextion library by Seithan / Athanasios Seitanis.
  Used to define and display the pages (others than menu)
*/
#include "Nextion.h"
#include "Nextion_Pages.h"
#include "translation.h"

/** 
 * @brief  This function is called in the main Nextion loop to handle what to do when pages are shown.
 * @param  _myNex: Reference to the EasyNex object.
 * @retval None
 */
uint8_t NexPages_Loop(EasyNex& _myNex)
{
    uint8_t _ret = 1;    // Function returns a value which divides the sleep time to speed up screen refresh when needed
    // Do not update switch status during 1s after a change of switch to allow the system to reflect it in real life
    // avoids switches to flicker on/off on the screen
    uint8_t _lang_locale = PMConfig.get<uint8_t>(LANG_LOCALE);

    if (((unsigned long)(millis() - LastUpdatedHome) > 1000)) // Update home page values every second
    {
    /******************************************
     * Home Page uses global variable (more memory waste but better look when page change)
     * Updated every second even when not shown so that values are always up to date (except when Nextion sleeps)
     * ****************************************/
    LastUpdatedHome = millis();

    // Date and Time
    sprintf(temp, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
    myNex.writeStr(F(GLOBAL".vaTime.txt"),temp);
    sprintf(temp, PSTR("%02d/%02d/%02d"), day(), month(), year()-2000);
    myNex.writeStr(F(GLOBAL".vaDate.txt"),temp);

    // PSI difference with Threshold
    if (PMData.PSIValue <= PMConfig.get<double>(PSI_MEDTHRESHOLD)) {
        myNex.writeNum(F("pageMenu800.vaPSINiddle.val"), 0);
    } else if (PMData.PSIValue > PMConfig.get<double>(PSI_HIGHTHRESHOLD)){
        myNex.writeNum(F("pageMenu800.vaPSINiddle.val"), 4);
    } else {
        myNex.writeNum(F("pageMenu800.vaPSINiddle.val"), 2);
    }

    // pH & Orp niddle position
    double _pHSetPoint = PMData.Ph_SetPoint;
    double _pHValue = PMData.PhValue;
    if(abs(_pHValue-_pHSetPoint) <= 0.1) 
        myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),0);
    if((_pHValue-_pHSetPoint) > 0.1 && (_pHValue-_pHSetPoint) <= 0.3)  
        myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),1);
    if((_pHValue-_pHSetPoint) < -0.1 && (_pHValue-_pHSetPoint) >= -0.3)  
        myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),-1);
    if((_pHValue-_pHSetPoint) > 0.3)  
        myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),2);
    if((_pHValue-_pHSetPoint) < -0.3)  
        myNex.writeNum(F("pageMenu800.vaPHNiddle.val"),-2);

    double _orpSetPoint = PMData.Orp_SetPoint;
    double _orpValue = PMData.OrpValue;
    if(abs(_orpValue-_orpSetPoint) <= 70.) 
        myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),0);
    if((_orpValue-_orpSetPoint) > 70. && (_orpValue-_orpSetPoint) <= 200.)  
        myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),1);
    if((_orpValue-_orpSetPoint) < -70. && (_orpValue-_orpSetPoint) >= -200.)  
        myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),-1);
    if((_orpValue-_orpSetPoint) > 200.)  
        myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),2);
    if((_orpValue-_orpSetPoint) < -200.)  
        myNex.writeNum(F("pageMenu800.vaOrpNiddle.val"),-2);

    // pH & Orp Values
    snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),_pHValue);
    myNex.writeStr(F(GLOBAL".vapH.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),_orpValue);
    myNex.writeStr(F(GLOBAL".vaOrp.txt"),temp);

    // Water Temperature
    snprintf_P(temp,sizeof(temp),PSTR("%4.1f°C"),PMData.WaterTemp);
    if(myNex.currentPageId==ENP_HOME800) {
        myNex.writeStr(F("pageMenu800.tTopRight.txt"),temp);
    } else if(myNex.currentPageId==ENP_H800_OV1) {
        myNex.writeStr(F("pageOVHome.tTopRight.txt"),temp);
    }
    }

    if(myNex.currentPageId==ENP_SPLASH || myNex.currentPageId==ENP_HOME)    //Splash & Home
    {
    // Home page data is loaded during splash screen to avoid lag when Home page appears
    snprintf_P(temp,sizeof(temp),PSTR("%02d-%02dh"),PMConfig.get<uint8_t>(FILTRATIONSTART),PMConfig.get<uint8_t>(FILTRATIONSTOP));
    myNex.writeStr(F(GLOBAL".vaStaSto.txt"),temp);
    sprintf(temp, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
    myNex.writeStr(F(GLOBAL".vaTime.txt"),temp);
    sprintf(temp, PSTR("%02d/%02d/%02d"), day(), month(), year()-2000);
    myNex.writeStr(F(GLOBAL".vaDate.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),PMData.PhValue);
    myNex.writeStr(F(GLOBAL".vapH.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),PMData.OrpValue);
    myNex.writeStr(F(GLOBAL".vaOrp.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%3.1f"),PMData.Ph_SetPoint);
    myNex.writeStr(F(GLOBAL".vapHSP.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),PMData.Orp_SetPoint);
    myNex.writeStr(F(GLOBAL".vaOrpSP.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),PMConfig.get<double>(WATERTEMP_SETPOINT));
    myNex.writeStr(F(GLOBAL".vaWTSP.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),PMConfig.get<double>(WATERTEMPLOWTHRESHOLD));
    myNex.writeStr(F(GLOBAL".vaWTLT.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%3.1f"),PMConfig.get<double>(PSI_MEDTHRESHOLD));
    myNex.writeStr(F(GLOBAL".vaPSIMin.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%3.1f"),PMConfig.get<double>(PSI_HIGHTHRESHOLD));
    myNex.writeStr(F(GLOBAL".vaPSIMax.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%4.1f"),PMData.WaterTemp);
    myNex.writeStr(F("pageHome.vaWT.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%4.1f"),PMData.AirTemp);
    myNex.writeStr(F("pageHome.vaAT.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),PMData.PSIValue);
    myNex.writeStr(F("pageHome.vaPSI.txt"),temp);

    // Update all gauge values
    long temp_gauge; 
    double _pHSetPoint = PMData.Ph_SetPoint;
    double _pHValue = PMData.PhValue;
    double _orpSetPoint = PMData.Orp_SetPoint;
    double _orpValue = PMData.OrpValue;

    temp_gauge = map(_pHValue, _pHSetPoint - 0.5, _pHSetPoint + 0.5, 0, 154);
    temp_gauge = constrain(temp_gauge, 0, 147);
    myNex.writeNum(F("pageHome.vaPercArrowPH.val"), temp_gauge);
    temp_gauge = map(_orpValue, _orpSetPoint - 50, _orpSetPoint + 50, 0, 154);
    temp_gauge = constrain(temp_gauge, 0, 147);
    myNex.writeNum(F("pageHome.vaPercArrowORP.val"), temp_gauge);
    temp_gauge = map(PMData.WaterTemp, PMConfig.get<double>(WATERTEMPLOWTHRESHOLD), PMConfig.get<double>(WATERTEMP_SETPOINT), 37, 110);
    temp_gauge = constrain(temp_gauge, 0, 147);
    myNex.writeNum(F("pageHome.vaPercArrowT.val"), temp_gauge);

    double _psiValue = PMData.PSIValue;
    double _psiMedThreshold = PMConfig.get<double>(PSI_MEDTHRESHOLD);
    if (_psiValue <= _psiMedThreshold) {
        temp_gauge = map(_psiValue, (double)0, _psiMedThreshold, 0, 24);
        temp_gauge = constrain(temp_gauge, 0, 78);
        myNex.writeNum(F("pageHome.vaPercArrowP.val"), temp_gauge);
    } else {
        temp_gauge = map(_psiValue, _psiMedThreshold, PMConfig.get<double>(PSI_HIGHTHRESHOLD), 25, 63);
        temp_gauge = constrain(temp_gauge, 0, 78);
        myNex.writeNum(F("pageHome.vaPercArrowP.val"), temp_gauge);
    }

    // Send pH and ORP Errors
    // 0 if within +/- 0.1 of the setpoint  (20 for ORP)
    // 1 or -1 if between 0.1 and 0.2 away from setpoint (20 and 40 for ORP)
    // 2 or -2 if more than 0.2 from the setpoint (more than 40 for ORP)

    if(abs(_pHValue-_pHSetPoint) <= 0.1) 
        myNex.writeNum(F("pageHome.vapHErr.val"),0);
    if(abs(_pHValue-_pHSetPoint) > 0.1 && abs(_pHValue-_pHSetPoint) <= 0.2)  
        myNex.writeNum(F("pageHome.vapHErr.val"),1);
    if(abs(_pHValue-_pHSetPoint) > 0.2)  
        myNex.writeNum(F("pageHome.vapHErr.val"),2);
    if(abs(_orpValue-_orpSetPoint) <= 20.) 
        myNex.writeNum(F("pageHome.vaOrpErr.val"),0);
    if(abs(_orpValue-_orpSetPoint) > 20. && abs(_orpValue-_orpSetPoint) <= 40.)  
        myNex.writeNum(F("pageHome.vaOrpErr.val"),1);
    if(abs(_orpValue-_orpSetPoint) > 40.)  
        myNex.writeNum(F("pageHome.vaOrpErr.val"),2);
    }

    if(myNex.currentPageId == ENP_MENU)     //Settings Menu
    {
    _ret=2;  // Accelerate TFT refresh when browsing menu

    // Rebuild menu if language has changed
    if(PMConfig.get<uint8_t>(LANG_LOCALE) != Current_Language)
    {
        Current_Language = PMConfig.get<uint8_t>(LANG_LOCALE);
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
    _ret=2;
    }

    if(myNex.currentPageId == ENP_CALIB)      //Calib
    {
    // Translations for page
    if(myNex.hasPageChanged()) {
        myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change

        myNex.writeStr(PSTR("tTitleCalib.txt"),Helpers::translated_word(FL_(NXT_CALIB_TITLE),_lang_locale));
        myNex.writeStr(PSTR("tReference.txt"),Helpers::translated_word(FL_(NXT_CALIB_REFERENCE),_lang_locale));
        myNex.writeStr(PSTR("tMeasured.txt"),Helpers::translated_word(FL_(NXT_CALIB_MEASURED),_lang_locale));
        myNex.writeStr(PSTR("b0.txt"),Helpers::translated_word(FL_(NXT_OK),_lang_locale));
        myNex.writeStr(PSTR("b1.txt"),Helpers::translated_word(FL_(NXT_OK),_lang_locale));
        myNex.writeStr(PSTR("b2.txt"),Helpers::translated_word(FL_(NXT_OK),_lang_locale));
        myNex.writeStr(PSTR("bApply.txt"),Helpers::translated_word(FL_(NXT_APPLY),_lang_locale));
        myNex.writeStr(PSTR("bAdd.txt"),Helpers::translated_word(FL_(NXT_ADD),_lang_locale));
        myNex.writeStr(PSTR("Calib_pH.txt"),Helpers::translated_word(FL_(NXT_CALIB_PHPROBE),_lang_locale));
        myNex.writeStr(PSTR("Calib_Orp.txt"),Helpers::translated_word(FL_(NXT_CALIB_ORPPROBE),_lang_locale));
    }

    snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),PMData.PhValue);
    myNex.writeStr(F(GLOBAL".vapH.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),PMData.OrpValue);
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
        myNex.writeStr(PSTR("tTitleElectro.txt"),Helpers::translated_word(FL_(NXT_ELECTRO_TITLE),_lang_locale));
        myNex.writeStr(PSTR("tTemp.txt"),Helpers::translated_word(FL_(NXT_ELECTRO_TEMP),_lang_locale));
        myNex.writeStr(PSTR("tDelay.txt"),Helpers::translated_word(FL_(NXT_ELECTRO_DELAY),_lang_locale));
        myNex.writeStr(PSTR("bApply.txt"),Helpers::translated_word(FL_(NXT_APPLY),_lang_locale));
    }
    myNex.writeNum(F(GLOBAL".vaElectroSec.val"), PMConfig.get<uint8_t>(SECUREELECTRO));
    myNex.writeNum(F(GLOBAL".vaElectroDelay.val"), PMConfig.get<uint8_t>(DELAYELECTRO));
    }

    if(myNex.currentPageId == ENP_NEWTANK)      //New Tank
    {
    // Translations for page
    if(myNex.hasPageChanged()) {
        myNex.writeStr(PSTR("tTitleNewTank.txt"),Helpers::translated_word(FL_(NXT_NEWTANK_TITLE),_lang_locale));
        myNex.writeStr(PSTR("tVol.txt"),Helpers::translated_word(FL_(NXT_NEWTANK_VOL),_lang_locale));
        myNex.writeStr(PSTR("tFill.txt"),Helpers::translated_word(FL_(NXT_NEWTANK_FILL),_lang_locale));
        myNex.writeStr(PSTR("bApply.txt"),Helpers::translated_word(FL_(NXT_APPLY),_lang_locale));
    }
    }

    if(myNex.currentPageId == ENP_INFO)      //Info
    {
    // Translations for page
    if(myNex.hasPageChanged()) {
        uptime::calculateUptime();

        myNex.writeStr(PSTR("tInfoTitle.txt"),Helpers::translated_word(FL_(NXT_INFO_TITLE),_lang_locale));
        myNex.writeStr(PSTR("tAlarms.txt"),Helpers::translated_word(FL_(NXT_INFO_ALARMS),_lang_locale));
        myNex.writeStr(PSTR("tCompiledTitle.txt"),Helpers::translated_word(FL_(NXT_INFO_COMPILE),_lang_locale));
        myNex.writeStr(PSTR("tCompiledValue.txt"), compile_date); //FIRMW
        myNex.writeStr(PSTR("tUpTime.txt"),Helpers::translated_word(FL_(NXT_INFO_UPTIME),_lang_locale));
        snprintf_P(temp,sizeof(temp),PSTR("%d%s %dh %dmn"),uptime::getDays(),
                                                            Helpers::translated_word(FL_(NXT_INFO_UPTIME_DAYS),_lang_locale),
                                                            uptime::getHours(),
                                                            uptime::getMinutes());
        myNex.writeStr(PSTR("tUpTimeValue.txt"),temp);
        myNex.writeStr(PSTR("bReboot.txt"),Helpers::translated_word(FL_(NXT_INFO_REBOOT),_lang_locale));
        myNex.writeStr(PSTR("bReboot.txt"),Helpers::translated_word(FL_(NXT_INFO_REBOOT),_lang_locale));
        myNex.writeNum(PSTR("vaDelay.val"),REBOOT_DELAY);
        myNex.writeStr(F(GLOBAL".vaMCFW.txt"), FIRMW); 
    }
    }

    if(myNex.currentPageId == ENP_DATETIME)      //Date Time
    {
    // Translations for page
    if(myNex.hasPageChanged()) {
        myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
        myNex.writeStr(PSTR("tDateTitle.txt"),Helpers::translated_word(FL_(NXT_DATE_TITLE),_lang_locale));
        myNex.writeStr(PSTR("bApply.txt"),Helpers::translated_word(FL_(NXT_APPLY),_lang_locale));
        myNex.writeStr(PSTR("tStatus.txt"),Helpers::translated_word(FL_(NXT_MQTT_STATUS),_lang_locale));
    }
    }

    if(myNex.currentPageId == ENP_WIFI_CONFIG)      //Wifi Config
    {
    // Translations for page
    if(myNex.hasPageChanged()) {
        myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
        myNex.writeStr(PSTR("tWifiTitle.txt"),Helpers::translated_word(FL_(NXT_WIFI_TITLE),_lang_locale));
        myNex.writeStr(PSTR("tNetwork.txt"),Helpers::translated_word(FL_(NXT_WIFI_NETWORK),_lang_locale));
        myNex.writeStr(PSTR("tPassword.txt"),Helpers::translated_word(FL_(NXT_WIFI_PASSWORD),_lang_locale));
        myNex.writeStr(PSTR("tWifi.txt"),Helpers::translated_word(FL_(NXT_WIFI_WIFI),_lang_locale));
        myNex.writeStr(PSTR("tIP.txt"),Helpers::translated_word(FL_(NXT_WIFI_IP),_lang_locale));
        myNex.writeStr(PSTR("tStatus.txt"),Helpers::translated_word(FL_(NXT_MQTT_STATUS),_lang_locale));
        myNex.writeStr(PSTR("bConnect.txt"),Helpers::translated_word(FL_(NXT_CONNECT),_lang_locale));
    }

    if(WiFi.status() != WL_CONNECTED) {
        myNex.writeStr(F(GLOBAL".vaSSID.txt"),Helpers::translated_word(FL_(NXT_WIFI_NOTCONNECTED),_lang_locale));
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
        myNex.writeStr(PSTR("tLights.txt"),Helpers::translated_word(FL_(NXT_SWITCH_LIGHTS),_lang_locale));
        myNex.writeStr(PSTR("tSpare.txt"),Helpers::translated_word(FL_(NXT_SWITCH_SPARE),_lang_locale));
        myNex.writeStr(PSTR("tRobot.txt"),Helpers::translated_word(FL_(NXT_SWITCH_ROBOT),_lang_locale));
    }
    }

    if(myNex.currentPageId == ENP_LANGUAGE)     //Page Language Selection
    {
    if(myNex.hasPageChanged()) {
        Debug.print(DBG_WARNING,"Language Requested");
        myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_LANG_TITLE),_lang_locale));
        printLanguages();
    }        
    }

    if(myNex.currentPageId == ENP_MQTT_CONFIG)     //MQTT Configuration
    {
    if(myNex.hasPageChanged()) {
        myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
        myNex.writeStr(PSTR("tMQTTTitle.txt"),Helpers::translated_word(FL_(NXT_MQTT_TITLE),_lang_locale));
        myNex.writeStr(PSTR("tServer.txt"),Helpers::translated_word(FL_(NXT_MQTT_SERVER),_lang_locale));
        myNex.writeStr(PSTR("tLogin.txt"),Helpers::translated_word(FL_(NXT_MQTT_LOGIN),_lang_locale));
        myNex.writeStr(PSTR("tPassword.txt"),Helpers::translated_word(FL_(NXT_MQTT_PASSWORD),_lang_locale));
        myNex.writeStr(PSTR("tSrvID.txt"),Helpers::translated_word(FL_(NXT_MQTT_ID),_lang_locale));
        myNex.writeStr(PSTR("tTopic.txt"),Helpers::translated_word(FL_(NXT_MQTT_TOPIC),_lang_locale));
        myNex.writeStr(PSTR("tMQTTStatus.txt"),Helpers::translated_word(FL_(NXT_MQTT_STATUS),_lang_locale));
        myNex.writeStr(PSTR("bConnect.txt"),Helpers::translated_word(FL_(NXT_CONNECT),_lang_locale));

        // Update MQTT parameters
        IPAddress _mqtt_ip = PMConfig.get<uint32_t>(MQTT_IP);
        snprintf_P(temp,sizeof(temp),PSTR("%s"),_mqtt_ip.toString().c_str());
        myNex.writeStr(F("vaMQTTSERVER.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%d"),PMConfig.get<uint32_t>(MQTT_PORT));
        myNex.writeStr(F("vaMQTTPORT.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%s"),PMConfig.get<const char*>(MQTT_LOGIN));
        myNex.writeStr(F("vaMQTTLOGIN.txt"),temp);   
        snprintf_P(temp,sizeof(temp),PSTR("%s"),PMConfig.get<const char*>(MQTT_PASS));
        myNex.writeStr(F("vaMQTTPASSWORD.txt"),temp);   
        snprintf_P(temp,sizeof(temp),PSTR("%s"),PMConfig.get<const char*>(MQTT_ID));
        myNex.writeStr(F("vaMQTTSRVID.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%s"),PMConfig.get<const char*>(MQTT_TOPIC));
        myNex.writeStr(F("vaMQTTTopic.txt"),temp);
    }
    }

    if(myNex.currentPageId == ENP_ALERTS)     //Alerts
    {
    if(myNex.hasPageChanged()) {
        myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_ALERTS_TITLE),_lang_locale));
        myNex.writeStr(PSTR("bClear.txt"),Helpers::translated_word(FL_(NXT_ALERTS_ACK),_lang_locale));
        myNex.writeStr(PSTR("tWater.txt"),Helpers::translated_word(FL_(NXT_ALERTS_WATER),_lang_locale));
    }
    }

    if(myNex.currentPageId == ENP_SMTP_CONFIG)     // SMTP Config
    {
    if(myNex.hasPageChanged()) {
        myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
        myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_SMTP_TITLE),_lang_locale));
        myNex.writeStr(PSTR("tServer.txt"),Helpers::translated_word(FL_(NXT_SMTP_SERVER),_lang_locale));
        myNex.writeStr(PSTR("tLogin.txt"),Helpers::translated_word(FL_(NXT_SMTP_LOGIN),_lang_locale));
        myNex.writeStr(PSTR("tPassword.txt"),Helpers::translated_word(FL_(NXT_SMTP_PASSWORD),_lang_locale));
        myNex.writeStr(PSTR("tSrcEmail.txt"),Helpers::translated_word(FL_(NXT_SMTP_SENDER),_lang_locale));
        myNex.writeStr(PSTR("tReciEmail.txt"),Helpers::translated_word(FL_(NXT_SMTP_RECIPIENT),_lang_locale));
        myNex.writeStr(PSTR("bSave.txt"),Helpers::translated_word(FL_(NXT_SMTP_SAVE),_lang_locale));

        // Update SMTP parameters
        snprintf_P(temp,sizeof(temp),PSTR("%s"),PMConfig.get<const char*>(SMTP_SERVER));
        myNex.writeStr(F("vaSMTPServer.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%d"),PMConfig.get<uint32_t>(SMTP_PORT));
        myNex.writeStr(F("vaSMTPPORT.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%s"),PMConfig.get<const char*>(SMTP_LOGIN));
        myNex.writeStr(F("vaSMTPLOGIN.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%s"),PMConfig.get<const char*>(SMTP_PASS));
        myNex.writeStr(F("vaSMTPPASSWORD.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%s"),PMConfig.get<const char*>(SMTP_SENDER));
        myNex.writeStr(F("vaSMTPEmail.txt"),temp);
        snprintf_P(temp,sizeof(temp),PSTR("%s"),PMConfig.get<const char*>(SMTP_RECIPIENT));
        myNex.writeStr(F("vaSMTPRcvEmail.txt"),temp);   
    }
    }

    if(myNex.currentPageId == ENP_PINS_CONFIG)     // PINs Config
    {
    if(myNex.hasPageChanged()) {
        myNex.Deactivate_Sleep(); // Deactivate Sleep until next page change
        myNex.writeStr(PSTR("tTitle0.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME0),_lang_locale));
        myNex.writeStr(PSTR("tTitle1.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME1),_lang_locale));
        myNex.writeStr(PSTR("tTitle2.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME2),_lang_locale));
        myNex.writeStr(PSTR("tTitle3.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME3),_lang_locale));
        myNex.writeStr(PSTR("tTitle4.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME4),_lang_locale));
        myNex.writeStr(PSTR("tTitle5.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME5),_lang_locale));
        myNex.writeStr(PSTR("tTitle6.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME6),_lang_locale));
        myNex.writeStr(PSTR("tTitle7.txt"),Helpers::translated_word(FL_(NXT_PIN_NAME7),_lang_locale));
        myNex.writeStr(PSTR("bClear.txt"),Helpers::translated_word(FL_(NXT_PIN_CLEAR),_lang_locale));
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
                myNex.writeStr(PSTR("tTitle.txt"),Helpers::translated_word(FL_(NXT_HELP_1_TITLE),_lang_locale));
                myNex.writeStr(PSTR("tContent.txt"),Helpers::translated_word(FL_(NXT_HELP_1_CONTENT),_lang_locale));
                break;
            }
        }
    }

    return _ret; // Return the value to divide the sleep time
}

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
    snprintf_P(temp,sizeof(temp),PSTR("%s"),Helpers::translated_word(FL_(NXT_WIFI_NONETWORKFOUND),PMConfig.get<uint8_t>(LANG_LOCALE)));
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