// I2C communication with ESP32 SuperVisor
// ***************************************


#include <Arduino.h>                // Arduino framework
#include "Config.h"
#include "PoolMaster.h"
#include <WiFi.h>

#if defined(_EXTENSIONS_)

#include "Extension_SuperVisor.h"
#include <uptime.h>                

const char _DELIMITER_[] = {0xEA,0xEA,0xEA};
#define I2C_MAXMESSAGE    64

ExtensionStruct mySuperVisor = {0};
uint8_t StatusLEDs = 0;

// External functions
extern void ExtensionsPublishTopic(char*, JsonDocument&);
extern void lockI2C();
extern void unlockI2C();
extern void mqttInit(void);
extern void mqttDisconnect(void);
extern bool saveConfig(void);
extern Preferences nvs;

// Configure Extension properties
// ******************************

void SuperVisor_SaveMeasures (void *pvParameters)
{
     if (!mySuperVisor.detected) return;
   // ExtensionsPublishTopic(mySuperVisor.MQTTTopicMeasures, root);
}


void SuperVisor_Message(const char *value, char* result) 
{
  lockI2C();

  Wire.beginTransmission(SUPERVISOR_I2C_Address);
  Wire.printf(value); // send the question to SuperVisor
  uint8_t error = Wire.endTransmission(true);
  delay(200);
  //Read from the I2C ESP slave, but read the entire buffer !
  uint8_t bytesReceived = Wire.requestFrom(SUPERVISOR_I2C_Address, I2C_MAXMESSAGE);
  int i=0;
  while (Wire.available()) result[i++] = (char)Wire.read(); 
  if (i>=I2C_MAXMESSAGE) i=I2C_MAXMESSAGE-1;
  result[i] = '\0';

  char *p = result;
  // find and remove our delimiter
  for (int i=0; *p && i<I2C_MAXMESSAGE; i++, p++) 
    if (strncmp(p, _DELIMITER_, sizeof(_DELIMITER_))==0) *p = '\0';
  if (i==I2C_MAXMESSAGE) Debug.print(DBG_DEBUG,"SuperVisor_Message==> NODELIMITER");

  // I2C-slave bug -- empty buffer
  while (Wire.available()) Wire.read() ; // empty buffer
 
  unlockI2C();
  delay(500);
}

void SuperVisor_DisplayPollMasterStatus()
{
  extern String Firmw;
  char buffer0[I2C_MAXMESSAGE+5]; 
  char buffer1[I2C_MAXMESSAGE+5];

  // send PoolMaster info to SuperVisor
  sprintf(buffer0, "PM_SSID=%s", WiFi.SSID().c_str());
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY
  sprintf(buffer0, "PM_RSSI=%d", WiFi.RSSI());
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY
  sprintf(buffer0, "PM_HOSTNAME=%s", WiFi.getHostname());
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY
  sprintf(buffer0, "PM_IP=%s", WiFi.localIP().toString());
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY
  sprintf(buffer0, "PM_MAC=%s",WiFi.macAddress().c_str());
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY
  sprintf(buffer0, "PM_FIRMW=%s", Firmw.c_str());
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY
  sprintf(buffer0, "PM_TFTFIRMW=%s", TFT_FIRMW);
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY
  uptime::calculateUptime();
  sprintf(buffer0, "PM_UPTIME=%dd-%02dh-%02dm-%02ds", uptime::getDays(), uptime::getHours(), uptime::getMinutes(), uptime::getSeconds());
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY
  sprintf(buffer0, "PM_MQTT_TOPIC=%s", storage.MQTT_TOPIC);
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY
  IPAddress MQTTservIP = storage.MQTT_IP;
  sprintf(buffer0, "PM_MQTT_SERVER=%s:%u", MQTTservIP.toString(), storage.MQTT_PORT);
  SuperVisor_Message(buffer0, buffer1);
  SANITYDELAY

  // send LED status to SuperVisor
  sprintf(buffer0, "PM_STATUSLEDS=%u", StatusLEDs);
  SuperVisor_Message(buffer0, buffer1);
}


void SuperVisor_Task(void *pvParameters)
{
  if (!mySuperVisor.detected) return;
  char buffer1[I2C_MAXMESSAGE+5]="";
  char buffer2[I2C_MAXMESSAGE+5]="";
  char buffer3[I2C_MAXMESSAGE+5]="";
  int restartwifi = false;
  static String currentSSID =  WiFi.SSID();
  static String currentPass = "";
  static String currentHostname = "";

 // SuperVisor_Message("GET_REBOOT", buffer1);
 // if (strlen(buffer1) && (strcmp(buffer1, "yes") == 0)) 
 //   ESP.restart();  

  // Check and get Wifi info from SuperVisor
  // Update wifi only when received 
  SuperVisor_Message("GET_HOSTNAME", buffer3);
  if (strlen(buffer3) && (strcmp(buffer3, WiFi.getHostname()) !=0)) {
    currentHostname = buffer3;
    nvs.begin("PoolMaster2",false);
    nvs.putString("hostname", currentHostname);
    nvs.end(); 
    restartwifi = true;
  }

  SuperVisor_Message("GET_WIFI_SSID", buffer1);
  if (strlen(buffer1) && (strcmp(buffer1, WiFi.SSID().c_str())) != 0) {
    SuperVisor_Message("GET_WIFI_PASS", buffer2);
    currentSSID = buffer1;
    currentPass = buffer2;
    restartwifi = true;
  }
  
  if (restartwifi) {
    WiFi.disconnect();
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    if (currentHostname != "") WiFi.setHostname(currentHostname.c_str());
    if (currentPass != "")     WiFi.begin(currentSSID.c_str(), currentPass.c_str());
    else WiFi.reconnect();
    SuperVisor_Message("Network updated !", buffer1);
    restartwifi = false;
  }

  // Check and get MQTT info from SuperVisor
  // Update MQTT only when received 
  SuperVisor_Message("GET_MQTT_TOPIC", buffer1);
  if (strlen(buffer1) && (strcmp(buffer1, storage.MQTT_TOPIC)) !=0) {
      strcpy(storage.MQTT_TOPIC, buffer1);
      SuperVisor_Message("GET_MQTT_USERNAME", buffer1);
      strcpy(storage.MQTT_LOGIN, buffer1);
      SuperVisor_Message("GET_MQTT_PASSWORD", buffer1);
      strcpy(storage.MQTT_PASS, buffer1);
      SuperVisor_Message("GET_MQTT_PORT", buffer1);
      char*out;
      storage.MQTT_PORT = strtol(buffer1, &out, 10);
      SuperVisor_Message("GET_MQTT_SERVER", buffer1);
      IPAddress servIP;
      servIP.fromString(buffer1);
      storage.MQTT_IP = servIP;
      saveConfig(); // and restart 
      delay(500);
      ESP.restart();
  }

  // Ask SuperVisor to show LEDs info on its OLED
  SuperVisor_DisplayPollMasterStatus();
}


ExtensionStruct SuperVisor_Init(const char *name, int IO)
{
    /* Initialize the library and interfaces */
    mySuperVisor.detected = false;
    lockI2C();

    Wire.beginTransmission(SUPERVISOR_I2C_Address);
    if (Wire.endTransmission() == 0)  {
        mySuperVisor.detected = true;
        Debug.print(DBG_INFO,"SuperVisor @ 0x%02X detected",SUPERVISOR_I2C_Address);
    }
    else Debug.print(DBG_INFO,"SuperVisor not detected");
 
    unlockI2C();
    
    mySuperVisor.name                = name;
    mySuperVisor.Task                = SuperVisor_Task;
    mySuperVisor.frequency           = 1000;     // check SuperVisor messages every 1 sec
    mySuperVisor.LoadSettings        = 0;
    mySuperVisor.SaveSettings        = 0;
    mySuperVisor.LoadMeasures        = 0;
    mySuperVisor.SaveMeasures        = SuperVisor_SaveMeasures;
    mySuperVisor.HistoryStats        = 0;
    mySuperVisor.MQTTTopicSettings   = 0;
    mySuperVisor.MQTTTopicMeasures   = ExtensionsCreateMQTTTopic(mySuperVisor.name, "");

    return mySuperVisor;
}

#endif

