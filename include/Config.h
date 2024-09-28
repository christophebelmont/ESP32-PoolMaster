// Firmware revisions
#define FIRMW "ESP-3.4b"
#define TFT_FIRMW "TFT-4.0"

#define DEBUG_LEVEL DBG_INFO     // Possible levels : NONE/ERROR/WARNING/INFO/DEBUG/VERBOSE

//Version of config stored in EEPROM
//Random value. Change this value (to any other value) to revert the config to default values
#define CONFIG_VERSION 60

// WiFi credentials
#ifdef DEVT
  #define WIFI_NETWORK "<WIFI_NETWORK>"
  #define WIFI_PASSWORD "<WIFI_PASSWORD>"
#else
  #define WIFI_NETWORK "<WIFI_NETWORK>"
  #define WIFI_PASSWORD "<WIFI_PASSWORD>"
#endif 

#define OTA_PWDHASH   "<OTA_PASS>"
#ifdef DEVT
  #define HOSTNAME "PoolMaster_Dev"
#else
  #define HOSTNAME "PoolMaster"
#endif 

// Mail parameters and credentials
//#define SMTP  // define to activate SMTP email notifications
#ifdef SMTP
  #define SMTP_HOST "your smtp server"
  #define SMTP_PORT 587  // check the port number
  #define AUTHOR_EMAIL "your email address"
  #define AUTHOR_LOGIN "your user name"
  #define AUTHOR_PASSWORD "your password"
  #define RECIPIENT_EMAIL "your recipient email address"
#endif

// PID Directions (either DIRECT or REVERSE depending on Ph/Orp correction vs water properties)
#define PhPID_DIRECTION REVERSE
#define OrpPID_DIRECTION DIRECT

#define FILTRATION_PUMP 32
#define ROBOT_PUMP	    33
#define PH_PUMP         25
#define CHL_PUMP        26
#define RELAY_R0        4    // Projecteur
#define RELAY_R1        23   // Spare, not connected
#define ORP_PROD        27

//Digital input pins connected to Acid and Chl tank level reed switches
#define CHL_LEVEL       39   // not wired. Use NO_LEVEL option of Pump class
#define PH_LEVEL        36   //                - " -

//One wire bus for the air/water temperature measurement
#define ONE_WIRE_BUS_A  14
#define ONE_WIRE_BUS_W  14

//I2C bus for analog measurement with ADS1115 of pH, ORP and water pressure 
//and status LED through PCF8574A 
#define I2C_SDA			4
#define I2C_SCL			5

//Type of pH and Orp sensors acquisition :
//INT_ADS1115 : single ended signal with internal ADS1115 ADC (default)
//EXT_ADS1115 : differential signal with external ADS1115 ADC (Loulou74 board)
#define EXT_ADS1115
#define INT_ADS1115_ADDR ADS1115ADDRESS
#define EXT_ADS1115_ADDR ADS1115ADDRESS+1 // or +2 or +3 depending on board setup

// Buzzer
#define BUZZER           2

// PCF8574 model could be:
//  1 - PCF8574AN
#define PCF8574_ADDR  0x24

#define WDT_TIMEOUT     10

// Server port
#define SERVER_PORT 8060

//OTA port
#define OTA_PORT    8063

//12bits (0,06°C) temperature sensors resolution
#define TEMPERATURE_RESOLUTION 12

//MQTT stuff including local broker/server IP address, login and pwd
//------------------------------------------------------------------

//interval (in millisec) between MQTT publishement of measurement data
#define PUBLISHINTERVAL 30000

#define MQTT_SERVER_IP IPAddress(192, 168, 1, 57)
#define MQTT_SERVER_PORT 1883

// Uncomment if MQTT broker needs login/pwd
#define MQTT_LOGIN 				
//#define MQTT_SERVER_ID    "ESP32Pool"		   // MQTT server ID
#define MQTT_SERVER_LOGIN "<MQTT_LOGIN>"
#define MQTT_SERVER_PWD   "<MQTT_PASSWORD>"

#ifdef DEVT
  #define MQTT_SERVER_ID "ESP32Pool-Dev"	
  #define POOLTOPIC "Home/Pool6/"
#else
  #define MQTT_SERVER_ID "ESP32Pool"	
  #define POOLTOPIC "Home/Pool/"
#endif 

// ElegantOTA Config
#define ELEGANT_OTA
#ifdef ELEGANT_OTA
  #define ELEGANT_OTA_AUTH
  #define ELEGANT_OTA_USERNAME  "<ELEGANT_OTA_USERNAME>"
  #define ELEGANT_OTA_PASSWORD  "<ELEGANT_OTA_PASSWORD>"
#endif
// Robot pump timing
#define ROBOT_DELAY 60     // Robot start delay after filtration in mn
#define ROBOT_DURATION 90  // Robot cleaning duration

//Display timeout before blanking
//-------------------------------
#define TFT_SLEEP 60000L 

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

//Periods 
// Task9 period is initialized with PUBLISHINTERVAL and can be changed dynamically
#define PT1 125
#define PT2 500
#define PT3 500
#define PT4 1000 / (1 << (12 - TEMPERATURE_RESOLUTION))
#define PT5 1000
#define PT6 1000
#define PT7 3000
#define PT8 30000

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

//#define CHRONO                    // Activate tasks timings traces for profiling
//#define SIMU                      // Used to simulate pH/ORP sensors. Very simple simulation:
                                    // the sensor value is computed from the output of the PID 
                                    // loop to reach linearly the theorical value produced by this
                                    // output after one hour