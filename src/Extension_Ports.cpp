
#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"


#if defined(_EXTENSIONS_)

// *************************
// Addons on Extension Ports
// *************************
// Check GPIO IDs possibilities and limitations 
// https://www.upesy.fr/blogs/tutorials/esp32-pinout-reference-gpio-pins-ultimate-guide
//
// GPIOs Available :
//  GPIO O5   --> Input+Output + pullup/down
//  GPIO 12   --> Input+Output + pullup/down
//  GPIO 14   --> Input+Output + pullup/down
//  GPIO 15   --> Input(pullup) + output 
//  GPIO 35   --> Input only, no pullup/pulldown
//  I2C       --> Address

#include "Extension_Ports.h"
#include "Extension_SuperVisor.h"
#include "Extension_TFAVenice_RF433T.h"
#include "Extension_WaterMeter_Pulse.h"
#include "Extension_BME680_0x77.h"
#include "Extension_BMP280_0x76.h" 
#include "Extension_SHT40_0x44.h"

#define _GPIO_TFA_RF433T_        5  // Water Temperature broadcasted to TFA 433Mhz receiver
#define _GPIO_WATERMETER_PULSE_ 15  // Count WaterMeter pulse from external reader.
#define _I2C_                    0
#define _OTHER_                 -1

struct ListExtensions
{
    const char *name;   // name of the extension
    initfunction init;  // function to init the extension
    int port;           // Port (GPIO, I2C, etc...)
};

struct ListExtensions knownI2C[] = {
    {"SUPERVISOR",  0, SUPERVISOR_I2C_Address},  // 0x07
    {"PCF8574",     0, 0x20 },  // PoolMaster StatusLights
    {"PCF8574A",    0, 0x38 },  // PoolMaster StatusLights
    {"SHT40",       0, 0x44 },  // ENV IV M5Stack = SHT40 + BMP280
    {"ADS1115",     0, 0x48 },  // PoolMaster AnalogPoll
    {"BMP280",      0, 0x76 },  // ENV IV M5Stack = SHT40 + BMP280
    {"BME680",      0, 0x77 },  // ENV Pro M5Stack
};

// *******************************************
// * This is where we declare our extensions *
// *******************************************
struct ListExtensions myListExtensions[] = {
    {"SuperVisor",              SuperVisor_Init,        _I2C_ },   // Talk to SuperVisor via I2C
    {"SuperVisor",              SuperVisor_Info_Init,   _OTHER_ }, // Talk to SuperVisor via Serial
    {"PoolRoom/TFAVenice",      TFAVenice_RF433T_Init,  _GPIO_TFA_RF433T_ },
    {"PoolRoom/TempHumidity",   SHT40_0x44_Init,        _I2C_ },
    {"PoolRoom/Temperature",    BMP280_0x76_Init,       _I2C_ },
    {"TechRoom/TempHumidity",   BME680_0x77_Init,       _I2C_ },
    {"TechRoom/WaterMeter",     WaterMeterPulse_Init,   _GPIO_WATERMETER_PULSE_ },
//    {"PoolRoom/MiLight",        MiLight_Init,           _I2C_ },
//    {"PoolRoom/PoolCover",      PoolCover_Init,         _OTHER_ },
//    {"TechRoom/PoolHeatPump",   PoolHeatPump_Init,      _OTHER_ },
};
// *******************************************

void stack_mon(UBaseType_t &);
ExtensionStruct *myExtensions = 0;
static int _NbExtensions = 0;

/* ********************************************************
 * The Async MQTT engine for Extensions
 * Can't use the Poolmaster mqtt instance
 * because of how we store and read values in mqtt broker
*/
AsyncMqttClient ExtensionsMqttClient;
#define PAYLOAD_BUFFER_LENGTH 200
static bool ExtensionsMqttOnRead = false;
static char ExtensionsMqttMsg[PAYLOAD_BUFFER_LENGTH] = "";
static char *ExtensionsMqttMsgTopic = 0;
TimerHandle_t mqttExtensionsReconnectTimer;

// Removes the duplicates "/" from the string provided as an argument
static void mqttRemoveDoubleSlash(char *str)
{
  int i,j,len,len1;
  /*calculating length*/
  for(len=0; str[len]!='\0'; len++);
  /*assign 0 to len1 - length of removed characters*/
  len1=0;
  /*Removing consecutive repeated characters from string*/
  for(i=0; i<(len-len1);) {
      if((str[i]==str[i+1]) && (str[i] == '/')) {
          /*shift all characters*/
          for(j=i;j<(len-len1);j++)
              str[j]=str[j+1];
          len1++;
      }
      else i++;
  }
}

void connectExtensionsToMqtt() {
 //  Serial.println("Extensions Connecting to MQTT...");
    ExtensionsMqttClient.connect();
}
void onExtensionsMqttConnect(bool sessionPresent) {
 //   Serial.println("Extensions Connected to MQTT.");
 //   Serial.print("Session present: ");
 //   Serial.println(sessionPresent);
}
void onExtensionsMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
//    Serial.println("Extensions Disconnected from MQTT.");
  
    if (WiFi.isConnected()) {
      xTimerStart(mqttExtensionsReconnectTimer, 0);
    }
  }
void onExtensionsMqttSubscribe(uint16_t packetId, uint8_t qos) {
 //   Serial.println("Extensions Subscribe acknowledged.");
 //   Serial.print("  packetId: ");
 //   Serial.println(packetId);
 //   Serial.print("  qos: ");
 //   Serial.println(qos);
  }
void onExtensionsMqttUnsubscribe(uint16_t packetId) {
 //   Serial.println("Extensions Unsubscribe acknowledged.");
 //   Serial.print("  packetId: ");
  //  Serial.println(packetId);
}
// Extension MQTT callback reading RETAINED values
void onExtensionsMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    if (!ExtensionsMqttMsgTopic) return;
    if (!topic) return;
    if (strcmp(topic, ExtensionsMqttMsgTopic) == 0)
        for (uint8_t i = 0; i < len; i++)
            ExtensionsMqttMsg[i] = payload[i];         
}
void onExtensionsMqttPublish(uint16_t packetId) {
 // Serial.println("Extensions Publish acknowledged.");
 // Serial.print("  packetId: ");
 // Serial.println(packetId);
}
void ExtensionsMqttInit()
{   
   // ExtensionsMqttClient.disconnect();
    // Init 2nd Async MQTT session for Extensions
    mqttExtensionsReconnectTimer = xTimerCreate("mqttExtensionsTimer",
        pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectExtensionsToMqtt));
 
    ExtensionsMqttClient.onConnect(onExtensionsMqttConnect);
    ExtensionsMqttClient.onDisconnect(onExtensionsMqttDisconnect);
    ExtensionsMqttClient.onSubscribe(onExtensionsMqttSubscribe);
    ExtensionsMqttClient.onUnsubscribe(onExtensionsMqttUnsubscribe);
    ExtensionsMqttClient.onMessage(onExtensionsMqttMessage);
    ExtensionsMqttClient.onPublish(onExtensionsMqttPublish);
    ExtensionsMqttClient.setServer(storage.MQTT_IP, storage.MQTT_PORT);
    if (strlen(storage.MQTT_LOGIN) > 0)
        ExtensionsMqttClient.setCredentials(storage.MQTT_LOGIN, storage.MQTT_PASS);

    //Debug.print(DBG_INFO,"[ExtensionsMqttInit] Connect to MQTT %s, %d, %s, %s, %s %s",
    //            storage.MQTT_IP.toString().c_str(),
    //           storage.MQTT_PORT,storage.MQTT_LOGIN,storage.MQTT_PASS,
    //           storage.MQTT_ID,storage.MQTT_TOPIC);

    if (storage.MQTT_ID) ExtensionsMqttClient.setClientId(storage.MQTT_ID);
    ExtensionsMqttClient.connect();
    //Debug.print(DBG_INFO, "[ExtensionsMqttInit] Connect to MQTT rc=%d", ExtensionsMqttClient.state());
}

char *ExtensionsCreateMQTTTopic(const char *t1, const char *t2)
{
    char *topic = (char *)malloc(strlen(storage.MQTT_TOPIC) + strlen(t1) + strlen(t2) + 1);
    sprintf(topic, "%s/%s%s", storage.MQTT_TOPIC, t1, t2);
    return topic;
}
void ExtensionsPublishTopic(char *topic, JsonDocument &root)
{
    ExtensionsMqttClient.connect();
    if (!ExtensionsMqttClient.connected()) {
       // Debug.print(DBG_ERROR, "[ExtensionsPublishTopic] Failed to connect to the MQTT broker for %s", topic);
        return;
    }
    char Payload[PAYLOAD_BUFFER_LENGTH];
    size_t n = serializeJson(root, Payload);
    mqttRemoveDoubleSlash(topic);

    if (ExtensionsMqttClient.publish(topic, 1, true, Payload, n) != 0) {
        delay(50);
//        Debug.print(DBG_DEBUG, "Publish Extensions: %s - size: %d/%d", Payload, root.size(), n);
        return;
    }

    Debug.print(DBG_DEBUG, "Unable to publish: %s", Payload);
}

int ExtensionsReadRetainedTopic(char *topic, JsonDocument &root)
{
    int nbTry = 5;
    while (ExtensionsMqttOnRead) {
        delay(200);
        nbTry--;
        if (!nbTry)
            ExtensionsMqttOnRead = false;
    }
    if (!ExtensionsMqttClient.connected()) {
      //  Debug.print(DBG_ERROR, "[ExtensionsReadRetainedTopic] Failed to connect to the MQTT broker for %s", topic);
        return 0;
    }
    mqttRemoveDoubleSlash(topic);
    ExtensionsMqttOnRead = true;
    ExtensionsMqttMsgTopic = topic;
    ExtensionsMqttClient.subscribe(topic, 1);
    ExtensionsMqttClient.unsubscribe(topic);
    ExtensionsMqttOnRead = false;
    if (!ExtensionsMqttMsg[0]) return 0;

    deserializeJson(root, ExtensionsMqttMsg);
    ExtensionsMqttOnRead = false;
    ExtensionsMqttMsgTopic = 0;
    ExtensionsMqttMsg[0] = '\0';
    return 1;
}

/* **********************************
 * End of MQTT 
 * **********************************
 * /

/* **********************************
 * Init All Extensions and run loops
 * **********************************
*/

void ExtensionsLoop(void *pvParameters)
{
    ExtensionStruct *TheExtension = (ExtensionStruct *)pvParameters;

    if (!TheExtension->detected) return;
    if (!TheExtension->Task)     return;

    while (!startTasks) delay(200);
    vTaskDelay(500 / portTICK_PERIOD_MS); // Scheduling offset

   // Debug.print(DBG_INFO, "[Start ExtensionLoop] name %s freq: %d ms", TheExtension->name, TheExtension->frequency);
    for (;;) {
    //    Debug.print(DBG_INFO, "[In ExtensionLoop] name %s freq: %d ms", TheExtension->name, TheExtension->frequency);
        TheExtension->Task(pvParameters);
        vTaskDelay(pdMS_TO_TICKS(TheExtension->frequency));
    }
}

const char *searchI2CName(int address)
{
    int nb = sizeof(knownI2C)/sizeof(knownI2C[0]);
    for (int i=0; i<nb; i++)
       if (address == knownI2C[i].port) return knownI2C[i].name;
    return "UNKNOWN";
}

void ExtensionsInit()
{
    // https://i2cdevices.org/addresses/
    Debug.print(DBG_INFO, "Scanning I2C bus...");
    byte error, address;
    for (address = 0x01; address < 0x7f; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) Debug.print(DBG_INFO,"\tI2C device %s found at address 0x%02X", searchI2CName(address), address); 
        else if (error == 4) Debug.print(DBG_INFO,"\tI2C device unknown, error at address 0x%02X", address);
    }
    Debug.print(DBG_INFO, "...done");

    ExtensionsMqttInit();
    
    Debug.print(DBG_INFO, "[Extensions Init Action]");
    if (sizeof(myListExtensions[0]))
        _NbExtensions = sizeof(myListExtensions)  / sizeof(myListExtensions[0]);
    Debug.print(DBG_INFO, "[Extensions Init] with %d extensions", _NbExtensions);

    if (_NbExtensions) myExtensions = (ExtensionStruct*) malloc(_NbExtensions * sizeof(ExtensionStruct));

    for (int i = 0; i < _NbExtensions; i++) {    
        myExtensions[i] = myListExtensions[i].init(myListExtensions[i].name, myListExtensions[i].port);
        if (myExtensions[i].detected)
            xTaskCreatePinnedToCore(
                ExtensionsLoop,
                myExtensions[i].name,
                3072, // can we decrease this value ??
                &myExtensions[i],
                1,
                nullptr,
                xPortGetCoreID());
        }
}
/*
void ExtensionsPublishSettings(void *pvParameters)
{
    for (int i = 0; i < _NbExtensions; i++)
    {
        if (myExtensions[i].SaveSettings) {
            myExtensions[i].SaveSettings(pvParameters);
            SANITYDELAY;
        }
    }
}

void ExtensionsPublishMeasures(void *pvParameters)
{
    for (int i = 0; i < _NbExtensions; i++) {
        if (myExtensions[i].SaveMeasures) {
            myExtensions[i].SaveMeasures(pvParameters);
            SANITYDELAY;
        }
    }
}

void ExtensionsHistoryStats(void *pvParameters)
{
    for (int i = 0; i < _NbExtensions; i++) {
        if (myExtensions[i].HistoryStats) {
            myExtensions[i].HistoryStats(pvParameters);
            SANITYDELAY;
        }
    }
}
*/
int ExtensionsNb()
{
    return _NbExtensions;
}

#endif