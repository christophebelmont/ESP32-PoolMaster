/*
  PoolMaster SuperVisor - Act as a watchdog, OOBE, upload manager and supervise PoolMaster
  WifiManager :
        https://github.com/tzapu/WiFiManager WiFi Configuration Magic

  I2C Slave and Master :
        https://deepbluembedded.com/arduino-i2c-slave/
        https://randomnerdtutorials.com/esp32-i2c-master-slave-arduino/

  HTML, DOM, CSS :
        https://mollify.noroff.dev/content/feu1/javascript-1/module-4/update-html?nav=
        https://www.w3schools.com/howto/howto_js_progressbar.asp
        https://www.youtube.com/watch?v=rtx27XZadIw

  Logs from Serial :
        https://github.com/WolfgangFranke/ESP32_remote_longterm_logging/blob/master/Ardunio-Code/ESP32_WebServer_Highcharts_WebLogger/ESP32_WebServer_Highcharts_WebLogger.ino
        https://stackoverflow.com/questions/61755373/create-html-table-using-json-data-in-javascript

  OTA : https://github.com/IPdotSetAF/ESPAsyncHTTPUpdateServer
        https://randomnerdtutorials.com/esp32-over-the-air-ota-programming/
        https://www.reddit.com/r/esp32/comments/gqostn/ota_parameters_from_url_webserver_vs/
*/

// TODO :
//  Propose to fully format ESP32-PoolMaster (copy bootloader+partition+firmware.bin)
//  https + authentication
//  draw progress bar on LCD when doing an update of software (poolmater+supervisor+nextion)


#include <WiFiManager.h>
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include <HTTPClient.h>
#include <ESPNexUpload.h>
#include "esp32_flasher.h"
#include "soc/rtc_wdt.h"
#include <Wire.h>
#include "driver/i2c.h"
//#include <ESPmDNS.h>
#include <Preferences.h>
#include <uptime.h>
#include <TimeLib.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <Elog.h>
#include "SuperVisor.h"
#include <nvs_flash.h>

const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// PaperTrail
#define PL_LOG 0  // PoolMaster Logs
#define WD_LOG 1  // WatchDog Logs

//how many clients should be able to telnet to this ESP32
#define MAX_SRV_CLIENTS 3

extern void createHAEntities();
extern void cleanHAEntities();

// Update triggers
volatile bool mustUpdateNextion       = false;
volatile bool mustUpdatePoolMaster    = false;
volatile bool mustUpdateSuperVisor    = false;
volatile bool mustRebootPoolMaster    = false;
volatile bool mustRebootSuperVisor    = false;
volatile bool mustRebootNextion       = false;
volatile bool mustCreateHAEntities    = false;
volatile bool mustCleanHAEntities     = false;
volatile bool mustRestartMQTT         = false;
volatile bool mustUpdateWMCounter     = true;

const char* defaultUpdatehost         = "myUpdateHttpServer:myport";
const char* defaultNextionPath        = "/build/Nextion.tft";
const char* defaultPoolmasterPath     = "/build/PoolMaster.bin";
const char* defaultSuperVisorPath     = "/build/SuperVisor.bin";

// PaperTrail log management
const char* defaultpapertrailhost      = "";
const char* defaultpapertrailport     = "21858";

// MQTT Server
const char* defaultmqtt_server        = "";
const char* defaultmqtt_port          = "1883";
const char* defaultmqtt_topic         = "PoolMaster";
const char* defaultmqtt_username      = "";
const char* defaultmqtt_password      = "";
const char* defaulthomeassistanttopic = "";

AsyncMqttClient MqttClient;
TimerHandle_t MqttReconnectTimer = 0;

char  myhostname[_LHOSTNAME_] = {0};
char  hostname[_LHOSTNAME_]   = {0};
char  currentUptime[25];

JsonDocument PMInfo;
JsonDocument SVSettings;

// Nextion/Poolmaster Update counter for feedback
int UpdateCounter = 0;
int UpdateinProgress=0;
int contentLength = 0;

char barBuf[64] = {0};

// Nextion PIN Numbers
#define NEXT_RX           33 // Nextion RX pin
#define NEXT_TX           32 // Nextion TX pin
#define NEXT_REBOOT       13 // Nextion reboot pin

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
#define I2C_MAXMESSAGE    64
#define SDA_M             14
#define SCL_M             27

// Wifi Manager
#define RESET_WIFI_PIN    23  // GPIO=LOW -> start WifiManager, reset settings when held 3sec, chagen to another GPIO if PCB=v3.2
WiFiManager wifiManager;
#define _DEFAULT_NAME_       "PoolMaster"
#define SuperVisor_Suffix     "_SV"
Preferences preferences;
bool shouldSaveConfig = false;

WiFiServer Telnetserver(23);
WiFiClient serverClients[MAX_SRV_CLIENTS];
AsyncWebServer Webserver(80);

// IR presence detector, wake up LCD TFT
extern void TFT_Refresh(bool);
extern void TFT_Init();
bool refreshTFT=false; // to force TFT refresh 
int IRDetected = -1; // by default, no IR detector

// Local logline buffers
char sbuf[BUFFER_SIZE];
char local_sbuf[LOG_BUFFER_SIZE];

#define LogLines_size   30  // max number of log-lines in log ring buffer
#define LogLines_maxLen 200 // max lenght of each Log-Line text
class LogsRingBuffer
{
  private:
    int readindex = -1;
    int writeindex = -1;
    char LogLines_array[LogLines_size][LogLines_maxLen] = {0};  
    
  public:
    void push(const char* prefix, char* line) {
      int src, dst;
      for (src = 0, dst = 0; src < strlen(line); src++)
        if ((line[src] != '\r') && (line[src] != '\n')) 
          line[dst++] = line[src];
      line[dst] = 0;
      if (line[0] == 0) return;
      writeindex++;
      writeindex %= LogLines_size;
      snprintf(LogLines_array[writeindex], LogLines_maxLen-1, "%s%s", prefix, line);
      LogLines_array[(writeindex+1)%LogLines_size][0] = 0 ; // tag next circular line, obsolete
    }
    void push(const char* prefix, const char* line) {
      strcpy(sbuf, line);
      push(prefix, sbuf);
    }
    char* pull() {
      readindex++;
      readindex %= LogLines_size;
      char* l = LogLines_array[readindex];
      if (*l == 0) { // obsolete data
        readindex--;
        return 0;
      }
      return l;
   }
};
LogsRingBuffer myLogsRingBuffer;

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

 const char* papertrailhost=SVSettings["Papertrail Host"];
  if ((strcmp(papertrailhost, "") != 0) && (strcmp(papertrailhost, defaultpapertrailhost) != 0) && (_targets & 4)) {  // Third bit is for ParerTrail
    // Cloud PaperTrail
    Logger.log(WD_LOG, 1, "%s", _log_message);
  }

  myLogsRingBuffer.push("sv ", _log_message);
}

// OTA
// ///

void onOTAStart() 
{
  // Log when OTA has started
  strcpy(local_sbuf,"OTA update started!");
  strcpy(barBuf, local_sbuf);
  Local_Logs_Dispatch(local_sbuf);
  UpdateinProgress=1;
}

void onOTAProgress(size_t current, size_t final) 
{
  UpdateinProgress = (float(current)/final)*100.0;
  snprintf(local_sbuf,sizeof(local_sbuf),"SuperVisor update %d%%",UpdateinProgress);
  strcpy(barBuf, local_sbuf);
  Local_Logs_Dispatch(local_sbuf);
}

void onOTAEnd(bool success) {
  if (success)  strcpy(local_sbuf,"OTA update finished successfully!");
  else          strcpy(local_sbuf,"There was an error during OTA update!");

  strcpy(barBuf, local_sbuf);
  Local_Logs_Dispatch(local_sbuf);
  delay(3000);
  UpdateinProgress=0;
  if (success) mustRebootSuperVisor=1;
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) 
{
  if (!index) {
    size_t content_len = request->contentLength();
    // check file names for type
    onOTAStart();
    int cmd = (filename.indexOf(F(".spiffs.bin")) > -1 ) ? U_SPIFFS : U_FLASH;
    Update.onProgress(onOTAProgress);
    if (!Update.begin(content_len, cmd)) {
       onOTAEnd(false);
       return;
    }
  }

  if (Update.write(data, len) != len) {
   // Update.printError(DEBUG_ESP_PORT);
    onOTAEnd(false);
  }

  delay(50);  // esp32 wifi crashes without this tempo

  if (final) {
    bool ok=Update.end(true);
    onOTAEnd(ok);
  }
}

// Monitor free stack (display smallest value)
void stack_mon(UBaseType_t &hwm)
{
  char key[30];
  UBaseType_t temp = uxTaskGetStackHighWaterMark(nullptr);
  if(!hwm || temp < hwm)
  {
    hwm = temp;
//    snprintf(local_sbuf,sizeof(local_sbuf),"[stack_mon] %s: %d bytes\n",pcTaskGetTaskName(NULL), hwm);
//    Local_Logs_Dispatch(local_sbuf);
    sprintf(key, "StackHWM %s", pcTaskGetTaskName(NULL));
    SVSettings[key] = hwm;
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
///////////////// Update SUPERVISOR HTTP ///////////////////
////////////////////////////////////////////////////////////

void TaskUpdateSuperVisor(void)
{
  HTTPClient http;
  http.setReuse(false);
  char url[_LURL_];
  const char* host=SVSettings["Update Host"];
  const char* path=SVSettings["SuperVisor Path"];
  sprintf(url, "http://%s%s",host,path);
  snprintf(local_sbuf,sizeof(local_sbuf),"Requesting URL: %s",url);
  Local_Logs_Dispatch(local_sbuf);
  // begin http client
  if (!http.begin(url)){
    http.end();
    Local_Logs_Dispatch("Connection failed");
    return;
  }

  // This will send the (get) request to the server
  int code          = http.GET();
  contentLength     = http.getSize();
  
  if (code != 200) { // 200=firmware found    Local_Logs_Dispatch("File received. Update PoolMaster...");
    Local_Logs_Dispatch("File not found !!");
    snprintf(local_sbuf,sizeof(local_sbuf),"HTTP error: %d",http.errorToString(code).c_str());
    Local_Logs_Dispatch(local_sbuf);
    http.end();
    return;
  }

  UpdateinProgress=1;
  Local_Logs_Dispatch("File received. Update PoolMaster...");
  snprintf(local_sbuf,sizeof(local_sbuf),"Start upload. File size is: %d bytes",contentLength);
  Local_Logs_Dispatch(local_sbuf);
  strcpy(barBuf, local_sbuf);

/*
    Update.writeStream(*http.getStreamPtr());  --> cause wifi crash
    bool ok=Update.end(true);
    onOTAEnd(ok);
*/

  Stream& stream = *http.getStreamPtr();
  uint32_t _undownloadByte = contentLength;
  uint8_t payload[512] = { 0 };  // Buffer for flash data chunks
  int c;
  Update.begin(contentLength);
  Update.onProgress(onOTAProgress);
  while (_undownloadByte > 0) {
    contentLength = stream.available();
    if(contentLength) {
			int c = stream.readBytes(payload, ((contentLength > sizeof(payload)) ? sizeof(payload) : contentLength));
      Update.write(payload, c);
      _undownloadByte -= c;
      delay(10);
    }
  }
  
  bool ok=Update.end(); //If all bytes are written this call will write the config to eboot
  onOTAEnd(ok);
  http.end();
  Local_Logs_Dispatch("Closing connection");
  UpdateinProgress=0;
  refreshTFT=true;
}

///////////////// Update POOLMASTER ///////////////////
///////////////////////////////////////////////////////
void TaskUpdatePoolMaster(void)
{
  static UBaseType_t hwm=0;     // free stack size
  //rtc_wdt_protect_off();
  //rtc_wdt_disable();
  HTTPClient http;
  http.setReuse(false);
  char url[_LURL_];
  const char* host=SVSettings["Update Host"];
  const char* path=SVSettings["Poolmaster Path"];
  sprintf(url, "http://%s%s",host,path);
  snprintf(local_sbuf,sizeof(local_sbuf),"Requesting URL: %s",url);
  Local_Logs_Dispatch(local_sbuf);
  // begin http client
  if(!http.begin(url)){
      Local_Logs_Dispatch("Connection failed");
    return;
  }

  // This will send the (get) request to the server
  int code          = http.GET();
  contentLength     = http.getSize();
        
  if(code == 200){
    UpdateinProgress=1;
    Local_Logs_Dispatch("File received. Update PoolMaster...");
    bool result;
    snprintf(local_sbuf,sizeof(local_sbuf),"Start upload. File size is: %d bytes",contentLength);
    strcpy(barBuf, local_sbuf);
    Local_Logs_Dispatch(local_sbuf);
    // Initialize ESP32Flasher
    ESP32Flasher espflasher;
    // set callback to print progress
    UpdateCounter=0;
    espflasher.setUpdateProgressCallback([](){
      UpdateCounter++;
      UpdateinProgress = (float(UpdateCounter*1024)/contentLength)*100.0;
      snprintf(local_sbuf,sizeof(local_sbuf),"PoolMaster update %d%%",UpdateinProgress);
      strcpy(barBuf, local_sbuf);
      Local_Logs_Dispatch(local_sbuf,1,"\r");
    });
    espflasher.espFlasherInit();//sets up Serial communication to another esp32
    int connect_status = espflasher.espConnect();
    if (connect_status != SUCCESS) 
      Local_Logs_Dispatch("Cannot connect to target");
    else {
      Local_Logs_Dispatch("Connected to target");
      espflasher.espFlashBinStream(*http.getStreamPtr(),contentLength);
    }
  }
  else {
    snprintf(local_sbuf,sizeof(local_sbuf),"HTTP error: %d",http.errorToString(code).c_str());
    Local_Logs_Dispatch(local_sbuf);
  }
  http.end();
  Local_Logs_Dispatch("Closing connection");
  UpdateinProgress=0;
  refreshTFT=true;
  stack_mon(hwm);
}

void TaskUpdateNextion(void)
{
  Local_Logs_Dispatch("Nextion Update Requested");
  Local_Logs_Dispatch("Stopping PoolMaster...");
  pinMode(ENPin, OUTPUT);
  digitalWrite(ENPin, LOW);
  Local_Logs_Dispatch("Upgrading Nextion ...");

  HTTPClient http;
  http.setReuse(false);
  char url[_LURL_];
  const char* host=SVSettings["Update Host"];
  const char* path=SVSettings["Nextion Path"];
  sprintf(url, "http://%s%s",host,path); 
  snprintf(local_sbuf,sizeof(local_sbuf),"Requesting URL: %s",url);
  Local_Logs_Dispatch(local_sbuf);
 
  if(!http.begin(url)){
    Local_Logs_Dispatch("Connection failed");
    return;
    }

  // This will send the (get) request to the server
  int code          = http.GET();
  contentLength     = http.getSize();
        
  // Update the nextion display
  if(code == 200){
    UpdateinProgress=1;
    Local_Logs_Dispatch("File received. Update Nextion...");
    bool result;
    // initialize ESPNexUpload
    ESPNexUpload nextion(115200);
    // set callback to print progress
    UpdateCounter=0;
    nextion.setUpdateProgressCallback([](){
      UpdateCounter++;
      UpdateinProgress = (float(UpdateCounter*2048)/contentLength)*100.0;
      snprintf(local_sbuf,sizeof(local_sbuf),"Nextion update %d%%",UpdateinProgress);
      strcpy(barBuf, local_sbuf);
      Local_Logs_Dispatch(local_sbuf,1,"\r");
    });
    // prepare upload: setup serial connection, send update command and send the expected update size
    result = nextion.prepareUpload(contentLength);
    if(!result){
        snprintf(local_sbuf,sizeof(local_sbuf),"Error: %s",nextion.statusMessage.c_str());
        Local_Logs_Dispatch(local_sbuf);
        //Serial.println("Error: " + nextion.statusMessage);
    } 
    else {
      snprintf(local_sbuf,sizeof(local_sbuf),"Start upload. File size is: %d bytes",contentLength);
      strcpy(barBuf, local_sbuf);
      Local_Logs_Dispatch(local_sbuf);
      // Upload the received byte Stream to the nextion
      result = nextion.upload(*http.getStreamPtr());
      if(result)
        Local_Logs_Dispatch("Successfully updated Nextion");
      else {
        snprintf(local_sbuf,sizeof(local_sbuf),"Error updating Nextion: %s",nextion.statusMessage.c_str());
        Local_Logs_Dispatch(local_sbuf);
      }

      // end: wait(delay) for the nextion to finish the update process, send nextion reset command and end the serial connection to the nextion
      nextion.end();
      pinMode(NEXT_RX,INPUT);
      pinMode(NEXT_TX,INPUT);

      UpdateinProgress=0;
      refreshTFT=true;
    }
  }
  else {
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

void TelnetToTaskUpdatePoolMaster()
{
  WiFiClient telnet;
  if (!telnet.connect(WiFi.localIP(), 23)) return;
  telnet.write("S\r\n");
  delay(3000); // must add a delay, otherwise telnet closes too early
  // telnet.end();
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
      if(digitalRead(ENPin)==LOW) {
        pinMode(ENPin, OUTPUT);
        digitalWrite(ENPin, HIGH);
        pinMode(ENPin, INPUT);
      }
    break;
    case 'S':  // PoolMaster Update
      //mustUpdatePoolMaster = true;
      TaskUpdatePoolMaster();
    break;
    case 'T':  // Nextion Update
      mustUpdateNextion = true;
    break;
    case 'V':  // Reboot Nextion
      mustRebootNextion = true;
    break;
    case 'W':  // WatchDog Update
      mustUpdateSuperVisor = true;
    break;

    case 'H':  // Help
      Local_Logs_Dispatch("***********************");  
      Local_Logs_Dispatch("Help Message:");
      Local_Logs_Dispatch("R: Reboot Supervisor");
      Local_Logs_Dispatch("W: Update Supervisor");
      Local_Logs_Dispatch("P: Stop PoolMaster");
      Local_Logs_Dispatch("Q: Start PoolMaster");
      Local_Logs_Dispatch("S: Update PoolMaster");
      Local_Logs_Dispatch("T: Update Nextion");
      Local_Logs_Dispatch("V: Reboot Nextion");
      Local_Logs_Dispatch("***********************");  
    break;
  }
}

void upcurrenttime()
{
    uptime::calculateUptime();
    sprintf(currentUptime, "%dd-%02dh-%02dm-%02ds", uptime::getDays(), uptime::getHours(), uptime::getMinutes(), uptime::getSeconds());
}


// MQTT Engine
// ***********

void connectToMqtt()
{
  if (MqttClient.connected()) return;
  const char* mqtt_server=SVSettings["MQTT Server"];
  if (strcmp(mqtt_server, "")==0) return;

  MqttClient.connect();

  // wait max 5 sec 
  int i=500;
  while (--i>0) {
    if (MqttClient.connected()) return;
    Local_Logs_Dispatch("Connecting to MQTT...\n");
    delay(10);
  }
  Local_Logs_Dispatch("Connecting to MQTT: timeout\n");
}

//void onMqttSubscribe(uint16_t packetId, uint8_t qos) {}
//void onMqttUnSubscribe(uint16_t packetId) {}
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  char Command[250] = "";
  const char *key, *setting;
  JsonDocument JSONSVCommand;

  uint8_t i;
  for (i=0 ; i<len ; i++) Command[i] = payload[i];
  Command[i] = 0;

  snprintf(local_sbuf,sizeof(local_sbuf),"onMqttMessage: %s",Command);
  Local_Logs_Dispatch(local_sbuf);

  deserializeJson(JSONSVCommand,Command);
  preferences.begin("PMSV", false);
  JsonObject root = JSONSVCommand.as<JsonObject>();
  for (JsonPair kv : root) {
    key     = kv.key().c_str();
    setting = kv.value().as<const char*>();
    if (key && setting) {
      if (strcmp(key, "Update SuperVisor")==0)
        mustUpdateSuperVisor = true;
      else if (strcmp(key, "Update PoolMaster")==0)
        mustUpdatePoolMaster = true;
      else if (strcmp(key, "Update Nextion")==0)
        mustUpdateNextion = true;
      else if (strcmp(key, "Reboot SuperVisor")==0)
        mustRebootSuperVisor = true;
      else if (strcmp(key, "Reboot PoolMaster")==0)
        mustRebootPoolMaster = true;
      else if (strcmp(key, "Reboot Nextion")==0)
        mustRebootNextion = true;
      else if (strcmp(key, "Create Home Assistant")==0)
        mustCreateHAEntities = true;
      else if (strcmp(key, "Clean Home Assistant")==0)
        mustCleanHAEntities = true;
      else {
        SVSettings[key] = String(setting);
        preferences.putString(key, setting);
        if (strcmp(key, "WaterMeter L") == 0) mustUpdateWMCounter = true;
      }
    }
  }
  preferences.end();
}

//void onMqttPublish(uint16_t packetId) {}
void onMqttConnect(bool sessionPresent)
{
 // Subscribe MQTT SuperVisor API
  char topic[_LTOPIC_];
  const char* roottopic = SVSettings["MQTT Topic"];
  if (!roottopic) return;
  if (strcmp(roottopic, "")==0) return;
  sprintf(topic, "%s/SVAPI", roottopic);
  MqttClient.subscribe(topic,2);
}

void WebHandleSVSettings(AsyncWebServerRequest *request);
void mqttPublish()
{ 
  // no activities when doing intense actions
  if (UpdateinProgress) return;
  if (mustCreateHAEntities) return;
  if (mustCleanHAEntities) return;

  char Payload[2048];
  char topic[_LTOPIC_];
  const char* roottopic = SVSettings["MQTT Topic"];
  if (!roottopic) return;
  if (roottopic[0] == '/') roottopic++;
  if (strcmp(roottopic, "")==0) return;

  sprintf(topic, "%s/PoolMaster", roottopic);

  connectToMqtt();
  if (!MqttClient.connected()) return;

 /* if (UpdateinProgress > 1) {
    JsonDocument cleanJson;
    cleanJson["Update in progress"] = barBuf;
    sprintf(topic, "%s/SuperVisor", roottopic);
    size_t n = serializeJson(cleanJson, Payload);
    if (MqttClient.publish(topic, 1, true, Payload, n) == 0) {
      snprintf(local_sbuf,sizeof(local_sbuf),"Supervisor, unable to publish MQTT %s Payload: %s - Payload size: %d", topic, Payload, sizeof(Payload));
      Local_Logs_Dispatch(local_sbuf);
    }
    return;
  }*/

  WebHandleSVSettings(NULL);  // refresh SVSettings
  {
    // remove unnecessary data
  JsonDocument cleanJson = PMInfo;
  cleanJson.remove("MQTT Server");
  cleanJson.remove("MQTT Port");
  cleanJson.remove("MQTT Topic");
  cleanJson.remove("MQTT Username");
  cleanJson.remove("MQTT Password");
  cleanJson.remove("HOME Assistant");
  cleanJson.remove("Update Host");
  cleanJson.remove("Poolmaster Path");
  cleanJson.remove("Nextion Path");
  cleanJson.remove("SuperVisor Path");
  cleanJson.remove("Papertrail Host");
  cleanJson.remove("Papertrail Port");
  cleanJson.remove("Presence Detected");

  size_t n = serializeJson(cleanJson, Payload);
  if (MqttClient.publish(topic, 1, true, Payload, n) == 0) {
      snprintf(local_sbuf,sizeof(local_sbuf),"Supervisor, unable to publish MQTT %s Payload: %s - Payload size: %d", topic, Payload, sizeof(Payload));
      Local_Logs_Dispatch(local_sbuf);
  }
  }

  sprintf(topic, "%s/SuperVisor", roottopic);
  connectToMqtt();
  if (!MqttClient.connected()) return;

  {
    // remove unnecessary data
  JsonDocument cleanJson = SVSettings;
  cleanJson.remove("MQTT Server");
  cleanJson.remove("MQTT Port");
  cleanJson.remove("MQTT Topic");
  cleanJson.remove("MQTT Username");
  cleanJson.remove("MQTT Password");
  cleanJson.remove("HOME Assistant");
  cleanJson.remove("Display Firmware");
  cleanJson.remove("MQTT Connected");
  if ((cleanJson["Presence Detected"] != "*black") && (cleanJson["Presence Detected"] != "none"))
        cleanJson["Presence Detected"] = "Yes";
  else  cleanJson["Presence Detected"] = "No";

  size_t n = serializeJson(cleanJson, Payload);
  if (MqttClient.publish(topic, 1, true, Payload, n) == 0) {
      snprintf(local_sbuf,sizeof(local_sbuf),"Supervisor, unable to publish MQTT %s Payload: %s - Payload size: %d", topic, Payload, sizeof(Payload));
      Local_Logs_Dispatch(local_sbuf);
  }
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) 
{
    if (WiFi.isConnected()) xTimerStart(MqttReconnectTimer, 0);
}

void MqttInit()
{   
  if (!MqttReconnectTimer) {
    MqttReconnectTimer = xTimerCreate("mqttTimer",
    pdMS_TO_TICKS(5000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
 
    MqttClient.onConnect(onMqttConnect);
    MqttClient.onDisconnect(onMqttDisconnect);
    //MqttClient.onSubscribe(onMqttSubscribe);
    //MqttClient.onUnsubscribe(onMqttUnSubscribe);
    MqttClient.onMessage(onMqttMessage);
    // MqttClient.onPublish(onMqttPublish);
  }
  else MqttClient.disconnect();

  const char* mqtt_server=SVSettings["MQTT Server"];
  int mqtt_port=atol(SVSettings["MQTT Port"]);
  const char* mqtt_username=SVSettings["MQTT Username"];
  const char* mqtt_password=SVSettings["MQTT Password"];

  if (strcmp(mqtt_server, "")==0) return;

  MqttClient.setServer(mqtt_server, mqtt_port);
  if (*mqtt_username != 0)
    MqttClient.setCredentials(mqtt_username, mqtt_password);

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

String GetAndDeleteValue(const char* key)
{
  String s = SVSettings[key];
  if (!s || (s=="") || (s=="null"))
    s = "none";
  SVSettings[key] = "none";
  return s;
}

String PMRequest(char *question)
{
  if (UpdateinProgress) return String("none");
  if (strcmp(question, "GET_WIFI_SSID") == 0)  return wifiManager.getWiFiSSID();
  if (strcmp(question, "GET_WIFI_PASS") == 0)  return wifiManager.getWiFiPass();
  if (strcmp(question, "GET_HOSTNAME") == 0)   return String(hostname);

  //  PM will update its MQTT
  if (strcmp(question, "GET_MQTT_SERVER") == 0)   return SVSettings["MQTT Server"];
  if (strcmp(question, "GET_MQTT_PORT") == 0)     return SVSettings["MQTT Port"];
  if (strcmp(question, "GET_MQTT_TOPIC") == 0)    return SVSettings["MQTT Topic"];
  if (strcmp(question, "GET_MQTT_USERNAME") == 0) return SVSettings["MQTT Username"];
  if (strcmp(question, "GET_MQTT_PASSWORD") == 0) return SVSettings["MQTT Password"];

  if (strcmp(question, "GET_WATERMETER_COUNTER") == 0)
     if (mustUpdateWMCounter) {
        mustUpdateWMCounter = false;
        return SVSettings["WaterMeter L"];
     }
     else return String("none");
  if (strcmp(question, "GET_WATERMETER_K") == 0)        return SVSettings["WaterMeter K"];
  if (strcmp(question, "GET_WATERMETER_D") == 0)        return SVSettings["WaterMeter D"];
  if (strcmp(question, "GET_WATERMETER_GPIO") == 0)     return SVSettings["WaterMeter GPIO"];

  if (strcmp(question, "GET_TFA_VENICE") == 0)          return SVSettings["TFA_Venice"];
  if (strcmp(question, "GET_COMMAND") == 0)             return GetAndDeleteValue("Command");
  return String("none");
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
  // consume too much I2C bandwidth, use Serial instead.
  //if (strncmp(question, "PM_", 3) == 0) {
  //  PMReport(question+3);
  //}
  // PoolMaster wants info from SuperVisor
  if (strncmp(question, "GET_", 4) == 0) {
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

// HTML pages ***
// **************
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>PoolMaster SuperVision</title>
  </head>
<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
  }
body{
  background: linear-gradient(AliceBlue, LightCyan, MediumBlue);
  background-size: cover;
  background-attachment: fixed;
  }
.navbar{
  height: 50px;
  display: flex; 
  align-items: center;
  width: 100%;
  padding: 14px 14px;
  background-color: #1b4cd3;
  position: fixed;
  z-index: 1000;
  }
.navbar .nav-header{
  display: inline;
  }
.navbar .nav-header .nav-logo{
  display: inline-block;
  margin-top: 5px;
  }
.navbar .nav-title{
    display: none;
  }
.navbar .nav-links{
  display: inline;
  float: left;
  font-size: 18px;
  }
.navbar .nav-links a{
  padding: 10px 12px;
  text-decoration: none;
  font-weight: 550;
  color: white;
  }
.navbar .nav-links button{
  padding: 10px 12px;
  text-decoration: none;
  font-weight: 550;
  cursor: pointer;
  color: white;
  border: none;
  outline: none;
  background-color: #1b4cd3;
  }
.navbar .nav-links a:hover{
  background-color: rgba(0,0,0,0.3);
  }
  .navbar .nav-links button:hover{
  background-color: rgba(0,0,0,0.3);
  }
.navbar #nav-check, .navbar .nav-btn{
  display: none;
  }
@media (max-width: 600px){
  .navbar .nav-btn{
    display: inline-block;
    position: absolute;
    top: 0px;
    right: -20px;
    }
  .navbar #idnav-title, .navbar .nav-title{
    margin-top: 3px;
    color: white;
    display: block;
    user-select: none; 
    font-size: large;
    padding: 15px;
  }
  .navbar .nav-btn label{
    display: inline-block;
    width: 80px;
    height: 50px;
    padding: 15px;
    }
  .navbar .nav-btn label span{
    display: block;
    height: 10px;
    width: 25px;
    border-top: 3px solid #eee;
    }
  .navbar .nav-btn label:hover, .navbar #nav-check:checked ~ .nav-btn label{
    background-color: rgb(10, 15, 60);
    }
  .navbar .nav-links{
    position: absolute;
    display: block;
    text-align: 50%;
    background-color: rgb(10, 15, 60);
    transition: all 0.3s ease-in;
    overflow-y: hidden;
    top: 50px;
    right: 0px;
    }
  .navbar .nav-links a{
    display: block;
    }
  .navbar .nav-links button{
    display: block;
    background-color: rgb(10, 15, 60);
    border: none;
    outline: none;
    }
  .navbar #nav-check:not(:checked) ~ .nav-links{
    height: 0px;
    }
  .navbar #nav-check:checked ~ .nav-links{
    height: calc(100vh - 50px);
    overflow-y: auto;
    }
  }
.tabcontent {
  color: black;
  display: none;
  padding: 60px 7px 10px;
  height: 100%;
}
.textblocklogs {
  background-color: white;
  font-size: small;
  }
.textblockabout {
  background-color: white;
  font-size: small;
  }
.infotable {
  background-color: white;
  padding: 0px 7px 0px 7px;
  font-size: small;
  }
.infotable th:nth-child(1) {
  text-align: left;
  width: 130px;
}
.infotable th:nth-child(2) {
  text-align: left;
  width: 60px;
}
.infotable th:nth-child(3) {
  text-align: left;
  white-space: nowrap;
  text-overflow:ellipsis;
  overflow: hidden;
}
.normalButton {
  background-color: Silver;
  display: inline-block; font-weight: bold; border: 1px solid #2d6898;
  color: white;
  padding: 10px 15px;
  border-radius: 4px;
  cursor: pointer;
  margin-right: 10px;
  height:40px;
  width:300px;
}
.blueButton {
  background-color: #00008B;
  display: inline-block; font-weight: bold; border: 1px solid #2d6898;
  color: white;
  padding: 10px 15px;
  border-radius: 4px;
  cursor: pointer;
  margin-right: 10px;
  height:40px;
  width:300px;
}
.redButton {
  background-color: Red;
  display: inline-block; font-weight: bold; border: 1px solid #2d6898;
  color: white;
  padding: 10px 15px;
  border-radius: 4px;
  cursor: pointer;
  margin-right: 10px;
  height:40px;
  width:300px;
}
.greenButton {
  background-color: Green;
  display: inline-block; font-weight: bold; border: 1px solid #2d6898;
  color: white;
  padding: 10px 15px;
  border-radius: 4px;
  cursor: pointer;
  margin-right: 10px;
  height:40px;
  width:300px;
}
.blackButton {
  background-color: Black;
  display: inline-block; font-weight: bold; border: 1px solid #2d6898;
  color: white;
  padding: 10px 15px;
  border-radius: 4px;
  cursor: pointer;
  margin-right: 10px;
  height:40px;
  width:300px;
}
.shareButton {
  background-color: SkyBlue;
  display: inline-block; font-weight: bold; border: 1px solid #2d6898;
  color: white;
  padding: 10px 15px;
  border-radius: 4px;
  cursor: pointer;
  margin-right: 10px;
  height:40px;
  width:300px;
}
.formupdates form  { display: table;      }
.formupdates p     { display: table-row;  }
.formupdates label { display: table-cell; }
.formupdates input { display: table-cell; width: 300px;}
.marge5 { margin-left: 2em; }
#myProgress {
  display: none
  width:300px;
  background-color: #A9A9A9;
  border-radius: 15px;
}
#myBar {
  display: none
  width: 0;
  height: 20px;
  background-color: blue;
  text-align: left;
  line-height: 20px;
  color: white;
  border-radius: 15px;
  white-space: nowrap;
}
[class*="led_"] {
  width: 30px;
  height: 8px;
  border: 1px solid black;
  border-radius: 90px;
}
.led_green {
  background-color: #2AFF00;
}
.led_blue {
  background-color: DeepSkyBlue;
}
.led_yellow {
  background-color: yellow;
}
.led_red {
  background-color: red;
}
.led_cyan {
  background-color: #15F4EE;
}
.led_magenta {
  background-color: #FF00FF;
}
.led_orange {
  background-color: #FF7F00;
}
.actionRedButton {
  background-color: Red;
  display: inline-block; font-weight: bold; border: 1px solid #2d6898;
  color: white;
  border-radius: 4px;
  cursor: pointer;
  height:30px;
  width:86px;
}
.actionGreenButton {
  background-color: Green;
  display: inline-block; font-weight: bold; border: 1px solid #2d6898;
  color: white;
  border-radius: 4px;
  cursor: pointer;
  height:30px;
  width:86px;
}
.actiontable th:nth-child(1) {
  text-align: left;
  width: 120px;
}
.actiontable th:nth-child(2) {
  text-align: center;
  width:  100px;
}
.actiontable th:nth-child(3) {
  text-align: center;
  white-space: nowrap;
  text-overflow:ellipsis;
  overflow: hidden;
}
</style>

<div class="navbar">
  <div class="nav-header">
    <div class="nav-logo"> 
      <a href="#">
         <img alt="logo" src="data:image/jpeg;base64,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" />
      </a>
    </div>
  </div>
  <div class="nav-title" id="idnav-title">PoolMaster Supervision</div>
  <input type="checkbox" id="nav-check">
  <div class="nav-btn">
    <label for="nav-check">
      <span></span>
      <span></span>
      <span></span>
    </label>
  </div>
  <div class="nav-links">
    <button class="tablink" onclick="openPage('Poolmaster', this)" id="defaultOpen">PoolMaster</button>
    <button class="tablink" onclick="openPage('Supervisor', this)">SuperVisor</button>
    <button class="tablink" onclick="openPage('Commands', this)">Commands</button>
    <button class="tablink" onclick="openPage('Settings', this)">Settings</button>
    <button class="tablink" onclick="openPage('Updates', this)">Updates</button>
    <button class="tablink" onclick="openPage('Logs', this)">Logs</button>
    <button class="tablink" onclick="openPage('About', this)">About</button>
  </div>
  <script>
function openPage(pageName,elmnt) {
  var i, tabcontent, tablinks;
  document.getElementById("nav-check").checked = false;
  tabcontent = document.getElementsByClassName("tabcontent");
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
  }
  tablinks = document.getElementsByClassName("tablink");
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].style.backgroundColor = "";
  }
  document.getElementById(pageName).style.display = "block";
  loadsettingsvalues();
}
function startTab() {
  document.getElementById("defaultOpen").click();
}
let mqtt_server = "";
let mqtt_port = "";
let mqtt_topic = "";
let mqtt_username = "@@@@@@@@@@@@@@@";
let mqtt_password = "@@@@@@@@@@@@@@@";
let papertrail_host = "";
let papertrail_port = "";
let update_host = "";
let poolmasterpath = "";
let nextionpath = "";
let supervisorpath = "";
let svversion = "";
let pmversion = "";
let watermeterL= "";
let watermeterK= "1";
let watermeterD= "250";
let watermeterGPIO= "";
let tfavenice = "0";
let command = "";
let homeassistanttopic = "";
let homeassistantcreate = "off";
</script>
</div>

<body>
  <script>
    function loadsettingsvalues() {
      document.getElementById("MQTT Server").value        = mqtt_server;
      document.getElementById("MQTT Port").value          = mqtt_port;
      document.getElementById("MQTT Topic").value         = mqtt_topic;
      document.getElementById("MQTT Username").value      = mqtt_username;
      document.getElementById("MQTT Password").value      = mqtt_password;
      document.getElementById("HOME Assistant").value     = homeassistanttopic;
      document.getElementById("Papertrail Host").value    = papertrail_host;
      document.getElementById("Papertrail Port").value    = papertrail_port;
      document.getElementById("WaterMeter L").value       = watermeterL;
      document.getElementById("WaterMeter K").value       = watermeterK;
      document.getElementById("WaterMeter D").value       = watermeterD;
      document.getElementById("WaterMeter GPIO").value    = watermeterGPIO;
      document.getElementById("Update Host").value        = update_host;
      document.getElementById("Poolmaster Path").value    = poolmasterpath;
      document.getElementById("Nextion Path").value       = nextionpath;
      document.getElementById("SuperVisor Path").value    = supervisorpath;
      document.getElementById("textsvversion").innerText  = "PoolMaster Supervision version: "+pmversion+" / "+svversion;
      document.getElementById("TFA_Venice").value         = tfavenice;
    }
    function generateTable(source, target, title) {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          let text = this.responseText;
          if (text == "") return;
          data = JSON.parse(text);
          let html = "<p><table border='0'>";
          html += "<tr><th>" + title + "</th></tr>";
          for (var key in data) {
            let value = key.substring(0, 5) + "";
            if (value == "HEAP ") { continue; }
            if (value == "Stack") { continue; }
            value = data[key] + "";
            if (value == "none") { continue; }
            html += "<tr><td>" + key + ":</td>";
            if (value.charAt(0) == '*') {
              value = value.substring(1);
              html += "<td class='led_" + value + "'></td><td></td></tr>";
            }
            else html += "<td colspan='2'>" + value + "</td></tr>";
          }
          html += "</table></p>";
          document.getElementById(target).innerHTML = html;
          if (title == "SuperVisor:") {
            mqtt_server         = data["MQTT Server"];
            mqtt_port           = data["MQTT Port"];
            mqtt_topic          = data["MQTT Topic"];
            homeassistanttopic  = data["HOME Assistant"];
            papertrail_host     = data["Papertrail Host"];
            papertrail_port     = data["Papertrail Port"];
            update_host         = data["Update Host"];
            poolmasterpath      = data["Poolmaster Path"];
            nextionpath         = data["Nextion Path"];
            supervisorpath      = data["SuperVisor Path"];
            svversion           = data["Firmware"];
            tfavenice           = data["TFA_Venice"];
            watermeterL         = data["WaterMeter L"];
            watermeterK         = data["WaterMeter K"];
            watermeterD         = data["WaterMeter D"];
            watermeterGPIO      = data["WaterMeter GPIO"];
          }
          if (title == "PoolMaster:") {
            pmversion = data["Firmware"];
          }
        }
      }
      xhttp.open("POST", source, true);
      xhttp.send();
    }
    function setUpdateBar() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          data = JSON.parse(this.responseText);
          var elem = document.getElementById('myBar');
          var progress = document.getElementById('myProgress');
          var pc=data['pc'];
          if (pc == 0) {
            elem.style.display = "none";
            progress.style.display = "none";
          } else {
            progress.style.display = "block";
            elem.style.display = "block";
            elem.style.width = pc +"%";
            elem.innerHTML = data['text'];
          }
        }
      }
      xhttp.open("GET", "/getprogressbar", true);
      xhttp.send();
    }
    setInterval(function() {
      generateTable("/getpminfo","pm-table-container","PoolMaster:");
      generateTable("/getsvinfo","sv-table-container","SuperVisor:");
      setUpdateBar();
    }, 500);
  </script>

<!-- INFO -->
<div id="Poolmaster" class="tabcontent">
  <div id="myProgress">
    <div id="myBar"></div>
  </div>
  <div class="infotable" id="pm-table-container"></div>
</div>
<div id="Supervisor" class="tabcontent">
  <div class="infotable" id="sv-table-container"></div>
</div>

<!-- SETTINGS -->
<div id="Settings" class="tabcontent">
  <form id="svsettings" action='/set' method='get'>
    <br>
    <p><table border='0'>
    <tr><th align="left">MQTT:</th></tr>
    <tr>
      <td>Server:</td>
      <td><input type="text" name="MQTT Server" id="MQTT Server" size=22></td>
    </tr>
    <tr>
      <td>Port:</td>
      <td><input type="text" name="MQTT Port" id="MQTT Port" size=22></td>
    </tr>
    <tr>
      <td>Topic:</td>
      <td><input type="text" name="MQTT Topic" id="MQTT Topic" size=22></td>
    </tr>
    <tr>
      <td>Username:</td>
      <td><input type="password" name="MQTT Username" id="MQTT Username" size=22></td>
    </tr>
    <tr>
      <td>Password:</td>
      <td><input type="password" name="MQTT Password" id="MQTT Password" size=22></td>
    </tr>
    <tr>
      <td>Home Assistant <br> Discovery Topic <br> (homeassistant):</td>
      <td><input type="text" name="HOME Assistant" id="HOME Assistant" size=22></td>
    </tr>
    <tr><td>&nbsp</td></tr>
    <tr><th align="left">PaperTrail:</th></tr>
    <tr>
      <td>Server:</td>
      <td><input type="text" name="Papertrail Host" id="Papertrail Host"  size=22></td>
    </tr>
    <tr>
      <td>Port:</td>
      <td><input type="text" name="Papertrail Port" id="Papertrail Port" size=22></td>
    </tr>
    <tr><td>&nbsp</td></tr>
    <tr><th align="left">WaterMeter:</th></tr>
    <tr>
      <td>GPIO (0=off, 35):</td>
      <td><input type="text" name="WaterMeter GPIO" id="WaterMeter GPIO" size=22></td>
    </tr>
    <tr>
      <td>Counter Liter (L):</td>
      <td><input type="text" name="WaterMeter L" id="WaterMeter L" size=22></td>
    </tr>
    <tr>
      <td>Coeff (K=L/pulse):</td>
      <td><input type="text" name="WaterMeter K" id="WaterMeter K" size=22></td>
    </tr>
    <tr>
      <td>Debounce (D=ms):</td>
      <td><input type="text" name="WaterMeter D" id="WaterMeter D" size=22></td>
    </tr>
    <tr><td>&nbsp</td></tr>
    <tr><th align="left">TFA Venice:</th></tr>
    <tr>
      <td>GPIO (0=off, 14):</td>
      <td><input type="text" name="TFA_Venice" id="TFA_Venice" size=22></td>
    </tr>
    </table></p>
    <br><br>
    <button type="submit" class="blueButton">Save</button>
    <br>
  </form>
</div>

<!-- UPDATES -->
<div id="Updates" class="tabcontent">
  <form id="Updates" action='/set' method='get'>
  <br>
  <p><table border='0'>
    <tr><th colspan="2" align="left">Update Host (http://):</th></tr>
    <tr>
      <td>Server:</td>
      <td><input type="text" name="Update Host" id="Update Host" size=26></td>
    </tr>
    <tr><td>&nbsp</td></tr>
    <tr><th colspan="2" align="left">Firmware Path:</th></tr>
    <tr>
      <td>PoolMaster:</td>
      <td><input type="text" name="Poolmaster Path" id="Poolmaster Path" size=26></td>
    </tr>
    <tr>
      <td>Display:</td>
      <td><input type="text" name="Nextion Path" id="Nextion Path" size=26></td>
    </tr>
    <tr>
      <td>SuperVisor:</td>
      <td><input type="text" name="SuperVisor Path" id="SuperVisor Path" size=26></td>
    </tr>
    </table></p>
    <br><br>
    <button type="submit" class="blueButton">Save</button>
    <br>
  </form>
  <br><br>
  <form action="/set">
    <input type="hidden" id="UpdatePoolMaster" name="UpdatePoolMaster">
    <input type="submit" class="redButton" value="Update PoolMaster (http)">
  </form>
  <br>
  <form action="/set">
    <input type="hidden" id="UpdateNextion" name="UpdateNextion">
    <input type="submit" class="redButton" value="Update Display Nextion (http)">
  </form>
  <br>
  <form action="/set">
    <input type="hidden" id="UpdateSuperVisor" name="UpdateSuperVisor">
    <input type="submit" class="redButton" value="Update SuperVisor (http)">
  </form>
  <br>
    <button class='redButton' onclick="document.getElementById('getfwFile').click()">Update SuperVisor (file)</button>
    <input type='file' accept='.bin' id='getfwFile' style='display:none'>
  <br>
  <script>
    let fileselect = document.getElementById('getfwFile');
    fileselect.onchange = function() {
      let filename = this.files[0];
      if (filename == "") return;
      const formData = new FormData();
      formData.append('fileAjax', filename, filename.name);
      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/doUpdate', true);
      xhr.send(formData);
      startTab();
    };
  </script>
  <br>
</div>

<!-- Commands -->
<div id="Commands" class="tabcontent">
  <br>
  <form action="/set">
    <input type="hidden" id="rebootPoolMaster" name="rebootPoolMaster">
    <input type="submit" class="blackButton" value="Restart PoolMaster">
  </form>
  <br>
  <form action="/set">
    <input type="hidden" id="rebootNextion" name="rebootNextion">
    <input type="submit" class="blackButton" value="Restart Display">
  </form>
  <br>
  <form action="/set">
    <input type="hidden" id="rebootSuperVisor" name="rebootSuperVisor">
    <input type="submit" class="blackButton" value="Restart SuperVisor">
  </form>
  <br><br>
  <div class="actiontable" id="action-buttons-container"></div>
  <br>
  <br>
  <script>
    function SendCmd(cmd) {
      var xhttp = new XMLHttpRequest();
      xhttp.open('GET', cmd, true);
      xhttp.send();
    }
    function AddRelayButtons(title, func, ON, OFF) {
      let json = "\/set\?Command={ \\\"" + func + "\\\":" + ON + " }";
      let h = "<tr><td>" + title + ":</td>";
          h += "<td><button class='actionGreenButton' onclick='SendCmd(\"" + json + "\")' >ON</button></td>";
          if (OFF != "") {
            json = "\/set\?Command={ \\\"" + func + "\\\":" + OFF + " }";
            h += "<td><button class='actionRedButton' onclick='SendCmd(\"" + json + "\")' >OFF</button></td>";
          }
          h += '</tr>';
      return h;
    }
    function JSONCommand() {
      let json = document.getElementById('Command').value;
      let cmd = "\/set\?Command=" + json;
      SendCmd(cmd);
    }
    function AddHTMLButtons() {
      let html = "<p><table border='0'>";
      html += "<tr><th></th></tr>";
      html += AddRelayButtons("Fill Pump",      "FillPump",     "1"    , "0");
      html += AddRelayButtons("Salt Water Gen", "Electrolyse",  "1"    , "0");
      html += AddRelayButtons("Filter Pump",    "FiltPump",     "1"    , "0");
      html += AddRelayButtons("Robot Pump",     "RobotPump",    "1"    , "0");
      html += AddRelayButtons("pH Pump",        "PhPump",       "1"    , "0");
      html += AddRelayButtons("Cl Pump",        "ChlPump",      "1"    , "0");
      html += AddRelayButtons("Light",          "Relay",        "[0,1]", "[0,0]");
      html += AddRelayButtons("Spare",          "Relay",        "[1,1]", "[1,0]");
      html += AddRelayButtons("pH PID",         "PhPID",        "1"    , "0");
      html += AddRelayButtons("Orp PID",        "OrpPID",       "1"    , "0");
      html += AddRelayButtons("Auto Mode",      "Mode",         "1"    , "0");
      html += AddRelayButtons("Winter Mode",    "Winter",       "1"    , "0");
      html += AddRelayButtons("Buzzer",         "Buzzer",       "1"    , "0");
      html += AddRelayButtons("Clear Errors",   "Clear",        "1"    , "" );
      html += "</table>";
      html += "<br>";
      html += "<label for='Command'>JSON Command:</label>";
      html += "<input type='text' name='Command' id='Command'>";
      html += "<br><br>";
      html += "<button onclick='JSONCommand()' class='blueButton'>Send JSON Command</button>";
      html += "</p>";
      document.getElementById('action-buttons-container').innerHTML = html;
    }
    AddHTMLButtons();
  </script>
</div>

<!-- LOGS -->
<div id="Logs" class="tabcontent">
  <script>
    var ScrollFlag = 0;
    function StopScroll()  { ScrollFlag = 0; }
    function StartScroll() { ScrollFlag = 1; }
    function ScrollClean() { 
      var container = document.getElementById('textblock-logs');
      container.innerText = "";
    }
    setInterval(function() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var container = document.getElementById('textblock-logs');
          var newElm = document.createElement('p');
          newElm.innerText = this.responseText.replace(/(\n\n|\r)/gm,"\n");
          container.appendChild(newElm);
          if (ScrollFlag == 1) {
            scrollingElement = (document.scrollingElement || document.body);
            scrollingElement.scrollTop = scrollingElement.scrollHeight;
           //container.scrollTop = scrollingElement.scrollHeight; 
          }}}
      xhttp.open("GET", "/getlogs", true);
      xhttp.send();
    }, 500);
  </script>
  <h3 id="Logs">Logs from PoolMaster (PM) and SuperVisor (SV)</h3>
  <br>
  <button onClick="StopScroll()">Stop Scrolling</button>
  <button onClick="StartScroll()">Start Scrolling</button>
  <button onClick="ScrollClean()">Clear</button>
  <hr /><div class="textblocklogs" id="textblock-logs"><p>Logs</p></div><hr />
  <button onClick="StopScroll()">Stop Scrolling</button>
  <button onClick="StartScroll()">Start Scrolling</button>
  <button onClick="ScrollClean()">Clear</button>
  <div><p></p></div>
</div>

<!-- ABOUT -->
<div id="About" class="tabcontent">
  <br>
  <div id='textsvversion'> </div>
  <br>
  <hr />
  <div class="textblockabout">
  THE SOFTWARE IS PROVIDED “AS IS”,
  <br>
  WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  <br> 
  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
  <br>
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
  <br>
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
  <br>
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
  <br>TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
  <br>
  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  </div><hr/>
  <br> 
  <hr />
  <div class="textblockabout">
  Credits:
  <br><a class="marge5" href="https://github.com/Loic74650/PoolMaster">Loic74650</a>
  <br><a class="marge5" href="https://github.com/Gixy31/ESP32-PoolMaster">Gixy31</a>
  <br><a class="marge5" href="https://github.com/christophebelmont">Christophe</a>
  <br><a class="marge5" href="https://github.com/stephdust">Stephane</a>
  </div><hr/>
  <p>
</div>

<script>
  startTab();
</script>
</body></html>)rawliteral";

void InitSortedSettings()
{
  // JSON will have values in this order
  // Required to display webpage info correctly.
  const char sortedSettings[][20] = {
    "Chip Model",
    "Flash Size",
    "CPU Freq (Mhz)",
    "CPU Cores",
    "Firmware",
    "Display Firmware",
    "Hostname",
    "IP Address",
    "MAC Address",
    "Wifi SSID",
    "Wifi RSSI",
    "Uptime",
    "Update Host",
    "Poolmaster Path",
    "Nextion Path",
    "SuperVisor Path",
    "Papertrail Host",
    "Papertrail Port",
    "Presence Detected",
    "MQTT Server",
    "MQTT Port",
    "MQTT Topic",
    "MQTT Connected",
  };

  int n = sizeof(sortedSettings)/sizeof(sortedSettings[0]);
  for (int i=0; i<n; i++) {
    const char* key=sortedSettings[i];
    PMInfo[key]     = "none";
    SVSettings[key] = "none";
  }
  // load static info
  SVSettings["MAC Address"]       = WiFi.macAddress();
  SVSettings["Firmware"]          = PMSV_VERSION;
  SVSettings["Chip Model"]        = ESP.getChipModel();
  SVSettings["Flash Size"]        = ESP.getFlashChipSize();
  SVSettings["CPU Freq (Mhz)"]    = ESP.getCpuFreqMHz();
  SVSettings["CPU Cores"]         = ESP.getChipCores();
}

void loadSettings()
{
  InitSortedSettings();
  preferences.begin("PMSV", true);
  //hostname = preferences.getString("Hostname", "");
  preferences.getString("Hostname",hostname,15);
  SVSettings["Hostname"]            = hostname;
  SVSettings["MQTT Server"]         = preferences.getString("MQTT Server",     defaultmqtt_server);
  SVSettings["MQTT Port"]           = preferences.getString("MQTT Port",       defaultmqtt_port);
  SVSettings["MQTT Topic"]          = preferences.getString("MQTT Topic",      defaultmqtt_topic);
  SVSettings["MQTT Username"]       = preferences.getString("MQTT Username",   defaultmqtt_username);
  SVSettings["MQTT Password"]       = preferences.getString("MQTT Password",   defaultmqtt_password);
  SVSettings["HOME Assistant"]      = preferences.getString("HOME Assistant",  defaulthomeassistanttopic);
  SVSettings["Update Host"]         = preferences.getString("Update Host",     defaultUpdatehost);
  SVSettings["Poolmaster Path"]     = preferences.getString("Poolmaster Path", defaultPoolmasterPath);
  SVSettings["Nextion Path"]        = preferences.getString("Nextion Path",    defaultNextionPath);
  SVSettings["SuperVisor Path"]     = preferences.getString("SuperVisor Path", defaultSuperVisorPath);
  SVSettings["Papertrail Host"]     = preferences.getString("Papertrail Host", defaultpapertrailhost);
  SVSettings["Papertrail Port"]     = preferences.getString("Papertrail Port", defaultpapertrailport);
  SVSettings["TFA_Venice"]          = preferences.getString("TFA_Venice",      "0");
  SVSettings["WaterMeter GPIO"]     = preferences.getString("WaterMeter GPIO", "0");
  SVSettings["WaterMeter K"]        = preferences.getString("WaterMeter K",    "1");
  SVSettings["WaterMeter D"]        = preferences.getString("WaterMeter D",    "250");
  SVSettings["WaterMeter L"]        = preferences.getString("WaterMeter L",    "0");

  preferences.end();
}

String WebHandleLogs()
{ 
  String Slogs = "";
  while (char* theline = myLogsRingBuffer.pull())
    Slogs += String(theline) + "\n";
  return Slogs; 
}

String WebHandleProgressBar()
{ 
  char Payload[200];
  sprintf(Payload, "{ \"text\":\"%s\", \"pc\":\"%d%\"}", barBuf, UpdateinProgress);
  return String(Payload);
}

void WebHandlePMInfo(AsyncWebServerRequest *request)
{ 
  if (UpdateinProgress>0) {
      request->send(200, "text/plain", "");
      return;
  }
  char Payload[PAYLOAD_BUFFER_LENGTH];
  size_t n = serializeJson(PMInfo, Payload);
  request->send(200, "text/plain", Payload);
}

void WebHandleSVSettings(AsyncWebServerRequest *request)
{ 
  if (UpdateinProgress>0) {
      request->send(200, "text/plain", "");
      return;
  }
  char buffer[15];
  upcurrenttime();
  SVSettings["Uptime"]          = currentUptime;
  SVSettings["Hostname"]        = WiFi.getHostname();
  SVSettings["IP Address"]      = WiFi.localIP().toString();
  SVSettings["Wifi SSID"]       = wifiManager.getWiFiSSID();
  SVSettings["Wifi RSSI"]       = WiFi.RSSI();
  dtostrf(ESP.getHeapSize() / 1024.0, 3, 3, buffer);
  SVSettings["HEAP size (KB)"]  = buffer;
  dtostrf(ESP.getFreeHeap() / 1024.0, 3, 3, buffer);
  SVSettings["HEAP free (KB)"]  = buffer;
  dtostrf(ESP.getMaxAllocHeap() / 1024.0, 3, 3, buffer);
  SVSettings["HEAP maxAlloc"]   = buffer;

  if (MqttClient.connected()) strcpy(buffer, "*green");
  else strcpy(buffer, "*black");
  SVSettings["MQTT Connected"]  = buffer;
  if (IRDetected != -1) {
    if (IRDetected > TFT_PowerSaving)  
          strcpy(buffer, "*black");
    else  strcpy(buffer, "*cyan");
    SVSettings["Presence Detected"] = buffer;
  }
  
  if (!request) return;
  // remove unnecessary data
  JsonDocument cleanJson = SVSettings;
  cleanJson.remove("MQTT Username");
  cleanJson.remove("MQTT Password");
  char Payload[PAYLOAD_BUFFER_LENGTH];
  size_t n = serializeJson(cleanJson, Payload);
  request->send(200, "text/plain", Payload);
}

bool WebSetActionManageParam(AsyncWebServerRequest *request, const char* key) 
{
  bool ret = false;
  if (!request->hasParam(key)) return ret;
  String setting=request->arg(key);
  if (setting == "@@@@@@@@@@@@@@@") return ret; // dummy value
  String old = SVSettings[key];
  // pre specific check
  if ((strcmp(key, "MQTT Topic") == 0) && (setting[0]== '/')) 
    setting.remove(0,1);
  if (old != setting) {
    SVSettings[key] = String(setting);
    preferences.putString(key, setting);
    ret = true;
    }
  return ret;
}

void WebSetAction(AsyncWebServerRequest *request) 
{
  preferences.begin("PMSV", false);
  if (WebSetActionManageParam(request, "MQTT Server"))    mustRestartMQTT = true;
  if (WebSetActionManageParam(request, "MQTT Port"))      mustRestartMQTT = true;
  if (WebSetActionManageParam(request, "MQTT Topic"))     mustRestartMQTT = true;
  if (WebSetActionManageParam(request, "MQTT Username"))  mustRestartMQTT = true;
  if (WebSetActionManageParam(request, "MQTT Password"))  mustRestartMQTT = true;
  if (WebSetActionManageParam(request, "HOME Assistant")) mustCreateHAEntities = true;
  WebSetActionManageParam(request, "Papertrail Host");
  WebSetActionManageParam(request, "Papertrail Port"); 
  if (WebSetActionManageParam(request, "WaterMeter L"))   mustUpdateWMCounter = true;
  WebSetActionManageParam(request, "WaterMeter K");
  WebSetActionManageParam(request, "WaterMeter D");
  WebSetActionManageParam(request, "WaterMeter GPIO");
  WebSetActionManageParam(request, "TFA_Venice");
  WebSetActionManageParam(request, "Update Host");
  WebSetActionManageParam(request, "Poolmaster Path");
  WebSetActionManageParam(request, "Nextion Path");
  WebSetActionManageParam(request, "SuperVisor Path");
  WebSetActionManageParam(request, "Command");
  preferences.end();

  if (request->hasParam("rebootSuperVisor")) mustRebootSuperVisor = true;
  if (request->hasParam("rebootPoolMaster")) mustRebootPoolMaster = true;
  if (request->hasParam("rebootNextion"))    mustRebootNextion    = true;
  if (request->hasParam("UpdateSuperVisor")) mustUpdateSuperVisor = true;
  if (request->hasParam("UpdatePoolMaster")) mustUpdatePoolMaster = true;
  if (request->hasParam("UpdateNextion"))    mustUpdateNextion    = true;
  if (request->hasParam("createHAEntities")) mustCreateHAEntities = true;
  //if (request->hasParam("cleanHAEntities"))  mustCleanHAEntities = true;
  
  request->redirect("/"); 
}

void InitWebServer()
{
   // Send web page with input fields to client
  Webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html); });
  Webserver.onNotFound([](AsyncWebServerRequest* request) { request->redirect("/"); });
  Webserver.on("/getlogs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", WebHandleLogs()); });
  Webserver.on("/getpminfo", HTTP_POST, [](AsyncWebServerRequest *request){ 
    WebHandlePMInfo(request); });
  Webserver.on("/getsvinfo", HTTP_POST, [](AsyncWebServerRequest *request){ 
    WebHandleSVSettings(request); });
  Webserver.on("/getprogressbar", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", WebHandleProgressBar()); });
  Webserver.on("/set", HTTP_GET, WebSetAction);
  Webserver.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    handleDoUpdate(request, filename, index, data, len, final); });

  Webserver.begin();
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
      nvs_flash_erase(); // erase the NVS partition and...
      nvs_flash_init(); // initialize the NVS partition.
      delay(300);
      ESP.restart();
    }
  }
}


///////////// Update NEXTION and al ////////////////
////////////////////////////////////////////////////
void messagesLoop()
{
    if(mustUpdateNextion) {
      TaskUpdateNextion();
      mustUpdateNextion = false;
      mustRebootSuperVisor=true;
    }
    if (mustUpdatePoolMaster) {
      //TelnetToTaskUpdatePoolMaster();
      TaskUpdatePoolMaster();
      mustUpdatePoolMaster = false;
      mustRebootSuperVisor=true;
    }

    if (mustUpdateSuperVisor) {
      TaskUpdateSuperVisor();
      mustUpdateSuperVisor = false;
    }

    if (mustRebootSuperVisor) {
      delay(1000);
      ESP.restart();
    }

    if (mustRebootPoolMaster) {
      mustRebootPoolMaster = false;
      pinMode(ENPin, OUTPUT);
      digitalWrite(ENPin, LOW);
      delay(pdMS_TO_TICKS(200));
      pinMode(ENPin, OUTPUT);
      digitalWrite(ENPin, HIGH);
      pinMode(ENPin, INPUT);
    }

    if (mustRebootNextion) {
      mustRebootNextion = false;
      digitalWrite(NEXT_REBOOT, HIGH);
      delay(pdMS_TO_TICKS(500));
      digitalWrite(NEXT_REBOOT, LOW);
      delay(pdMS_TO_TICKS(1500));
      digitalWrite(NEXT_REBOOT, HIGH);
      delay(pdMS_TO_TICKS(500));
      digitalWrite(NEXT_REBOOT, LOW);
    }

    if (mustCreateHAEntities) {
      createHAEntities();
      mustCreateHAEntities = false;
    }

    if (mustCleanHAEntities) {
      cleanHAEntities();
      mustCleanHAEntities = false;
    }

    if (mustRestartMQTT) {
      MqttInit();
      mustRestartMQTT = false;
    }  
}

void TFTRefreshLoop(void *pvParameters)
{
  static int refreshLCD=0; // bug with TFT, force a refresh
  static UBaseType_t hwm=0;     // free stack size

  for (;;) {
    delay(300);
    if (++refreshLCD > 2000) {  // refresh TFT each 10min
      refreshTFT=true;
      refreshLCD = 0;           // reset counter
    }
    TFT_Refresh(refreshTFT);
    if (refreshTFT) refreshTFT=false;

    stack_mon(hwm);
  }
}


//////////////////////// SETUP //////////////////////////
/////////////////////////////////////////////////////////

extern void configModeCallback(WiFiManager*);

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Local_Logs_Dispatch(local_sbuf);
  Local_Logs_Dispatch("Disconnected from WiFi access point");
  snprintf(local_sbuf,sizeof(local_sbuf),"WiFi lost connection. Reason: %d", info.wifi_sta_disconnected.reason);
  Local_Logs_Dispatch(local_sbuf);
}

void setup() {
  Serial.begin(115200);

  loadSettings();

  pinMode(RESET_WIFI_PIN, INPUT_PULLUP);
  pinMode(IR_DETECTOR_PIN, INPUT);
  pinMode(NEXT_REBOOT, OUTPUT);
  
  rtc_wdt_protect_off();
  rtc_wdt_disable();

  TFT_Init();

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
  wifiManager.setAPCallback(configModeCallback);
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
    preferences.putString("Hostname", customhostname);
    preferences.end();
    delay(pdMS_TO_TICKS(1000));
		ESP.restart();    // ESP32 must reboot but why, anyway
  }

//  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  // to fix later : seems to cause reboot
  //MDNS.addService("http", "tcp", 80);
  //MDNS.begin(myhostname);      

  Local_Logs_Dispatch("");
  Local_Logs_Dispatch("WiFi connected ");
  snprintf(local_sbuf,sizeof(local_sbuf),"Hostname %s, IP address: %s ", WiFi.getHostname(), WiFi.localIP().toString().c_str());
  Local_Logs_Dispatch(local_sbuf);

  // Start I2C Slave channel, talking to PoolMaster
  // Using PoolMaster "Extensions"
  Wire.onReceive(onI2CReceive);
  Wire.onRequest(onI2CRequest);
  Wire.begin(SUPERVISOR_I2C_Address, SDA_S, SCL_S, 100000U);

  // Start I2C Master channel
  //Wire1.begin(SDA_M, SCL_M);

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
  Serial2.setTimeout(300);
  Serial2.setRxBufferSize(BUFFER_LENGTH);
  Serial2.begin(115200);

  //Start Telnet Server
  Telnetserver.begin();
  Telnetserver.setNoDelay(true);
  Local_Logs_Dispatch("Use 'telnet 23' to connect");

  // Start PaperTrail Logging
  const char* papertrailhost=SVSettings["Papertrail Host"];
  const char* papertrailport=SVSettings["Papertrail Port"];
  if ((strcmp(papertrailhost, "") != 0) && (strcmp(papertrailhost, defaultpapertrailhost) != 0)) {
    const uint16_t port = atoi(papertrailport);
    Logger.configureSyslog(papertrailhost, port, ""); // Syslog server IP, port and device name
    //Logger.registerSerial(COUNTER, DEBUG, "COUNT", Serial); // Log both to serial...
    Logger.registerSyslog(PL_LOG, ELOG_LEVEL_DEBUG, ELOG_FAC_USER, "poolmaster"); // ...and syslog. Set the facility to user
    Logger.registerSyslog(WD_LOG, ELOG_LEVEL_DEBUG, ELOG_FAC_USER, "supervisor"); // ...and syslog. Set the facility to user
    Logger.log(WD_LOG, ELOG_LEVEL_INFO, "PoolMaster Logs Started");
    Logger.log(WD_LOG, ELOG_LEVEL_INFO, "SuperVisor Logs Started");
  }

  // Create loop tasks in the scheduler.
  //------------------------------------
  xTaskCreatePinnedToCore(
    TFTRefreshLoop, 
    "TFTRefreshLoop",
    2560, // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
          // current stack size consumed is 1764 (TFT refresh)
    NULL,  // No parameter is used 
    1,  // Priority
    nullptr,  // Task handle is not used here
    0 // run on core 0, rest is running on core1 by default
  );

}

void parseMsgFromPM(char* msg)
{
  char *end;
  while (end = strchr(msg,  _DELIMITER_[0])) {
    *end = 0;
    char *key = msg;
    char *val = strstr(msg, "=");
    *val = 0;
    val++;
    if (key && val) PMInfo[key] = val;
    msg=end+1;
  }
}

void incomingSerialData()
{
  uint8_t i;

  //check if there are any new clients
  if (Telnetserver.hasClient()) {
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      //find free/disconnected spot
      if (!serverClients[i] || !serverClients[i].connected()) {
        if (serverClients[i]) serverClients[i].stop();
        serverClients[i] = Telnetserver.accept();
        if (!serverClients[i]) Local_Logs_Dispatch("available broken");
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
    }
    else if (serverClients[i]) serverClients[i].stop();
  }

  //check UART for data
  if (Serial2.available()) {
    if (readUntil(Serial2, sbuf, "\n")) {
      // `buf` contains the delimiter, it can now be used for parsing.

      if (char* msg = strchr(sbuf,  _DELIMITER_[0])) {  // is it a private message from PoolMaster ?
        parseMsgFromPM(msg+1);
      }
      else {
      //push UART data to all connected telnet clients
        for (i = 0; i < MAX_SRV_CLIENTS; i++) {
          if (serverClients[i] && serverClients[i].connected()) {
            serverClients[i].write(sbuf, strlen(sbuf));
            delay(10);
          }
        }

        // Cleanup buffer and send to the web
        for (int src = 0, dst = 0; src < sizeof(sbuf); src++)
        if (sbuf[src] != '\r') sbuf[dst++] = sbuf[src];
        myLogsRingBuffer.push("pm ", sbuf);

        // Cloud Logger PaperTrail
        const char* papertrailhost=SVSettings["Papertrail Host"];
        if ((strcmp(papertrailhost, "") != 0) && (strcmp(papertrailhost, defaultpapertrailhost) != 0)) {
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

          Logger.log(PL_LOG, logLevel, "%s", sbuf);
        }
      }
    }
  }
//  delay(pdMS_TO_TICKS(1000));
}


//////////////////////// MAIN LOOP //////////////////////////
/////////////////////////////////////////////////////////////
void loop()
{
  static int delaymqtt=0;   // send mqtt data every xx cycles
  static UBaseType_t hwm=0; // free stack size

  WifiManagerCheckResetLoop();
  if (WiFi.status() == WL_CONNECTED) {
    incomingSerialData();
    messagesLoop();
    if (++delaymqtt > 20000) {  // sent mqtt info every 20000 = ~5 sec
      delaymqtt=0;              // MQTT in a xcreatetask loop crashes
      mqttPublish();
    }
  }
  else {
    Local_Logs_Dispatch("WiFi not connected!");
    for (int i = 0; i < MAX_SRV_CLIENTS; i++)
      if (serverClients[i]) serverClients[i].stop();
  }
  stack_mon(hwm);
}
