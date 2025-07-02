// Tech Room : Air Temp., Pressure, Humidity BASED ON BME68X - i2c

// Bosch Bsec librares do not work !
// https://github.com/m5stack/M5Unit-ENV/blob/master/examples/ENV_PRO/ENV_PRO.ino
// BME68x Sensor library: https://github.com/boschsensortec/Bosch-BME68x-Library
// BSEC2 Software Library: https://github.com/boschsensortec/Bosch-BSEC2-Library
// 
// but use Adafruit BME680 library from example: 
// https://github.com/bborncr/ESP32_BME688/blob/main/ESP32_BME688.ino

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)

#include "Extension_BME680_0x77.h"                     
#include "Adafruit_BME680.h"

// Internal object
ExtensionStruct myBME680_0x77 = {0};
static float myBME680_0x77_Temperature  = -1;
static float myBME680_0x77_Humidity     = -1;
static float myBME680_0x77_Pressure     = -1;
static float myBME680_0x77_Gaz          = -1;

static Adafruit_BME680 myAdaBme(&Wire); // I2C_0

#define BME688_I2C_Address 0x77 

// External functions
extern void ExtensionsPublishTopic(char*, JsonDocument&);
extern void lockI2C();
extern void unlockI2C();

// Extension properties
// ********************

void BME680_0x77_SaveMeasures (void *pvParameters)
{
    if (!myBME680_0x77.detected) return;

    //send a JSON to MQTT broker 
    DynamicJsonDocument root(1024);

    char value[15];
    sprintf(value, "%.1f", myBME680_0x77_Temperature);
    root["Temperature"] = value;
    sprintf(value, "%.1f", myBME680_0x77_Humidity);
    root["Humidity"]    = value;
    sprintf(value, "%.1f", myBME680_0x77_Pressure);
    root["Pressure"]    = value;
    sprintf(value, "%.1f", myBME680_0x77_Gaz);
    root["Gaz"]         = myBME680_0x77_Gaz;

    ExtensionsPublishTopic(myBME680_0x77.MQTTTopicMeasures, root);
}

void BME680_0x77_Task(void *pvParameters)
{
    if (!myBME680_0x77.detected) return;

    int delta_temp = 0;
    lockI2C();
    if (! myAdaBme.performReading()) {
        Debug.print(DBG_ERROR,"[BME680_0x77Task] Failed to perform reading :(");
    }
    else {
        myBME680_0x77_Temperature   = myAdaBme.temperature + delta_temp;
        myBME680_0x77_Humidity      = myAdaBme.humidity;
        myBME680_0x77_Pressure      = myAdaBme.pressure / 100.0;
        myBME680_0x77_Gaz           = myAdaBme.gas_resistance / 1000.0;
    }
    unlockI2C();
    
    BME680_0x77_SaveMeasures(pvParameters);
}

ExtensionStruct BME680_0x77_Init(const char *name, int IO)
{
    /* Initialize the library and interfaces */
    myBME680_0x77.detected = false;
    lockI2C();
    Wire.beginTransmission(BME688_I2C_Address);
    byte error = Wire.endTransmission();
    //   if (error==0) Debug.print(DBG_INFO,"BME68X %x detected",BME688_I2C_Address);
    //  else Debug.print(DBG_INFO,"BME68X %s not detected",BME688_I2C_Address);
    if (error==0) {
        myAdaBme.begin(BME688_I2C_Address);
        myBME680_0x77.detected = true;
        // Set up oversampling and filter initialization
        myAdaBme.setTemperatureOversampling(BME680_OS_8X);
        myAdaBme.setHumidityOversampling(BME680_OS_2X);
        myAdaBme.setPressureOversampling(BME680_OS_4X);
        myAdaBme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        myAdaBme.setGasHeater(320, 150); // 320*C for 150 ms
   
     //   Debug.print(DBG_INFO,"BME68X %x detected",BME688_I2C_Address);
    }
   // else Debug.print(DBG_INFO,"BME68X %s not detected",BME688_I2C_Address);
     
    unlockI2C();

    myBME680_0x77.name                = name;
    myBME680_0x77.Task                = BME680_0x77_Task;
    myBME680_0x77.frequency           = 30000;     // Update values every 30 secs.
    myBME680_0x77.LoadSettings        = 0;
    myBME680_0x77.SaveSettings        = 0;
    myBME680_0x77.LoadMeasures        = 0;
    myBME680_0x77.SaveMeasures        = BME680_0x77_SaveMeasures;
    myBME680_0x77.HistoryStats        = 0;
    myBME680_0x77.MQTTTopicSettings   = 0;
    myBME680_0x77.MQTTTopicMeasures   = ExtensionsCreateMQTTTopic(myBME680_0x77.name, "");

    return myBME680_0x77;
}


#endif

