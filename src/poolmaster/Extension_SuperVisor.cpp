// I2C communication with ESP32 SuperVisor
// ***************************************


#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include <WiFi.h>
//#include <freertos/task.h>

#if defined(_EXTENSIONS_)

#include "Extension_SuperVisor.h"
#include <uptime.h>                

//const char _DELIMITER_[] = {0xEA,0xEA,0xEA}; // ΩΩΩ

ExtensionStruct mySuperVisor      = {0};
ExtensionStruct mySuperVisor_Info = {0};
ExtensionStruct myNone = {0};

// External functions
extern void lockI2C();
extern void unlockI2C();
extern void mqttInit(void);
extern void mqttDisconnect(void);

// Configure Extension properties
// ******************************

void SuperVisor_SaveMeasures (void *pvParameters)
{
     if (!mySuperVisor.detected) return;
}

bool validchar(unsigned char c)
{
  if (isalnum(c)) return true;
  if (c=='\\')    return true;
  if (c=='/')     return true;
  if (c==0xEA)    return true; // Ω
  if (c==':')     return true;
  if (c=='.')     return true;
  if (c==',')     return true;
  if (c=='%')     return true;
  if (c=='=')     return true;
  if (c=='-')     return true;
  if (c=='+')     return true;
  if (c=='*')     return true;
  if (c=='#')     return true;
  if (c=='@')     return true;
  if (c=='(')     return true;
  if (c==')')     return true;
  if (c=='\0')    return true;
  if (c=='&')     return true;
  if (c=='[')     return true;
  if (c==']')     return true;
  if (c=='_')     return true;
  if (c=='{')     return true;
  if (c=='}')     return true;
  if (c=='$')     return true;
  if (c==0x80)    return true; // €
  if (c==0xA3)    return true; // £
  if (c==0xB5)    return true; // µ
  if (c=='<')     return true;
  if (c=='>')     return true;
  if (c=='"')     return true;
  if (c=='|')     return true;
  if (c==0xB0)    return true; // °
  if (c==';')     return true;
  if (c=='!')     return true;
  if (c==0xA7)    return true; // §
  if (c=='?')     return true;
  if (c=='"')     return true;
  if (c=='\'')    return true;
  if (c=='^')     return true;
  if (c==0xA4)    return true; // ¤
  return false;
}

void SuperVisor_Message(const char *value, char* result) 
{
  if (result) strcpy(result, "none");
  if (!mySuperVisor.detected) return;

  lockI2C();
  Wire.beginTransmission(SUPERVISOR_I2C_Address);
  Wire.printf(value); // send the question to SuperVisor
  uint8_t error = Wire.endTransmission(true);
  delay(100);
  //Read from the I2C ESP slave, but read the entire buffer !
  uint8_t bytesReceived = Wire.requestFrom(SUPERVISOR_I2C_Address, I2C_MAXMESSAGE);

  if (!result) { // no need for response , clear the queue
    // I2C-slave bug -- empty buffer
    while (Wire.available()) Wire.read() ; // empty buffer
    unlockI2C();
    delay(50);
    return;
  }

  int i=0;
  while (Wire.available()) {
    unsigned char c = (unsigned char)Wire.read();
    if (!validchar(c)) continue; // something wrong from I2C
    result[i++] = c;
    if (i>=I2C_MAXMESSAGE) i=I2C_MAXMESSAGE-1; // reach buffer's end
  }

  unlockI2C();

  result[i] = '\0';
  char *p = result;
  // find and remove our delimiter
  for (int i=0; *p && i<I2C_MAXMESSAGE; i++, p++)
    if (strncmp(p, _DELIMITER_, sizeof(_DELIMITER_))==0) *p = '\0';

  if (i==I2C_MAXMESSAGE) {
    strcpy(result, "none");
    Debug.print(DBG_INFO,"SuperVisor_Message==> NODELIMITER");
  }
}

void SuperVisor_LoadSettings(void *pvParameters)
{
  if (!mySuperVisor.detected) return;
  char buffer[I2C_MAXMESSAGE+5] = {0};
  int restartwifi = false;
  char newSSID[16] = {0};
  char newPass[16] = {0};
  char newHostname[16] = {0};
  int restartmqtt = false;

  // Check and get Wifi info from SuperVisor
  const char* currentHostname = WiFi.getHostname();
  SuperVisor_Message("GET_HOSTNAME", buffer);
  if ((strcmp(buffer, "none") !=0) && (strcmp(buffer, currentHostname) !=0)) {
    strcpy(newHostname, buffer);
    restartwifi = true;
  }

  String ScurrentSSID = WiFi.SSID();
  const char* currentSSID = ScurrentSSID.c_str();
  SuperVisor_Message("GET_WIFI_SSID", buffer);
  if ((strcmp(buffer, "none") !=0) && (strcmp(buffer, currentSSID)) != 0) {
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
    // MDNS.addService("http", "tcp", 80);
    //if (strlen(newHostname)) MDNS.begin(newHostname);  
    restartwifi = false;
  }

  // Check and get MQTT info from SuperVisor
  // Update MQTT only when new values 
 
  SuperVisor_Message("GET_MQTT_TOPIC", buffer);
  if ((strcmp(buffer, "none") !=0) &&
      (strcmp(buffer, "Ok") !=0)   &&
      (strcmp(buffer, "/") !=0)    &&
      (strcmp(buffer, "") !=0)     &&
      (strlen(buffer) > 1)         &&
      (strcmp(buffer, PMConfig.get<const char*>(MQTT_TOPIC)) !=0)) {
    PMConfig.put<const char*>(MQTT_TOPIC, buffer);
    restartmqtt = true;
  }
  SuperVisor_Message("GET_MQTT_USERNAME", buffer);
  if ((strcmp(buffer, "none") !=0) && (strcmp(buffer, PMConfig.get<const char*>(MQTT_LOGIN)) !=0)) {
    PMConfig.put<const char*>(MQTT_LOGIN, buffer);
    restartmqtt = true;
  }
  SuperVisor_Message("GET_MQTT_PASSWORD", buffer);
  if ((strcmp(buffer, "none") !=0) && (strcmp(buffer, PMConfig.get<const char*>(MQTT_PASS)) !=0)) {
    PMConfig.put<const char*>(MQTT_PASS, buffer);
    restartmqtt = true;
  }
  SuperVisor_Message("GET_MQTT_PORT", buffer);
  if ((strcmp(buffer, "none") !=0) &&
      (strcmp(buffer, "0") !=0)) {
    char* out;
    uint32_t mqttport = strtol(buffer, &out, 10);
    if (mqttport != PMConfig.get<uint32_t>(MQTT_PORT)) {
      PMConfig.put<uint32_t>(MQTT_PORT, mqttport);
      restartmqtt = true;
    }
  }
  SuperVisor_Message("GET_MQTT_SERVER", buffer);
  if (strcmp(buffer, "none") !=0) {
    IPAddress servIP;
    servIP.fromString(buffer);
    if (servIP != PMConfig.get<uint32_t>(MQTT_IP)) {
      PMConfig.put<uint32_t>(MQTT_IP, servIP);
      restartmqtt = true;
    }
  }
  if (restartmqtt) {
    mqttDisconnect();
    mqttInit();
  }

  SuperVisor_Message("GET_COMMAND", buffer);
  if (strcmp(buffer, "none")!=0)
    if (xQueueSendToBack(queueIn, &buffer, 0) == pdPASS)
          Debug.print(DBG_INFO,"Command added to queue: %s",buffer);
    else  Debug.print(DBG_ERROR,"Queue full, command: %s not added", buffer);
}


static int addInfo(char *dest, const char* key, const char *value)
{
  sprintf(dest, "%s=%s%c", key, value, _DELIMITER_[0]);
  return strlen(dest);
}

static int addInfo(char *dest, const char* key, uint8_t value)
{
  sprintf(dest, "%s=%d%c", key, value, _DELIMITER_[0]);
  return strlen(dest);
}

static int addInfo(char *dest, const char* key, uint32_t value)
{
  sprintf(dest, "%s=%d%c", key, value, _DELIMITER_[0]);
  return strlen(dest);
}

static int addInfo(char *dest, const char* key, int value)
{
  sprintf(dest, "%s=%d%c", key, value, _DELIMITER_[0]);
  return strlen(dest);
}
static int addInfo(char *dest, const char* key, double value)
{
  sprintf(dest, "%s=%.1f%c", key, value, _DELIMITER_[0]);
  return strlen(dest);
}
static int addInfo(char *dest, const char* key, bool value, const char* color)
{
  value &= 1;
  if (value) // this is a LED
        sprintf(dest, "%s=*%s%c", key, color  , _DELIMITER_[0]);
  else  sprintf(dest, "%s=*%s%c", key, "black", _DELIMITER_[0]);
  return strlen(dest);
}

extern ListExtensions myListExtensions[];
extern int _NbExtensions;

int GetRunTimeStats(char *buffer, char* buffer1)
{
  char taskname[40];
  char key[30];
  TaskHandle_t handle;
  int index = 0;

  for (int i = 0; i < _NbExtensions; i++) {
    sprintf(taskname, "%s", myListExtensions[i].name);
    if (strlen(taskname) < 16) handle = xTaskGetHandle(taskname);
    else handle = NULL;
    if (handle) {
      UBaseType_t temp = uxTaskGetStackHighWaterMark(handle);
      if (!myListExtensions[i].taskStackHighWaterMark || temp < myListExtensions[i].taskStackHighWaterMark)
        myListExtensions[i].taskStackHighWaterMark = temp;
      sprintf(buffer1, "%lu", myListExtensions[i].taskStackHighWaterMark);
      sprintf(key, "StackHhM %s", taskname);
      index += addInfo(buffer+index, key, buffer1);
    }

    // Monitor the 2nd loop (ended with S) checking updated settings (if any)
    sprintf(taskname, "%sS", myListExtensions[i].name);
    if (strlen(taskname) < 16) handle = xTaskGetHandle(taskname);
    else handle = NULL;
    if (handle) {
      UBaseType_t temp = uxTaskGetStackHighWaterMark(handle);
      if (!myListExtensions[i].settingsStackHighWaterMark || temp < myListExtensions[i].settingsStackHighWaterMark)
        myListExtensions[i].settingsStackHighWaterMark = temp;
      sprintf(buffer1, "%lu", myListExtensions[i].settingsStackHighWaterMark);
      sprintf(key, "StackHWM %s", taskname);
      index += addInfo(buffer+index, key, buffer1);
    }
  }
  return index;
}

void SuperVisor_Task(void *pvParameters)
{
  extern String Firmw;
  char buffer[1024]={0};
  char buffer1[64];
  int index=0;

  index += addInfo(buffer+index, "Firmware",          Firmw.c_str());
  index += addInfo(buffer+index, "Display Firmware",  TFT_FIRMW);
  index += addInfo(buffer+index, "Hostname",          WiFi.getHostname());
  index += addInfo(buffer+index, "IP Address",        WiFi.localIP().toString().c_str());
  index += addInfo(buffer+index, "MAC Address",       WiFi.macAddress().c_str());
  index += addInfo(buffer+index, "Wifi SSID",         WiFi.SSID().c_str());
  index += addInfo(buffer+index, "Wifi RSSI",         WiFi.RSSI());
  index += addInfo(buffer+index, "MQTT Topic",        PMConfig.get<const char*>(MQTT_TOPIC));
  IPAddress MQTTservIP = PMConfig.get<uint32_t>(MQTT_IP);
  sprintf(buffer1, "%s", MQTTservIP.toString().c_str());
  index += addInfo(buffer+index, "MQTT Server",  buffer1);
  sprintf(buffer1, "%u", PMConfig.get<uint32_t>(MQTT_PORT));
  index += addInfo(buffer+index, "MQTT Port",  buffer1);
  uptime::calculateUptime();
  sprintf(buffer1, "%dd-%02dh-%02dm-%02ds", uptime::getDays(), uptime::getHours(), uptime::getMinutes(), uptime::getSeconds());
  index += addInfo(buffer+index, "Uptime",            buffer1);

  Serial.printf("%c%s\n", _DELIMITER_[0], buffer);
  index=0;

  index += addInfo(buffer+index, "Chip Model",        ESP.getChipModel());
  index += addInfo(buffer+index, "Flash Size",        ESP.getFlashChipSize());
  index += addInfo(buffer+index, "CPU Freq (Mhz)",    ESP.getCpuFreqMHz());
  index += addInfo(buffer+index, "CPU Cores",         ESP.getChipCores());
  dtostrf(ESP.getHeapSize() / 1024.0, 3, 3, buffer1);
  index += addInfo(buffer+index, "HEAP size (KB)",    buffer1);
  dtostrf(ESP.getFreeHeap() / 1024.0, 3, 3, buffer1);
  index += addInfo(buffer+index, "HEAP free (KB)",    buffer1);
  dtostrf(ESP.getMaxAllocHeap() / 1024.0, 3, 3, buffer1);
  index += addInfo(buffer+index, "HEAP maxAlloc",     buffer1);

  Serial.printf("%c%s\n", _DELIMITER_[0], buffer);
  index=0;

  // report all StackHighWaterMark per loops
  index += GetRunTimeStats(buffer+index, buffer1);
  if (index !=0) Serial.printf("%c%s\n", _DELIMITER_[0], buffer);
  index=0;

  /* useless now
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
  */

  // send Diag Status to SuperVisor (LEDs)
  index += addInfo(buffer+index, "MQTT Connected",    (bool)mqttClient.connected(),       "green");
  index += addInfo(buffer+index, "Auto Mode",         (bool)PMConfig.get<bool>(AUTOMODE), "green");
  index += addInfo(buffer+index, "Anti Freeze",       (bool)AntiFreezeFiltering,          "green");
  index += addInfo(buffer+index, "Filling Pump Error",(bool)FillingPump.UpTimeError,      "red");
  index += addInfo(buffer+index, "PSI Error",         (bool)PSIError,                     "red");
  index += addInfo(buffer+index, "pH PID",            (bool)PhPID.GetMode(),              "magenta");
  index += addInfo(buffer+index, "Orp PID",           (bool)OrpPID.GetMode(),             "magenta");
  index += addInfo(buffer+index, "pH Tank Level low", (bool)!PhPump.TankLevel(),          "blue");
  index += addInfo(buffer+index, "Cl Tank Level low", (bool)!ChlPump.TankLevel(),         "blue");
  index += addInfo(buffer+index, "pH Pump Error",     (bool)PhPump.UpTimeError,           "red");
  index += addInfo(buffer+index, "Cl Pump Error",     (bool)ChlPump.UpTimeError,          "red");
  auto device = PoolDeviceManager.GetDevice(DEVICE_POOL_LEVEL);
  index += addInfo(buffer + index, "Pool Level low", (device != nullptr) ? (bool)!device->IsActive() : false, "yellow");
  
  Serial.printf("%c%s\n", _DELIMITER_[0], buffer);
  index=0;

  // send live info to SuperVisor
  index += addInfo(buffer+index, "Air Temperature",   PMData.AirTemp);
  index += addInfo(buffer+index, "Water Temperature", PMData.WaterTemp);
  index += addInfo(buffer+index, "pH",                PMData.PhValue);
  index += addInfo(buffer+index, "Orp",               (int)PMData.OrpValue);
  index += addInfo(buffer+index, "Water Pressure",    (int)(PMData.PSIValue*1000.0));
  index += addInfo(buffer+index, "RFU",               (int)rfu_sensor_value);

  Serial.printf("%c%s\n", _DELIMITER_[0], buffer);
  index=0;

  extern ExtensionStruct* myExtensions;
  for (int i=0; i < ExtensionsNb(); i++) {
    if (strcmp(myExtensions[i].name, "SuperVisor")==0) continue;
    if (!myExtensions[i].detected) continue;
    if (!myExtensions[i].Values)   continue;
    myExtensions[i].Values(buffer1);
    index += addInfo(buffer+index, myExtensions[i].name, buffer1);
  }
  Serial.printf("%c%s\n", _DELIMITER_[0], buffer);
}

ExtensionStruct SuperVisor_Init(char *name, int IO)
{
    /* Initialize library and interfaces */
    mySuperVisor.detected = true;
 
/*   lockI2C();
    Wire.beginTransmission(SUPERVISOR_I2C_Address);
    if (Wire.endTransmission() == 0)
         mySuperVisor.detected = true;
    else Debug.print(DBG_INFO,"SuperVisor not detected");
    unlockI2C();
*/
    mySuperVisor.name                = name;
    mySuperVisor.Task                = SuperVisor_Task;
    mySuperVisor.frequency           = 1500;     // receive and send SuperVisor messages every x msec
    mySuperVisor.LoadSettings        = SuperVisor_LoadSettings;
    mySuperVisor.SaveSettings        = 0;
    mySuperVisor.LoadMeasures        = 0;
    mySuperVisor.SaveMeasures        = SuperVisor_SaveMeasures;
    mySuperVisor.HistoryStats        = 0;

    return mySuperVisor;
}


ExtensionStruct noInit(char *name, int IO)
{
    /* Initialize the library and interfaces */
    myNone.detected = false;

    myNone.name                = name;
    myNone.Task                = 0;
    myNone.frequency           = 1;
    myNone.LoadSettings        = 0;
    myNone.SaveSettings        = 0;
    myNone.LoadMeasures        = 0;
    myNone.SaveMeasures        = 0;
    myNone.HistoryStats        = 0;

    return myNone;
}

#endif
