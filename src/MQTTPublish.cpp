// MQTT publish tasks
// - PublishSettings (PoolTopicSet1-5)
// - PublishMeasures (PoolTopicMeas1-2)
// The tasks are waiting for notification to publish. PublishMeasures has also a timeout allowing to publish
// measures regularly

#undef __STRICT_ANSI__
#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

//Size of the buffer to store outgoing JSON messages
#define PAYLOAD_BUFFER_LENGTH 200

// BitMaps with GPIO states
static uint8_t BitMap1 = 0;
static uint8_t BitMap2 = 0;
static uint8_t BitMap3 = 0;

static const char* PoolTopicMeas1 = "Meas1";
static const char* PoolTopicMeas2 = "Meas2";
static const char* PoolTopicSet1  = "Set1";
static const char* PoolTopicSet2  = "Set2";
static const char* PoolTopicSet3  = "Set3";
static const char* PoolTopicSet4  = "Set4";
static const char* PoolTopicSet5  = "Set5";

static char tempTopicSet[100];
static char tempTopicMeas[100];

int freeRam(void);
void stack_mon(UBaseType_t&);

//Encode digital inputs states into one Byte (more efficient to send over MQTT)
void EncodeBitMap()
{
  BitMap1 = 0;
  BitMap2 = 0;
  BitMap3 = 0;

  BitMap1 |= (FiltrationPump.IsRunning() & 1) << 7;
  BitMap1 |= (PhPump.IsRunning() & 1) << 6;
  BitMap1 |= (ChlPump.IsRunning() & 1) << 5;
  BitMap1 |= (PhPump.TankLevel() & 1) << 4;
  BitMap1 |= (ChlPump.TankLevel() & 1) << 3;
  BitMap1 |= (PSIError & 1) << 2;
  BitMap1 |= (PhPump.UpTimeError & 1) << 1;
  BitMap1 |= (ChlPump.UpTimeError & 1) << 0;

  BitMap2 |= (PhPID.GetMode() & 1) << 7;
  BitMap2 |= (OrpPID.GetMode() & 1) << 6;
  BitMap2 |= (PMConfig.get<bool>(AUTOMODE) & 1) << 5;
  BitMap2 |= (RobotPump.IsRunning() & 1) << 4;  // Different from ParisBrest
  BitMap2 |= (RELAYR0.IsActive() & 1) << 3;
  BitMap2 |= (RELAYR1.IsActive() & 1) << 2;
  BitMap2 |= (PMConfig.get<bool>(WINTERMODE) & 1) << 1;
  BitMap2 |= (PoolWaterLevelSensor.IsActive() & 1) << 0;

  BitMap3 |= (SWGPump.UpTimeError & 1U) << 7;       // 128
  BitMap3 |= (PMConfig.get<bool>(BUZZERON) & 1) << 6;           // 64
  BitMap3 |= (FillingPump.UpTimeError & 1)  << 5;   // 32
  BitMap3 |= (FillingPump.IsRunning() & 1) << 4;    // 16
  BitMap3 |= (PMConfig.get<bool>(ELECTROLYSEMODE) & 1) << 3;    // 8
  BitMap3 |= (SWGPump.IsRunning() & 1) << 2;        // 4
  BitMap3 |= (PMConfig.get<bool>(PHAUTOMODE) & 1) << 1;         // 2
  BitMap3 |= (PMConfig.get<bool>(ORPAUTOMODE) & 1) << 0;        // 1
}

void PublishTopic(const char* topic, JsonDocument& root)
{
  char Payload[PAYLOAD_BUFFER_LENGTH];

  size_t n=serializeJson(root,Payload);
  if (mqttClient.publish(topic, 1, true, Payload,n) != 0)
  {
    delay(50);
    Debug.print(DBG_DEBUG,"Publish: %s - size: %d/%d",Payload,root.size(),n);
  }
  else
  {
    Debug.print(DBG_DEBUG,"Unable to publish: %s",Payload);
  }
}

// Publishes system settings to MQTT broker
void SettingsPublish(void *pvParameters)
{
  while(!startTasks);
  vTaskDelay(DT9);                                // Scheduling offset 

  uint32_t mod1 = xTaskGetTickCount() % 1000;     // This is the offset to respect for future resume
  uint32_t mod2;
  TickType_t waitTime;

  static UBaseType_t hwm = 0;

  #ifdef CHRONO
  unsigned long td;
  int t_act=0,t_min=999,t_max=0;
  float t_mean=0.;
  int n=1;
  #endif
  
  for(;;)
  {
    #ifdef CHRONO
    td = millis();
    #endif

    Debug.print(DBG_DEBUG,"[PublishSettings] start");
         
    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 192 bytes
        const int capacity = JSON_OBJECT_SIZE(10) + 10; // value recommended by ArduinoJson Assistant with slack for the Firmw string
        StaticJsonDocument<capacity> root;

        root["FW"]     = Firmw;                            //firmware revision
        root["FSta"]   = PMConfig.get<uint8_t>(FILTRATIONSTART);          //Computed filtration start hour, in the morning (hours)
        root["FStaM"]  = PMConfig.get<uint8_t>(FILTRATIONSTARTMIN);       //Earliest Filtration start hour, in the morning (hours)
        root["FDu"]    = (uint8_t)PMData.FiltrDuration;       //Computed filtration duration based on water temperature (hours)
        root["FStoM"]  = PMConfig.get<uint8_t>(FILTRATIONSTOPMAX);        //Latest hour for the filtration to run. Whatever happens, filtration won't run later than this hour
        root["FSto"]   = PMConfig.get<uint8_t>(FILTRATIONSTOP);           //Computed filtration stop hour, equal to FSta + FDu (hour)
        root["pHUTL"]  = PhPump.MaxUpTime / 1000 / 60;  //Max allowed daily run time for the pH pump (/!\ mins)
        root["ChlUTL"] = ChlPump.MaxUpTime / 1000 / 60; //Max allowed daily run time for the Chl pump (/!\ mins)
        root["FMiUT"]  = FillingPump.MinUpTime / 1000 / 60;  //Min allowed daily run time for the Pool Filling pump (/!\ mins)
        root["FMaUT"]  = FillingPump.MaxUpTime / 1000 / 60;  //Max allowed daily run time for the Pool Filling pump (/!\ mins)


        snprintf(tempTopicSet,sizeof(tempTopicSet),"%s/%s",PMConfig.get<const char*>(MQTT_TOPIC),PoolTopicSet1);
        remove_duplicates_slash(tempTopicSet);
        PublishTopic(tempTopicSet, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        const int capacity = JSON_OBJECT_SIZE(8);
        StaticJsonDocument<capacity> root;

        root["pHWS"]  = PMConfig.get<unsigned long>(PHPIDWINDOWSIZE) / 1000 / 60;        //pH PID window size (/!\ mins)
        root["ChlWS"] = PMConfig.get<unsigned long>(ORPPIDWINDOWSIZE) / 1000 / 60;       //Orp PID window size (/!\ mins)
        root["pHSP"]  = PMData.Ph_SetPoint * 100;                  //pH setpoint (/!\ x100)
        root["OrpSP"] = PMData.Orp_SetPoint;                       //Orp setpoint
        root["WSP"]   = PMConfig.get<double>(WATERTEMP_SETPOINT) * 100;           //Water temperature setpoint (/!\ x100)
        root["WLT"]   = PMConfig.get<double>(WATERTEMPLOWTHRESHOLD) * 100;        //Water temperature low threshold to activate anti-freeze mode (/!\ x100)
        root["PSIHT"] = PMConfig.get<double>(PSI_HIGHTHRESHOLD) * 100;            //Water pressure high threshold to trigger error (/!\ x100)
        root["PSIMT"] = PMConfig.get<double>(PSI_MEDTHRESHOLD) * 100;            //Water pressure medium threshold (unused yet) (/!\ x100)

        snprintf(tempTopicSet,sizeof(tempTopicSet),"%s/%s",PMConfig.get<const char*>(MQTT_TOPIC),PoolTopicSet2);
        remove_duplicates_slash(tempTopicSet);
        PublishTopic(tempTopicSet, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        const int capacity = JSON_OBJECT_SIZE(6);
        StaticJsonDocument<capacity> root;

        root["pHC0"]  = PMConfig.get<double>(PHCALIBCOEFFS0);            //pH sensor calibration coefficient C0
        root["pHC1"]  = PMConfig.get<double>(PHCALIBCOEFFS1);            //pH sensor calibration coefficient C1
        root["OrpC0"] = PMConfig.get<double>(ORPCALIBCOEFFS0);           //Orp sensor calibration coefficient C0
        root["OrpC1"] = PMConfig.get<double>(ORPCALIBCOEFFS1);           //Orp sensor calibration coefficient C1
        root["PSIC0"] = PMConfig.get<double>(PSICALIBCOEFFS0);           //Pressure sensor calibration coefficient C0
        root["PSIC1"] = PMConfig.get<double>(PSICALIBCOEFFS1);           //Pressure sensor calibration coefficient C1

        snprintf(tempTopicSet,sizeof(tempTopicSet),"%s/%s",PMConfig.get<const char*>(MQTT_TOPIC),PoolTopicSet3);
        remove_duplicates_slash(tempTopicSet);
        PublishTopic(tempTopicSet, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        const int capacity = JSON_OBJECT_SIZE(8);
        StaticJsonDocument<capacity> root;

        root["pHKp"]  = PMConfig.get<double>(PH_KP);    //pH PID coeffcicient Kp
        root["pHKi"]  = PMConfig.get<double>(PH_KI);    //pH PID coeffcicient Ki
        root["pHKd"]  = PMConfig.get<double>(PH_KD);    //pH PID coeffcicient Kd

        root["OrpKp"] = PMConfig.get<double>(ORP_KP);    //Orp PID coeffcicient Kp
        root["OrpKi"] = PMConfig.get<double>(ORP_KI);    //Orp PID coeffcicient Ki
        root["OrpKd"] = PMConfig.get<double>(ORP_KD);    //Orp PID coeffcicient Kd

        root["Dpid"]  = PMConfig.get<uint8_t>(DELAYPIDS);     //Delay from FSta for the water regulation/PIDs to start (mins) 
        root["PubP"]  = PMConfig.get<unsigned long>(PUBLISHPERIOD) / 1000; // Settings publish period in sec

        snprintf(tempTopicSet,sizeof(tempTopicSet),"%s/%s",PMConfig.get<const char*>(MQTT_TOPIC),PoolTopicSet4);
        remove_duplicates_slash(tempTopicSet);
        PublishTopic(tempTopicSet, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        const int capacity = JSON_OBJECT_SIZE(8);
        StaticJsonDocument<capacity> root;

        root["pHTV"]  = PhPump.GetTankVolume();         //Acid tank nominal volume (Liters)
        root["ChlTV"] = ChlPump.GetTankVolume();         //Chl tank nominal volume (Liters)
        root["pHFR"]  = PhPump.GetFlowRate();           //Acid pump flow rate (L/hour)
        root["OrpFR"] = ChlPump.GetFlowRate();           //Chl pump flow rate (L/hour)
        root["SWGSec"] = PMConfig.get<uint8_t>(SECUREELECTRO);           //SWG Chlorine Generator Secure Temperature (°C)
        root["SWGDel"] = PMConfig.get<uint8_t>(DELAYELECTRO);           //SWG Chlorine Generator Delay before start after pump (mn)
        root["SWGUTL"] = SWGPump.MaxUpTime / 1000 / 60;           //SWG Max Up Time (mn)
        root["FPUTL"] = FiltrationPump.MaxUpTime / 1000 / 60;           //Filtration Pump Max Up Time (mn)

        snprintf(tempTopicSet,sizeof(tempTopicSet),"%s/%s",PMConfig.get<const char*>(MQTT_TOPIC),PoolTopicSet5);
        remove_duplicates_slash(tempTopicSet);
        PublishTopic(tempTopicSet, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    //display remaining RAM space. For debug
    Debug.print(DBG_DEBUG,"[memCheck]: %db",freeRam());

    #ifdef CHRONO
    t_act = millis() - td;
    if(t_act > t_max) t_max = t_act;
    if(t_act < t_min) t_min = t_act;
    t_mean += (t_act - t_mean)/n;
    ++n;
    Debug.print(DBG_INFO,"[PublishSettings] td: %d t_act: %d t_min: %d t_max: %d t_mean: %4.1f",td,t_act,t_min,t_max,t_mean);
    #endif

    stack_mon(hwm);    
    ulTaskNotifyTake(pdFALSE,portMAX_DELAY);
    mod2 = xTaskGetTickCount() % 1000;
    if(mod2 <= mod1)
      waitTime = mod1 - mod2;
    else
      waitTime = 1000 +  mod1 - mod2;
    vTaskDelay(waitTime);  
  }  
}

//PublishData loop. Publishes system info/data to MQTT broker every XX secs (30 secs by default)
//or when notified.
//There is two timing computations:
//-If notified, wait for the next offset to always being at the same place in the scheduling cycle
//-Then compute the time spent to do the job and reload the timeout for the next cycle

void MeasuresPublish(void *pvParameters)
{ 
  while(!startTasks);
  vTaskDelay(DT8);                                // Scheduling offset 
  uint32_t mod1 = xTaskGetTickCount() % 1000;     // This is the offset to respect for future resume

  TickType_t WaitTimeOut;
  TickType_t StartTime;
  TickType_t StopTime;
  TickType_t DeltaTime;
  uint32_t rc;
  uint32_t mod2;
  TickType_t waitTime;

  static UBaseType_t hwm = 0;

  #ifdef CHRONO
  unsigned long td;
  int t_act=0,t_min=999,t_max=0;
  float t_mean=0.;
  int n=1;
  #endif

  WaitTimeOut = (TickType_t)PMConfig.get<unsigned long>(PUBLISHPERIOD)/portTICK_PERIOD_MS;

  for(;;)
  {   
    rc = ulTaskNotifyTake(pdFALSE,WaitTimeOut); 
  
    if(rc != 0)   // notification => wait for next offset
    {
      mod2 = xTaskGetTickCount() % 1000;
      if(mod2 <= mod1)
        waitTime = mod1 - mod2;
      else
        waitTime = 1000 +  mod1 - mod2;
      vTaskDelay(waitTime);
    }

    StartTime = xTaskGetTickCount();   
    Debug.print(DBG_DEBUG,"[PublishMeasures] start");  

    #ifdef CHRONO
    td = millis();
    #endif    
    //Store the GPIO states in one Byte (more efficient over MQTT)
    EncodeBitMap();

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 192 bytes
        //Will publish something like {"Tmp":818,"pH":321,"PSI":56,"Orp":583,"FilUpT":8995,"PhUpT":0,"ChlUpT":0}
        const int capacity = JSON_OBJECT_SIZE(7);
        StaticJsonDocument<capacity> root;

        root["TE"]      = PMData.AirTemp * 100;        // /!\ x100
        root["Tmp"]     = PMData.WaterTemp * 100;
        root["pH"]      = PMData.PhValue * 100;
        root["PSI"]     = PMData.PSIValue * 100;
        root["Orp"]     = PMData.OrpValue;
        root["PhUpT"]   = PhPump.UpTime / 1000;
        root["ChlUpT"]  = ChlPump.UpTime / 1000;

        snprintf(tempTopicMeas,sizeof(tempTopicMeas),"%s/%s",PMConfig.get<const char*>(MQTT_TOPIC),PoolTopicMeas1);
        remove_duplicates_slash(tempTopicMeas);
        PublishTopic(tempTopicMeas, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    //Second MQTT publish to limit size of payload at once
    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        //Will publish something like {"AcidF":100,"ChlF":100,"IO":11,"IO2":0}
        const int capacity = JSON_OBJECT_SIZE(8);
        StaticJsonDocument<capacity> root;

        root["AcidF"] = PhPump.GetTankFill();
        root["ChlF"]  = ChlPump.GetTankFill();
        root["IO"]    = BitMap1;
        root["IO2"]   = BitMap2;
        root["IO3"]   = BitMap3;
        root["SWGUpT"]   = SWGPump.UpTime / 1000;
        root["FIUpT"]   = FiltrationPump.UpTime / 1000; // Filtration Pump Up Time in seconds
        root["FPUpT"]   = FillingPump.UpTime / 1000; // Pool Filling Pump Up Time in seconds

        snprintf(tempTopicMeas,sizeof(tempTopicMeas),"%s/%s",PMConfig.get<const char*>(MQTT_TOPIC),PoolTopicMeas2);
        remove_duplicates_slash(tempTopicMeas);
        PublishTopic(tempTopicMeas, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    //display remaining RAM space. For debug
    Debug.print(DBG_DEBUG,"[memCheck]: %db",freeRam());
    stack_mon(hwm);     

    #ifdef CHRONO
    t_act = millis() - td;
    if(t_act > t_max) t_max = t_act;
    if(t_act < t_min) t_min = t_act;
    t_mean += (t_act - t_mean)/n;
    ++n;
    Debug.print(DBG_INFO,"[PublishMeasures] td: %d t_act: %d t_min: %d t_max: %d t_mean: %4.1f",td,t_act,t_min,t_max,t_mean);
    #endif  

// Compute elapsed time to adjust next waiting time, taking into account a possible rollover of ticks count
    StopTime = xTaskGetTickCount();
    if(StartTime <= StopTime)
      DeltaTime = StopTime - StartTime;
    else
      DeltaTime = StopTime + (~TickType_t(0) - StartTime) + 1;

    WaitTimeOut = (TickType_t)PMConfig.get<unsigned long>(PUBLISHPERIOD)/portTICK_PERIOD_MS - DeltaTime;
  }
}

