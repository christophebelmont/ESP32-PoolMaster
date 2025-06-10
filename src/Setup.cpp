#undef __STRICT_ANSI__              // work-around for Time Zone definition
#include <stdint.h>                 // std lib (types definitions)
#include <Arduino.h>                // Arduino framework
#include <esp_sntp.h>
#include <stdarg.h>
#include "Config.h"
#include "PoolMaster.h"

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

//Settings structure and its default values
// si pH+ : Kp=2250000.
// si pH- : Kp=2700000.
// Saved 7.3, 720.0, 1.8, 0.3, 16.0, 27.0, 3.0, -2.49, 6.87, 431.03, 0, 0.377923399, -0.17634473,
#ifdef EXT_ADS1115
StoreStruct storage =
{ 
  CONFIG_VERSION,
  0, 0, 1, 0,
  8, 11, 19, 8, 22, 20,
  30000,
  1800000, 1800000, 0, 0,
  7.3, 720.0, 1.8, 0.3, 16.0, 27.0, 3.0, -2.50133333, 6.9, 431.03, 0, 0.377923399, -0.17634473,
  2700000.0, 0.0, 0.0, 18000.0, 0.0, 0.0, 0.0, 0.0, 28.0, 7.3, 720., 1.3,
  //100.0, 100.0, 20.0, 20.0, 1.5, 1.5,
  15, 2,  //ajout
  SWG_MODE_ADJUST, 8,
  0, 1, 0, 0,
  0,
  IPAddress(192,168,0,0), // IP
  1883,
  "","",MQTTID,POOLTOPIC,
  "",
  587,
  "", "","","",  
  //FILLING_PUMP_MINI_DURATION,FILLING_PUMP_MAXI_DURATION,
  0,
  {
    {  FILTRATION, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING, 0., 0., 100., NO_TANK, 0, 0},
    {  PH_PUMP, OUTPUT_DIGITAL, 0,ACTIVE_LOW, MODE_LATCHING , 1.5, 20., 100., PH_LEVEL, 0, PH_PUMP_MAX_UPTIME*60},
    {  CHL_PUMP, OUTPUT_DIGITAL, 0,ACTIVE_LOW, MODE_LATCHING , 1.5, 20., 100., CHL_LEVEL, 0, CHL_PUMP_MAX_UPTIME*60},
    {  ROBOT, OUTPUT_DIGITAL, 0, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0},
    {  SWG_PUMP, OUTPUT_DIGITAL, 0, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0},
    {  FILL_PUMP, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, FILLING_PUMP_MIN_UPTIME*60, FILLING_PUMP_MAX_UPTIME*60},
    {  PROJ, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0},
    {  SPARE, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0},
   }
};
#else
StoreStruct storage =
{ 
  CONFIG_VERSION,
  0, 0, 1, 0,
  13, 8, 21, 8, 22, 20,
  30000,
  1800000, 1800000, 0, 0,
  7.3, 720.0, 1.8, 0.7, 10.0, 18.0, 3.0, 0.9583, 4.834, 129.2, 384.1, 1.31, -0.1,
  2700000.0, 0.0, 0.0, 18000.0, 0.0, 0.0, 0.0, 0.0, 28.0, 7.3, 720., 1.3,
  //25.0, 60.0, 20.0, 20.0, 1.5, 1.5,
  15, 2,  //ajout
  SWG_MODE_ADJUST, 8,
  0, 1, 0, 0,
  0,
  IPAddress(192,168,0,0), // IP
  1883,
  "","",MQTTID,POOLTOPIC,
  "",
  587,
  "", "","","",  
  //FILLING_PUMP_MIN_UPTIME,FILLING_PUMP_MAX_UPTIME,
  0,
  {
    {  FILTRATION, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING, 0., 0., 100., NO_TANK, 0, 0},
    {  PH_PUMP, OUTPUT_DIGITAL, 0,ACTIVE_LOW, MODE_LATCHING , 1.5, 20., 100., PH_LEVEL, 0, PH_PUMP_MAX_UPTIME*60},
    {  CHL_PUMP, OUTPUT_DIGITAL, 0,ACTIVE_LOW, MODE_LATCHING , 1.5, 20., 100., CHL_LEVEL, 0, CHL_PUMP_MAX_UPTIME*60},
    {  ROBOT, OUTPUT_DIGITAL, 0, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0},
    {  SWG_PUMP, OUTPUT_DIGITAL, 0, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0},
    {  FILL_PUMP, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, FILLING_PUMP_MIN_UPTIME*60, FILLING_PUMP_MAX_UPTIME*60},
    {  PROJ, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0},
    {  SPARE, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0},
   }
};
#endif
tm timeinfo;

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
Pump FiltrationPump(FILTRATION,0,NO_TANK,ACTIVE_LOW,MODE_LATCHING,0.,0.,100.);
// pHPump: This Pump has no low-level switch so remaining volume is estimated. It is interlocked with the relay of the FilrationPump
//Pump PhPump(PH_PUMP, PH_LEVEL, ACTIVE_LOW, MODE_LATCHING, storage.pHPumpFR, storage.pHTankVol, storage.AcidFill);
Pump PhPump(PH_PUMP,1,PH_LEVEL,ACTIVE_LOW,MODE_LATCHING,1.5,20.,100.);
// ChlPump: This Pump has no low-level switch so remaining volume is estimated. It is interlocked with the relay of the FilrationPump
//Pump ChlPump(CHL_PUMP, CHL_LEVEL, ACTIVE_LOW, MODE_LATCHING, storage.ChlPumpFR, storage.ChlTankVol, storage.ChlFill);
Pump ChlPump(CHL_PUMP,2,CHL_LEVEL,ACTIVE_LOW,MODE_LATCHING,1.5,20.,100.);
// RobotPump: This Pump is not injecting liquid so tank is associated to it. It is interlocked with the relay of the FilrationPump
Pump RobotPump(ROBOT,3,NO_TANK,ACTIVE_LOW,MODE_LATCHING,0.,0.,100.);
// SWG: This Pump is associated with a Salt Water Chlorine Generator. It turns on and off the equipment to produce chlorine.
// It has no tank associated. It is interlocked with the relay of the FilrationPump
Pump SWGPump(SWG_PUMP,4,NO_TANK,ACTIVE_LOW,MODE_LATCHING,0.,0.,100.);
// Filling Pump: This pump is autonomous, not interlocked with filtering pump.
Pump FillingPump(FILL_PUMP,5,NO_TANK,ACTIVE_LOW,MODE_LATCHING,0.,0.,100.);
//Pump *FillingPump;

// The Relays class to activate and deactivate digital pins
Relay RELAYR0(PROJ,6,OUTPUT_DIGITAL,ACTIVE_LOW,MODE_LATCHING); // Relay for the projector
Relay RELAYR1(SPARE,7,OUTPUT_DIGITAL,ACTIVE_LOW,MODE_LATCHING); // Relay for the spare

// Input ports
InputSensor PoolWaterLevelSensor(POOL_LEVEL, 8); // Pool water level sensor (pool level ok if HIGH and pool level problem if LOW)

// List of all the equipment of PoolMaster
//std::vector<PIN*> Pool_Equipment;
DeviceManager PoolDeviceManager;

// PIDs instances
//Specify the direction and initial tuning parameters
PID PhPID(&storage.PhValue, &storage.PhPIDOutput, &storage.Ph_SetPoint, storage.Ph_Kp, storage.Ph_Ki, storage.Ph_Kd, PhPID_DIRECTION);
PID OrpPID(&storage.OrpValue, &storage.OrpPIDOutput, &storage.Orp_SetPoint, storage.Orp_Kp, storage.Orp_Ki, storage.Orp_Kd, OrpPID_DIRECTION);

// Publishing tasks handles to notify them
static TaskHandle_t pubSetTaskHandle;
static TaskHandle_t pubMeasTaskHandle;

// Used for ElegantOTA
//unsigned long ota_progress_millis = 0;

// Mutex to share access to I2C bus among two tasks: AnalogPoll and StatusLights
static SemaphoreHandle_t mutex;

// Functions prototypes
void StartTime(void);
bool readLocalTime(void);
bool loadConfig(void);
bool saveConfig(void);
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
uint8_t loadParam(const char*, uint8_t);
bool loadParam(const char* ,bool);
bool saveParam(const char*,uint8_t );
bool saveParam(const char*,bool );
bool savePumpsConf();
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

  //Define pins directions
  pinMode(BUZZER, OUTPUT);

  // Warning: pins used here have no pull-ups, provide external ones
  pinMode(CHL_LEVEL, INPUT);
  pinMode(PH_LEVEL, INPUT);
  //pinMode(POOL_LEVEL, INPUT);

  // Fill the table of equipments (FiltrationPump is index [0])
  // save their configs
  // The order MUST correspond to the index when the Pumps and Relays objects are created
  /*Pool_Equipment.push_back(&FiltrationPump);
  Pool_Equipment.push_back(&PhPump);
  Pool_Equipment.push_back(&ChlPump);
  Pool_Equipment.push_back(&RobotPump);
  Pool_Equipment.push_back(&SWGPump);
  Pool_Equipment.push_back(&FillingPump);
  Pool_Equipment.push_back(&RELAYR0);
  Pool_Equipment.push_back(&RELAYR1);*/

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

  // Load configuration parameters from NVS
  PoolDeviceManager.LoadPreferences();
  PoolDeviceManager.InitDevicesInterlock();
  PoolDeviceManager.Begin();

  // Assign globals configuration parameters to pumps (pin number, high/low level and operation mode)
  /*int i=0;
  for(auto& equi: Pool_Equipment)
  {
    equi->SetPinNumber(storage.PumpsConfig[i].pin_number,storage.PumpsConfig[i].pin_direction, storage.PumpsConfig[i].pin_active_level);
    equi->SetOperationMode(storage.PumpsConfig[i].relay_operation_mode);
    equi->SetFlowRate(storage.PumpsConfig[i].pump_flow_rate);
    equi->SetTankVolume(storage.PumpsConfig[i].tank_vol);
    equi->SetTankFill(storage.PumpsConfig[i].tank_fill);
    equi->SetTankLevelPIN(storage.PumpsConfig[i].tank_level_pin);
    equi->SetMaxUpTime(storage.PumpsConfig[i].pump_max_uptime * 1000);

    // Initialize the interlocks
    if(storage.PumpsConfig[i].pin_interlock != NO_INTERLOCK)
    {
      for(auto equi_lock: Pool_Equipment)
      {
        if(equi_lock->GetPinId() == storage.PumpsConfig[i].pin_interlock)
        {
          equi->SetInterlock(equi_lock);
          Debug.print(DBG_INFO,"Configure Interlock %d = %d",i, Pool_Equipment[i]->GetInterlockId());
        }
      }
    }
    // Start Pump Operation
    equi->Begin();
    i++;
  }*/

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
  storage.PhPIDwindowStartTime  = millis();
  storage.OrpPIDwindowStartTime = millis();
  
  // Limit the PIDs output range in order to limit max. pumps runtime (safety first...)
  PhPID.SetTunings(storage.Ph_Kp, storage.Ph_Ki, storage.Ph_Kd);
  PhPID.SetControllerDirection(PhPID_DIRECTION);
  PhPID.SetSampleTime((int)storage.PhPIDWindowSize);
  PhPID.SetOutputLimits(0, storage.PhPIDWindowSize);    //Whatever happens, don't allow continuous injection of Acid for more than a PID Window

  OrpPID.SetTunings(storage.Orp_Kp, storage.Orp_Ki, storage.Orp_Kd);
  OrpPID.SetControllerDirection(OrpPID_DIRECTION);
  OrpPID.SetSampleTime((int)storage.OrpPIDWindowSize);
  OrpPID.SetOutputLimits(0, storage.OrpPIDWindowSize);  //Whatever happens, don't allow continuous injection of Chl for more than a PID Window

  // PIDs off at start
  SetPhPID (false);
  SetOrpPID(false);

  // Robot pump off at start
  //RobotPump.Stop();
  
  // Pool Filling pump off at start
  //FillingPump.Stop();

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

/*#ifdef ELEGANT_OTA
// ELEGANTOTA Configuration
  //server.on("/", []() {
  //  server.send(200, "text/plain", "NA");
  //});


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
#endif*/

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

/*
void onOTAStart() {
  // Log when OTA has started
  Debug.print(DBG_WARNING,"OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  esp_task_wdt_reset();           // reset Watchdog as upload may last some time...
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Debug.print(DBG_INFO,"OTA Progress Current: %u bytes, Final: %u bytes", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Debug.print(DBG_INFO,"OTA update finished successfully!");
  } else {
    Debug.print(DBG_ERROR,"There was an error during OTA update!");
  }
  // <Add your own code here>
}*/

bool loadConfig()
{
  size_t read_len = 0;
  nvs.begin("PoolMaster",true);

  storage.ConfigVersion         = nvs.getUChar("ConfigVersion",0);
  storage.Ph_RegulationOnOff    = nvs.getBool("Ph_RegOnOff",false);
  storage.Orp_RegulationOnOff   = nvs.getBool("Orp_RegOnOff",false);  
  storage.AutoMode              = nvs.getBool("AutoMode",true);
  storage.WinterMode            = nvs.getBool("WinterMode",false);
  storage.FiltrationDuration    = nvs.getUChar("FiltrDuration",12);
  storage.FiltrationStart       = nvs.getUChar("FiltrStart",8);
  storage.FiltrationStop        = nvs.getUChar("FiltrStop",20);
  storage.FiltrationStartMin    = nvs.getUChar("FiltrStartMin",8);
  storage.FiltrationStopMax     = nvs.getUChar("FiltrStopMax",22);
  storage.DelayPIDs             = nvs.getUChar("DelayPIDs",0);
  storage.PublishPeriod         = nvs.getULong("PublishPeriod",PUBLISHINTERVAL);
  storage.PhPIDWindowSize       = nvs.getULong("PhPIDWSize",60000);
  storage.OrpPIDWindowSize      = nvs.getULong("OrpPIDWSize",60000);
  storage.PhPIDwindowStartTime  = nvs.getULong("PhPIDwStart",0);
  storage.OrpPIDwindowStartTime = nvs.getULong("OrpPIDwStart",0);
  storage.Ph_SetPoint           = nvs.getDouble("Ph_SetPoint",7.3);
  storage.Orp_SetPoint          = nvs.getDouble("Orp_SetPoint",750);
  storage.PSI_HighThreshold     = nvs.getDouble("PSI_High",0.5);
  storage.PSI_MedThreshold      = nvs.getDouble("PSI_Med",0.25);
  storage.WaterTempLowThreshold = nvs.getDouble("WaterTempLow",10.);
  storage.WaterTemp_SetPoint    = nvs.getDouble("WaterTempSet",27.);
  storage.AirTemp               = nvs.getDouble("TempExternal",3.);
  storage.pHCalibCoeffs0        = nvs.getDouble("pHCalibCoeffs0",-2.50133333);
  storage.pHCalibCoeffs1        = nvs.getDouble("pHCalibCoeffs1",6.9);
  storage.OrpCalibCoeffs0       = nvs.getDouble("OrpCalibCoeffs0",431.03);
  storage.OrpCalibCoeffs1       = nvs.getDouble("OrpCalibCoeffs1",0);
  storage.PSICalibCoeffs0       = nvs.getDouble("PSICalibCoeffs0",0.377923399);
  storage.PSICalibCoeffs1       = nvs.getDouble("PSICalibCoeffs1",-0.17634473);
  storage.Ph_Kp                 = nvs.getDouble("Ph_Kp",2000000.);
  storage.Ph_Ki                 = nvs.getDouble("Ph_Ki",0.);
  storage.Ph_Kd                 = nvs.getDouble("Ph_Kd",0.);
  storage.Orp_Kp                = nvs.getDouble("Orp_Kp",2500.);
  storage.Orp_Ki                = nvs.getDouble("Orp_Ki",0.);
  storage.Orp_Kd                = nvs.getDouble("Orp_Kd",0.);
  storage.PhPIDOutput           = nvs.getDouble("PhPIDOutput",0.);
  storage.OrpPIDOutput          = nvs.getDouble("OrpPIDOutput",0.);
  storage.WaterTemp             = nvs.getDouble("TempValue",18.);
  storage.PhValue               = nvs.getDouble("PhValue",0.);
  storage.OrpValue              = nvs.getDouble("OrpValue",0.);
  storage.PSIValue              = nvs.getDouble("PSIValue",0.4);
  storage.SecureElectro         = nvs.getUChar("SecureElectro",15); 
  storage.DelayElectro          = nvs.getUChar("DelayElectro",2); 
  storage.ElectroRunMode       = nvs.getBool("ElectroRunMode",SWG_MODE_ADJUST); 
  storage.ElectroRuntime       = nvs.getUChar("ElectroRunTime",8);
  storage.ElectrolyseMode       = nvs.getBool("ElectrolyseMode",false); 
  storage.pHAutoMode            = nvs.getBool("pHAutoMode",false);
  storage.OrpAutoMode           = nvs.getBool("OrpAutoMode",false); 
  storage.Lang_Locale           = nvs.getUChar("Lang_Locale",0);
  storage.MQTT_IP               = nvs.getUInt("MQTT_IP",IPAddress(192,168,0,0));
  storage.MQTT_PORT             = nvs.getUInt("MQTT_PORT",1883);
  storage.SMTP_PORT             = nvs.getUInt("SMTP_PORT",587);
  
  nvs.getString("SMTP_SERVER",storage.SMTP_SERVER,49); 
  nvs.getString("SMTP_LOGIN",storage.SMTP_LOGIN,62); 
  nvs.getString("SMTP_PASS",storage.SMTP_PASS,62); 
  nvs.getString("SMTP_SENDER",storage.SMTP_SENDER,149); 
  nvs.getString("SMTP_RECIPIENT",storage.SMTP_RECIPIENT,149); 

  nvs.getString("MQTT_LOGIN",storage.MQTT_LOGIN,62); 
  nvs.getString("MQTT_PASS",storage.MQTT_PASS,62);
  if(nvs.getString("MQTT_ID",storage.MQTT_ID,29) == 0) {
    snprintf(storage.MQTT_ID,sizeof(storage.MQTT_ID),"%s",MQTTID);
  }
  if(nvs.getString("MQTT_TOPIC",storage.MQTT_TOPIC,49) == 0) {
    snprintf(storage.MQTT_TOPIC,sizeof(storage.MQTT_TOPIC),"%s",POOLTOPIC);
  }
  
  storage.BuzzerOn              = nvs.getBool("BuzzerOn",true);

  // Retreive Pumps Config if not existent take default
  if(nvs.getBytes("PumpsConf", &storage.PumpsConfig, sizeof(storage.PumpsConfig)) == 0) {
    storage.PumpsConfig[0] = {  FILTRATION, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING, 0., 0., 100., NO_TANK, 0, 0};
    storage.PumpsConfig[1] = {  PH_PUMP, OUTPUT_DIGITAL, 0, ACTIVE_LOW, MODE_LATCHING , 1.5, 20., 100., PH_LEVEL, 0, PH_PUMP_MAX_UPTIME*60};
    storage.PumpsConfig[2] = {  CHL_PUMP, OUTPUT_DIGITAL, 0, ACTIVE_LOW, MODE_LATCHING , 1.5, 20., 100., CHL_LEVEL, 0, CHL_PUMP_MAX_UPTIME*60};
    storage.PumpsConfig[3] = {  ROBOT, OUTPUT_DIGITAL, 0, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0};
    storage.PumpsConfig[4] = {  SWG_PUMP, OUTPUT_DIGITAL, 0, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0};
    storage.PumpsConfig[5] = {  FILL_PUMP, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, FILLING_PUMP_MIN_UPTIME, FILLING_PUMP_MAX_UPTIME};
    storage.PumpsConfig[6] = {  PROJ, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0};
    storage.PumpsConfig[7] = {  SPARE, OUTPUT_DIGITAL, NO_INTERLOCK, ACTIVE_LOW, MODE_LATCHING , 0., 0., 100., NO_TANK, 0, 0};
  }
 
  nvs.end();

  Debug.print(DBG_INFO,"%d",storage.ConfigVersion);
  Debug.print(DBG_INFO,"%d, %d, %d, %d",storage.Ph_RegulationOnOff,storage.Orp_RegulationOnOff,storage.AutoMode,storage.WinterMode);
  Debug.print(DBG_INFO,"%d, %d, %d, %d, %d, %d",storage.FiltrationDuration,storage.FiltrationStart,storage.FiltrationStop,
              storage.FiltrationStartMin,storage.FiltrationStopMax,storage.DelayPIDs);
  delay(100);
  Debug.print(DBG_INFO,"%d",storage.PublishPeriod);
  
  Debug.print(DBG_INFO,"%d, %d, %d, %d",storage.PhPIDWindowSize,storage.OrpPIDWindowSize,storage.PhPIDwindowStartTime,storage.OrpPIDwindowStartTime);
  Debug.print(DBG_INFO,"%3.1f, %4.0f, %3.1f, %3.1f, %3.0f, %3.0f, %4.1f, %8.6f, %9.6f, %11.6f, %11.6f, %3.1f, %3.1f",
              storage.Ph_SetPoint,storage.Orp_SetPoint,storage.PSI_HighThreshold,
              storage.PSI_MedThreshold,storage.WaterTempLowThreshold,storage.WaterTemp_SetPoint,storage.AirTemp,
              storage.pHCalibCoeffs0,storage.pHCalibCoeffs1,storage.OrpCalibCoeffs0,storage.OrpCalibCoeffs1,
              storage.PSICalibCoeffs0,storage.PSICalibCoeffs1);
  delay(100);
  Debug.print(DBG_INFO,"%8.0f, %3.0f, %3.0f, %6.0f, %3.0f, %3.0f, %7.0f, %7.0f, %4.2f, %4.2f, %4.0f, %4.2f",
              storage.Ph_Kp,storage.Ph_Ki,storage.Ph_Kd,storage.Orp_Kp,storage.Orp_Ki,storage.Orp_Kd,
              storage.PhPIDOutput,storage.OrpPIDOutput,storage.WaterTemp,storage.PhValue,storage.OrpValue,storage.PSIValue);
  Debug.print(DBG_INFO,"%d, %d, %d, %d, %d %d",storage.SecureElectro,storage.DelayElectro,storage.ElectrolyseMode,storage.pHAutoMode,
              storage.OrpAutoMode,storage.Lang_Locale);
  delay(100);
  Debug.print(DBG_INFO,"%s, %d, %s, %s, %s %s",storage.MQTT_IP.toString().c_str(),storage.MQTT_PORT,storage.MQTT_LOGIN,storage.MQTT_PASS,
    storage.MQTT_ID,storage.MQTT_TOPIC);
  delay(100);
  Debug.print(DBG_INFO,"%s, %d, %s, %s, %s",storage.SMTP_SERVER,storage.SMTP_PORT, storage.SMTP_LOGIN,storage.SMTP_SENDER,storage.SMTP_RECIPIENT);
  
  
  return (storage.ConfigVersion == CONFIG_VERSION);
}

bool saveConfig()
{
  nvs.begin("PoolMaster",false);

  size_t i = nvs.putUChar("ConfigVersion",storage.ConfigVersion);
  i += nvs.putBool("Ph_RegOnOff",storage.Ph_RegulationOnOff);
  i += nvs.putBool("Orp_RegOnOff",storage.Orp_RegulationOnOff);  
  i += nvs.putBool("AutoMode",storage.AutoMode);
  i += nvs.putBool("WinterMode",storage.WinterMode);
  i += nvs.putUChar("FiltrDuration",storage.FiltrationDuration);
  i += nvs.putUChar("FiltrStart",storage.FiltrationStart);
  i += nvs.putUChar("FiltrStop",storage.FiltrationStop);
  i += nvs.putUChar("FiltrStartMin",storage.FiltrationStartMin);
  i += nvs.putUChar("FiltrStopMax",storage.FiltrationStopMax);
  i += nvs.putUChar("DelayPIDs",storage.DelayPIDs);
  i += nvs.putULong("PublishPeriod",storage.PublishPeriod);
  i += nvs.putULong("PhPIDWSize",storage.PhPIDWindowSize);
  i += nvs.putULong("OrpPIDWSize",storage.OrpPIDWindowSize);
  i += nvs.putULong("PhPIDwStart",storage.PhPIDwindowStartTime);
  i += nvs.putULong("OrpPIDwStart",storage.OrpPIDwindowStartTime);
  i += nvs.putDouble("Ph_SetPoint",storage.Ph_SetPoint);
  i += nvs.putDouble("Orp_SetPoint",storage.Orp_SetPoint);
  i += nvs.putDouble("PSI_High",storage.PSI_HighThreshold);
  i += nvs.putDouble("PSI_Med",storage.PSI_MedThreshold);
  i += nvs.putDouble("WaterTempLow",storage.WaterTempLowThreshold);
  i += nvs.putDouble("WaterTempSet",storage.WaterTemp_SetPoint);
  i += nvs.putDouble("TempExternal",storage.AirTemp);
  i += nvs.putDouble("pHCalibCoeffs0",storage.pHCalibCoeffs0);
  i += nvs.putDouble("pHCalibCoeffs1",storage.pHCalibCoeffs1);
  i += nvs.putDouble("OrpCalibCoeffs0",storage.OrpCalibCoeffs0);
  i += nvs.putDouble("OrpCalibCoeffs1",storage.OrpCalibCoeffs1);
  i += nvs.putDouble("PSICalibCoeffs0",storage.PSICalibCoeffs0);
  i += nvs.putDouble("PSICalibCoeffs1",storage.PSICalibCoeffs1);
  i += nvs.putDouble("Ph_Kp",storage.Ph_Kp);
  i += nvs.putDouble("Ph_Ki",storage.Ph_Ki);
  i += nvs.putDouble("Ph_Kd",storage.Ph_Kd);
  i += nvs.putDouble("Orp_Kp",storage.Orp_Kp);
  i += nvs.putDouble("Orp_Ki",storage.Orp_Ki);
  i += nvs.putDouble("Orp_Kd",storage.Orp_Kd);
  i += nvs.putDouble("PhPIDOutput",storage.PhPIDOutput);
  i += nvs.putDouble("OrpPIDOutput",storage.OrpPIDOutput);
  i += nvs.putDouble("TempValue",storage.WaterTemp);
  i += nvs.putDouble("PhValue",storage.PhValue);
  i += nvs.putDouble("OrpValue",storage.OrpValue);
  i += nvs.putDouble("PSIValue",storage.PSIValue);
  i += nvs.putUChar("SecureElectro",storage.SecureElectro);
  i += nvs.putUChar("DelayElectro",storage.DelayElectro);
  i += nvs.putBool("ElectroRunMode",storage.ElectroRunMode);
  i += nvs.putUChar("ElectroRunTime",storage.ElectroRuntime);
  i += nvs.putBool("ElectrolyseMode",storage.ElectrolyseMode);
  i += nvs.putBool("pHAutoMode",storage.pHAutoMode);
  i += nvs.putBool("OrpAutoMode",storage.OrpAutoMode);
  i += nvs.putBool("Lang_Locale",storage.Lang_Locale); 
  i += nvs.putUInt("MQTT_IP",storage.MQTT_IP); 
  i += nvs.putUInt("MQTT_PORT",storage.MQTT_PORT); 
  i += nvs.putString("MQTT_LOGIN",storage.MQTT_LOGIN); 
  i += nvs.putString("MQTT_PASS",storage.MQTT_PASS); 
  i += nvs.putString("MQTT_ID",storage.MQTT_ID); 
  i += nvs.putString("MQTT_TOPIC",storage.MQTT_TOPIC);
  i += nvs.putString("SMTP_SERVER",storage.SMTP_SERVER); 
  i += nvs.putUInt("SMTP_PORT",storage.SMTP_PORT); 
  i += nvs.putString("SMTP_LOGIN",storage.SMTP_LOGIN); 
  i += nvs.putString("SMTP_PASS",storage.SMTP_PASS); 
  i += nvs.putString("SMTP_SENDER",storage.SMTP_SENDER); 
  i += nvs.putString("SMTP_RECIPIENT",storage.SMTP_RECIPIENT); 
  i += nvs.putBool("BuzzerOn",storage.BuzzerOn);

  i += nvs.putBytes("PumpsConf", (byte*)(&storage.PumpsConfig), sizeof(storage.PumpsConfig));

  nvs.end();

  Debug.print(DBG_INFO,"Bytes saved: %d / %d\n",i,sizeof(storage));
  return (i == sizeof(storage)) ;

}

bool savePumpsConf()
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putBytes("PumpsConf", (byte*)(&storage.PumpsConfig), sizeof(storage.PumpsConfig));
  size_t whatsLeft = nvs.freeEntries();
  Debug.print(DBG_DEBUG,"Bytes saved: %d (freeentries %d)\n",i,whatsLeft);
  return(i == sizeof(storage.PumpsConfig));
}

// functions to save any type of parameter (4 overloads with same name but different arguments)
uint8_t loadParam(const char* key, uint8_t val)
{
  nvs.begin("PoolMaster",false);
  return nvs.getUChar(key,val);
}

bool loadParam(const char* key, bool val)
{
  nvs.begin("PoolMaster",false);
  return nvs.getBool(key,val);
}

bool saveParam(const char* key, uint8_t val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putUChar(key,val);
  size_t whatsLeft = nvs.freeEntries();
  Debug.print(DBG_DEBUG,"Bytes saved: %d / %s = %d (freeentries %d)\n",i,key, val,whatsLeft);
  return(i == sizeof(val));
}

bool saveParam(const char* key, bool val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putBool(key,val);
  size_t whatsLeft = nvs.freeEntries();
  Debug.print(DBG_DEBUG,"Bytes saved: %d / %s = %d (freeentries %d)\n",i,key, val,whatsLeft);
  return(i == sizeof(val));
}

bool saveParam(const char* key, unsigned long val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putULong(key,val);
  size_t whatsLeft = nvs.freeEntries();
  Debug.print(DBG_DEBUG,"Bytes saved: %d / %s = %lu (freeentries %d)\n",i,key, val,whatsLeft);

  return(i == sizeof(val));
}

bool saveParam(const char* key, double val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putDouble(key,val);
  size_t whatsLeft = nvs.freeEntries();
  Debug.print(DBG_DEBUG,"Bytes saved: %d / %s = %e (freeentries %d)\n",i,key, val,whatsLeft);
  return(i == sizeof(val));
}

bool saveParam(const char* key, u_int val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putUInt(key,val);
  size_t whatsLeft = nvs.freeEntries();
  Debug.print(DBG_DEBUG,"Bytes saved: %d / %s = %d (freeentries %d)\n",i,key, val,whatsLeft);
  return(i == sizeof(val));
}

bool saveParam(const char* key,char* val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putString(key,val);
  size_t whatsLeft = nvs.freeEntries();
  Debug.print(DBG_DEBUG,"Bytes saved: %d / %s = %s (freeentries %d)\n",i,key, val,whatsLeft);
  return(i == sizeof(val));
}

bool saveParam(const char* key,IPAddress val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putUInt(key,val);
  size_t whatsLeft = nvs.freeEntries();
  Debug.print(DBG_DEBUG,"Bytes saved: %d / %s = %s (freeentries %d)\n",i,key, val.toString().c_str(),whatsLeft);
  return(i == sizeof(val));
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
    //Serial.print(".");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  //Serial.println("");
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

