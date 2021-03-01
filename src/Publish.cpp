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
#define PAYLOAD_BUFFER_LENGTH 150

// BitMaps with GPIO states
static uint8_t BitMap1 = 0;
static uint8_t BitMap2 = 0;

static const char* PoolTopicMeas1 = "Home/Pool6/Meas1";
static const char* PoolTopicMeas2 = "Home/Pool6/Meas2";
static const char* PoolTopicSet1  = "Home/Pool6/Set1";
static const char* PoolTopicSet2  = "Home/Pool6/Set2";
static const char* PoolTopicSet3  = "Home/Pool6/Set3";
static const char* PoolTopicSet4  = "Home/Pool6/Set4";
static const char* PoolTopicSet5  = "Home/Pool6/Set5";

int freeRam(void);
void stack_mon(UBaseType_t&);

//Encode digital inputs states into one Byte (more efficient to send over MQTT)
void EncodeBitMap()
{
  BitMap1 = 0;
  BitMap2 = 0;
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
  BitMap2 |= (storage.AutoMode & 1) << 5;
  BitMap2 |= (RobotPump.IsRunning() & 1) << 4;

  BitMap2 |= !digitalRead(RELAY_R0) << 3;
  BitMap2 |= !digitalRead(RELAY_R1) << 2;
  BitMap2 |= (storage.WinterMode & 1) << 1;
  BitMap2 |= (0 & 1U) << 0;      

}

void PublishTopic(const char* topic, JsonDocument& root)
{
  char Payload[PAYLOAD_BUFFER_LENGTH];

  size_t n=serializeJson(root,Payload);
  if (mqttClient.publish(topic, 1, true, Payload,n) != 0)
  {
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

  static UBaseType_t hwm = 0;
  
  for(;;)
  {
    stack_mon(hwm);    
    ulTaskNotifyTake(pdFALSE,portMAX_DELAY);    
    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        const int capacity = JSON_OBJECT_SIZE(8) + 8; // +8 as there is the Firmw String
        StaticJsonDocument<capacity> root;

        root["FW"]     = Firmw;                            //firmware revision
        root["FSta"]   = storage.FiltrationStart;          //Computed filtration start hour, in the morning (hours)
        root["FStaM"]  = storage.FiltrationStartMin;       //Earliest Filtration start hour, in the morning (hours)
        root["FDu"]    = storage.FiltrationDuration;       //Computed filtration duration based on water temperature (hours)
        root["FStoM"]  = storage.FiltrationStopMax;        //Latest hour for the filtration to run. Whatever happens, filtration won't run later than this hour (hour)
        root["FSto"]   = storage.FiltrationStop;           //Computed filtration stop hour, equal to FSta + FDu (hour)
        root["pHUTL"]  = storage.PhPumpUpTimeLimit / 60;   //Max allowed daily run time for the pH pump (/!\ mins)
        root["ChlUTL"] = storage.ChlPumpUpTimeLimit / 60;  //Max allowed daily run time for the Chl pump (/!\ mins)

        PublishTopic(PoolTopicSet1, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        const int capacity = JSON_OBJECT_SIZE(8);
        StaticJsonDocument<capacity> root;

        root["pHWS"]  = storage.PhPIDWindowSize / 1000 / 60;        //pH PID window size (/!\ mins)
        root["ChlWS"] = storage.OrpPIDWindowSize / 1000 / 60;       //Orp PID window size (/!\ mins)
        root["pHSP"]  = storage.Ph_SetPoint * 100;                  //pH setpoint (/!\ x100)
        root["OrpSP"] = storage.Orp_SetPoint;                       //Orp setpoint
        root["WSP"]   = storage.WaterTemp_SetPoint * 100;           //Water temperature setpoint (/!\ x100)
        root["WLT"]   = storage.WaterTempLowThreshold * 100;        //Water temperature low threshold to activate anti-freeze mode (/!\ x100)
        root["PSIHT"] = storage.PSI_HighThreshold * 100;            //Water pressure high threshold to trigger error (/!\ x100)
        root["PSIMT"] = storage.PSI_MedThreshold * 100;             //Water pressure medium threshold (unused yet) (/!\ x100)

        PublishTopic(PoolTopicSet2, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        const int capacity = JSON_OBJECT_SIZE(6);
        StaticJsonDocument<capacity> root;

        root["pHC0"]  = storage.pHCalibCoeffs0;            //pH sensor calibration coefficient C0
        root["pHC1"]  = storage.pHCalibCoeffs1;            //pH sensor calibration coefficient C1
        root["OrpC0"] = storage.OrpCalibCoeffs0;           //Orp sensor calibration coefficient C0
        root["OrpC1"] = storage.OrpCalibCoeffs1;           //Orp sensor calibration coefficient C1
        root["PSIC0"] = storage.PSICalibCoeffs0;           //Pressure sensor calibration coefficient C0
        root["PSIC1"] = storage.PSICalibCoeffs1;           //Pressure sensor calibration coefficient C1

        PublishTopic(PoolTopicSet3, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        const int capacity = JSON_OBJECT_SIZE(8);
        StaticJsonDocument<capacity> root;

        root["pHKp"]  = storage.Ph_Kp;    //pH PID coeffcicient Kp
        root["pHKi"]  = storage.Ph_Ki;    //pH PID coeffcicient Ki
        root["pHKd"]  = storage.Ph_Kd;    //pH PID coeffcicient Kd

        root["OrpKp"] = storage.Orp_Kp;    //Orp PID coeffcicient Kp
        root["OrpKi"] = storage.Orp_Ki;    //Orp PID coeffcicient Ki
        root["OrpKd"] = storage.Orp_Kd;    //Orp PID coeffcicient Kd

        root["Dpid"]   = storage.DelayPIDs;     //Delay from FSta for the water regulation/PIDs to start (mins) 
        root["PubP"]   = storage.PublishPeriod/1000; // Settings publish period in sec

        PublishTopic(PoolTopicSet4, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        const int capacity = JSON_OBJECT_SIZE(4);
        StaticJsonDocument<capacity> root;

        root["pHTV"]  = storage.pHTankVol;           //Acid tank nominal volume (Liters)
        root["ChlTV"] = storage.ChlTankVol;          //Chl tank nominal volume (Liters)
        root["pHFR"]  = storage.pHPumpFR;            //Acid pump flow rate (L/hour)
        root["OrpFR"] = storage.ChlPumpFR;           //Chl pump flow rate (L/hour)

        PublishTopic(PoolTopicSet5, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    //display remaining RAM space. For debug
    Debug.print(DBG_DEBUG,"[memCheck]: %db",freeRam());
  }  
}

//PublishData loop. Publishes system info/data to MQTT broker every XX secs (30 secs by default)
void MeasuresPublish(void *pvParameters)
{ 
  while(!startTasks);

  static UBaseType_t hwm = 0;

  for(;;)
  {
    stack_mon(hwm);      
    ulTaskNotifyTake(pdFALSE,(TickType_t)storage.PublishPeriod);
    
    //Store the GPIO states in one Byte (more efficient over MQTT)
    EncodeBitMap();

    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        //Will publish something like {"Tmp":818,"pH":321,"PSI":56,"Orp":583,"FilUpT":8995,"PhUpT":0,"ChlUpT":0}
        const int capacity = JSON_OBJECT_SIZE(7);
        StaticJsonDocument<capacity> root;

        root["TE"]      = storage.TempExternal * 100;        // /!\ x100
        root["Tmp"]     = storage.TempValue * 100;
        root["pH"]      = storage.PhValue * 100;
        root["PSI"]     = storage.PSIValue * 100;
        root["Orp"]     = storage.OrpValue;
        root["PhUpT"]   = PhPump.UpTime / 1000;
        root["ChlUpT"]  = ChlPump.UpTime / 1000;

        PublishTopic(PoolTopicMeas1, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    //Second MQTT publish to limit size of payload at once
    if (mqttClient.connected())
    {
        //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
        //Will publish something like {"AcidF":100,"ChlF":100,"IO":11,"IO2":0}
        const int capacity = JSON_OBJECT_SIZE(4);
        StaticJsonDocument<capacity> root;

        root["AcidF"] = storage.AcidFill - PhPump.GetTankUsage();
        root["ChlF"]  = storage.ChlFill - ChlPump.GetTankUsage();
        root["IO"]    = BitMap1;
        root["IO2"]   = BitMap2;

        PublishTopic(PoolTopicMeas2, root);
    }
    else
        Debug.print(DBG_ERROR,"Failed to connect to the MQTT broker");

    //display remaining RAM space. For debug
    Debug.print(DBG_DEBUG,"[memCheck]: %db",freeRam());
  }  
}