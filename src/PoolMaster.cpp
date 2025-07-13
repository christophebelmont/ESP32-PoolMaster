// Supervisory task

#include <Arduino.h>                // Arduino framework
#include "Config.h"
#include "PoolMaster.h"
#ifdef SMTP
#include <ESP_Mail_Client.h>
#endif
#ifdef ELEGANT_OTA
static WiFiClient wificlient;
AsyncWebServer server(80);
#endif

#ifdef SMTP
SMTPSession smtp;
Session_Config config;
SMTP_Message message;
#endif
// Functions prototypes

bool readLocalTime(void);
void SetPhPID(bool);
void SetOrpPID(bool);
void mqttErrorPublish(const char*);
void PublishSettings(void);

//void SetFullyLoaded(void);
//void SetNTPReady(bool);
void stack_mon(UBaseType_t&);
#ifdef SMTP
void smtpCallback(SMTP_Status);
bool SMTP_Connect(void);
void Send_Email(void);
#endif

void PoolMaster(void *pvParameters)
{
  bool DoneForTheDay = false;                     // Reset actions done once per day
  bool NTPCheked = false;                         // If disconnected attemps to connect NTP every 10 minutes
  bool d_calc = false;                            // Filtration duration computed

  static UBaseType_t hwm=0;                       // free stack size
  #ifdef SMTP
  MailClient.networkReconnect(true);
  #ifndef SILENT_MODE
    smtp.debug(1);
  #endif
  smtp.callback(smtpCallback);
  config.server.host_name = PMConfig.get<const char*>(SMTP_SERVER);
  config.server.port = PMConfig.get<uint32_t>(SMTP_PORT);
  config.login.email = PMConfig.get<const char*>(SMTP_LOGIN);
  config.login.password = PMConfig.get<const char*>(SMTP_PASS);
  config.login.user_domain = "127.0.0.1";

  message.sender.name = F("PoolMaster");
  message.sender.email = PMConfig.get<const char*>(SMTP_SENDER);
  message.subject = F("PoolMaster Event");
  message.addRecipient(F("Home"), PMConfig.get<const char*>(SMTP_RECIPIENT));
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  message.priority = esp_mail_smtp_priority_low;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;
  #endif
  while(!startTasks);
  vTaskDelay(DT3);                                // Scheduling offset 

  esp_task_wdt_add(nullptr);
  TickType_t period = PT3;  
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

    // Handle OTA update
    ArduinoOTA.handle();

#ifdef ELEGANT_OTA
    //server.handleClient();
    ElegantOTA.loop();
#endif

    // Call pumps and relays loop function
    //for(auto equi: Pool_Equipment)
    //{
    //  equi->loop();
    //}
    // Call pumps and relays loop function
    PoolDeviceManager.Loop(); // Call the loop function of the device manager


    //reset time counters at midnight and send sync request to time server
    if (hour() == 0 && !DoneForTheDay)
    {
      // Reset all devices uptimes
      PoolDeviceManager.ResetUptimes(); // Reset UpTime for all devices

      d_calc = false;
      DoneForTheDay = true;
      cleaning_done = false;

      // Sync with NTP everyday at midnight
      if (readLocalTime()) {
        setTime(timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec,timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year-100);
        syncESP2RTC(second(),minute(),hour(),day(),month(),year()); // Send to Nextion RTC
        Debug.print(DBG_INFO,"From NTP time %d/%02d/%02d %02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());
        PoolMaster_NTPReady = true;
      } else {
        syncRTC2ESP();  // If NTP not available, get from Nextion RTC
        Debug.print(DBG_INFO,"From RTC time %d/%02d/%02d %02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());
        PoolMaster_NTPReady = false;
      }

      // Security: if WiFi disconnected in spite of system auto-reconnect, try to restart once a day
      //if(WiFi.status() != WL_CONNECTED) esp_restart(); // Commented out, if not connection hour is possibly 0 (midnight at bootup causing infinite reboot)
    }
    else if((hour() == 1) && DoneForTheDay)
    {
        DoneForTheDay = false;
    }

    // If NTP is not in synch, try to synch every 10 minutes
    if(!PoolMaster_NTPReady && (minute()%10 == 0) && !NTPCheked) {
      // NTP is disconnected, attemps to reconnect (if WIFI is up)
      if (PoolMaster_WifiReady && readLocalTime()) {
        setTime(timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec,timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year-100);
        syncESP2RTC(second(),minute(),hour(),day(),month(),year()); // Send to Nextion RTC
        Debug.print(DBG_INFO,"From NTP time %d/%02d/%02d %02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());
        PoolMaster_NTPReady = true;
      } else {
        syncRTC2ESP();  // If NTP not available, get from Nextion RTC
        Debug.print(DBG_INFO,"From RTC time %d/%02d/%02d %02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());
      }
      NTPCheked = true;
    } else if ((minute()%10 == 1) && NTPCheked) {
      NTPCheked = false;
    }

    // Compute next Filtering duration and start/stop hours at 15:00 (to filter during the hotest period of the day)
    // Wait at least 5mn after filtration start in order to let the temperature stabilizes in pipes, and to avoid
    // taking into account not yet measured temperature if the system starts at 15:xx. 
    // Depending on water temperature, the filtration duration is either 2 hours, temp/3 or temp/2 hours.
    #ifdef DEBUG
    if (second() == 0 && (millis() - FiltrationPump.StartTime) > 300000 && !d_calc)
    #else
    if (hour() == 15 && (millis() - FiltrationPump.StartTime) > 300000 && !d_calc)
    #endif
    {
        if (PMData.WaterTemp < PMConfig.get<double>(WATERTEMPLOWTHRESHOLD)){
            PMData.FiltrDuration = 2; // Set Filtration duration to 2 hours
        }
        else if (PMData.WaterTemp >= PMConfig.get<double>(WATERTEMPLOWTHRESHOLD) && PMData.WaterTemp < PMConfig.get<double>(WATERTEMP_SETPOINT)){
            PMData.FiltrDuration = round(PMData.WaterTemp / 3.);
        }
        else if (PMData.WaterTemp >= PMConfig.get<double>(WATERTEMP_SETPOINT)){
            PMData.FiltrDuration = round(PMData.WaterTemp / 2.);
        }

        uint8_t _FiltStart, _FiltStop;
        _FiltStart = 15 - (int)round(PMData.FiltrDuration / 2.);
        if (_FiltStart < PMConfig.get<uint8_t>(FILTRATIONSTARTMIN))
            _FiltStart = PMConfig.get<uint8_t>(FILTRATIONSTARTMIN);
        _FiltStop = _FiltStart + PMData.FiltrDuration;
        if (_FiltStop > PMConfig.get<uint8_t>(FILTRATIONSTOPMAX))
            _FiltStop = PMConfig.get<uint8_t>(FILTRATIONSTOPMAX);
        
        PMConfig.put<uint8_t>(FILTRATIONSTART, _FiltStart);
        PMConfig.put<uint8_t>(FILTRATIONSTOP, _FiltStop);

        Debug.print(DBG_INFO,"Filtration duration: %dh",PMData.FiltrDuration);
        Debug.print(DBG_INFO,"Start: %dh - Stop: %dh",PMConfig.get<uint8_t>(FILTRATIONSTART),PMConfig.get<uint8_t>(FILTRATIONSTOP));

        PublishSettings();

        d_calc = true;
    }
    #ifdef DEBUG
    if(second() == 30 && d_calc) d_calc = false;
    #endif


    #ifdef SMTP
    //Send email if alarm(s) occured
    Send_Email();
    #endif

    #ifdef CHRONO
    t_act = millis() - td;
    if(t_act > t_max) t_max = t_act;
    if(t_act < t_min) t_min = t_act;
    t_mean += (t_act - t_mean)/n;
    ++n;
    Debug.print(DBG_INFO,"[PoolMaster] td: %d t_act: %d t_min: %d t_max: %d t_mean: %4.1f",td,t_act,t_min,t_max,t_mean);
    #endif 

    stack_mon(hwm);
    vTaskDelayUntil(&ticktime,period);
    PoolMaster_FullyLoaded = true;
  }
}

//Enable/Disable pH PID
void SetPhPID(bool Enable)
{
  if (Enable)
  {
    //Start PhPID
    PhPump.ClearErrors();
    PMData.PhPIDOutput = 0.0;
    PMData.PhPIDwStart = millis();
    PhPID.SetMode(AUTOMATIC);
    PMData.Ph_RegOnOff = true; // Set the runtime data to true
  }
  else
  {
    //Stop PhPID
    PhPID.SetMode(MANUAL);
    PMData.Ph_RegOnOff = false; // Set the runtime data to false
    PMData.PhPIDOutput = 0.0; // Reset the PID output
    //PhPump.Stop(); // Do not stop PhPump here, it is controlled by the PID or directly by the user
  }
}

//Enable/Disable Orp PID
void SetOrpPID(bool Enable)
{
  if (Enable)
  {
    //Start OrpPID
    ChlPump.ClearErrors();
    PMData.OrpPIDOutput = 0.0;
    PMData.OrpPIDwStart = millis();
    OrpPID.SetMode(AUTOMATIC);
    PMData.Orp_RegOnOff = true; // Set the runtime data to true
  }
  else
  {
    //Stop OrpPID
    OrpPID.SetMode(MANUAL);
    PMData.Orp_RegOnOff = false; // Set the runtime data to false
    PMData.OrpPIDOutput = 0.0; // Reset the PID output
    //ChlPump.Stop(); // Do not stop ChlPump here, it is controlled by the PID or directly by the user
  }
}

#ifdef SMTP
bool SMTP_Connect(){
  Debug.print(DBG_DEBUG,"SMTP Connection starts");
  if (!smtp.connect(&config)){
    Debug.print(DBG_ERROR,"SMTP Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return false;
  } else Debug.print(DBG_INFO,"SMTP Connected");
  if (!smtp.isLoggedIn()) Debug.print(DBG_ERROR,"Not yet logged in.");
  else{
    if (smtp.isAuthenticated()) Debug.print(DBG_INFO,"SMTP Successfully logged in.");
    else Debug.print(DBG_ERROR,"SMTP Connected with no Auth.");
  }
  return true;
}

void Send_Email(){

    char texte[80];
    static bool notif_sent[5] = {0,0,0,0,0};

    if(PSIError)
    {
      if(!notif_sent[0])
      {
        sprintf(texte,"Water pressure alert: %4.2fbar",PMData.PSIValue);
        message.text.content = texte;
        if(SMTP_Connect()){   
          if(!MailClient.sendMail(&smtp, &message))
            Debug.print(DBG_ERROR,"Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());     
          else notif_sent[0] = true;
        }
      }    
    } else notif_sent[0] = false;

    if(!ChlPump.TankLevel())
    {
      if(!notif_sent[1])
      {
        sprintf(texte,"Chlorine level LOW: %3.0f %",ChlPump.GetTankFill());
        message.text.content = texte;
        if(SMTP_Connect()){
          if(!MailClient.sendMail(&smtp, &message))
            Debug.print(DBG_ERROR,"Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());    
          else notif_sent[1] = true;
        }  
      }
    } else notif_sent[1] = false;

    if(!PhPump.TankLevel())
    {
      if(!notif_sent[2])
      {
        sprintf(texte,"Acid level LOW: %3.0f %",PhPump.GetTankFill());
        message.text.content = texte;
        if(SMTP_Connect()){ 
          if(!MailClient.sendMail(&smtp, &message))
            Debug.print(DBG_ERROR,"Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());    
          else notif_sent[2] = true;
        }  
      }

    } else notif_sent[2] = false;

    if(ChlPump.UpTimeError)
    {
      if(!notif_sent[3])
      {
        sprintf(texte,"Chlorine pump uptime: %2.0fmn",round(ChlPump.UpTime/60000.));
        message.text.content = texte; 
        if(SMTP_Connect()){       
          if(!MailClient.sendMail(&smtp, &message))
            Debug.print(DBG_ERROR,"Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());    
          else notif_sent[3] = true;
        }  
      }
    } else notif_sent[3] = false;

    if(PhPump.UpTimeError)
    {
      if(!notif_sent[4])
      {
        sprintf(texte,"Acid pump uptime: %2.0fmn",round(PhPump.UpTime/60000.));
        message.text.content = texte;
        if(SMTP_Connect()){
          if(!MailClient.sendMail(&smtp, &message))
            Debug.print(DBG_ERROR,"Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());    
          else notif_sent[4] = true;
        }  
      }
    } else notif_sent[4] = false; 
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status){
  /* Print the current status */
  Debug.print(DBG_INFO,"Email send status: %d",status.info());

  /* Print the sending result */
  if (status.success()){
    Debug.print(DBG_INFO,"Message sent success: %d", status.completedCount());
    Debug.print(DBG_INFO,"Message sent failed: %d", status.failedCount());

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)
      
      Debug.print(DBG_INFO,"Message No: %d", i + 1);
      Debug.print(DBG_INFO,"Status: %s", result.completed ? "success" : "failed");
      Debug.print(DBG_INFO,"Date/Time: %s", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      Debug.print(DBG_INFO,"Recipient: %s", result.recipients.c_str());
      Debug.print(DBG_INFO,"Subject: %s", result.subject.c_str());
    }

    // Clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}
#endif
