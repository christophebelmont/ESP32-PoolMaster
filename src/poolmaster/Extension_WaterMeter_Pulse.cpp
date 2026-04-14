
// Credits : 
//  https://github.com/hugokernel/esphome-water-meter/blob/master/README.md

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)

#include "Extension_WaterMeter_Pulse.h"
extern void SuperVisor_Message(const char *, char*);

ExtensionStruct myWaterMeterPulse = {0};
volatile double   myWaterMeterCounter = 0;
volatile double   myWMLiterPerPulse   = 1.0;  // K=1 liter/pulse
volatile uint32_t myWMDebounce        = 250;  // Debounce time in ms
volatile int      myWMGPIO          = 0;    // disabled by default, suggest GPIO 15

extern void PublishTopic(const char*, JsonDocument&);
void WaterMeterPulsePubMQTT(void)
{
    static double oldvalues = -1;
    double n = myWaterMeterCounter+myWMLiterPerPulse+myWMDebounce+myWMGPIO;
    if (n != oldvalues) oldvalues=n;
    else return; // no change to publish

    DynamicJsonDocument root(1024);
    root["GPIO"] = myWMGPIO;
    root["K"]    = myWMLiterPerPulse;
    root["D"]    = myWMDebounce;
    root["L"]    = (int)myWaterMeterCounter;

    char topic[50];
    const char *roottopic = PMConfig.get<const char*>(MQTT_TOPIC);
    if (strcmp(roottopic, "") == 0) return;
    if (strcmp(roottopic, "none") == 0) return;
    sprintf(topic, "%s/%s", roottopic, myWaterMeterPulse.name);
 //   PublishTopic(topic, root);
}

void WaterMeterPulseValues(char* buffer)
{
    if (myWMGPIO>0) sprintf(buffer, "%.0f", myWaterMeterCounter);
    else            strcpy(buffer, "none");
}

void WaterMeterPulseLoadSettings(void *pvParameters)
{
    int newgpio = 0;
 
    // Get Watermeter settings from SuperVisor, if any
    char buffer[I2C_MAXMESSAGE+5] = {0};
    SuperVisor_Message("GET_WATERMETER_COUNTER", buffer);
    if ((strcmp(buffer, "none")!=0) &&
        (strcmp(buffer, "")!=0)) {
        sscanf(buffer, "%lf", &myWaterMeterCounter);
    }
    SuperVisor_Message("GET_WATERMETER_K", buffer);
    if ((strcmp(buffer, "none")!=0) &&
        (strcmp(buffer, "")!=0)) {
        sscanf(buffer, "%lf", &myWMLiterPerPulse);
    }
    SuperVisor_Message("GET_WATERMETER_D", buffer);
    if ((strcmp(buffer, "none")!=0) &&
        (strcmp(buffer, "")!=0)) {
        sscanf(buffer, "%d", &myWMDebounce);
    }
    SuperVisor_Message("GET_WATERMETER_GPIO", buffer);
    if (strcmp(buffer, "none")!=0) {
        if (strcmp(buffer, "")==0) newgpio = 0;
        else sscanf(buffer, "%d", &newgpio);
        if (myWMGPIO != newgpio) myWMGPIO = newgpio;
    }
    WaterMeterPulsePubMQTT();
}

void WaterMeterPulseTask(void *pvParameters)
{
    if (myWMGPIO<1) return;
}

volatile uint8_t LastReading = HIGH;
volatile bool meterblocked = false;   // meter can stop when GPIO is at LOW level
  
void ARDUINO_ISR_ATTR WMinterrupt() {
    if (myWMGPIO<1) return;
    
    uint8_t Reading = digitalRead(myWMGPIO);

    if (Reading != LastReading) meterblocked = false; // state has changed
    LastReading = Reading;

    if ((Reading == LOW) && (!meterblocked)) {
        meterblocked = true;
        myWaterMeterCounter += myWMLiterPerPulse;
        WaterMeterPulsePubMQTT();
    }
}

ExtensionStruct WaterMeterPulse_Init(char *name, int defaultIO)
{
    // Init structure
    myWaterMeterPulse.name              = name;
    myWaterMeterPulse.Task              = WaterMeterPulseTask;
    myWaterMeterPulse.detected          = true;
    myWaterMeterPulse.frequency         = myWMDebounce;     // check every xxx ms if counter changes (~ debounce time)
    myWaterMeterPulse.LoadSettings      = WaterMeterPulseLoadSettings;
    myWaterMeterPulse.SaveSettings      = 0;
    myWaterMeterPulse.LoadMeasures      = 0;
    myWaterMeterPulse.SaveMeasures      = 0;
    myWaterMeterPulse.Values            = WaterMeterPulseValues;
    myWaterMeterPulse.HistoryStats      = 0;
    
    pinMode(myWMGPIO, INPUT_PULLDOWN);
    attachInterrupt(myWMGPIO, WMinterrupt, CHANGE);

    return myWaterMeterPulse;
}

#endif

