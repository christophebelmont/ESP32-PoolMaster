// Firmware revisions
#define FIRMW "ESP-4.62"
#define TFT_FIRMW "TFT-4.0" // For compatibility

// Choose Nextion version to Compile (only one choice possible)
// NEXTION_V1 (default) for initial interface or blue theme interface
// NEXTION_V2 for Nextion v5.0 them which redefines full interface and pages
//#define NEXTION_V1
#define NEXTION_V2

#define DEBUG_LEVEL DBG_INFO     // Possible levels : NONE/ERROR/WARNING/INFO/DEBUG/VERBOSE

//Version of config stored in EEPROM
//Random value. Change this value (to any other value) to revert the config to default values
#define CONFIG_VERSION 70

// Compile on development environment or production (if not defined)
//#define DEVT // Value defined in platformio.ini

// WiFi credentials
#define WIFI_SCAN_INTERVAL  10000

#define OTA_PWDHASH   "<OTA_PASS>"
#ifdef DEVT
  #define HOSTNAME "PoolMaster_Dev"
#else
  #define HOSTNAME "PoolMaster"
#endif 

// Mail parameters and credentials
//#define SMTP  // define to activate SMTP email notifications

// PID Directions (either DIRECT or REVERSE depending on Ph/Orp correction vs water properties)
#define PhPID_DIRECTION REVERSE
#define OrpPID_DIRECTION DIRECT

// Default ports (can be changed at runtime)
#define FILTRATION      32
#define ROBOT     	    33
#define PH_PUMP         25
#define CHL_PUMP        26
#define PROJ            27  // Projecteur
#define SPARE           4
#define SWG_PUMP        13
#define FILL_PUMP       23

#define ALL_PINS        "4|13|23|25|26|27|32|33" // List of all usable PINs on the ESP32 (to be sent to Nextion)

//Digital input pins connected to Acid and Chl tank level reed switches
#define CHL_LEVEL       39   // If chlorine tank empty switch used (contact open if low level)
#define PH_LEVEL        36   // If acid tank empty switch used (contact open if low level)
#define POOL_LEVEL      34   //	If pool level switch used (contact open if low level)

//One wire bus for the air/water temperature measurement
#define ONE_WIRE_BUS_A  18
#define ONE_WIRE_BUS_W  19

//I2C bus for analog measurement with ADS1115 of pH, ORP and water pressure 
//and status LED through PCF8574A 
#define I2C_SDA			21
#define I2C_SCL			22
#define PCF8574ADDRESS  0x20 // 0x20 for PCF8574(N) or 0x38 for PCF8574A(N)

//Type of pH and Orp sensors acquisition :
//INT_ADS1115 : single ended signal with internal ADS1115 ADC (default)
//EXT_ADS1115 : differential signal with external ADS1115 ADC (Loulou74 board)
#define EXT_ADS1115
#define INT_ADS1115_ADDR ADS1115ADDRESS
#define EXT_ADS1115_ADDR ADS1115ADDRESS+1 // or +2 or +3 depending on board setup

// Buzzer
#define BUZZER           2

// Task TimeOut before reboot
#define WDT_TIMEOUT     10

// Delay when instructed to reboot
#define REBOOT_DELAY  10000

// Server port
//#define SERVER_PORT 8060

//OTA port
#define OTA_PORT    8063

//12bits (0,06°C) temperature sensors resolution
#define TEMPERATURE_RESOLUTION 12

// Time to wait for Wifi to connect. If not connected resume startup without network connection
#define WIFI_TIMEOUT  10000

//MQTT stuff including local broker/server IP address, login and pwd
//------------------------------------------------------------------

//interval (in millisec) between MQTT publishement of measurement data
// can be configured at runtime
#define PUBLISHINTERVAL 30000

#define CONFIG_NVS_NAME "MasterConfig" // NVS namespace for configuration storage

// Default values if nothing better is recorded at runtime
#define POOLTOPIC "Home/Pool/"
#define MQTTID "PoolMaster"
// ElegantOTA Config
//#define ELEGANT_OTA

#ifdef ELEGANT_OTA
  //#define ELEGANT_OTA_AUTH
  //#define ELEGANT_OTA_USERNAME  "username"
  //#define ELEGANT_OTA_PASSWORD  "password"
#endif
// Robot pump timing
#define ROBOT_DELAY 60     // Robot start delay after filtration in mn
#define ROBOT_DURATION 90  // Robot cleaning duration

#define SWG_MODE_ADJUST 0   // Adjust SWG production time according to the pool ORP
#define SWG_MODE_FIXED 1    // Fixed time for SWG production (in hours)

//Display timeout before blanking
//-------------------------------
//#define TFT_SLEEP 60000L  // Moved to Nextion Library

// Loop tasks scheduling parameters
//---------------------------------
// T1: AnalogPoll
// T2: PoolServer
// T3: PoolMaster
// T4: getTemp
// T5: OrpRegulation
// T6: pHRegulation
// T7: StatusLights
// T8: PublishMeasures
// T9: PublishSettings 
// T10: Nextion Screen Refresh On (/2 when screen on, /4 if menu page for faster refresh)
// T11: Every 2 minutes, statistics recording, MQTT and NTP reconnects

//Periods 
// Task9 period is initialized with PUBLISHINTERVAL and can be changed dynamically
#define PT1 125
#define PT2 500
#define PT3 500
#define PT4 1000 / (1 << (12 - TEMPERATURE_RESOLUTION))
#define PT5 1000
#define PT6 1000
#define PT7 3000
#define PT8 30000 //Unused, stored as config parameter and can be modified at runtime
#define PT9 1000
#define PT10 1000
#define PT11 120000 // Run once every 2 minutes


//Start offsets to spread tasks along time
// Task1 has no delay
#define DT2 190/portTICK_PERIOD_MS
#define DT3 310/portTICK_PERIOD_MS
#define DT4 440/portTICK_PERIOD_MS
#define DT5 560/portTICK_PERIOD_MS
#define DT6 920/portTICK_PERIOD_MS
#define DT7 100/portTICK_PERIOD_MS
#define DT8 570/portTICK_PERIOD_MS
#define DT9 940/portTICK_PERIOD_MS
#define DT10 50/portTICK_PERIOD_MS
#define DT11 2000/portTICK_PERIOD_MS

//#define CHRONO                    // Activate tasks timings traces for profiling
//#define SIMU                      // Used to simulate pH/ORP sensors. Very simple simulation:
                                    // the sensor value is computed from the output of the PID 
                                    // loop to reach linearly the theorical value produced by this
                                    // output after one hour
