
#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)

#include "Extension_INA226_0x40.h"
#include <INA226_WE.h>

// Internal object
ExtensionStruct INA226_0x40 = {0};
static float shuntVoltage_mV = 0.0;
static float loadVoltage_V = 0.0;
static float busVoltage_V = 0.0;
static float current_mA = 0.0;
static float power_mW = 0.0; 

#define INA226_I2C_Address 0x40
static INA226_WE ina226(&Wire, INA226_I2C_Address); // I2C_0

// External functions
extern void PublishTopic(const char*, JsonDocument&);
extern void lockI2C();
extern void unlockI2C();

// Extension properties
// ********************

void INA226_0x40_SaveMeasures (void *pvParameters)
{
    if (!INA226_0x40.detected) return;

    //send a JSON to MQTT broker 
    DynamicJsonDocument root(1024);

    char value[15];
  /*  sprintf(value, "%.1f", shuntVoltage_mV);
    root["shuntVoltage_mV"] = value;
    sprintf(value, "%.1f", busVoltage_V);
    root["busVoltage_V"]    = value;*/
    sprintf(value, "%.1f", current_mA);
    root["current_mA"]      = value;
    sprintf(value, "%.1f", power_mW);
    root["power_mW"]        = value;
    sprintf(value, "%.1f", loadVoltage_V);
    root["loadVoltage_V"]   = value;

    char topic[50];
    const char *roottopic = PMConfig.get<const char*>(MQTT_TOPIC);
    sprintf(topic, "%s/%s", roottopic, INA226_0x40.name);
 //   PublishTopic(topic, root);
}

void INA226_0x40_Values(char* buffer)
{
    //sprintf(buffer, "sV=%.1fmV bV=%.1fV C=%.1fmA P=%.1fmW lV=%.1fV", shuntVoltage_mV, busVoltage_V, current_mA, power_mW, loadVoltage_V);
    sprintf(buffer, "C=%.1fmA P=%.1fmW V=%.1fV", current_mA, power_mW, loadVoltage_V);
}

void INA226_0x40_Task(void *pvParameters)
{
    if (!INA226_0x40.detected) return;

    lockI2C();
    ina226.readAndClearFlags();
    shuntVoltage_mV = ina226.getShuntVoltage_mV();
    busVoltage_V = ina226.getBusVoltage_V();
    current_mA = ina226.getCurrent_mA();
    power_mW = ina226.getBusPower();
    loadVoltage_V  = busVoltage_V + (shuntVoltage_mV/1000);
    unlockI2C();
    INA226_0x40_SaveMeasures(pvParameters);
}

ExtensionStruct INA226_0x40_Init(char *name, int IO)
{
    /* Initialize the library and interfaces */
    INA226_0x40.detected = false;
    lockI2C();
    Wire.beginTransmission(INA226_I2C_Address);
    byte error = Wire.endTransmission();

    if (error==0) {
        ina226.init();
        INA226_0x40.detected = true;
    }
        
    unlockI2C();

    INA226_0x40.name                = name;
    INA226_0x40.Task                = INA226_0x40_Task;
    INA226_0x40.frequency           = 5000;     // Update values every xxx msecs.
    INA226_0x40.LoadSettings        = 0;
    INA226_0x40.SaveSettings        = 0;
    INA226_0x40.LoadMeasures        = 0;
    INA226_0x40.SaveMeasures        = INA226_0x40_SaveMeasures;
    INA226_0x40.Values              = INA226_0x40_Values;
    INA226_0x40.HistoryStats        = 0;

    return INA226_0x40;
}


#endif

