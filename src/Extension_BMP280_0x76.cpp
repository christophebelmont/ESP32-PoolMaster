// Adafruit BMP280 library
// https://github.com/adafruit/Adafruit_BMP280

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)

#include "Extension_BMP280_0x76.h"                     
#include <Adafruit_BMP280.h>

// Internal object
ExtensionStruct myBMP280_0x76 = {0};
static float myBMP280_0x76_Temperature  = -1;
static float myBMP280_0x76_Pressure     = -1;
static Adafruit_BMP280 myAdaBmp(&Wire); // I2C_0

#define BMP280_I2C_Address 0x76

// External functions
extern void ExtensionsPublishTopic(char*, JsonDocument&);
extern void lockI2C();
extern void unlockI2C();

// Extension properties
// ********************

void BMP280_0x76_SaveMeasures (void *pvParameters)
{
    if (!myBMP280_0x76.detected) return;

    //send a JSON to MQTT broker 
    DynamicJsonDocument root(1024);

    char value[15];
    sprintf(value, "%.1f", myBMP280_0x76_Temperature);
    root["Temperature"] = value;
    sprintf(value, "%.1f", myBMP280_0x76_Pressure);
    root["Pressure"]    = value;

    ExtensionsPublishTopic(myBMP280_0x76.MQTTTopicMeasures, root);
}

void BMP280_0x76_Task(void *pvParameters)
{
    if (!myBMP280_0x76.detected) return;

    int delta_temp = 0;
    lockI2C();

    myBMP280_0x76_Temperature   = myAdaBmp.readTemperature() + delta_temp;
    myBMP280_0x76_Pressure      = myAdaBmp.readPressure() / 100.0;

    unlockI2C();
    
    BMP280_0x76_SaveMeasures(pvParameters);
}

ExtensionStruct BMP280_0x76_Init(const char *name, int IO)
{
    /* Initialize library and interfaces */
    myBMP280_0x76.detected = false;
    lockI2C();
    Wire.beginTransmission(BMP280_I2C_Address);
    byte error = Wire.endTransmission();
    //   if (error==0) Debug.print(DBG_INFO,"BMP280 %x detected",BMP280_I2C_Address);
    //  else Debug.print(DBG_INFO,"BMP280 %s not detected",BMP280_I2C_Address);
    if (error==0) {
        myAdaBmp.begin(BMP280_I2C_Address);
        myBMP280_0x76.detected = true;
        // Set up oversampling and filter initialization
        myAdaBmp.setSampling(Adafruit_BMP280::MODE_NORMAL,  /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,             /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,            /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,              /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500);         /* Standby time. */
    }

    unlockI2C();

    myBMP280_0x76.name                = name;
    myBMP280_0x76.Task                = BMP280_0x76_Task;
    myBMP280_0x76.frequency           = 3000;     // Update values every xxx msecs.
    myBMP280_0x76.LoadSettings        = 0;
    myBMP280_0x76.SaveSettings        = 0;
    myBMP280_0x76.LoadMeasures        = 0;
    myBMP280_0x76.SaveMeasures        = BMP280_0x76_SaveMeasures;
    myBMP280_0x76.HistoryStats        = 0;
    myBMP280_0x76.MQTTTopicSettings   = 0;
    myBMP280_0x76.MQTTTopicMeasures   = ExtensionsCreateMQTTTopic(myBMP280_0x76.name, "");

    return myBMP280_0x76;
}


#endif

