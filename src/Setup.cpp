#undef __STRICT_ANSI__              // work-around for Time Zone definition
#include <stdint.h>                 // std lib (types definitions)
#include <Arduino.h>                // Arduino framework
#include <esp_sntp.h>
#include <stdarg.h>
#include "Config.h"
#include "PoolMaster.h"
#include "State_Machine.h"         // State machine for PoolMaster

#ifdef SIMU
bool init_simu = true;
double pHLastValue = 7.;
unsigned long pHLastTime = 0;
double OrpLastValue = 730.;
unsigned long OrpLastTime = 0;
double pHTab [3] {0.,0.,0.};
double ChlTab [3] {0.,0.,0.};
uint8_t iw = 0;
uint8_t jw = 0;
bool newpHOutput = false;
bool newChlOutput = false;
double pHCumul = 0.;
double ChlCumul = 0.;
#endif

// Firmware revision
String Firmw = FIRMW;

tm timeinfo;

RunTimeData PMData =
{ 
  0, 0,
  13,
  0, 0,
  3.0, 
  0.0, 0.0,
  28.0, 
  7.2, 720., 1.3,
  7.2, 720
};

// Various global flags
volatile bool startTasks = false;               // Signal to start loop tasks

// Store millis to allow Wifi Connection Timeout
static unsigned long ConnectionTimeout = 0; // Last action time done on TFT. Go to sleep after TFT_SLEEP
bool MDNSStatus = false;

bool AntiFreezeFiltering = false;               // Filtration anti freeze mode
//bool EmergencyStopFiltPump = false;             // flag will be (re)set by double-tapp button
bool PSIError = false;                          // Water pressure OK
bool cleaning_done = false;                     // daily cleaning done   

// NTP & MQTT Connected
bool PoolMaster_BoardReady = false;      // Is Board Up
bool PoolMaster_WifiReady = false;      // Is Wifi Up
bool PoolMaster_MQTTReady = false;      // Is MQTT Connected
bool PoolMaster_NTPReady = false;      // Is NTP Connected
bool PoolMaster_FullyLoaded = false;      // At startup gives time for everything to start before exiting Nextion's splash screen


// Queue object to store incoming JSON commands (up to 10)
QueueHandle_t queueIn;

// NVS Non Volatile SRAM (eqv. EEPROM)
Preferences nvs;      

// Instanciations of Pump and PID objects to make them global. But the constructors are then called 
// before loading of the storage struct. At run time, the attributes take the default
// values of the storage struct as they are compiled, just a few lines above, and not those which will 
// be read from NVS later. This means that the correct objects attributes must be set later in
// the setup function (fortunatelly, init methods exist).

// The five pumps of the system (instanciate the Pump class)
// In this case, all pumps start/Stop are managed by relays. pH, ORP, Robot and Electrolyse SWG pumps are interlocked with 
// filtration pump
// Pump class takes the following parameters:
//    1/ The MCU relay output pin number to be switched by this pump
//    2/ The MCU digital input pin number connected to the tank low level switch (can be NO_TANK or NO_LEVEL)
//    3/ The relay level used for the pump. ACTIVE_HIGH if relay ON when digital output is HIGH and ACTIVE_LOW if relay ON when digital output is LOW.
//    4/ The flow rate of the pump in Liters/Hour, typically 1.5 or 3.0 L/hour for peristaltic pumps for pools.
//       This is used to estimate how much of the tank we have emptied out.
//    5/ Tankvolume is used to compute the percentage of tank used/remaining
// IMPORTANT NOTE: second argument is ID and MUST correspond to the equipment index in the "Pool_Equipment" vector
// FiltrationPump: This Pump controls the filtration, no tank attached and not interlocked to any element. SSD relay attached works with HIGH level.

Pump FiltrationPump(FILTRATION);
// pHPump: This Pump has no low-level switch so remaining volume is estimated. It is interlocked with the relay of the FilrationPump
//Pump PhPump(PH_PUMP, PH_LEVEL, ACTIVE_LOW, MODE_LATCHING, storage.pHPumpFR, storage.pHTankVol, storage.AcidFill);
Pump PhPump(PH_PUMP,PH_LEVEL);
// ChlPump: This Pump has no low-level switch so remaining volume is estimated. It is interlocked with the relay of the FilrationPump
//Pump ChlPump(CHL_PUMP, CHL_LEVEL, ACTIVE_LOW, MODE_LATCHING, storage.ChlPumpFR, storage.ChlTankVol, storage.ChlFill);
Pump ChlPump(CHL_PUMP,CHL_LEVEL);
// RobotPump: This Pump is not injecting liquid so tank is associated to it. It is interlocked with the relay of the FilrationPump
Pump RobotPump(ROBOT);
// SWG: This Pump is associated with a Salt Water Chlorine Generator. It turns on and off the equipment to produce chlorine.
// It has no tank associated. It is interlocked with the relay of the FilrationPump
Pump SWGPump(SWG_PUMP);
// Filling Pump: This pump is autonomous, not interlocked with filtering pump.
Pump FillingPump(FILL_PUMP);

// The Relays class to activate and deactivate digital pins
Relay RELAYR0(PROJ,OUTPUT_DIGITAL); // Relay for the projector
Relay RELAYR1(SPARE,OUTPUT_DIGITAL); // Relay for the spare

// Input ports
InputSensor PoolWaterLevelSensor(POOL_LEVEL); // Pool water level sensor (pool level ok if HIGH and pool level problem if LOW)

// List of all the equipment of PoolMaster
//std::vector<PIN*> Pool_Equipment;
DeviceManager PoolDeviceManager;

double pHValue = 0.0;                // pH value

// PIDs instances
//Specify the direction and initial tuning parameters
PID PhPID(&PMData.PhValue, &PMData.PhPIDOutput, &PMData.Ph_SetPoint, 0, 0, 0, PhPID_DIRECTION);
PID OrpPID(&PMData.OrpValue, &PMData.OrpPIDOutput, &PMData.Orp_SetPoint, 0, 0, 0, OrpPID_DIRECTION);

// Publishing tasks handles to notify them
static TaskHandle_t pubSetTaskHandle;
static TaskHandle_t pubMeasTaskHandle;

// Used for ElegantOTA
//unsigned long ota_progress_millis = 0;

// Configuration Manager
ConfigManager PMConfig;

// Mutex to share access to I2C bus among two tasks: AnalogPoll and StatusLights
static SemaphoreHandle_t mutex;

// Functions prototypes
void StartTime(void);
bool readLocalTime(void);
//bool loadConfig(void);
//bool saveConfig(void);
void WiFiEvent(WiFiEvent_t);
void initTimers(void);
void InitWiFi(void);
void ScanWiFiNetworks(void);
void connectToWiFi(void);
void mqttInit(void);                     
void ResetTFT(void);
void PublishSettings(void);
void SetPhPID(bool);
void SetOrpPID(bool);
int  freeRam (void);
void AnalogInit(void);
void TempInit(void);
unsigned stack_hwm();
void stack_mon(UBaseType_t&);
void info();

// Functions used as Tasks
void PoolMaster(void*);
void AnalogPoll(void*);
void pHRegulation(void*);
void OrpRegulation(void*);
void getTemp(void*);
void ProcessCommand(void*);
void SettingsPublish(void*);
void MeasuresPublish(void*);
void StatusLights(void*);
void UpdateTFT(void*);
void HistoryStats(void *);

// For ElegantOTA
/*void onOTAStart(void);
void onOTAProgress(size_t,size_t);
void onOTAEnd(bool);*/

// Setup
void setup()
{
  //Serial port for debug info
  Serial.begin(115200);

  // Set appropriate debug level. The level is defined in PoolMaster.h
  Debug.setDebugLevel(DEBUG_LEVEL);
  Debug.timestampOn();
  Debug.debugLabelOn();
  Debug.newlineOn();
  Debug.formatTimestampOn();

  //get board info
  info();
  Debug.print(DBG_INFO,"Booting PoolMaster Version: %s",FIRMW);
  // Initialize Nextion TFT
  ResetTFT();
  PoolMaster_BoardReady = true;
  /*
  //Read ConfigVersion. If does not match expected value, restore default values
  if(nvs.begin("PoolMaster",true))
  {
    uint8_t vers = nvs.getUChar("ConfigVersion",0);
    Debug.print(DBG_INFO,"Stored version: %d",vers);
    nvs.end();

    if (vers == CONFIG_VERSION)
    {
      Debug.print(DBG_INFO,"Same version: %d / %d. Loading settings from NVS",vers,CONFIG_VERSION);
      if(loadConfig()) Debug.print(DBG_INFO,"Config loaded"); //Restore stored values from NVS
    }
    else
    {
      Debug.print(DBG_INFO,"New version: %d / %d. Loading new default settings",vers,CONFIG_VERSION);      
      if(saveConfig()) Debug.print(DBG_INFO,"Config saved");  //First time use. Save new default values to NVS
    }

  } else {
    Debug.print(DBG_ERROR,"NVS Error");
    nvs.end();
    Debug.print(DBG_INFO,"New version: %d. First saving of settings",CONFIG_VERSION);      
      if(saveConfig()) Debug.print(DBG_INFO,"Config saved");  //First time use. Save new default values to NVS
  }  
*/
  // Initialize configuration manager
  // List of parameters are in PoolMaster.h, in the ParamID enum
  // Please update the list if you add new parameters
  // Supported types are: bool, uint8_t, unsigned long, double, char* and uint32_t
  PMConfig.SetNamespace(CONFIG_NVS_NAME);
  PMConfig.initParam(AUTOMODE,            "AutoMode",               false);
  PMConfig.initParam(WINTERMODE,          "WinterMode",             false);
  PMConfig.initParam(FILTRATIONSTART,     "FiltrStart",             (uint8_t)8); 
  PMConfig.initParam(FILTRATIONSTOP,      "FiltrStop",              (uint8_t)20); 
  PMConfig.initParam(FILTRATIONSTARTMIN,  "FiltrStartMin",          (uint8_t)8); 
  PMConfig.initParam(FILTRATIONSTOPMAX,   "FiltrStopMax",           (uint8_t)22); 
  PMConfig.initParam(DELAYPIDS,           "DelayPIDs",              (uint8_t)15); 
  PMConfig.initParam(PUBLISHPERIOD,       "PublishPeriod",          (unsigned long)PUBLISHINTERVAL);
  PMConfig.initParam(PHPIDWINDOWSIZE,     "PhPIDWSize",             (unsigned long)60000);
  PMConfig.initParam(ORPPIDWINDOWSIZE,    "OrpPIDWSize",            (unsigned long)30000);
  PMConfig.initParam(PH_SETPOINT,         "PhSetPoint",             (double)7.2);
  PMConfig.initParam(ORP_SETPOINT,        "OrpSetPoint",            (double)750.0);
  PMConfig.initParam(PSI_HIGHTHRESHOLD,   "PSIHigh",                (double)0.5);
  PMConfig.initParam(PSI_MEDTHRESHOLD,    "PSIMed",                 (double)0.25);
  PMConfig.initParam(WATERTEMPLOWTHRESHOLD,"WaterTempLow",          (double)10.0);
  PMConfig.initParam(WATERTEMP_SETPOINT,  "WaterTempSet",           (double)27.0);
  #ifdef EXT_ADS1115
    PMConfig.initParam(PHCALIBCOEFFS0,      "pHCalibCoeffs0",         (double)-2.50133333);
    PMConfig.initParam(PHCALIBCOEFFS1,      "pHCalibCoeffs1",         (double)6.9);
    PMConfig.initParam(ORPCALIBCOEFFS0,     "OrpCalibCoeffs0",        (double)431.03);
    PMConfig.initParam(ORPCALIBCOEFFS1,     "OrpCalibCoeffs1",        (double)0.0);
  #else
    PMConfig.initParam(PHCALIBCOEFFS0,      "pHCalibCoeffs0",         (double)0.9583);
    PMConfig.initParam(PHCALIBCOEFFS1,      "pHCalibCoeffs1",         (double)4.834);
    PMConfig.initParam(ORPCALIBCOEFFS0,     "OrpCalibCoeffs0",        (double)129.2);
    PMConfig.initParam(ORPCALIBCOEFFS1,     "OrpCalibCoeffs1",        (double)384.1);
  #endif
  PMConfig.initParam(PSICALIBCOEFFS0,     "PSICalibCoeffs0",        (double)0.377923399);
  PMConfig.initParam(PSICALIBCOEFFS1,     "PSICalibCoeffs1",        (double)-0.17634473);
  //Default values for pH and ORP PIDs
  // si pH+ : Kp=2250000.
  // si pH- : Kp=2700000.
  PMConfig.initParam(PH_KP,               "PhKp",                   (double)2700000.0);
  PMConfig.initParam(PH_KI,               "PhKi",                   (double)0.0);
  PMConfig.initParam(PH_KD,               "PhKd",                   (double)0.0);
  PMConfig.initParam(ORP_KP,              "OrpKp",                  (double)2500.0);
  PMConfig.initParam(ORP_KI,              "OrpKi",                  (double)0.0);
  PMConfig.initParam(ORP_KD,              "OrpKd",                  (double)0.0);
  PMConfig.initParam(SECUREELECTRO,       "SecureElectro",          (uint8_t)15);
  PMConfig.initParam(DELAYELECTRO,        "DelayElectro",           (uint8_t)2);
  PMConfig.initParam(ELECTRORUNMODE,      "ElectroRunMode",         (bool)SWG_MODE_ADJUST);
  PMConfig.initParam(ELECTRORUNTIME,      "ElectroRunTime",         (uint8_t)8);
  PMConfig.initParam(ELECTROLYSEMODE,     "ElectrolyseMode",        (bool)false);
  PMConfig.initParam(PHAUTOMODE,          "PhAutoMode",             (bool)false);
  PMConfig.initParam(ORPAUTOMODE,         "OrpAutoMode",            (bool)false);
  PMConfig.initParam(FILLAUTOMODE,        "FillAutoMode",           (bool)false);
  PMConfig.initParam(LANG_LOCALE,         "LangLocale",             (uint8_t)0);
  PMConfig.initParam(MQTT_IP,             "MqttIP",                 (uint32_t)IPAddress(192,168,0,0));
  PMConfig.initParam(MQTT_PORT,           "MqttPort",               (uint32_t)1883);
  PMConfig.initParam(MQTT_LOGIN,          "MqttLogin",              (char*)"");
  PMConfig.initParam(MQTT_PASS,           "MqttPass",               (char*)"");
  PMConfig.initParam(MQTT_ID,             "MqttId",                 (char*)MQTTID);
  PMConfig.initParam(MQTT_TOPIC,          "MqttTopic",              (char*)POOLTOPIC);
  PMConfig.initParam(SMTP_SERVER,         "SmtpServer",             (char*)"");
  PMConfig.initParam(SMTP_PORT,           "SmtpPort",               (uint32_t)587);
  PMConfig.initParam(SMTP_LOGIN,          "SmtpLogin",              (char*)"");
  PMConfig.initParam(SMTP_PASS,           "SmtpPass",               (char*)"");
  PMConfig.initParam(SMTP_SENDER,         "SmtpSender",             (char*)"");
  PMConfig.initParam(SMTP_RECIPIENT,      "SmtpRecipient",          (char*)"");
  PMConfig.initParam(BUZZERON,            "BuzzerOn",               (bool)true);
  // End of configuration manager initialization

  PMConfig.printAllParams(); // Print all parameters to Serial for debug

  //Define pins directions
  pinMode(BUZZER, OUTPUT);

  // Warning: pins used here have no pull-ups, provide external ones
  pinMode(CHL_LEVEL, INPUT);
  pinMode(PH_LEVEL, INPUT);

  // Initialize default's devices name
  FiltrationPump.SetName("Filtration Pump");
  PhPump.SetName("pH Pump");
  ChlPump.SetName("Chlorine Pump");
  RobotPump.SetName("Robot Pump");
  SWGPump.SetName("SWG Pump");
  FillingPump.SetName("Filling Pump");
  RELAYR0.SetName("Projector Relay");
  RELAYR1.SetName("Spare Relay");
  PoolWaterLevelSensor.SetName("Water Level Sensor");

  // Sets all default values for the pumps. If preferences are loaded from NVS, these values will be overwritten on a value per value basis.
  PhPump.SetInterlock(DEVICE_FILTPUMP); // pH Pump interlocked with Filtration Pump
  ChlPump.SetInterlock(DEVICE_FILTPUMP); // Chlorine Pump interlocked with Filtration Pump
  RobotPump.SetInterlock(DEVICE_FILTPUMP); // Robot Pump interlocked with Filtration Pump
  SWGPump.SetInterlock(DEVICE_FILTPUMP); // SWG Pump interlocked
  
  // Default values for the pumps
  FillingPump.SetMinUpTime(5*60*1000);  // Minimum running uptime for Filling Pump is 5 minutes (avoir short runs)
  FiltrationPump.SetMaxUpTime(0); // No Maximum uptime for Filtration Pump

  // Initialize the state machine handlers for the pumps
  FillingPump.SetHandlers(FillingPump_StartCondition,FillingPump_StopCondition,FillingPump_StartAction,FillingPump_StopAction);
  FiltrationPump.SetHandlers(FiltrationPump_StartCondition,FiltrationPump_StopCondition,FiltrationPump_StartAction,FiltrationPump_StopAction);
  FiltrationPump.SetLoopHandler(FiltrationPump_LoopActions);
  RobotPump.SetHandlers(RobotPump_StartCondition,RobotPump_StopCondition,RobotPump_StartAction,RobotPump_StopAction);
  SWGPump.SetHandlers(SWGPump_StartCondition,SWGPump_StopCondition,SWGPump_StartAction,SWGPump_StopAction);

  // Fill DeviceManager with the list of devices
  PoolDeviceManager.AddDevice(DEVICE_FILTPUMP,&FiltrationPump);
  PoolDeviceManager.AddDevice(DEVICE_PH_PUMP,&PhPump);
  PoolDeviceManager.AddDevice(DEVICE_CHL_PUMP,&ChlPump);
  PoolDeviceManager.AddDevice(DEVICE_ROBOT,&RobotPump);
  PoolDeviceManager.AddDevice(DEVICE_SWG,&SWGPump);
  PoolDeviceManager.AddDevice(DEVICE_FILLING_PUMP,&FillingPump);
  PoolDeviceManager.AddDevice(DEVICE_RELAY0,&RELAYR0);
  PoolDeviceManager.AddDevice(DEVICE_RELAY1,&RELAYR1);
  PoolDeviceManager.AddDevice(DEVICE_POOL_LEVEL,&PoolWaterLevelSensor);

  // Load configuration parameters from NVS for all devices
  PoolDeviceManager.LoadPreferences();
  PoolDeviceManager.InitDevicesInterlock();
  PoolDeviceManager.Begin();

  // Initialize watch-dog
  esp_task_wdt_init(WDT_TIMEOUT, true);

  //Initialize MQTT
  mqttInit();

  // Initialize WiFi events management (on connect/disconnect)
  WiFi.onEvent(WiFiEvent);
  initTimers();
  InitWiFi();
  //ScanWiFiNetworks();
  connectToWiFi();

  delay(500);    // let task start-up and wait for connection
  ConnectionTimeout = millis();
  while((WiFi.status() != WL_CONNECTED)&&((unsigned long)(millis() - ConnectionTimeout) < WIFI_TIMEOUT)) {
    delay(500);
    Serial.print(".");
  }
  
  // Start I2C for ADS1115 and status lights through PCF8574A
  Wire.begin(I2C_SDA,I2C_SCL);

  // Init pH, ORP and PSI analog measurements
  AnalogInit();

  // Init Water and Air temperatures measurements
  TempInit();

  // Clear status LEDs
  Wire.beginTransmission(PCF8574ADDRESS);
  Wire.write((uint8_t)0xFF);
  Wire.endTransmission();

  // Initialize PIDs
  PMData.PhPIDwStart  = millis();
  PMData.OrpPIDwStart  = millis();

  // Limit the PIDs output range in order to limit max. pumps runtime (safety first...)
  PhPID.SetTunings(PMConfig.get<double>(PH_KP), PMConfig.get<double>(PH_KI), PMConfig.get<double>(PH_KD));
  PhPID.SetControllerDirection(PhPID_DIRECTION);
  PhPID.SetSampleTime((int)PMConfig.get<unsigned long>(PHPIDWINDOWSIZE));
  PhPID.SetOutputLimits(0, PMConfig.get<unsigned long>(PHPIDWINDOWSIZE));    //Whatever happens, don't allow continuous injection of Acid for more than a PID Window

  OrpPID.SetTunings(PMConfig.get<double>(ORP_KP), PMConfig.get<double>(ORP_KI), PMConfig.get<double>(ORP_KD));
  OrpPID.SetControllerDirection(OrpPID_DIRECTION);
  OrpPID.SetSampleTime((int)PMConfig.get<unsigned long>(ORPPIDWINDOWSIZE));
  OrpPID.SetOutputLimits(0, PMConfig.get<unsigned long>(ORPPIDWINDOWSIZE));  //Whatever happens, don't allow continuous injection of Chl for more than a PID Window

  // PIDs off at start
  SetPhPID (false);
  SetOrpPID(false);

   // Create queue for external commands
  queueIn = xQueueCreate((UBaseType_t)QUEUE_ITEMS_NBR,(UBaseType_t)QUEUE_ITEM_SIZE);

  // Create loop tasks in the scheduler.
  //------------------------------------
  int app_cpu = xPortGetCoreID();

  Debug.print(DBG_DEBUG,"Creating loop Tasks");

  // Create I2C sharing mutex
  mutex = xSemaphoreCreateMutex();

  // Analog measurement polling task
  xTaskCreatePinnedToCore(
    AnalogPoll,
    "AnalogPoll",
    3072,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // MQTT commands processing
  xTaskCreatePinnedToCore(
    ProcessCommand,
    "ProcessCommand",
    3584,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // PoolMaster: Supervisory task
  xTaskCreatePinnedToCore(
    PoolMaster,
    "PoolMaster",
    5120,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // Temperatures measurement
  xTaskCreatePinnedToCore(
    getTemp,
    "GetTemp",
    3072,
    NULL,
    1,
    nullptr,
    app_cpu
  );
  
 // ORP regulation loop
    xTaskCreatePinnedToCore(
    OrpRegulation,
    "ORPRegulation",
    2048,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // pH regulation loop
    xTaskCreatePinnedToCore(
    pHRegulation,
    "pHRegulation",
    2048,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // Status lights display
  xTaskCreatePinnedToCore(
    StatusLights,
    "StatusLights",
    2048,
    NULL,
    1,
    nullptr,
    app_cpu
  );  

  // Measures MQTT publish 
  xTaskCreatePinnedToCore(
    MeasuresPublish,
    "MeasuresPublish",
    3072,
    NULL,
    1,
    &pubMeasTaskHandle,               // needed to notify task later
    app_cpu
  );

  // MQTT Settings publish 
  xTaskCreatePinnedToCore(
    SettingsPublish,
    "SettingsPublish",
    3584,
    NULL,
    1,
    &pubSetTaskHandle,                // needed to notify task later
    app_cpu
  );

  // NEXTION Screen Update
  xTaskCreatePinnedToCore(
    UpdateTFT,
    "UpdateTFT",
    3072,
    NULL,
    1,
    nullptr, 
    app_cpu
  );

  // History Stats Storage
  xTaskCreatePinnedToCore(
    HistoryStats,
    "HistoryStats",
    2048,
    NULL,
    1,
    nullptr,
    app_cpu
  );

#ifdef ELEGANT_OTA
// ELEGANTOTA Configuration
  server.on("/", []() {
  server.send(200, "text/plain", "NA");
  });
  ElegantOTA.begin(&server);    // Start ElegantOTA
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  server.begin();
  Serial.println("HTTP server started");
  // Set Authentication Credentials
#ifdef ELEGANT_OTA_AUTH
  ElegantOTA.setAuth(ELEGANT_OTA_USERNAME, ELEGANT_OTA_PASSWORD);
#endif
#endif

  // Initialize OTA (On The Air update)
  //-----------------------------------
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname("PoolMaster");
  ArduinoOTA.setPasswordHash(OTA_PWDHASH);
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    Debug.print(DBG_INFO,"Start updating %s",type);
  });
  ArduinoOTA.onEnd([]() {
  Debug.print(DBG_INFO,"End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    esp_task_wdt_reset();           // reset Watchdog as upload may last some time...
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Debug.print(DBG_ERROR,"Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Debug.print(DBG_ERROR,"Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Debug.print(DBG_ERROR,"Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Debug.print(DBG_ERROR,"Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Debug.print(DBG_ERROR,"Receive Failed");
    else if (error == OTA_END_ERROR)     Debug.print(DBG_ERROR,"End Failed");
  });

  ArduinoOTA.begin();

  //display remaining RAM/Heap space.
  Debug.print(DBG_DEBUG,"[memCheck] Stack: %d bytes - Heap: %d bytes",stack_hwm(),freeRam());

  // Start loops tasks
  Debug.print(DBG_INFO,"Init done, starting loop tasks");
  startTasks = true;

  delay(1000);          // wait for tasks to start
}

//Compute free RAM
//useful to check if it does not shrink over time
int freeRam () {
  int v = xPortGetFreeHeapSize();
  return v;
}

// Get current free stack 
unsigned stack_hwm(){
  return uxTaskGetStackHighWaterMark(nullptr);
}

// Monitor free stack (display smallest value)
void stack_mon(UBaseType_t &hwm)
{
  UBaseType_t temp = uxTaskGetStackHighWaterMark(nullptr);
  if(!hwm || temp < hwm)
  {
    hwm = temp;
    Debug.print(DBG_DEBUG,"[stack_mon] %s: %d bytes",pcTaskGetTaskName(NULL), hwm);
  }  
}

// Get exclusive access of I2C bus
void lockI2C(){
  xSemaphoreTake(mutex, portMAX_DELAY);
}

// Release I2C bus access
void unlockI2C(){
  xSemaphoreGive(mutex);  
}

// Set time parameters, including DST
void StartTime(){
  configTime(0, 0,"0.pool.ntp.org","1.pool.ntp.org","2.pool.ntp.org"); // 3 possible NTP servers
  setenv("TZ","CET-1CEST,M3.5.0/2,M10.5.0/3",3);                       // configure local time with automatic DST  
  tzset();
  int retry = 0;
  const int retry_count = 15;
  while(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count){
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  Debug.print(DBG_INFO,"NTP configured");
}

bool readLocalTime(){
  if(!getLocalTime(&timeinfo,5000U)){
    Debug.print(DBG_WARNING,"Failed to obtain time");
    return false;
  }
  //Debug.print("%A, %B %d %Y %H:%M:%S",&timeinfo);
  return true;
}

// Notify PublishSettings task 
void PublishSettings()
{
  xTaskNotifyGive(pubSetTaskHandle);
}

// Notify PublishMeasures task
void PublishMeasures()
{
  xTaskNotifyGive(pubMeasTaskHandle);
}

//board info
void info(){
  esp_chip_info_t out_info;
  esp_chip_info(&out_info);
  Debug.print(DBG_INFO,"CPU frequency       : %dMHz",ESP.getCpuFreqMHz());
  Debug.print(DBG_INFO,"CPU Cores           : %d",out_info.cores);
  Debug.print(DBG_INFO,"Flash size          : %dMB",ESP.getFlashChipSize()/1000000);
  Debug.print(DBG_INFO,"Free RAM            : %d bytes",ESP.getFreeHeap());
  Debug.print(DBG_INFO,"Min heap            : %d bytes",esp_get_free_heap_size());
  Debug.print(DBG_INFO,"tskIDLE_PRIORITY    : %d",tskIDLE_PRIORITY);
  Debug.print(DBG_INFO,"confixMAX_PRIORITIES: %d",configMAX_PRIORITIES);
  Debug.print(DBG_INFO,"configTICK_RATE_HZ  : %d",configTICK_RATE_HZ);
}


// Pseudo loop, which deletes loopTask of the Arduino framework
void loop()
{
  delay(1000);
  vTaskDelete(nullptr);
}

