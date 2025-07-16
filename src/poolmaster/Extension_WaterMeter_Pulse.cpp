
// Credits : 
//  https://github.com/hugokernel/esphome-water-meter/blob/master/README.md

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)

#include "Extension_WaterMeter_Pulse.h"
#include <driver/pcnt.h>

ExtensionStruct myWaterMeterPulse = {0};

static int      myWMPulsePerLiter   = 1;       // 1 pulse each liter
static int16_t  myWaterMeterCounter = 1;

void ExtensionsPublishTopic(char*, JsonDocument&);
int ExtensionsReadRetainedTopic(char*, JsonDocument&);

void WaterMeterPulseSaveMeasures(void *pvParameters)
{
    if (!myWaterMeterPulse.detected) return;

    //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
    DynamicJsonDocument root(1024);
    
    if (myWaterMeterCounter)    root["Counter"]       = myWaterMeterCounter;
    if (myWMPulsePerLiter)      root["PulsePerLiter"] = myWMPulsePerLiter;

    ExtensionsPublishTopic(myWaterMeterPulse.MQTTTopicMeasures, root);
}

void WaterMeterPulseLoadMeasures(void *pvParameters)
{
    if (!myWaterMeterPulse.detected) return;

    //send a JSON to MQTT broker. /!\ Split JSON if longer than 100 bytes
    DynamicJsonDocument root(1024);
    if (!ExtensionsReadRetainedTopic(myWaterMeterPulse.MQTTTopicMeasures, root)) 
        return;

    int value = root["Counter"];
    if (value) myWaterMeterCounter = value;
    value = root["PulsePerLiter"];
    if (value) myWMPulsePerLiter   = value;
}

void WaterMeterPulseTask(void *pvParameters)
{
   // Debug.print(DBG_INFO,"[WaterMeterPulseTask] Start");
    if (!myWaterMeterPulse.detected) return;
    int16_t pulseCount      = 0;
    int16_t InitPulseCount  = 0;

   // Debug.print(DBG_DEBUG,"[WaterMeterPulseTask] Load measures");
    WaterMeterPulseLoadMeasures(pvParameters);  // read mqtt values, might be externally updated

    // loop till counter does not change anymore
    pcnt_get_counter_value(PCNT_UNIT_0, &pulseCount);
 //   Debug.print(DBG_DEBUG,"[WaterMeterPulseTask] pcnt_get_counter_value %d %d",pulseCount, InitPulseCount );
 
 //   if (!pulseCount) return;

    while (pulseCount != InitPulseCount) {
        InitPulseCount = pulseCount;
        delay(2000);
        pcnt_get_counter_value(PCNT_UNIT_0, &pulseCount);
   //     Debug.print(DBG_DEBUG,"[WaterMeterPulseTask in the loop] pcnt_get_counter_value %d",pulseCount );
    }
  
    // save Counter to MQTT and ready for next loop
    myWaterMeterCounter += pulseCount * myWMPulsePerLiter;
    WaterMeterPulseSaveMeasures(pvParameters);
   // Debug.print(DBG_DEBUG,"[WaterMeterPulseTask] counter=%d", myWaterMeterCounter);
    
    pcnt_counter_pause(PCNT_UNIT_0);
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);
}

ExtensionStruct WaterMeterPulse_Init(const char *name, int IO)
{
    // PCNT counter
#define PCNT_H_LIM_VAL      1000        //  review these values, based on your watermerter  
#define PCNT_THRESH1_VAL    500         //
#define PCNT_THRESH0_VAL   -500         //

    // *************verif pullup ?
    // *************verif sens input 
    pcnt_config_t pcnt_config = {};
    pcnt_config.pulse_gpio_num = IO;
    pcnt_config.ctrl_gpio_num = PCNT_PIN_NOT_USED;
    pcnt_config.channel = PCNT_CHANNEL_0;
    pcnt_config.unit = PCNT_UNIT_0;
    pcnt_config.pos_mode = PCNT_COUNT_INC;
    pcnt_config.neg_mode = PCNT_COUNT_DIS;
    pcnt_config.lctrl_mode = PCNT_MODE_KEEP;
    pcnt_config.hctrl_mode = PCNT_MODE_KEEP;
    pcnt_config.counter_h_lim = PCNT_H_LIM_VAL;
    pcnt_unit_config(&pcnt_config);

    // *************verif filter
    pcnt_set_filter_value(PCNT_UNIT_0, 100);
    pcnt_filter_enable(PCNT_UNIT_0);
    pcnt_set_event_value(PCNT_UNIT_0, PCNT_EVT_THRES_1, PCNT_THRESH1_VAL);
    pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_THRES_1);
    /* Enable events on zero, maximum and minimum limit values */
    pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_ZERO);
    pcnt_event_enable(PCNT_UNIT_0, PCNT_EVT_H_LIM);
    pcnt_counter_pause(PCNT_UNIT_0);
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);
  
    // Init structure
    myWaterMeterPulse.name              = name;
    myWaterMeterPulse.Task              = WaterMeterPulseTask;
    myWaterMeterPulse.detected          = true;
    myWaterMeterPulse.frequency         = 800;     // check every 800 ms if counter changes
    myWaterMeterPulse.LoadSettings      = 0;
    myWaterMeterPulse.SaveSettings      = 0;
    myWaterMeterPulse.LoadMeasures      = WaterMeterPulseLoadMeasures;
    myWaterMeterPulse.SaveMeasures      = WaterMeterPulseSaveMeasures;
    myWaterMeterPulse.HistoryStats      = 0;
    myWaterMeterPulse.MQTTTopicSettings = 0;
    myWaterMeterPulse.MQTTTopicMeasures = ExtensionsCreateMQTTTopic(myWaterMeterPulse.name, "");

    return myWaterMeterPulse;
}

#endif

