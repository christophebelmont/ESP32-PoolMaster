// Adafruit SHT40 library
// https://github.com/adafruit/Adafruit_SHT4X

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)

#include "Extension_SHT40_0x44.h"                     
#include <Adafruit_SHT4x.h>

// Internal object
ExtensionStruct mySHT40_0x44 = {0};
static float mySHT40_0x44_Temperature  = -1;
static float mySHT40_0x44_Humidity     = -1;
static Adafruit_SHT4x myAdaSht40 = Adafruit_SHT4x();

#define SHT40_I2C_Address 0x44

// External functions
extern void PublishTopic(const char*, JsonDocument&);
extern void lockI2C();
extern void unlockI2C();

// Extension properties
// ********************

void SHT40_0x44_SaveMeasures (void *pvParameters)
{
    if (!mySHT40_0x44.detected) return;

    //send a JSON to MQTT broker 
    DynamicJsonDocument root(1024);

    char value[15];
    sprintf(value, "%.1f", mySHT40_0x44_Temperature);
    root["Temperature"] = value;
    sprintf(value, "%.1f", mySHT40_0x44_Humidity);
    root["Humidity"]    = value;

    char topic[50];
    const char *roottopic = PMConfig.get<const char*>(MQTT_TOPIC);
    sprintf(topic, "%s/%s", roottopic, mySHT40_0x44.name);
 //   PublishTopic(topic, root);
}

void SHT40_0x44_Values(char* buffer)
{
    sprintf(buffer, "T=%d°C H=%d%%rh", (int)mySHT40_0x44_Temperature, (int)mySHT40_0x44_Humidity);
}

void SHT40_0x44_Task(void *pvParameters)
{
    if (!mySHT40_0x44.detected) return;

    int delta_temp = 0;
    sensors_event_t humidity, temp;
    lockI2C();
    myAdaSht40.getEvent(&humidity, &temp);
    mySHT40_0x44_Temperature   = temp.temperature + delta_temp;
    mySHT40_0x44_Humidity      = humidity.relative_humidity;
    unlockI2C();
    SHT40_0x44_SaveMeasures(pvParameters);
}

ExtensionStruct SHT40_0x44_Init(char *name, int IO)
{
    /* Initialize the library and interfaces */
    mySHT40_0x44.detected = false;
    lockI2C();
    Wire.beginTransmission(SHT40_I2C_Address);
    byte error = Wire.endTransmission();
    //   if (error==0) Debug.print(DBG_INFO,"SHT40 %x detected",SHT40_I2C_Address);
    //  else Debug.print(DBG_INFO,"SHT40 %s not detected",SHT40_I2C_Address);
    if (error==0) {
        myAdaSht40.begin(&Wire);
        mySHT40_0x44.detected = true;
        myAdaSht40.setPrecision(SHT4X_MED_PRECISION);
        myAdaSht40.setHeater(SHT4X_NO_HEATER);
    }

    unlockI2C();

    mySHT40_0x44.name                = name;
    mySHT40_0x44.Task                = SHT40_0x44_Task;
    mySHT40_0x44.frequency           = 30000;     // Update values every xxx msecs.
    mySHT40_0x44.LoadSettings        = 0;
    mySHT40_0x44.SaveSettings        = 0;
    mySHT40_0x44.LoadMeasures        = 0;
    mySHT40_0x44.SaveMeasures        = SHT40_0x44_SaveMeasures;
    mySHT40_0x44.Values              = SHT40_0x44_Values;
    mySHT40_0x44.HistoryStats        = 0;

    return mySHT40_0x44;
}

#endif

