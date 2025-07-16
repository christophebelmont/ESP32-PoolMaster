/*
  PoolMaster SuperVisor - Act as a watchdog, OOBE, upload manager and supervise PoolMaster
  WifiManager : https://github.com/tzapu/WiFiManager WiFi Configuration Magic
  I2C Slave and Master :  https://deepbluembedded.com/arduino-i2c-slave/
                          https://randomnerdtutorials.com/esp32-i2c-master-slave-arduino/
*/

#define PMSV_VERSION  "0.85"

// TODO
//  bugs :  Restart PoolMaster does not work
//          Upgrade PoolMaster from webinterface does not work 

//  create dynamic webpage for information part (remove Refresh button)
//  webpage : display popup and progress bar when doing an upgrade
//  Propose to format ESP32-PoolMaster (copy bootloader+partition+firmware.bin)
//  replace elegantOTA and Webserial (remove commercial stuffs)
//  check statusLEDS are correct
//  draw a nicer status of LEDs
//  future : send info to external OLED on I2c Master
//  clean and reduce footprint, remove unsed libs/include, image is too big
//  PaperTrail consumes 8% of the disk !

#define TARGET_TELNET
#define TARGET_WEBSERIAL

// PAPERTRAIL consumes 8% of the flashdisk !
//#define TARGET_PAPERTRAIL

#include <WiFiManager.h>          
#include <WiFi.h>
#include <WiFiMulti.h>

#ifdef TARGET_WEBSERIAL
#include <WebSerial.h>
#endif

#include <TimeLib.h>
#include <ElegantOTA.h>

#ifdef TARGET_PAPERTRAIL
#include <Elog.h>
#include <ElogMacros.h>
#endif

#include <ESPNexUpload.h>
#include <HTTPClient.h>
#include "soc/rtc_wdt.h"
#include "esp32_flasher.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/i2c.h"
#include <ESPmDNS.h>
#include <Preferences.h>
#include <uptime.h>
#include <AsyncMqttClient.h>      // Async. MQTT client
#include <ArduinoJson.h>          // JSON library

const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

#ifdef TARGET_PAPERTRAIL
// PaperTrail
#define PL_LOG 0  // PoolMaster Logs
#define WD_LOG 1  // WatchDog Logs
#endif

#ifdef TARGET_TELNET
  //how many clients should be able to telnet to this ESP32
  #define MAX_SRV_CLIENTS 3
#endif

#define BUFFER_SIZE 400
#define LOG_BUFFER_SIZE 400

// Update triggers
volatile bool mustUpdateNextion       = false;
volatile bool mustUpdatePoolMaster    = false;
volatile bool mustRebootPoolMaster    = false;

// Update binary storage (HTTP Server)
const char* defaultUpdatehost         = "myUpdateHttpServer:myport";
const char* defaultnextionpath        = "/poolmaster/Nextion.tft";
const char* defaultpoolmasterpath     = "/poolmaster/PoolMaster.bin";
//const char* defaultUpdateurlwatchdog = "/poolmaster/WatchDog.bin"; // not used today

#ifdef TARGET_PAPERTRAIL
// PaperTrail log management
const char* defaultpapertrailhos      = "mypapertrailserver";
const char* defaultpapertrailport     = "21858";
#endif
// MQTT Server
const char* defaultmqtt_server        = "mymqttserver";
const char* defaultmqtt_port          = "1883";
const char* defaultmqtt_topic         = "PoolMaster";
const char* defaultmqtt_username      = "mymqttusername";
const char* defaultmqtt_password      = "mymqttuserpassword";

AsyncMqttClient MqttClient;
TimerHandle_t MqttReconnectTimer;
#define PAYLOAD_BUFFER_LENGTH 200

#define _LHOSTNAME_   16
#define _LSSID_       32
#define _LURL_        32
#define _LVERSION_    12
#define _LTOPIC_      64
#define _LMAC_        20

String Updatehost;
String nextionpath;
String poolmasterpath;
// char  Updateurlwatchdog[_LURL_]; // not used today
String mqtt_server;
String mqtt_port;
String mqtt_topic;
String mqtt_username;
String mqtt_password;
char  myhostname[_LHOSTNAME_] = {0};
char  hostname[_LHOSTNAME_]   = {0};
char  currentUptime[25];

#ifdef TARGET_PAPERTRAIL
String papertrailhost;
String papertrailport;
#endif

char  PoolMaster_Hostname[_LHOSTNAME_]  = "unknown";
char  PoolMaster_SSID[_LSSID_]          = "unknown";
char  PoolMaster_RSSI[8]                = "unknown";
char  PoolMaster_IP[_LURL_]             = "unknown";
char  PoolMaster_MAC[_LMAC_]            = "unknown";
char  PoolMaster_Version[_LVERSION_]    = "unknown";
char  PoolMaster_TFTVersion[_LVERSION_] = "unknown";
char  PoolMaster_Uptime[64]             = "unknown";
char  PoolMaster_MQTT_Server[_LURL_]    = "unknown";
char  PoolMaster_MQTT_Topic[_LTOPIC_]   = "unknown";
uint8_t PoolMaster_StatusLEDs           = 0;

#define  autoConfTimeout          500  // autoConf of PoolMaster network/mqtt is valid during 500 cycles
int   autoConfNetwork             = 0;
int   autoConfMQTT                = 0;

// Nextion Update counter for feedback
int UpdateCounter = 0;
int contentLength = 0;

// Nextion PIN Numbers
#define NEXT_RX           33 // Nextion RX pin
#define NEXT_TX           32 // Nextion TX pin

// PoolMaster PIN Numbers
#define ENABLE_PIN        25
#define BOOT_PIN          26
// Enable and Boot pin numbers
const int ENPin = ENABLE_PIN;
const int BOOTPin = BOOT_PIN;

// this ESP32 is a I2C Slave for PoolMaster
// PoolMaster can get info from SuperVisor and vice-versa
#define SDA_S             SDA
#define SCL_S             SCL
#define I2C_SLAVE_ADDR    0x07
#define I2C_MAXMESSAGE    64
const char _DELIMITER_[] = {0xEA,0xEA,0xEA}; // ΩΩΩ

// IC2 Master for future OLED display
#define SDA_M             14
#define SCL_M             12

// Wifi Manager
# define RESET_WIFI_PIN     19  // 27 or 19.  GPIO=LOW -> start WifiManager, reset settings when held 3sec
WiFiManager wifiManager;
#define _DEFAULT_NAME_       "PoolMaster"
#define SuperVisor_Suffix     "_SV"
Preferences preferences;
bool shouldSaveConfig = false;

#ifdef TARGET_TELNET
  WiFiServer Telnetserver(23);
  WiFiClient serverClients[MAX_SRV_CLIENTS];
#endif
AsyncWebServer Webserver(80);

// Local logline buffers
char sbuf[BUFFER_SIZE];
char local_sbuf[LOG_BUFFER_SIZE];

// OTA
unsigned long ota_progress_millis = 0;

/*! Send Logs to the various facilities
 *
 * \param _log_message The message to be sent.
 * \param _targets The targets where message should be printed (1-Telnet 2-WebSerial 3-PaperTrail or any combination)
 * \param _telnet_separator The newline character to be used when printing with Telnet
 *
 * \return None
 */
void Local_Logs_Dispatch(const char *_log_message, uint8_t _targets = 7, const char* _telnet_separator = "\r\n")
{

#ifdef TARGET_TELNET
  if(_targets & 1) {  // First bit is for telnet
    // Telnet
    for (int i = 0; i < MAX_SRV_CLIENTS; i++) {
      if (serverClients[i] && serverClients[i].connected()) {
        serverClients[i].write(_log_message, strlen(_log_message));
        delay(1);
        serverClients[i].write(_telnet_separator, strlen(_telnet_separator));
        delay(1);
      }
    }
  }
#endif

#ifdef TARGET_WEBSERIAL
  if(_targets & 2) {  // Second bit is for WebSerial
    // WebSerial
    WebSerial.printf("%s",_log_message);
  }
#endif

#ifdef TARGET_PAPERTRAIL
  if ((papertrailhost != "") && (papertrailhost != defaultpapertrailhost) && (_targets & 4)) {  // Third bit is for ParerTrail
    // Cloud PaperTrail
    logger.log(WD_LOG, 1, "%s", _log_message);
  }
#endif
}

// Monitor free stack (display smallest value)
void stack_mon(UBaseType_t &hwm)
{
  UBaseType_t temp = uxTaskGetStackHighWaterMark(nullptr);
  if(!hwm || temp < hwm)
  {
    hwm = temp;
    Serial.printf("[stack_mon] %s: %d bytes\n",pcTaskGetTaskName(NULL), hwm);
  }  
}

/*! Thread safe, memory safe non blocking read until delimiter is found.
 *
 * \param stream Stream.
 * \param buf Receiving buffer.
 * \param delim Delimeter.
 * \param i Buffer index.
 * \param j Delimiter index.
 *
 * \return true when delimiter is found or the buffer is full, false otherwise.
 */
template <size_t n> bool readUntil_r(
  Stream& stream, char (&buf)[n], char const* delim, size_t& i, size_t& j) {
for (; i < n and delim[j]; i++) {
  if (not stream.available()) {
    return false;
  }
  buf[i] = stream.read();

  if (buf[i] == delim[j]) {
    j++;
  }
  else {
    j = 0;
  }
}
for (; i < n; i++) {
  buf[i] = 0;
}
i = 0;
j = 0;
return true;
}

/*! Memory safe non blocking read until delimiter is found.
*
* \param stream Stream.
* \param buf Receiving buffer.
* \param delim Delimeter.
*
* \return true when delimiter is found or the buffer is full, false otherwise.
*/
template <size_t n> bool readUntil(Stream& stream, char (&buf)[n], char const* delim) {
  static size_t i = 0;
  static size_t j = 0;
  return readUntil_r(stream, buf, delim, i, j);
}


///////////////// Update POOLMASTER ///////////////////
////////////////////////////////////////////////////////
//void TaskUpdatePoolMaster(void *pvParameters=nullptr)
void TaskUpdatePoolMaster(void)
{
  //static UBaseType_t hwm=0;     // free stack size
  //rtc_wdt_protect_off();
  //rtc_wdt_disable();
  //for (;;) {
  //  if(mustUpdatePoolMaster) {
    //  mustUpdatePoolMaster = false;
      HTTPClient http;
      
      // begin http client
        if(!http.begin(String("http://") + Updatehost + poolmasterpath)){
          Local_Logs_Dispatch("Connection failed");
        return;
      }
      snprintf(local_sbuf,sizeof(local_sbuf),"Requesting URL: %s",poolmasterpath.c_str());
      Local_Logs_Dispatch(local_sbuf);
    
      // This will send the (get) request to the server
      int code          = http.GET();
      contentLength     = http.getSize();
        
      // Update the nextion display
      if(code == 200){
        Local_Logs_Dispatch("File received. Update PoolMaster...");
        bool result;
        snprintf(local_sbuf,sizeof(local_sbuf),"Start upload. File size is: %d bytes",contentLength);
        Local_Logs_Dispatch(local_sbuf);
        // Initialize ESP32Flasher
        ESP32Flasher espflasher;
        // set callback: What to do / show during upload..... Optional! Called every transfert integer %
        UpdateCounter=0;
   //   espflasher.setUpdateProgressCallback(nullptr);
        espflasher.setUpdateProgressCallback([](){
          UpdateCounter++;
        //  snprintf(local_sbuf,sizeof(local_sbuf),"PoolMaster Update Progress %02d%%",UpdateCounter);
          snprintf(local_sbuf,sizeof(local_sbuf),"PoolMaster Update Progress %4.1f%%",((((float)UpdateCounter*1024)/contentLength)*100));
          Local_Logs_Dispatch(local_sbuf,1,"\r");
        });
        espflasher.espFlasherInit();//sets up Serial communication to another esp32

        int connect_status = espflasher.espConnect();

        if (connect_status != SUCCESS) {
          Local_Logs_Dispatch("Cannot connect to target");
        }else{
          Local_Logs_Dispatch("Connected to target");

          espflasher.espFlashBinStream(*http.getStreamPtr(),contentLength);
        }

      }else{
        // else print http error
        snprintf(local_sbuf,sizeof(local_sbuf),"HTTP error: %d",http.errorToString(code).c_str());
        Local_Logs_Dispatch(local_sbuf);
      }

      http.end();
      Local_Logs_Dispatch("Closing connection");
//    }
    //stack_mon(hwm);
  //}
}

void TaskUpdateNextion(void)
{
      Local_Logs_Dispatch("Nextion Update Requested");
      Local_Logs_Dispatch("Stopping PoolMaster...");
      pinMode(ENPin, OUTPUT);
      digitalWrite(ENPin, LOW);
      Local_Logs_Dispatch("Upgrading Nextion ...");
   
      HTTPClient http;
      
      // begin http client
        if(!http.begin(String("http://") + Updatehost + nextionpath)){
          Local_Logs_Dispatch("Connection failed");
        return;
      }
      snprintf(local_sbuf,sizeof(local_sbuf),"Requesting URL: %s",nextionpath.c_str());
      Local_Logs_Dispatch(local_sbuf);
    
      // This will send the (get) request to the server
      int code          = http.GET();
      contentLength     = http.getSize();
        
      // Update the nextion display
      if(code == 200){
        Local_Logs_Dispatch("File received. Update Nextion...");
        bool result;

        // initialize ESPNexUpload
        ESPNexUpload nextion(115200);
        // set callback: What to do / show during upload..... Optional! Called every 2048 bytes
        UpdateCounter=0;
        nextion.setUpdateProgressCallback([](){
          UpdateCounter++;
          snprintf(local_sbuf,sizeof(local_sbuf),"Nextion Update Progress %4.1f%%",((((float)UpdateCounter*2048)/contentLength)*100));
          Local_Logs_Dispatch(local_sbuf,1,"\r");
        });
        // prepare upload: setup serial connection, send update command and send the expected update size
        result = nextion.prepareUpload(contentLength);
        
        if(!result){
            snprintf(local_sbuf,sizeof(local_sbuf),"Error: %s",nextion.statusMessage.c_str());
            Local_Logs_Dispatch(local_sbuf);
            //Serial.println("Error: " + nextion.statusMessage);
        }else{
            snprintf(local_sbuf,sizeof(local_sbuf),"Start upload. File size is: %d bytes",contentLength);
            Local_Logs_Dispatch(local_sbuf);
            // Upload the received byte Stream to the nextion
            result = nextion.upload(*http.getStreamPtr());
            
            if(result){
              Local_Logs_Dispatch("Successfully updated Nextion");
            }else{
              snprintf(local_sbuf,sizeof(local_sbuf),"Error updating Nextion: %s",nextion.statusMessage.c_str());
              Local_Logs_Dispatch(local_sbuf);
            }

            // end: wait(delay) for the nextion to finish the update process, send nextion reset command and end the serial connection to the nextion
            nextion.end();
            pinMode(NEXT_RX,INPUT);
            pinMode(NEXT_TX,INPUT);
        }
        
      }else{
        // else print http error
        snprintf(local_sbuf,sizeof(local_sbuf),"HTTP error: %d",http.errorToString(code).c_str());
        Local_Logs_Dispatch(local_sbuf);
      }

      http.end();
      Local_Logs_Dispatch("Closing connection");
      Local_Logs_Dispatch("Starting PoolMaster ...");
      digitalWrite(ENPin, HIGH);
      pinMode(ENPin, INPUT);
      //rtc_wdt_enable();
      //rtc_wdt_protect_on();
}

///////////// Update NEXTION and al ////////////////
////////////////////////////////////////////////////
void TheTasksLoop(void *pvParameters)
{
  static UBaseType_t hwm=0;     // free stack size
  rtc_wdt_protect_off();
  rtc_wdt_disable();

  for (;;) {
    delay(500);

    if(mustUpdateNextion) {
      mustUpdateNextion = false;
      TaskUpdateNextion();
    }

    if (mustUpdatePoolMaster) {
      mustUpdatePoolMaster = false;
      TaskUpdatePoolMaster();
    }

    if (mustRebootPoolMaster) {
      mustRebootPoolMaster = false;
//      Local_Logs_Dispatch("Stopping PoolMaster ...");
      pinMode(ENPin, OUTPUT);
      digitalWrite(ENPin, LOW);
      delay(pdMS_TO_TICKS(200));
//      Local_Logs_Dispatch("Starting PoolMaster ...");
      pinMode(ENPin, OUTPUT);
      digitalWrite(ENPin, HIGH);
      pinMode(ENPin, INPUT);
    }
    stack_mon(hwm);
  }
}

//////////////////////// ELEGANT OTA //////////////////////////
////////////////////////////////////////////////////////////
void onOTAStart() {
  // Log when OTA has started
  Local_Logs_Dispatch("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    snprintf(local_sbuf,sizeof(local_sbuf),"OTA Progress Current: %u bytes, Final: %u bytes", current, final);
    Local_Logs_Dispatch(local_sbuf);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Local_Logs_Dispatch("OTA update finished successfully!");
  } else {
    Local_Logs_Dispatch("There was an error during OTA update!");
  }
  // <Add your own code here>
}

void initElegantOTA() {
  ElegantOTA.begin(&Webserver);    // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
}

//////////////////////// COMMANDS //////////////////////////
////////////////////////////////////////////////////////////
void cmdExecute(char _command) {
  //snprintf(local_sbuf,sizeof(local_sbuf),"Command Arrived %s",_command);
  //Local_Logs_Dispatch(local_sbuf);
  switch (_command) {
    case 'R': // WatchDog Reboot
      delay(100);
      ESP.restart();
    break;
    case 'P':  // PoolMaster Stop
      Local_Logs_Dispatch("Stopping PoolMaster ...");
      pinMode(ENPin, OUTPUT);
      digitalWrite(ENPin, LOW);
    break;
    case 'Q':  // PoolMaster Start
      Local_Logs_Dispatch("Starting PoolMaster ...");
      if(digitalRead(ENPin)==LOW)
      {
        pinMode(ENPin, OUTPUT);
        digitalWrite(ENPin, HIGH);
        pinMode(ENPin, INPUT);
      }
    break;

    case 'S':  // PoolMaster Update
 //     mustUpdatePoolMaster = true;
      TaskUpdatePoolMaster();
    break;
    case 'T':  // Nextion Update
      mustUpdateNextion = true;
    break;
    case 'H':  // Help
      Local_Logs_Dispatch("***********************");  
      Local_Logs_Dispatch("Help Message:");
      Local_Logs_Dispatch("R: Reboot WatchDog");
      Local_Logs_Dispatch("P: Stop PoolMaster");
      Local_Logs_Dispatch("Q: Start PoolMaster");
      Local_Logs_Dispatch("S: Update PoolMaster");
      Local_Logs_Dispatch("T: Update Nextion");
      Local_Logs_Dispatch("***********************");  
    break;
  }
}

//////////////////////// WEBSERIAL //////////////////////////
////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
// Message Callback WebSocket
// ----------------------------------------------------------------------------
#ifdef TARGET_WEBSERIAL
void recvMsg(uint8_t *data, size_t len) {
  //WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  //cmdExecute(data[0]);
  //WebSerial.println(d);
}
#endif

void uptime()
{
    uptime::calculateUptime();
    sprintf(currentUptime, "%dd-%02dh-%02dm-%02ds", uptime::getDays(), uptime::getHours(), uptime::getMinutes(), uptime::getSeconds());
}

// MQTT Engine
// ***********

  /*void onMqttSubscribe(uint16_t packetId, uint8_t qos) {}
void onMqttUnSubscribe(uint16_t packetId) {}
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {}
void onMqttPublish(uint16_t packetId) {}
*/

const char* convert2bin(uint8_t data)
{
  const char *bit_rep[16] = {
    [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
    [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
    [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
    [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111", };
  static char result[9];
  sprintf(result, "%s%s", bit_rep[data >> 4], bit_rep[data & 0x0F]);
  return result;
}

void mqttPublish()
{
  String subtopic = "/Supervision";
  char Payload[PAYLOAD_BUFFER_LENGTH];
  if (!MqttClient.connected()) return;

  {
    DynamicJsonDocument root(2048);
    String Stopic = mqtt_topic + subtopic + "/PoolMaster/Version";
    root["Version"]    = PoolMaster_Version;
    root["TFTVersion"] = PoolMaster_TFTVersion;
    size_t n = serializeJson(root, Payload);
    if (MqttClient.publish(Stopic.c_str(), 1, true, Payload, n) == 0) {
      snprintf(local_sbuf,sizeof(local_sbuf),"Supervisor, unable to publish MQTT %s Payload: %s - Payload size: %d", Stopic.c_str(), Payload, sizeof(Payload));
      Local_Logs_Dispatch(local_sbuf);
    }
  }
  {
    DynamicJsonDocument root(2048);
    String Stopic = mqtt_topic + subtopic + "/PoolMaster/Network";
    root["Hostname"]   = PoolMaster_Hostname;
    root["IP"]         = PoolMaster_IP;
    root["MAC"]        = PoolMaster_MAC;
    root["SSID"]       = PoolMaster_SSID;
    root["RSSI"]       = PoolMaster_RSSI;
    size_t n = serializeJson(root, Payload);
    MqttClient.publish(Stopic.c_str(), 1, true, Payload, n);
  }
  {
    DynamicJsonDocument root(2048);
    String Stopic = mqtt_topic + subtopic + "/PoolMaster/Status";
    root["Uptime"]     = PoolMaster_Uptime;
    root["StatusLEDs"] = convert2bin(PoolMaster_StatusLEDs);
    size_t n = serializeJson(root, Payload);
    MqttClient.publish(Stopic.c_str(), 1, true, Payload, n);
  }
  {
    DynamicJsonDocument root(2048);
    String Stopic = mqtt_topic + subtopic + "/SuperVisor/Version";
    root["Version"]    = PMSV_VERSION;
    size_t n = serializeJson(root, Payload);
    MqttClient.publish(Stopic.c_str(), 1, true, Payload, n);
  }
  {
    DynamicJsonDocument root(2048);
    String Stopic = mqtt_topic + subtopic + "/SuperVisor/Network";
    root["Hostname"]   = WiFi.getHostname();
    root["IP"]         = WiFi.localIP().toString();
    root["MAC"]        = WiFi.macAddress();
    root["SSID"]       = wifiManager.getWiFiSSID();
    root["RSSI"]       = WiFi.RSSI();
    size_t n = serializeJson(root, Payload);
    MqttClient.publish(Stopic.c_str(), 1, true, Payload, n);
  }
  {
    DynamicJsonDocument root(2048);
    String Stopic = mqtt_topic + subtopic + "/SuperVisor/Status";
    uptime();
    root["Uptime"]     = currentUptime;
    size_t n = serializeJson(root, Payload);
    MqttClient.publish(Stopic.c_str(), 1, true, Payload, n);
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) 
{
    if (WiFi.isConnected()) xTimerStart(MqttReconnectTimer, 0);
}

void onMqttConnect(bool sessionPresent) 
{
  mqttPublish();
  delay(1000);
  MqttClient.disconnect();
}

void connectToMqtt() {
  MqttClient.connect();
// if (MqttClient.connected()) 
//    snprintf(local_sbuf,sizeof(local_sbuf),"Supervisor connected to MQTT %s=%d=%s=%s", mqtt_server.c_str(), atol(mqtt_port.c_str()), mqtt_username.c_str(), mqtt_password.c_str());
//   else snprintf(local_sbuf,sizeof(local_sbuf),"Supervisor NOT connected to MQTT %s=%d=%s=%s", mqtt_server.c_str(), atol(mqtt_port.c_str()), mqtt_username.c_str(), mqtt_password.c_str());
//  Local_Logs_Dispatch(local_sbuf);
}

void MqttInit()
{   
    MqttReconnectTimer = xTimerCreate("mqttTimer",
        pdMS_TO_TICKS(5000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
 
    MqttClient.onConnect(onMqttConnect);
    MqttClient.onDisconnect(onMqttDisconnect);
    //MqttClient.onSubscribe(onMqttSubscribe);
   // MqttClient.onUnsubscribe(onMqttUnSubscribe);
   // MqttClient.onMessage(onMqttMessage);
   // MqttClient.onPublish(onMqttPublish);

    MqttClient.setServer(mqtt_server.c_str(), atol(mqtt_port.c_str()));
    if (mqtt_username != "")
        MqttClient.setCredentials(mqtt_username.c_str(), mqtt_password.c_str());

    connectToMqtt();
}

void onI2CRequest(void) 
{
  // ESP32 Slave I2c has a strange behavior (S3 or C3 are differents)
  // https://forum.arduino.cc/t/understanding-esp32-wireslave-example/1000680/7
  // OnRequest must exist but empty
  // I2C slave flushes the buffer when onRequest is triggered !
  return; 
}

void PMReport(char *info)
{
  char* delim=strstr(info, "=");
  if (!delim) return;
  delim[0]=0;
  delim++;
  if (delim[0]==0) return;

  if (strcmp(info, "PM_SSID")==0) {
    strcpy(PoolMaster_SSID, delim);
    return;
  }
  if (strcmp(info, "PM_RSSI")==0) {
    strcpy(PoolMaster_RSSI, delim);
    return;
  }
  if (strcmp(info, "PM_HOSTNAME")==0) {
    strcpy(PoolMaster_Hostname, delim);
    return;
  }
  if (strcmp(info, "PM_IP")==0) {
    strcpy(PoolMaster_IP, delim);
    return;
  } 
  if (strcmp(info, "PM_MAC")==0) {
    strcpy(PoolMaster_MAC, delim);
    return;
  } 
  if (strcmp(info, "PM_FIRMW")==0) {
    strcpy(PoolMaster_Version, delim);
    return;
  }
  if (strcmp(info, "PM_TFTFIRMW")==0) {
    strcpy(PoolMaster_TFTVersion, delim);
    return;
  }
  if (strcmp(info, "PM_UPTIME")==0) {
    strcpy(PoolMaster_Uptime, delim);
    return;
  }
  if (strcmp(info, "PM_MQTT_SERVER")==0) {
    strcpy(PoolMaster_MQTT_Server, delim);
    return;
  }
  if (strcmp(info, "PM_MQTT_TOPIC")==0) {
    strcpy(PoolMaster_MQTT_Topic, delim);
    return;
  }
  if (strcmp(info, "PM_STATUSLEDS")==0) {
    uint8_t led = 0;
    sscanf(delim, "%u", &led);
    PoolMaster_StatusLEDs = led;
    return;
  }

}

String PMRequest(char *question)
{
  // when pressed "share network button"
  // configure poolmaster network but only during a certain time
  // to avoid conflicts when config is done from Nextion display

  if (autoConfNetwork > 0) {
    if (strcmp(question, "GET_WIFI_SSID") == 0)  return wifiManager.getWiFiSSID();
    if (strcmp(question, "GET_WIFI_PASS") == 0)  return wifiManager.getWiFiPass();
    if (strcmp(question, "GET_HOSTNAME") == 0)   return String(hostname);
    autoConfNetwork--;
  }
  // when pressed "share mqtt Button"
  // configure poolmaster mqtt 
  if (autoConfMQTT > 0) {
    if (strcmp(question, "GET_MQTT_SERVER") == 0)   return mqtt_server;
    if (strcmp(question, "GET_MQTT_PORT") == 0)     return mqtt_port;
    if (strcmp(question, "GET_MQTT_TOPIC") == 0)    return mqtt_topic;
    if (strcmp(question, "GET_MQTT_USERNAME") == 0) return mqtt_username;
    if (strcmp(question, "GET_MQTT_PASSWORD") == 0) return mqtt_password;
    autoConfMQTT--;
  }
  return String("");
}


void onI2CReceive(int len) 
{
  // Received a request from PoolMaster
  char question[I2C_MAXMESSAGE];
  int i=0;
  while (Wire.available()>0) question[i++] = Wire.read();
  question[i] = '\0';

  // deal with the message recevied from PoolMaster
  String Svalue = "Ok";

  // PoolMaster sends reports when question starts with PM_
  if (strncmp(question, "PM_", 3) == 0) {
    PMReport(question);
  }
  // PoolMaster wants info from SuperVisor
  else if (strncmp(question, "GET_", 4) == 0) {
    Svalue = PMRequest(question);
  }
  else {  // Just a message to print
    sprintf(local_sbuf,"%s",question);
    Local_Logs_Dispatch(local_sbuf);
  }
 
  // SlaveWrite should be in function onRequest except for ESP32 nonS3-nonC3 !
  // fill the buffer, use I2C_MAXMESSAGE not strlen, _DELIMITER_ is EOL
  Svalue += _DELIMITER_;
  Wire.slaveWrite((uint8_t *)Svalue.c_str(), I2C_MAXMESSAGE); 
}



void saveConfigCallback() 
{
   shouldSaveConfig = true;
}

String createHTML()
{
  // This is a test
  // Create html pages with inline CSS 
  // todo :create nicer interface
  // https://codingtorque.com/tabs-using-pure-html-and-css/
  // should fit in mobile phone screen
  // logo convertor https://elmah.io/tools/base64-image-encoder/
  
  String h = "";
  String css="";
  css += "li {display:inline; padding-right: 1em;}";
  css += ".stick { position: sticky; top: 0; padding: 10px 16px; background-color: Gainsboro;}"; // doesn't work !
  //css += "li {display:inline; position: sticky; top: 0; background-color: Gainsboro;}";
  css += ".submitButton {";
  css += "  background-color: #4caf50;";
  css += "  display: inline-block; font-weight: bold; border: 1px solid #2d6898;";
  css += "  color: white;";
  css += "  padding: 10px 15px;";
  css += "  border-radius: 4px;";
  css += "  cursor: pointer;";
  css += "  margin-right: 10px; }";
  css += ".UpdateButton {";
  css += "  background-color: FireBrick;";
  css += "  display: inline-block; font-weight: bold; border: 1px solid #2d6898;";
  css += "  color: white;";
  css += "  padding: 10px 15px;";
  css += "  border-radius: 4px;";
  css += "  cursor: pointer;";
  css += "  margin-right: 10px; }";
  css += ".shareButton {";
  css += "  background-color: SkyBlue;";
  css += "  display: inline-block; font-weight: bold; border: 1px solid #2d6898;";
  css += "  color: white;";
  css += "  padding: 10px 15px;";
  css += "  border-radius: 4px;";
  css += "  cursor: pointer;";
  css += "  margin-right: 10px; }";
  css += ".normalButton {";
  css += "  background-color: Silver;";
  css += "  display: inline-block; font-weight: bold; border: 1px solid #2d6898;";
  css += "  color: white;";
  css += "  padding: 10px 15px;";
  css += "  border-radius: 4px;";
  css += "  cursor: pointer;";
  css += "  margin-right: 10px; }";
  css += ".rebootButton {";
  css += "  background-color: Black;";
  css += "  display: inline-block; font-weight: bold; border: 1px solid #2d6898;";
  css += "  color: white;";
  css += "  padding: 10px 15px;";
  css += "  border-radius: 4px;";
  css += "  cursor: pointer;";
  css += "  margin-right: 10px; }";

  css += "img { float: left; margin-right: 20px; }";
  css += ".text-container { margin-left: 20px; }";
  //css += "form { display: inline-block;text-align: left;max-width: 400px;margin: 20px auto;}";
  css += "label { display: block; width: 200px; margin-top: 8px;}";
  //css += "input { width: 100%; padding: 8px; margin-bottom: 12px; box-sizing: border-box;}";

  h += "<!DOCTYPE HTML><html><head><title>PoolMaster SuperVisor</title>";
  //h += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  h += "<style>";
  h += css;
  h += "</style>";

  h += "</head>";
  h += "<body>";
  h +=  "<div class=\"image-container\">";
  h +=    "<img src=\"data:image/jpeg;base64,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\" />";
  h +=    "<div class=\"text-container\">";
  h +=      "<p><h1>PoolMaster SuperVisor</h1></p>";
  h +=    "</div>";
  h +=    "<ul>";
  h +=      "<li class=\"stick\"><a href=\"#Info\">Info</a></li>";
  h +=      "<li class=\"stick\"><a href=\"#Settings\">Settings</a></li>";
  h +=      "<li class=\"stick\"><a href=\"#Update\">Update</a></li>";
  #ifdef TARGET_WEBSERIAL
  h +=      "<li class=\"stick\"><a href=\"#Logs\">Logs</a></li>";
  #endif
  h +=    "</ul>";

  // Info
  h +=    "<br>";
  h +=    "<h2 id=\"Info\">Information:</h1>";
  h +=      "<p>";
  h +=      "<table border=\"0\">";
  h +=        "<tr><td>PoolMaster Version :</td><td>"     + String(PoolMaster_Version)        + "</td></tr>";
  h +=        "<tr><td>PoolMaster TFT Version :</td><td>" + String(PoolMaster_TFTVersion)     + "</td></tr>";
  h +=        "<tr><td>PoolMaster Hostname :</td><td>"    + String(PoolMaster_Hostname)       + "</td></tr>";
  h +=        "<tr><td>PoolMaster IP Address :</td><td>"  + String(PoolMaster_IP)             + "</td></tr>";
  h +=        "<tr><td>PoolMaster MAC Address :</td><td>" + String(PoolMaster_MAC) 
             + "</td></tr>";
  h +=        "<tr><td>PoolMaster Wifi SSID :</td><td>"   + String(PoolMaster_SSID)           + "</td></tr>";
  h +=        "<tr><td>PoolMaster Wifi RSSI :</td><td>"   + String(PoolMaster_RSSI)           + "</td></tr>";
  h +=        "<tr><td>PoolMaster MQTT Server :</td><td>" + String(PoolMaster_MQTT_Server)    + "</td></tr>";
  h +=        "<tr><td>PoolMaster MQTT Topic :</td><td>"  + String(PoolMaster_MQTT_Topic)     + "</td></tr>";
  h +=        "<tr><td>PoolMaster Uptime :</td><td>"      + String(PoolMaster_Uptime)         + "</td></tr>";
  h +=        "<tr><td>PoolMaster LEDs :</td><td>"        + String(convert2bin(PoolMaster_StatusLEDs))+ "</td></tr>";
  h +=        "<tr><td></td><td></td></tr>";
  h +=        "<tr><td></td><td></td></tr>";
  h +=        "<tr><td></td><td></td></tr>";
  h +=        "<tr><td>SuperVisor Version :</td><td>"     + String(PMSV_VERSION)              + "</td></tr>";
  h +=        "<tr><td>SuperVisor Hostname :</td><td>"    + String(WiFi.getHostname())        + "</td></tr>";
  h +=        "<tr><td>SuperVisor IP Address :</td><td>"  + WiFi.localIP().toString()         + "</td></tr>";
  h +=        "<tr><td>SuperVisor MAC Address :</td><td>" + WiFi.macAddress();                + "</td></tr>";
  h +=        "<tr><td>SuperVisor Wifi SSID :</td><td>"   + wifiManager.getWiFiSSID()         + "</td></tr>";
  h +=        "<tr><td>SuperVisor Wifi RSSI :</td><td>"   + String(WiFi.RSSI())               + "</td></tr>";
  h +=        "<tr><td>SuperVisor MQTT Server :</td><td>" + mqtt_server + ":"+ mqtt_port      + "</td></tr>";
  h +=        "<tr><td>SuperVisor MQTT Topic :</td><td>"  + mqtt_topic                        + "</td></tr>";
  uptime();
  h +=        "<tr><td>SuperVisor Uptime :</td><td>"      + String(currentUptime)             + "</td></tr>";
  h +=      "</table>";
  h +=      "</p>";
  h +=      "<button id=\"refresh\" class=\"normalButton\" onclick=\"document.location.reload(false)\"> Refresh </button>";
  h +=       "<br><br>";
  h +=       "<form action=\"/get\">";
  h +=            "<input type=\"hidden\" id=\"rebootPoolMaster\" name=\"rebootPoolMaster\">";
  h +=            "<input type=\"submit\" class=\"rebootButton\" id=\"rebootPoolMaster\" value=\"Restart PoolMaster\">";
  h +=       "</form>";
  h +=       "<br>";
  h +=       "<form action=\"/get\">";
  h +=            "<input type=\"hidden\" id=\"rebootSuperVisor\" name=\"rebootSuperVisor\">";
  h +=            "<input type=\"submit\" class=\"rebootButton\" id=\"rebootSuperVisor\" value=\"Restart SuperVisor\">";
  h +=       "</form>";
  h +=    "<br>";

  // Settings
  h +=    "<h2 id=\"Settings\">Settings:</h1>";
  h +=      "<p>";
  h +=        "<form action=\"/get\">";
  h +=            "<input type=\"hidden\" id=\"autoconfnet\" name=\"autoconfnet\">";
  h +=            "<input type=\"submit\" class=\"shareButton\" id=\"shareNetButton\" value=\"Auto-configure PoolMaster network\">";
  h +=        "</form>";
  h +=        "<br><br>";
  h +=        "<form action=\"/get\">";
  h +=            "<label for=\"mqtt_server\">MQTT Server:</label>";
  h +=            "<input type=\"text\" id=\"mqtt_server\" name=\"mqtt_server\" size=30 value=\"" + mqtt_server + "\">";
  h +=            "<br>";
  h +=            "<label for=\"mqtt_port\">MQTT Port :</label>";
  h +=            "<input type=\"text\" id=\"mqtt_port\" name=\"mqtt_port\" size=30 value=\"" + mqtt_port + "\">";
  h +=            "<br>";
  h +=            "<label for=\"mqtt_topic\">MQTT Topic :</label>";
  h +=            "<input type=\"text\" id=\"mqtt_topic\" name=\"mqtt_topic\" size=30 value=\"" + mqtt_topic + "\">";
  h +=            "<br>";
  h +=            "<label for=\"mqtt_username\">MQTT Username :</label>";
  h +=            "<input type=\"text\" id=\"mqtt_username\" name=\"mqtt_username\" size=30 value=\"" + mqtt_username + "\">";
  h +=            "<br>";
  h +=            "<label for=\"mqtt_password\">MQTT Password :</label>";
  h +=            "<input type=\"password\" id=\"mqtt_password\" name=\"mqtt_password\" size=30 value=\"" + mqtt_password + "\">";
  h +=            "<br>";
  h +=            "<br>";
  h +=            "<input type=\"submit\" class=\"submitButton\" id=\"submitButton\" value=\"Save MQTT settings\">";
  h +=            "<br>";
  //h +=            "<h4>(then wait few minutes)</h4>";
  h +=        "</form>";
  h +=        "<br>";
  h +=        "<form action=\"/get\">";
  h +=            "<input type=\"hidden\" id=\"autoconfmqtt\" name=\"autoconfmqtt\">";
  h +=            "<input type=\"submit\" class=\"shareButton\" id=\"shareMqttButton\" value=\"Auto-configure PoolMaster MQTT\">";
  h +=        "</form>";
  h +=        "<br>";

#ifdef TARGET_PAPERTRAIL
  h +=        "<br><br>";
  h +=        "<form action=\"/get\">";
  h +=            "<label for=\"papertrailhost\">Papertrail Server :</label>";
  h +=            "<input type=\"text\" id=\"papertrailhost\" name=\"papertrailhost\" size=30 value=\"" + papertrailhost + "\">";
  h +=            "<br>";
  h +=            "<label for=\"papertrailport\">Papertrail Port :</label>";
  h +=            "<input type=\"text\" id=\"papertrailport\" name=\"papertrailport\" size=30 value=\"" + papertrailport + "\">";
  h +=            "<br>";
  h +=            "<br>";
  h +=            "<input type=\"submit\" class=\"submitButton\" id=\"submitButton\" value=\"Save Papertrail settings\">";
  h +=            "<br>";
  h +=        "</form>";
  #endif

  h +=      "</p>";
  h +=    "<br>";

  // Update
  h +=    "<h2 id=\"Update\">Update:</h1>";
  h +=      "<p>";
  h +=        "<form action=\"/get\">";
  h +=          "<label for=\"Updatehost\">Update Host (http):</label>";
  h +=          "<input type=\"text\" id=\"Updatehost\" name=\"Updatehost\" size=30 value=\"" + Updatehost + "\">";
  h +=          "<br>";
  h +=          "<label for=\"poolmasterpath\">PoolMaster firmware path :</label>";
  h +=          "<input type=\"text\" id=\"poolmasterpath\" name=\"poolmasterpath\" size=30 value=\"" + poolmasterpath + "\">"; 
  h +=          "<br>";
  h +=          "<label for=\"nextionpath\">Nextion firmware path :</label>";
  h +=          "<input type=\"text\" id=\"nextionpath\" name=\"nextionpath\" size=30 value=\"" + nextionpath + "\">";    
  h +=          "<br>";
  h +=          "<br>";
  h +=          "<input type=\"submit\" class=\"submitButton\" id=\"submitButton\" value=\"Save Update settings\">";
  h +=        "</form>";
  h +=        "<br><br>";
  
  h +=        "<form action=\"/get\">";
  h +=            "<input type=\"hidden\" id=\"UpdatePoolMaster\" name=\"UpdatePoolMaster\">";
  h +=            "<input type=\"submit\" class=\"UpdateButton\" id=\"UpdatePoolMaster\" value=\"Update PoolMaster\">";
  h +=        "</form>";
  h +=        "<br><br>";
  h +=        "<form action=\"/get\">";
  h +=            "<input type=\"hidden\" id=\"UpdateNextion\" name=\"UpdateNextion\">";
  h +=            "<input type=\"submit\" class=\"UpdateButton\" id=\"UpdateNextion\" value=\"Update Display Nextion\">";
  h +=        "</form>";
  h +=        "<br><br>";
  h +=        "<a href=\"/update\"><button class=\"UpdateButton\">Update SuperVisor (OTA)</button></a>";
  h +=        "<br><br>";
  h +=      "</p>";
#ifdef TARGET_WEBSERIAL
  // Webserial
  h +=    "<h2 id=\"Logs\">Logs:</h1>";
  h +=      "<p>";
  h +=        "<br>";
  h +=        "<embed type=\"text/html\" src=\"/webserial\"  width=\"700\" height=\"1000\">";
  h +=     "</p>";
#endif
  h += "</body></html>";
  return h;
} 

void loadSettings()
{
  preferences.begin("PMSV", true);
  //hostname = preferences.getString("hostname", "");
  preferences.getString("hostname",hostname,15); 

  mqtt_server   = preferences.getString("mqtt_server", defaultmqtt_server);
  mqtt_port     = preferences.getString("mqtt_port", defaultmqtt_port);
  mqtt_topic    = preferences.getString("mqtt_topic", defaultmqtt_topic);
  mqtt_username = preferences.getString("mqtt_username", defaultmqtt_username);
  mqtt_password = preferences.getString("mqtt_password", defaultmqtt_password);

  Updatehost     = preferences.getString("Updatehost", defaultUpdatehost);
  poolmasterpath  = preferences.getString("poolmasterpath", defaultpoolmasterpath);
  nextionpath     = preferences.getString("nextionpath", defaultnextionpath);

#ifdef TARGET_PAPERTRAIL
  papertrailhost  = preferences.getString("nextionpath", defaultpapertrailhost);
  papertrailport  = preferences.getString("nextionpath", defaultpapertrailport);
#endif

  preferences.end();
}

void InitWebServer()
{

  initElegantOTA();

  #ifdef TARGET_WEBSERIAL
  // Start WebSerial
  WebSerial.begin(&Webserver);
  WebSerial.onMessage(&recvMsg); /* Attach Message Callback */
  Webserver.onNotFound([](AsyncWebServerRequest* request) { request->redirect("/"); });
  #endif

   // Send web page with input fields to client
  Webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", createHTML());
  });

  // Send a GET request to <ESP_IP>
  Webserver.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
  
    preferences.begin("PMSV", false);
    if (request->hasParam("mqtt_server")) {
      mqtt_server = request->getParam("mqtt_server")->value();
      preferences.putString("mqtt_server", mqtt_server);
    }
    if (request->hasParam("mqtt_port")) {
      mqtt_port = request->getParam("mqtt_port")->value();
      preferences.putString("mqtt_port", mqtt_port);
    }
    if (request->hasParam("mqtt_topic")) {
      mqtt_topic = request->getParam("mqtt_topic")->value();
      preferences.putString("mqtt_topic", mqtt_topic);
    }
    if (request->hasParam("mqtt_username")) {
      mqtt_username = request->getParam("mqtt_username")->value();
      preferences.putString("mqtt_username", mqtt_username);
    }
    if (request->hasParam("mqtt_password")) {
      mqtt_password = request->getParam("mqtt_password")->value();
      preferences.putString("mqtt_password", mqtt_password);
    }
    if (request->hasParam("autoconfnet")) {
      autoConfNetwork = autoConfTimeout;
    }
    if (request->hasParam("autoconfmqtt")) {
      autoConfMQTT = autoConfTimeout;
    }

    if (request->hasParam("Updatehost")) {
      Updatehost = request->getParam("Updatehost")->value();
      preferences.putString("Updatehost", Updatehost);
    }
    if (request->hasParam("poolmasterpath")) {
      poolmasterpath = request->getParam("poolmasterpath")->value();
      preferences.putString("poolmasterpath", poolmasterpath);
    }
    if (request->hasParam("nextionpath")) {
      nextionpath = request->getParam("nextionpath")->value();
      preferences.putString("nextionpath", nextionpath);
    }

#ifdef TARGET_PAPERTRAIL    
    if (request->hasParam("papertrailhost")) {
      papertrailhost = request->getParam("papertrailhost")->value();
      preferences.putString("papertrailhost", papertrailhost);
    }
    
    if (request->hasParam("papertrailport")) {
      papertrailport = request->getParam("papertrailport")->value();
      preferences.putString("papertrailport", papertrailport);
    }
#endif

    preferences.end();

    if (request->hasParam("rebootSuperVisor")) {
      request->redirect("/"); 
      delay(300);
      ESP.restart();
    }

    if (request->hasParam("rebootPoolMaster")) {
      mustRebootPoolMaster = true;
    }
    
    if (request->hasParam("UpdatePoolMaster")) {
      // does not work with the loop
      mustUpdatePoolMaster = true;
      // does not work too when called directly !
      //  TaskUpdatePoolMaster();
    }

    if (request->hasParam("UpdateNextion")) {
      mustUpdateNextion = true;
    }

    request->redirect("/"); 

    });

  Webserver.begin();
}

//////////////////////// SETUP //////////////////////////
/////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  loadSettings();

  // Start WifiManager and resetPin
  pinMode(RESET_WIFI_PIN, INPUT_PULLUP);

  // Set hostname if any
  if (hostname[0] != '\0') {  // with ESP32, set hostname before wifi.mode !
    sprintf(myhostname, "%s%s", hostname, SuperVisor_Suffix); // SuperVisor Hostname
    WiFi.setHostname(myhostname);
  }
  WiFi.mode(WIFI_STA);
  
  // Create uniq SSID
  char ssid[32];uint32_t id=0;
  for(int i=0; i<17; i=i+8) id |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  sprintf(ssid, "%s%08X", _DEFAULT_NAME_, id);

  // Choose Hostname(s) for our ESP32s in WifiManager
  WiFiManagerParameter custom_hostname("Hostname", "Hostname", _DEFAULT_NAME_, 40);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_hostname);
  std::vector<const char*> wmMenuItems = {"wifi"};
  wifiManager.setMenu(wmMenuItems);
  wifiManager.setCustomHeadElement("<style>body{max-width:500px;margin:auto}</style>");
  if (!wifiManager.autoConnect(ssid)) {
		delay(pdMS_TO_TICKS(3000));
		ESP.restart();
  }
  if (shouldSaveConfig) {
    //* this is the 1st reboot after wifimanager
    String customhostname = custom_hostname.getValue();
    preferences.begin("PMSV", false);
    preferences.putString("hostname", customhostname);
    preferences.end();
    delay(pdMS_TO_TICKS(1000));
		ESP.restart();    // ESP32 must reboot but why, anyway
  }

  MDNS.addService("http", "tcp", 80);
  MDNS.begin(myhostname);      

  Local_Logs_Dispatch("");
  Local_Logs_Dispatch("WiFi connected ");
  snprintf(local_sbuf,sizeof(local_sbuf),"Hostname %s, IP address: %s ", WiFi.getHostname(), WiFi.localIP());
  Local_Logs_Dispatch(local_sbuf);

  // Start I2C Slave channel, talking to PoolMaster
  // Using PoolMaster "Extensions"
  Wire.onReceive(onI2CReceive);
  Wire.onRequest(onI2CRequest);
  Wire.begin(I2C_SLAVE_ADDR, SDA_S, SCL_S, 100000U);

  // Start I2C Master channel for OLED
  Wire1.begin(SDA_M, SCL_M);

  // Config NTP
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < SECS_YR_2000) {
    delay(100);
    now = time(nullptr);
  }
  setTime(now);

  InitWebServer();
  MqttInit();

  //Connect to serial receiving messages from PoolMaster
  Serial2.setTimeout(100);
  Serial2.setRxBufferSize(1024);
  Serial2.begin(115200);

  //Start Telnet Server
  #ifdef TARGET_TELNET
  Telnetserver.begin();
  Telnetserver.setNoDelay(true);
  #endif

  Local_Logs_Dispatch("Ready! Use 'telnet ");
  Local_Logs_Dispatch(" 23' to connect");

  
#ifdef TARGET_PAPERTRAIL
  // Start PaperTrail Logging
  if ((papertrailhost != "") && (papertrailhost != defaultpapertrailhost)) {
    uint16_t port = atoi(papertrailport.c_str());
    logger.configureSyslog(papertrailhost.c_str(), port, ""); // Syslog server IP, port and device name
    //logger.registerSerial(COUNTER, DEBUG, "COUNT", Serial); // Log both to serial...
    logger.registerSyslog(PL_LOG, DEBUG, FAC_LOCAL0, "poolmaster"); // ...and syslog. Set the facility to user
    logger.registerSyslog(WD_LOG, DEBUG, FAC_LOCAL0, "watchdog"); // ...and syslog. Set the facility to user
    logger.log(WD_LOG, INFO, "PoolMaster Logs Started");
    logger.log(WD_LOG, INFO, "WatchDog Logs Started");
  }
 #endif  

  Local_Logs_Dispatch("Creating Tasks");
  // Create loop tasks in the scheduler.
  //------------------------------------
  int app_cpu = xPortGetCoreID();

  xTaskCreatePinnedToCore(
    TheTasksLoop, 
    "TheTasksLoop",
//    4500, // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    6000, // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    NULL,  // No parameter is used
    1,  // Priority
    nullptr,  // Task handle is not used here
    app_cpu
  );
}


void WifiManagerCheckResetLoop() {
  // check for button press
  if (digitalRead(RESET_WIFI_PIN) == LOW ) {
    Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings
    delay(pdMS_TO_TICKS(3000)); // reset delay hold
    if (digitalRead(RESET_WIFI_PIN) == LOW ) {
      Serial.println("Wifi Button Held");
      Serial.println("Erasing Config, restarting");
      preferences.begin("PMSV", false);
      preferences.clear();
      preferences.end();
      wifiManager.resetSettings();
      ESP.restart();
    }
  }
}



//////////////////////// MAIN LOOP //////////////////////////
/////////////////////////////////////////////////////////////
void loop() {
  uint8_t i;

  if (WiFi.status() == WL_CONNECTED) {
    #ifdef TARGET_TELNET
    //check if there are any new clients
    if (Telnetserver.hasClient()) {
      for (i = 0; i < MAX_SRV_CLIENTS; i++) {
        //find free/disconnected spot
        if (!serverClients[i] || !serverClients[i].connected()) {
          if (serverClients[i]) {
            serverClients[i].stop();
          }
          serverClients[i] = Telnetserver.accept();
          if (!serverClients[i]) {
            Local_Logs_Dispatch("available broken");
          }
          IPAddress ip_temp=serverClients[i].remoteIP();
          snprintf(local_sbuf,sizeof(local_sbuf),"New Client %d (%d.%d.%d.%d)",i,ip_temp[0],ip_temp[1],ip_temp[2],ip_temp[3]);
          Local_Logs_Dispatch(local_sbuf);
          break;
        }
      }
      if (i >= MAX_SRV_CLIENTS) {
        //no free/disconnected spot so reject
        Telnetserver.accept().stop();
      }
    }
    //check clients for data
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      if (serverClients[i] && serverClients[i].connected()) {
        if (serverClients[i].available()) {
          //get data from the telnet client and push it to the UART
          while (serverClients[i].available()) {
            //Serial.write(serverClients[i].read());
            cmdExecute(serverClients[i].read());
          }
        }
      } else {
        if (serverClients[i]) {
          serverClients[i].stop();
        }
      }
    }
    #endif
    //check UART for data
    if (Serial2.available()) {
      if (readUntil(Serial2, sbuf, "\n")) {
        // `buf` contains the delimiter, it can now be used for parsing.     
        #ifdef TARGET_TELNET 
        //push UART data to all connected telnet clients
        for (i = 0; i < MAX_SRV_CLIENTS; i++) {
          if (serverClients[i] && serverClients[i].connected()) {
            serverClients[i].write(sbuf, strlen(sbuf));
            delay(1);
          }
        }
        #endif

        // Cleanup buffer and send to the web
        for (int src = 0, dst = 0; src < sizeof(sbuf); src++)
        if (sbuf[src] != '\r') sbuf[dst++] = sbuf[src];

        #ifdef TARGET_WEBSERIAL
        // WebSerial
        WebSerial.printf("%s",sbuf);
        #endif

        
#ifdef TARGET_PAPERTRAIL
        // Cloud Logger PaperTrail
        if ((papertrailhost != "") && (papertrailhost != defaultpapertrailhost)) {

          int logLevel = 0;
          bool must_send_to_server = false;
          char *loglevel_position;
          // Parse line to get log level and convert to standard syslog levels
          loglevel_position = strstr(sbuf,"[DBG_ERROR  ]");
          if (loglevel_position != NULL) {
            logLevel = 3;
            must_send_to_server = true;
          }

          loglevel_position = strstr(sbuf,"[DBG_WARNING]");
          if (loglevel_position != NULL) {
            logLevel = 4;
            must_send_to_server = true;
          }

          loglevel_position = strstr(sbuf,"[DBG_INFO   ]");
          if (loglevel_position != NULL) {
            logLevel = 5;
            must_send_to_server = true;
          }

          loglevel_position = strstr(sbuf,"[DBG_DEBUG  ]");
          if (loglevel_position != NULL) {
            logLevel = 6;
            must_send_to_server = true;
          }

          // Do not send the verbose to the server
          loglevel_position = strstr(sbuf,"[DBG_VERBOSE]");
          if (loglevel_position != NULL)
            logLevel = 7;

          logger.log(PL_LOG, logLevel, "%s", sbuf);
        }
#endif
      }
    }
  } else {
    Local_Logs_Dispatch("WiFi not connected!");
    #ifdef TARGET_TELNET
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      if (serverClients[i]) {
        serverClients[i].stop();
      }
    }
    #endif
    delay(pdMS_TO_TICKS(1000));
  }

  WifiManagerCheckResetLoop();

  // Check for updates
  ElegantOTA.loop();
  #ifdef TARGET_WEBSERIAL
  // Update WebSerial
  WebSerial.loop();
  #endif
}