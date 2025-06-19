// I2C communication with ESP32 SuperVisor
// ***************************************


#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include <WiFi.h>

#if defined(_EXTENSIONS_)

#include "Extension_SuperVisor.h"
#include <uptime.h>                

const char _DELIMITER_[] = {0xEA,0xEA,0xEA}; // ΩΩΩ
#define I2C_MAXMESSAGE     64

ExtensionStruct mySuperVisor      = {0};
ExtensionStruct mySuperVisor_Info = {0};
uint8_t StatusLEDs = 0;

// External functions
extern void ExtensionsPublishTopic(char*, JsonDocument&);
extern void lockI2C();
extern void unlockI2C();
extern void mqttInit(void);
extern void mqttDisconnect(void);
extern bool saveConfig(void);

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
  
  if (!result) { // no need for response 
    // I2C-slave bug -- empty buffer
    while (Wire.available()) Wire.read() ; // empty buffer
    unlockI2C();
    delay(500);
    return;
  }

  int i=0;
  while (Wire.available()) {
      result[i++] = (char)Wire.read(); 
      if (i>=I2C_MAXMESSAGE) i=I2C_MAXMESSAGE-1;
      result[i] = '\0';
    }
  char *p = result;
  // find and remove our delimiter
  for (int i=0; *p && i<I2C_MAXMESSAGE; i++, p++) 
    if (strncmp(p, _DELIMITER_, sizeof(_DELIMITER_))==0) *p = '\0';
  if (i==I2C_MAXMESSAGE) Debug.print(DBG_DEBUG,"SuperVisor_Message==> NODELIMITER");

  unlockI2C();
  delay(500);
}

void SuperVisor_Task(void *pvParameters)
{
  if (!mySuperVisor.detected) return;
  char buffer[I2C_MAXMESSAGE+5]="";
  int restartwifi = false;
  char newSSID[16] = {0};
  char newPass[16] = {0};
  char newHostname[16] = {0};

  // Check and get Wifi info from SuperVisor
  // Update wifi only when received 
  const char* currentHostname = WiFi.getHostname();
  SuperVisor_Message("GET_HOSTNAME", buffer);
  if ((strcmp(buffer, "") !=0) && (strcmp(buffer, currentHostname) !=0)) {
    strcpy(newHostname, buffer);
    restartwifi = true;
  }

  String ScurrentSSID = WiFi.SSID();
  const char* currentSSID = ScurrentSSID.c_str();
  SuperVisor_Message("GET_WIFI_SSID", buffer);
  if (strlen(buffer) && (strcmp(buffer, currentSSID)) != 0) {
    strcpy(newSSID, buffer);
    SuperVisor_Message("GET_WIFI_PASS", buffer);
    strcpy(newPass, buffer);
    restartwifi = true;
  }
  
  if (restartwifi) {
    WiFi.disconnect();
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    if (strlen(newHostname)) WiFi.setHostname(newHostname);
    if (strlen(newSSID))
          WiFi.begin(newSSID, newPass);
    else  WiFi.reconnect();
    MDNS.addService("http", "tcp", 80);
    if (strlen(newHostname)) MDNS.begin(newHostname);  
    restartwifi = false;
  }

  // Check and get MQTT info from SuperVisor
  // Update MQTT only when received 
  SuperVisor_Message("GET_MQTT_TOPIC", buffer);
  if (strlen(buffer) && (strcmp(buffer, storage.MQTT_TOPIC)) !=0) {
      strcpy(storage.MQTT_TOPIC, buffer);
      SuperVisor_Message("GET_MQTT_USERNAME", buffer);
      strcpy(storage.MQTT_LOGIN, buffer);
      SuperVisor_Message("GET_MQTT_PASSWORD", buffer);
      strcpy(storage.MQTT_PASS, buffer);
      SuperVisor_Message("GET_MQTT_PORT", buffer);
      char* out;
      storage.MQTT_PORT = strtol(buffer, &out, 10);
      SuperVisor_Message("GET_MQTT_SERVER", buffer);
      IPAddress servIP;
      servIP.fromString(buffer);
      storage.MQTT_IP = servIP;
      saveConfig(); // and restart 
      delay(500);
      ESP.restart();
  }
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
    mySuperVisor.frequency           = 3000;     // check SuperVisor messages every x msec
    mySuperVisor.LoadSettings        = 0;
    mySuperVisor.SaveSettings        = 0;
    mySuperVisor.LoadMeasures        = 0;
    mySuperVisor.SaveMeasures        = SuperVisor_SaveMeasures;
    mySuperVisor.HistoryStats        = 0;
    mySuperVisor.MQTTTopicSettings   = 0;
    mySuperVisor.MQTTTopicMeasures   = ExtensionsCreateMQTTTopic(mySuperVisor.name, "");

    return mySuperVisor;
}

static int addInfo(char *dest, const char* key, const char *value)
{
  sprintf(dest, "%s=%s%c", key, value, _DELIMITER_[0]);
  return strlen(dest);
}

void SuperVisor_Info_Task(void *pvParameters)
{
  extern String Firmw;
  char buffer[1024]={0};
  int index=0;
  char buffer1[25];
  index += addInfo(buffer+index, "Firmware",          Firmw.c_str());
  index += addInfo(buffer+index, "Display Firmware",  TFT_FIRMW);
  index += addInfo(buffer+index, "Hostname",          WiFi.getHostname());
  index += addInfo(buffer+index, "IP Address",        WiFi.localIP().toString().c_str());
  index += addInfo(buffer+index, "MAC Address",       WiFi.macAddress().c_str());
  index += addInfo(buffer+index, "Wifi SSID",         WiFi.SSID().c_str());
  sprintf(buffer1, "%d", WiFi.RSSI());
  index += addInfo(buffer+index, "Wifi RSSI",         buffer1);
  index += addInfo(buffer+index, "MQTT Topic",        storage.MQTT_TOPIC);
  IPAddress MQTTservIP = storage.MQTT_IP;
  sprintf(buffer1, "%s", MQTTservIP.toString().c_str());
  index += addInfo(buffer+index, "MQTT Server",  buffer1);
  sprintf(buffer1, "%u", storage.MQTT_PORT);
  index += addInfo(buffer+index, "MQTT Port",  buffer1);
  uptime::calculateUptime();
  sprintf(buffer1, "%dd-%02dh-%02dm-%02ds", uptime::getDays(), uptime::getHours(), uptime::getMinutes(), uptime::getSeconds());
  index += addInfo(buffer+index, "Uptime",            buffer1);
  index += addInfo(buffer+index, "Chip Model",        ESP.getChipModel());
  sprintf(buffer1, "%d", ESP.getFlashChipSize());
  index += addInfo(buffer+index, "Flash Size",        buffer1);
  sprintf(buffer1, "%d", ESP.getCpuFreqMHz());
  index += addInfo(buffer+index, "CPU Freq (Mhz)",    buffer1);
  sprintf(buffer1, "%d", ESP.getChipCores());
  index += addInfo(buffer+index, "CPU Cores",         buffer1);
  dtostrf(ESP.getHeapSize() / 1024.0, 3, 3, buffer1);
  index += addInfo(buffer+index, "HEAP size (KB)",    buffer1);
  dtostrf(ESP.getFreeHeap() / 1024.0, 3, 3, buffer1);
  index += addInfo(buffer+index, "HEAP free (KB)",    buffer1);
  dtostrf(ESP.getMaxAllocHeap() / 1024.0, 3, 3, buffer1);
  index += addInfo(buffer+index, "HEAP maxAlloc",     buffer1);

  // send LED status to SuperVisor
  static const char *bit_rep[16] = {
    [ 0] = "....", [ 1] = "...*", [ 2] = "..*.", [ 3] = "..**",
    [ 4] = ".*..", [ 5] = ".*.*", [ 6] = ".**.", [ 7] = ".***",
    [ 8] = "*...", [ 9] = "*..*", [10] = "*.*.", [11] = "*.**",
    [12] = "**..", [13] = "**.*", [14] = "***.", [15] = "****", };
  char part1 =  StatusLEDs & 0x0F;
  char part2 = (StatusLEDs & 0x80) >> 7;
  part2     |= (StatusLEDs & 0x40) >> 5;
  part2     |= (StatusLEDs & 0x20) >> 3;
  part2     |= (StatusLEDs & 0x10) >> 1;
  sprintf(buffer1, "%s%s", bit_rep[part2], bit_rep[part1]);
  index += addInfo(buffer+index, "LED",             buffer1);

  Serial.printf("%c%s\n", _DELIMITER_[0], buffer);
}


ExtensionStruct SuperVisor_Info_Init(const char *name, int IO)
{
    /* Initialize the library and interfaces */
    mySuperVisor_Info.detected = true;

    mySuperVisor_Info.name                = name;
    mySuperVisor_Info.Task                = SuperVisor_Info_Task;
    mySuperVisor_Info.frequency           = 1000;     // send PoolMaster heartbeat every x msec
    mySuperVisor_Info.LoadSettings        = 0;
    mySuperVisor_Info.SaveSettings        = 0;
    mySuperVisor_Info.LoadMeasures        = 0;
    mySuperVisor_Info.SaveMeasures        = 0;
    mySuperVisor_Info.HistoryStats        = 0;
    mySuperVisor_Info.MQTTTopicSettings   = 0;
    mySuperVisor_Info.MQTTTopicMeasures   = 0;

    return mySuperVisor_Info;
}

#endif
