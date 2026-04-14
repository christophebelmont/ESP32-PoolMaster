// HOME ASSISTANT ENTITIES
// ***********************
// create or delete Home-Assistant PoolMAster entities

#include <Preferences.h>
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <WiFiManager.h>
#include "SuperVisor.h"

//#define _LTOPIC_      128
//#define BUFFER_SIZE 1024
//#define LOG_BUFFER_SIZE 1024

extern AsyncMqttClient MqttClient;
extern char local_sbuf[LOG_BUFFER_SIZE];
extern void Local_Logs_Dispatch(const char *_log_message, uint8_t _targets = 7, const char* _telnet_separator = "\r\n");
extern JsonDocument PMInfo;
extern JsonDocument SVSettings;
extern void connectToMqtt();

void remove_unwanted(char* s, char rep) 
{
    char* d = s;
    do {
      while (*d == rep) ++d;
    } while (*s++ = *d++);
}

void replacespacein(char *s, const char pattern[2], char rep)
{
  char *p = strstr(s, pattern);
  while (p) {
    char *q = strchr(p, ' ');
    if (q) *q = rep;
    p = strstr(s, pattern);
  }
}

void cleanJson(char* js)
{
  #define _DUMMYCHAR_ 127
  replacespacein(js, "  ",  _DUMMYCHAR_);
  replacespacein(js, " \"", _DUMMYCHAR_);
  replacespacein(js, ": ",  _DUMMYCHAR_);
  replacespacein(js, ", ",  _DUMMYCHAR_);
  replacespacein(js, "{ ",  _DUMMYCHAR_);
  replacespacein(js, " }",  _DUMMYCHAR_);
  remove_unwanted(js,       _DUMMYCHAR_);
}

void cleanHAEntities()
{
  const char *hatopic;
  hatopic   = SVSettings["HOME Assistant"];

  if (!hatopic) return;
  if (strcmp(hatopic, "")==0) return;
  if (strcmp(hatopic, "none")==0) return;

  connectToMqtt();
  if (!MqttClient.connected()) return;

  char hadiscoverytopic[_LTOPIC_];
  char type[][20] = {
    "sensor",
    "binary_sensor",
    "select",
    "switch",
    "button",
    "number",
    "text",
  };
  for (int i=0; i<(sizeof(type)/sizeof(type[0])); i++) {
    sprintf(hadiscoverytopic, "%s/%s/PoolMaster", hatopic, type[i]);
    MqttClient.publish(hadiscoverytopic, 0, true);
    delay(100);
  }
}

void createHAEntitie(char* type, char *name, char* topic, char *js, char *JScommon, const char* hatopic, const char* commandtopic = NULL)
{
  char Payload[820]; // max size payload seen is 756
  char hadiscoverytopic[_LTOPIC_];
  //static int maxpayload = 0;
  // remove maximum of spaces and unnecessary char, reaching stack limit
  cleanJson(js);

  if (commandtopic == NULL)
       sprintf(Payload, "{\"name\":\"%s\",\"state_topic\":\"%s\",\"json_attributes_topic\":\"%s\",%s%s}", name, topic, topic, js, JScommon);
  else sprintf(Payload, "{\"name\":\"%s\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"qos\":1,%s%s}", name, topic, commandtopic, js, JScommon);
  Payload[sizeof(Payload)-1] = 0;

  //if (strlen(Payload) > maxpayload) maxpayload = strlen(Payload);
  remove_unwanted(name, ' '); // home assistant can't deal with those in keyname
  remove_unwanted(name, '/');
  remove_unwanted(name, '\\');
  remove_unwanted(name, ':');
  remove_unwanted(name, '(');
  remove_unwanted(name, ')');
  sprintf(hadiscoverytopic, "%s/%s/PoolMaster/%s/config", hatopic, type, name);

  if (MqttClient.publish(hadiscoverytopic, 1, true, Payload, strlen(Payload)) == 0) {
      snprintf(local_sbuf,sizeof(local_sbuf),"createHAEntitie, error publish MQTT %s Payload: %s - Payload size: %d", hadiscoverytopic, Payload, strlen(Payload));
      Local_Logs_Dispatch(local_sbuf);
  }
  delay(200); // without delay, entries are missing in MQTT (too fast)
/*
  static int maxlen=0;
  if (maxlen < strlen(Payload)) maxlen = strlen(Payload);
  snprintf(local_sbuf,sizeof(local_sbuf),"createHAEntitie, publish MQTT %s - Payload maxsize: %d", hadiscoverytopic, maxlen);
  Local_Logs_Dispatch(local_sbuf);*/
}

void createHAEntities()
{
  const char* roottopic,*hatopic;
  roottopic = SVSettings["MQTT Topic"];
  hatopic   = SVSettings["HOME Assistant"];

  if (!roottopic) return;
  if (!hatopic) return;
  if (strcmp(hatopic, "")==0) return;
  if (strcmp(hatopic, "none")==0) return;

  connectToMqtt();
  if (!MqttClient.connected()) return;

  char topic[_LTOPIC_];
  char commandtopic[_LTOPIC_];
  char entitytype[20];
  char JScommon[400];

  // Create Home Assistant Entities for automatic discovery
  sprintf(JScommon, "\"availability\": { \
    \"topic\": \"PoolMaster/Status\", \
    \"value_template\": \"{{ value_json['PoolMaster Online'] }}\", \
    \"payload_available\": \"1\", \
    \"payload_not_available\": \"0\" \
  }, \
  \"device\": { \
    \"name\": \"PoolMaster\", \
    \"model\": \"PoolMaster ESP32\", \
    \"manufacturer\": \"Collectif\", \
    \"identifiers\": \"PoolMaster\", \
    \"configuration_url\": \"http://%s\" }", WiFi.localIP().toString().c_str());
  
  // remove maximum of spaces and unnecessary char, reaching stack limit
  cleanJson(JScommon);

  // Creation of Sensors
  // *******************
  strcpy(entitytype, "sensor");

  {char name[] = "IP PoolMaster";
   sprintf(topic, "%s/PoolMaster", roottopic);
   char JSEntity[] = "\
      \"value_template\": \"{{ value_json['IP Address'] }}\", \
      \"unique_id\": \"poolmaster_ip\", \
      \"icon\": \"mdi:ip-network-outline\", \
      \"entity_category\": \"diagnostic\", ";
   createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

  {char name[] = "MAC PoolMaster";
   sprintf(topic, "%s/PoolMaster", roottopic);
   char JSEntity[] = "\
      \"value_template\": \"{{ value_json['MAC Address'] }}\", \
      \"unique_id\": \"poolmaster_mac\", \
      \"icon\": \"mdi:network-pos\", \
      \"entity_category\": \"diagnostic\", ";
   createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

   {char name[] = "Hostname PoolMaster";
   sprintf(topic, "%s/PoolMaster", roottopic);
   char JSEntity[] = "\
      \"value_template\": \"{{ value_json['Hostname'] }}\", \
      \"unique_id\": \"poolmaster_hostname\", \
      \"icon\": \"mdi:network-outline\", \
      \"entity_category\": \"diagnostic\", ";
   createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

   {char name[] = "Uptime PoolMaster";
   sprintf(topic, "%s/PoolMaster", roottopic);
   char JSEntity[] = "\
      \"value_template\": \"{{ value_json['Uptime'] }}\", \
      \"unique_id\": \"poolmaster_uptime\", \
      \"icon\": \"mdi:timeline-clock-outline\", \
      \"entity_category\": \"diagnostic\", ";
   createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

   {char name[] = "IP SuperVisor";
   sprintf(topic, "%s/SuperVisor", roottopic);
   char JSEntity[] = "\
      \"value_template\": \"{{ value_json['IP Address'] }}\", \
      \"unique_id\": \"supervisor_ip\", \
      \"icon\": \"mdi:ip-network-outline\", \
      \"entity_category\": \"diagnostic\", ";
   createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

  {char name[] = "MAC SuperVisor";
   sprintf(topic, "%s/SuperVisor", roottopic);
   char JSEntity[] = "\
      \"value_template\": \"{{ value_json['MAC Address'] }}\", \
      \"unique_id\": \"supervisor_mac\", \
      \"icon\": \"mdi:network-pos\", \
      \"entity_category\": \"diagnostic\", ";
   createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

   {char name[] = "Hostname SuperVisor";
   sprintf(topic, "%s/SuperVisor", roottopic);
   char JSEntity[] = "\
      \"value_template\": \"{{ value_json['Hostname'] }}\", \
      \"unique_id\": \"supervisor_hostname\", \
      \"icon\": \"mdi:network-outline\", \
      \"entity_category\": \"diagnostic\", ";
   createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

   {char name[] = "Uptime SuperVisor";
   sprintf(topic, "%s/SuperVisor", roottopic);
   char JSEntity[] = "\
      \"value_template\": \"{{ value_json['Uptime'] }}\", \
      \"unique_id\": \"supervisor_uptime\", \
      \"icon\": \"mdi:timeline-clock-outline\", \
      \"entity_category\": \"diagnostic\", ";
   createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}
  {
    char name[] = "Firmware PoolMaster";
    sprintf(topic, "%s/PoolMaster", roottopic);
    char JSEntity[] = "\
      \"value_template\": \"{{ value_json['Firmware'] }}\", \
      \"unique_id\": \"poolmaster_firmware\", \
      \"icon\": \"mdi:barcode\", \
      \"entity_category\": \"diagnostic\", ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
    char name[] = "Firmware Nextion";
    sprintf(topic, "%s/PoolMaster", roottopic);
    char JSEntity[] = " \
      \"value_template\": \"{{ value_json['Display Firmware'] }}\", \
      \"unique_id\": \"display_firmware\", \
      \"icon\": \"mdi:barcode\", \
      \"entity_category\": \"diagnostic\", ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
    char name[] = "Firmware SuperVisor";
    sprintf(topic, "%s/SuperVisor", roottopic);
    char JSEntity[] = " \
      \"value_template\": \"{{ value_json['Firmware'] }}\", \
      \"unique_id\": \"supervisor_firmware\", \
      \"icon\": \"mdi:barcode\", \
      \"entity_category\": \"diagnostic\", ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}
  {char name[] = "Presence Detected";
   sprintf(topic, "%s/SuperVisor", roottopic);
   char JSEntity[] = "\
      \"value_template\": \"{{ value_json['Presence Detected'] }}\", \
      \"unique_id\": \"presence_detected\", \
      \"icon\": \"mdi:motion-sensor\", ";
   createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}
  {
    char name[] = "Set1";
    sprintf(topic, "%s/Set1", roottopic);
    char JSEntity[] = " \
      \"value_template\": \"{{ value_json['FW'] }}\", \
      \"unique_id\": \"poolmaster_set1\", \
      \"icon\": \"mdi:cog-play\", \
      \"entity_category\": \"diagnostic\", ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
    char name[] = "Set2";
    sprintf(topic, "%s/Set2", roottopic);
    char JSEntity[] = " \
      \"value_template\": \"{{ value_json['pHWS'] }}\", \
      \"unique_id\": \"poolmaster_set2\", \
      \"icon\": \"mdi:cog-play\", \
      \"entity_category\": \"diagnostic\", ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
    char name[] = "Set3";
    sprintf(topic, "%s/Set3", roottopic);
    char JSEntity[] = " \
      \"value_template\": \"{{ value_json['pHC0'] }}\", \
      \"unique_id\": \"poolmaster_set3\", \
      \"icon\": \"mdi:cog-play\", \
      \"entity_category\": \"diagnostic\", ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
    char name[] = "Set4";
    sprintf(topic, "%s/Set4", roottopic);
    char JSEntity[] = " \
      \"value_template\": \"{{ value_json['pHKp'] }}\", \
      \"unique_id\": \"poolmaster_set4\", \
      \"icon\": \"mdi:cog-play\", \
      \"entity_category\": \"diagnostic\", ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
    char name[] = "Set5";
    sprintf(topic, "%s/Set5", roottopic);
    char JSEntity[] = " \
      \"value_template\": \"{{ value_json['pHTV'] }}\", \
      \"unique_id\": \"poolmaster_set5\", \
      \"icon\": \"mdi:cog-play\", \
      \"entity_category\": \"diagnostic\", ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
    char name[] = "pH Sensor";
    sprintf(topic, "%s/Meas1", roottopic);
    char JSEntity[] = " \
      \"value_template\": \"{{ value_json.pH | float | multiply(0.01) | round(2) }}\", \
      \"unique_id\": \"poolmaster_meas_ph\", \
      \"device_class\": \"ph\", \
      \"icon\": \"mdi:ph\", \
      \"state_class\": \"measurement\", \
      \"suggested_display_precision\": \"2\", \
      \"expire_after\": \"240\", \
      ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
    char name[] = "Orp/Redox Sensor";
    sprintf(topic, "%s/Meas1", roottopic);
    char JSEntity[] = " \
        \"value_template\": \"{{ value_json.Orp | round(0) }}\", \
        \"device_class\": \"voltage\", \
        \"suggested_display_precision\": \"0\", \
        \"unique_id\": \"poolmaster_meas_orp\", \
        \"state_class\": \"measurement\", \
        \"unit_of_measurement\": \"mV\", \
        \"icon\": \"mdi:water-check\", \
        \"expire_after\": \"240\", \
        ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
    char name[] = "Pump Pressure";
    sprintf(topic, "%s/Meas1", roottopic);
    char JSEntity[] = " \
        \"value_template\": \"{{ value_json.PSI | float | multiply(0.01) | round(2) | abs }}\", \
        \"device_class\": \"pressure\", \
        \"suggested_display_precision\": \"2\", \
        \"unique_id\": \"poolmaster_meas_psi\", \
        \"state_class\": \"measurement\", \
        \"unit_of_measurement\": \"bar\", \
        \"icon\": \"mdi:gauge-low\", \
        \"expire_after\": \"240\", \
        ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }

  {
  char name[] = "Air Temperature";
  sprintf(topic, "%s/Meas1", roottopic);
    char JSEntity[] = " \
        \"value_template\": \"{{ value_json.TE | float | multiply(0.01) | round(2) }}\", \
        \"device_class\": \"temperature\", \
        \"suggested_display_precision\": \"2\", \
        \"unique_id\": \"poolmaster_meas_TE\", \
        \"state_class\": \"measurement\", \
        \"unit_of_measurement\": \"°C\", \
        \"icon\": \"mdi:sun-thermometer\", \
        \"expire_after\": \"240\", \
        ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
    }
  {
  char name[] = "Water Temperature";
  sprintf(topic, "%s/Meas1", roottopic);
    char JSEntity[] = " \
        \"value_template\": \"{{ value_json.Tmp | float | multiply(0.01) | round(2) }}\", \
        \"device_class\": \"temperature\", \
        \"suggested_display_precision\": \"2\", \
        \"unique_id\": \"poolmaster_meas_Tmp\", \
        \"state_class\": \"measurement\", \
        \"unit_of_measurement\": \"°C\", \
        \"icon\": \"mdi:thermometer-water\", \
        \"expire_after\": \"240\", \
        ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "pH Tank Level";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.AcidF | float | round(2) }}\", \
    \"suggested_display_precision\": \"2\", \
    \"unique_id\": \"poolmaster_meas_phtank\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"%\", \
    \"icon\": \"mdi:hydraulic-oil-level\", \
    \"expire_after\": \"240\", \
    ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "pH Pump Uptime";
  sprintf(topic, "%s/Meas1", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.PhUpT | int }}\", \
    \"suggested_display_precision\": \"0\", \
    \"unique_id\": \"poolmaster_meas_phuptime\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"s\", \
    \"icon\": \"mdi:progress-clock\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "SWG Pump Uptime";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.SWGUpT / 60 | int }}\", \
    \"suggested_display_precision\": \"0\", \
    \"unique_id\": \"poolmaster_meas_swguptime\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"mn\", \
    \"icon\": \"mdi:progress-clock\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Filtration Pump Uptime";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.FIUpT / 60 | int }}\", \
    \"suggested_display_precision\": \"0\", \
    \"unique_id\": \"poolmaster_meas_filtuptime\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"mn\", \
    \"icon\": \"mdi:progress-clock\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Filling Pump Uptime";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.FPUpT / 60 | int }}\", \
    \"suggested_display_precision\": \"0\", \
    \"unique_id\": \"poolmaster_meas_fillpuptime\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"mn\", \
    \"icon\": \"mdi:progress-clock\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Chl Tank Level";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.ChlF | float | round(2) }}\", \
    \"suggested_display_precision\": \"2\", \
    \"unique_id\": \"poolmaster_meas_Chltank\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"%\", \
    \"icon\": \"mdi:hydraulic-oil-level\", \
    \"expire_after\": \"240\", \
    ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Chl Pump Uptime";
  sprintf(topic, "%s/Meas1", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.ChlUpT | int }}\", \
    \"suggested_display_precision\": \"0\", \
    \"unique_id\": \"poolmaster_meas_chluptime\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"s\", \
    \"icon\": \"mdi:progress-clock\", \
    \"expire_after\": \"240\", \
    ";
    createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Filtration Start";
  sprintf(topic, "%s/Set1", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.FSta | int }}\", \
    \"suggested_display_precision\": \"0\", \
    \"unique_id\": \"poolmaster_meas_fsta\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"h\", \
    \"icon\": \"mdi:clock-start\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Filtration Stop";
  sprintf(topic, "%s/Set1", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.FSto | int }}\", \
    \"suggested_display_precision\": \"0\", \
    \"unique_id\": \"poolmaster_meas_fsto\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"h\", \
    \"icon\": \"mdi:clock-end\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Filtration Duration";
  sprintf(topic, "%s/Set1", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.FDu | int }}\", \
    \"suggested_display_precision\": \"0\", \
    \"unique_id\": \"poolmaster_meas_fdu\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"h\", \
    \"icon\": \"mdi:progress-clock\", \
     ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
 {char name[] = "BME680 Temperature";
  sprintf(topic, "%s/BME680", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json['Temperature'] }}\", \
    \"unique_id\": \"bme680_temperature\", \
    \"device_class\": \"temperature\", \
    \"suggested_display_precision\": \"2\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"°C\", \
    \"icon\": \"mdi:thermometer\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

  {char name[] = "BME680 Humidity";
  sprintf(topic, "%s/BME680", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json['Humidity'] }}\", \
    \"unique_id\": \"bme680_humidity\", \
    \"device_class\": \"moisture\", \
    \"suggested_display_precision\": \"0\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"%\", \
    \"icon\": \"mdi:water-percent\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

  {char name[] = "BME680 Pressure";
  sprintf(topic, "%s/BME680", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json['Pressure'] }}\", \
    \"unique_id\": \"bme680_pressure\", \
    \"device_class\": \"pressure\", \
    \"suggested_display_precision\": \"0\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"mbar\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

  {char name[] = "BME680 Gaz";
  sprintf(topic, "%s/BME680", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json['Gaz'] }}\", \
    \"unique_id\": \"bme680_gaz\", \
    \"device_class\": \"gas\", \
    \"state_class\": \"measurement\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

{char name[] = "BMP280 Temperature";
  sprintf(topic, "%s/BMP280", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json['Temperature'] }}\", \
    \"unique_id\": \"bmp280_temperature\", \
    \"device_class\": \"temperature\", \
    \"suggested_display_precision\": \"2\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"°C\", \
    \"icon\": \"mdi:thermometer\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

  {char name[] = "BMP280 Pressure";
  sprintf(topic, "%s/BMP280", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json['Pressure'] }}\", \
    \"unique_id\": \"bmp280_pressure\", \
    \"device_class\": \"pressure\", \
    \"suggested_display_precision\": \"0\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"mbar\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

{char name[] = "SHT40 Temperature";
  sprintf(topic, "%s/SHT40", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json['Temperature'] }}\", \
    \"unique_id\": \"sht40_temperature\", \
    \"device_class\": \"temperature\", \
    \"suggested_display_precision\": \"2\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"°C\", \
    \"icon\": \"mdi:thermometer\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

  {char name[] = "SHT40 Humidity";
  sprintf(topic, "%s/SHT40", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json['Humidity'] }}\", \
    \"unique_id\": \"sht40_humidity\", \
    \"device_class\": \"moisture\", \
    \"suggested_display_precision\": \"0\", \
    \"state_class\": \"measurement\", \
    \"unit_of_measurement\": \"%\", \
    \"icon\": \"mdi:water-percent\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

  {char name[] = "Water Meter";
  sprintf(topic, "%s/WaterMeter", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json['L'] }}\", \
    \"unique_id\": \"watermeter\", \
    \"device_class\": \"water\", \
    \"suggested_display_precision\": \"0\", \
    \"state_class\": \"total_increasing\", \
    \"unit_of_measurement\": \"L\", \
    \"icon\": \"mdi:counter\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);}

// Binary Sensor
// *************
  strcpy(entitytype, "binary_sensor");
  {
  char name[] = "pH Level Status";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.IO | int | bitwise_and(16) == 16 }}\", \
    \"payload_on\": \"False\", \
    \"payload_off\": \"True\", \
    \"unique_id\": \"poolmaster_meas_phlevelerror\", \
    \"icon\": \"mdi:water-alert\", \
    \"device_class\": \"problem\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Chl Level Status";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.IO | int | bitwise_and(8) == 8 }}\", \
    \"payload_on\": \"False\", \
    \"payload_off\": \"True\", \
    \"unique_id\": \"poolmaster_meas_chllevelerror\", \
    \"icon\": \"mdi:water-alert\", \
    \"device_class\": \"problem\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "pH Uptime Status";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.IO | int | bitwise_and(2) == 2 }}\", \
    \"payload_on\": \"True\", \
    \"payload_off\": \"False\", \
    \"unique_id\": \"poolmaster_meas_phuptimeerror\", \
    \"icon\": \"mdi:water-alert\", \
    \"device_class\": \"problem\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Chl Uptime Status";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.IO | int | bitwise_and(1) == 1 }}\", \
    \"payload_on\": \"True\", \
    \"payload_off\": \"False\", \
    \"unique_id\": \"poolmaster_meas_chluptimeerror\", \
    \"icon\": \"mdi:water-alert\", \
    \"device_class\": \"problem\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "SWG Uptime Status";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.IO3 | int | bitwise_and(128) == 128 }}\", \
    \"payload_on\": \"True\", \
    \"payload_off\": \"False\", \
    \"unique_id\": \"poolmaster_meas_swguptimeerror\", \
    \"icon\": \"mdi:water-alert\", \
    \"device_class\": \"problem\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "PSI Status";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.IO | int | bitwise_and(4) == 4 }}\", \
    \"payload_on\": \"True\", \
    \"payload_off\": \"False\", \
    \"unique_id\": \"poolmaster_meas_psierror\", \
    \"icon\": \"mdi:water-alert\", \
    \"device_class\": \"problem\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Water Level Status";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.IO2 | int | bitwise_and(1) == 1 }}\", \
    \"payload_on\": \"False\", \
    \"payload_off\": \"True\", \
    \"unique_id\": \"poolmaster_meas_wlevelerror\", \
    \"icon\": \"mdi:wave-arrow-down\", \
    \"device_class\": \"problem\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }
  {
  char name[] = "Fill Pump Uptime Status";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"value_template\": \"{{ value_json.IO3 | int | bitwise_and(32) == 32 }}\", \
    \"payload_on\": \"True\", \
    \"payload_off\": \"False\", \
    \"unique_id\": \"poolmaster_meas_filluptimeerror\", \
    \"icon\": \"mdi:water-alert\", \
    \"device_class\": \"problem\", \
    \"expire_after\": \"240\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, NULL);
  }

  
  // SELECT
  // ********
  strcpy(entitytype, "select");

  {char name[] = "SWH Run Mode";
  sprintf(topic, "%s/Meas1", roottopic);
  sprintf(commandtopic, "%s/API", roottopic);
  char JSEntity[] = " \
    \"unique_id\": \"poolmaster_meas_swgrunmode\", \
    \"icon\": \"mdi:water-sync\", \
    \"entity_category\": \"config\", \
    \"optimistic\": \"false\", \
    \"command_template\": \"{% set values = { 'Timed':0, 'Auto':1} %}{ 'ElectroRunMode': {{ values[value] }}}\", \
    \"value_template\": \"{% set values = { 0:'Timed', 1:'Auto' } %} {{ values[value_json.IO4 | int | bitwise_and(1) == 1] }}\", \
    \"options\": [\"Timed\", \"Auto\"], \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "TFA Venice GPIO";
  sprintf(topic, "%s/TFA_Venice", roottopic);
  sprintf(commandtopic, "%s/SVAPI", roottopic);
  char JSEntity[] = " \
    \"unique_id\": \"poolmaster_tfavenice\", \
    \"icon\": \"mdi:car-brake-temperature\", \
    \"entity_category\": \"config\", \
    \"optimistic\": \"false\", \
    \"command_template\": \"{% set values = { 'Off':'0', '5':'5'} %} { 'TFA_Venice': '{{ values[value] }}' }\", \
    \"value_template\":   \"{% set values = { 0:'Off', 5:'5'} %} {{ values[value_json.GPIO | int] }}\", \
    \"options\": [\"Off\", \"5\"], \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
  {char name[] = "WaterMeter GPIO";
  sprintf(topic, "%s/WaterMeter", roottopic);
  sprintf(commandtopic, "%s/SVAPI", roottopic);
  char JSEntity[] = " \
    \"unique_id\": \"poolmaster_watermeter_gpio\", \
    \"icon\": \"mdi:water\", \
    \"entity_category\": \"config\", \
    \"optimistic\": \"false\", \
    \"command_template\": \"{% set values = { 'Off':'0', '15':'15'} %} { 'WaterMeter GPIO': '{{ values[value] }}' }\", \
    \"value_template\":   \"{% set values = { 0:'Off', 15:'15'} %} {{ values[value_json.GPIO | int] }}\", \
    \"options\": [\"Off\", \"15\"], \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}


  // Switch
  // ******
  strcpy(entitytype, "switch");
  sprintf(commandtopic, "%s/API", roottopic);

  {char name[] = "Ph Pump";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"payload_on\": \"{PhPump: 1}\", \
    \"payload_off\": \"{PhPump: 0}\", \
    \"value_template\": \"{{ value_json.IO | int | bitwise_and(64) == 64 }}\", \
    \"state_on\": \"True\", \
    \"state_off\": \"False\", \
    \"unique_id\": \"poolmaster_meas_phpump\", \
    \"icon\": \"mdi:water-sync\", \
    \"optimistic\": \"false\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Chl Pump";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
    \"payload_on\": \"{ChlPump: 1}\", \
    \"payload_off\": \"{ChlPump: 0}\", \
    \"value_template\": \"{{ value_json.IO | int | bitwise_and(32) == 32 }}\", \
    \"state_on\": \"True\", \
    \"state_off\": \"False\", \
    \"unique_id\": \"poolmaster_meas_chlpump\", \
    \"icon\": \"mdi:water-sync\", \
    \"optimistic\": \"false\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Filtration";
   sprintf(topic, "%s/Meas2", roottopic);
   char JSEntity[] = " \
    \"payload_on\": \"{FiltPump: 1}\", \
    \"payload_off\": \"{FiltPump: 0}\", \
    \"value_template\": \"{{ value_json.IO | int | bitwise_and(128) == 128 }}\", \
    \"state_on\": \"True\", \
    \"state_off\": \"False\", \
    \"unique_id\": \"poolmaster_meas_io_pump\", \
    \"icon\": \"mdi:pump\", \
    \"optimistic\": \"false\", \
    ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

 {char name[] = "SWG";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{Electrolyse: 1}\", \
  \"payload_off\": \"{Electrolyse: 0}\", \
  \"value_template\": \"{{ value_json.IO3 | int | bitwise_and(4) == 4 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io3_swg\", \
  \"icon\": \"mdi:flash\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Pool Fill Pump";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{FillPump: 1}\", \
  \"payload_off\": \"{FillPump: 0}\", \
  \"value_template\": \"{{ value_json.IO3 | int | bitwise_and(16) == 16 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io3_fill_pump\", \
  \"icon\": \"mdi:waves-arrow-up\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "SWG Mode (Auto/Manual)";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{ElectrolyseMode: 1}\", \
  \"payload_off\": \"{ElectrolyseMode: 0}\", \
  \"value_template\": \"{{ value_json.IO3 | int | bitwise_and(8) == 8 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io3_swgmode\", \
  \"icon\": \"mdi:flash-auto\", \
  \"entity_category\": \"config\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Filtration Mode (Auto/Manual)";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{Mode: 1}\", \
  \"payload_off\": \"{Mode: 0}\", \
  \"value_template\": \"{{ value_json.IO2 | int | bitwise_and(32) == 32 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io2_filtermode\", \
  \"icon\": \"mdi:auto-mode\", \
  \"optimistic\": \"false\", \
  \"entity_category\": \"config\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Winter Mode";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{Winter: 1}\", \
  \"payload_off\": \"{Winter: 0}\", \
  \"value_template\": \"{{ value_json.IO2 | int | bitwise_and(2) == 2 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io2_wintermode\", \
  \"icon\": \"mdi:snowflake-alert\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "pH PID";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{PhPID: 1}\", \
  \"payload_off\": \"{PhPID: 0}\", \
  \"value_template\": \"{{ value_json.IO2 | int | bitwise_and(128) == 128 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io2_phpid\", \
  \"icon\": \"mdi:chart-bell-curve-cumulative\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Orp PID";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{OrpPID: 1}\", \
  \"payload_off\": \"{OrpPID: 0}\", \
  \"value_template\": \"{{ value_json.IO2 | int | bitwise_and(64) == 64 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io2_orpid\", \
  \"icon\": \"mdi:chart-bell-curve-cumulative\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "pH Auto Mode";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{PhAutoMode: 1}\", \
  \"payload_off\": \"{PhAutoMode: 0}\", \
  \"value_template\": \"{{ value_json.IO3 | int | bitwise_and(2) == 2 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io2_phautomode\", \
  \"icon\": \"mdi:chart-bell-curve-cumulative\", \
  \"entity_category\": \"config\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Orp Auto Mode";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{OrpAutoMode: 1}\", \
  \"payload_off\": \"{OrpAutoMode: 0}\", \
  \"value_template\": \"{{ value_json.IO3 | int | bitwise_and(1) == 1 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io2_orpautomode\", \
  \"icon\": \"mdi:chart-bell-curve-cumulative\", \
  \"entity_category\": \"config\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Buzzer";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{Buzzer: 1}\", \
  \"payload_off\": \"{Buzzer: 0}\", \
  \"value_template\": \"{{ value_json.IO3 | int | bitwise_and(64) == 64 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io3_buzzer\", \
  \"icon\": \"mdi:volume-high\", \
  \"entity_category\": \"config\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Robot";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{RobotPump: 1}\", \
  \"payload_off\": \"{RobotPump: 0}\", \
  \"value_template\": \"{{ value_json.IO2 | int | bitwise_and(16) == 16 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io2_robotpump\", \
  \"icon\": \"mdi:vacuum\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Relay 0 (Lights)";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{Relay:[0,1]}\", \
  \"payload_off\": \"{Relay:[0,0]}\", \
  \"value_template\": \"{{ value_json.IO2 | int | bitwise_and(8) == 8 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io2_relay0\", \
  \"icon\": \"mdi:spotlight-beam\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Relay 1";
  sprintf(topic, "%s/Meas2", roottopic);
  char JSEntity[] = " \
  \"payload_on\": \"{Relay:[1,1]}\", \
  \"payload_off\": \"{Relay:[1,0]}\", \
  \"value_template\": \"{{ value_json.IO2 | int | bitwise_and(4) == 4 }}\", \
  \"state_on\": \"True\", \
  \"state_off\": \"False\", \
  \"unique_id\": \"poolmaster_meas_io2_relay1\", \
  \"icon\": \"mdi:toggle-switch-off-outline\", \
  \"optimistic\": \"false\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

 // BUTTONS
 // ***********
  strcpy(entitytype, "button");
  strcpy(topic, "");
  sprintf(commandtopic, "%s/API", roottopic);

  {char name[] = "Reboot";
  char JSEntity[] = " \
  \"payload_press\": \"{Reboot:1}\", \
  \"unique_id\": \"poolmaster_meas_reboot\", \
  \"icon\": \"mdi:restart\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Publish Settings";
  char JSEntity[] = " \
  \"payload_press\": \"{Settings:1}\", \
  \"unique_id\": \"poolmaster_publish_settings\", \
  \"icon\": \"mdi:publish\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Clear Errors";
  char JSEntity[] = " \
  \"payload_press\": \"{Clear:1}\", \
  \"unique_id\": \"poolmaster_meas_clear\", \
  \"icon\": \"mdi:eraser\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Reset pH Probe Calibration";
  char JSEntity[] = " \
  \"payload_press\": \"{RstpHCal:1}\", \
  \"unique_id\": \"poolmaster_meas_resetph\", \
  \"icon\": \"mdi:eraser\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Reset Orp Probe Calibration";
  char JSEntity[] = " \
  \"payload_press\": \"{RstOrpCal:1}\", \
  \"unique_id\": \"poolmaster_meas_resetorp\", \
  \"icon\": \"mdi:eraser\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Reset Pressure Probe Calibration";
  char JSEntity[] = " \
  \"payload_press\": \"{RstPSICal:1}\", \
  \"unique_id\": \"poolmaster_meas_resetpsi\", \
  \"icon\": \"mdi:eraser\", \
  \"entity_category\": \"config\", \
  ";
 createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Fill Chlorine Tank";
  char JSEntity[] = " \
  \"payload_press\": \"{ChlTank:[20,100]}\", \
  \"unique_id\": \"poolmaster_meas_refillchltank\", \
  \"icon\": \"mdi:format-color-fill\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Fill pH Tank";
  char JSEntity[] = " \
  \"payload_press\": \"{pHTank:[20,100]}\", \
  \"unique_id\": \"poolmaster_meas_refillphtank\", \
  \"icon\": \"mdi:format-color-fill\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  sprintf(commandtopic, "%s/SVAPI", roottopic);
  {char name[] = "Update PoolMaster";
  char JSEntity[] = " \
  \"payload_press\": \"{ 'Update PoolMaster':'1' }\", \
  \"unique_id\": \"poolmaster_update_poolmaster\", \
  \"icon\": \"mdi:server-network-outline\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Update SuperVisor";
  char JSEntity[] = " \
  \"payload_press\": \"{ 'Update SuperVisor':'1' }\", \
  \"unique_id\": \"poolmaster_update_supervisor\", \
  \"icon\": \"mdi:server-network-outline\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Update Nextion";
  char JSEntity[] = " \
  \"payload_press\": \"{ 'Update Nextion':'1' }\", \
  \"unique_id\": \"poolmaster_update_nextion\", \
  \"icon\": \"mdi:server-network-outline\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Reboot PoolMaster";
  char JSEntity[] = " \
  \"payload_press\": \"{ 'Reboot PoolMaster':'1' }\", \
  \"unique_id\": \"poolmaster_reboot_poolmaster\", \
  \"icon\": \"mdi:restart\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Reboot SuperVisor";
  char JSEntity[] = " \
  \"payload_press\": \"{ 'Reboot SuperVisor':'1' }\", \
  \"unique_id\": \"poolmaster_reboot_supervisor\", \
  \"icon\": \"mdi:restart\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Reboot Nextion";
  char JSEntity[] = " \
  \"payload_press\": \"{ 'Reboot Nextion':'1' }\", \
  \"unique_id\": \"poolmaster_reboot_nextion\", \
  \"icon\": \"mdi:restart\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

 // NUMBERS
 // ***********
 strcpy(entitytype, "number");
 sprintf(commandtopic, "%s/API", roottopic);
  
 {char name[] = "Filtration Start Time (min)";
  sprintf(topic, "%s/Set1", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ FiltT0: {{value}} }\", \
  \"min\": \"0\", \
  \"max\": \"24\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.FStaM | int }}\", \
  \"unique_id\": \"poolmaster_set_min_filtration_hour\", \
  \"unit_of_measurement\": \"h\", \
  \"icon\": \"mdi:clock-minus\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Filtration End Time (max)";
  sprintf(topic, "%s/Set1", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ FiltT1: {{value}} }\", \
  \"min\": \"0\", \
  \"max\": \"24\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.FStoM | int }}\", \
  \"unique_id\": \"poolmaster_set_max_filtration_hour\", \
  \"unit_of_measurement\": \"h\", \
  \"icon\": \"mdi:clock-plus\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "pH Pump Uptime Limit (mn)";
  sprintf(topic, "%s/Set1", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PumpMaxUp: [1,{{value}}] }\", \
  \"min\": \"0\", \
  \"max\": \"60\", \
  \"mode\": \"box\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.pHUTL | int }}\", \
  \"unique_id\": \"poolmaster_set_max_ph_pump_uptime\", \
  \"unit_of_measurement\": \"mn\", \
  \"icon\": \"mdi:clock-end\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Chlorine Pump Uptime Limit (mn)";
  sprintf(topic, "%s/Set1", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PumpMaxUp: [2,{{value}}] }\", \
  \"min\": \"0\", \
  \"max\": \"60\", \
  \"mode\": \"box\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.ChlUTL | int }}\", \
  \"unique_id\": \"poolmaster_set_max_chl_pump_uptime\", \
  \"unit_of_measurement\": \"mn\", \
  \"icon\": \"mdi:clock-plus\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Pool Fill Pump Max Uptime (mn)";
  sprintf(topic, "%s/Set1", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PumpMaxUp: [5,{{value}}] }\", \
  \"min\": \"0\", \
  \"max\": \"60\", \
  \"mode\": \"box\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.FMaUT | int }}\", \
  \"unique_id\": \"poolmaster_set_max_fill_pump_uptime\", \
  \"unit_of_measurement\": \"mn\", \
  \"icon\": \"mdi:clock-plus\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Pool Fill Pump Min Uptime (mn)";
  sprintf(topic, "%s/Set1", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ FillMinUpTime: {{value}} }\", \
  \"min\": \"0\", \
  \"max\": \"60\", \
  \"mode\": \"box\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.FMiUT | int }}\", \
  \"unique_id\": \"poolmaster_set_min_fill_pump_uptime\", \
  \"unit_of_measurement\": \"mn\", \
  \"icon\": \"mdi:clock-minus\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "SWG Uptime Limit (mn)";
  sprintf(topic, "%s/Set5", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PumpMaxUp:[4,{{value}}] }\", \
  \"min\": \"0\", \
  \"max\": \"1440\", \
  \"mode\": \"box\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.SWGUTL | int }}\", \
  \"unique_id\": \"poolmaster_set_max_swg_uptime\", \
  \"unit_of_measurement\": \"mn\", \
  \"icon\": \"mdi:clock-end\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Filtration Uptime Limit (mn)";
  sprintf(topic, "%s/Set5", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PumpMaxUp: [0,{{value}}] }\", \
  \"min\": \"0\", \
  \"max\": \"1440\", \
  \"mode\": \"box\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.FPUTL | int }}\", \
  \"unique_id\": \"poolmaster_set_max_filration_pump_uptime\", \
  \"unit_of_measurement\": \"mn\", \
  \"icon\": \"mdi:clock-end\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "MQTT Publication (s)";
  sprintf(topic, "%s/Set4", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PubPeriod: {{value}} }\", \
  \"min\": \"1\", \
  \"max\": \"60\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.PubP | int }}\", \
  \"unique_id\": \"poolmaster_set_mqtt_pub\", \
  \"unit_of_measurement\": \"s\", \
  \"icon\": \"mdi:email-newsletter\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "PSI High Threshold";
  sprintf(topic, "%s/Set2", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PSIHigh: {{value}} }\", \
  \"min\": \"0.1\", \
  \"max\": \"3\", \
  \"mode\": \"slider\", \
  \"step\": \"0.1\", \
  \"value_template\": \"{{ value_json.PSIHT | float | multiply(0.01) | round(2) }}\", \
  \"unique_id\": \"poolmaster_set_psi_high\", \
  \"unit_of_measurement\": \"bar\", \
  \"icon\": \"mdi:alert\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "PSI Low Threshold";
  sprintf(topic, "%s/Set2", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PSILow: {{value}} }\", \
  \"min\": \"0\", \
  \"max\": \"3\", \
  \"mode\": \"slider\", \
  \"step\": \"0.1\", \
  \"value_template\": \"{{ value_json.PSIMT | float | multiply(0.01) | round(2)}}\", \
  \"unique_id\": \"poolmaster_set_psi_low\", \
  \"unit_of_measurement\": \"bar\", \
  \"icon\": \"mdi:alert\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Delay PID (mn)";
  sprintf(topic, "%s/Set4", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ DelayPID: {{value}} }\", \
  \"min\": \"1\", \
  \"max\": \"59\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.Dpid | int }}\", \
  \"unique_id\": \"poolmaster_set_delay_pid\", \
  \"unit_of_measurement\": \"mn\", \
  \"icon\": \"mdi:timer-sync\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "pH Pump Flow Rate (L/s)";
  sprintf(topic, "%s/Set5", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ pHPumpFR: {{value}} }\", \
  \"min\": \"1\", \
  \"max\": \"5\", \
  \"mode\": \"slider\", \
  \"step\": \"0.1\", \
  \"value_template\": \"{{ value_json.pHFR | float }}\", \
  \"unique_id\": \"poolmaster_set_ph_flowrate\", \
  \"unit_of_measurement\": \"l/s\", \
  \"icon\": \"mdi:needle\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Chl Pump Flow Rate (L/s)";
  sprintf(topic, "%s/Set5", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ ChlPumpFR: {{value}} }\", \
  \"min\": \"1\", \
  \"max\": \"5\", \
  \"mode\": \"slider\", \
  \"step\": \"0.1\", \
  \"value_template\": \"{{ value_json.OrpFR | float }}\", \
  \"unique_id\": \"poolmaster_set_chl_flowrate\", \
  \"unit_of_measurement\": \"l/s\", \
  \"icon\": \"mdi:needle\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Water Temperature Set Point";
  sprintf(topic, "%s/Set2", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ WSetPoint: {{value}} }\", \
  \"min\": \"0\", \
  \"max\": \"40\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.WSP | float | multiply(0.01) | round(2) }}\", \
  \"unique_id\": \"poolmaster_wtemp_setpoint\", \
  \"unit_of_measurement\": \"°C\", \
  \"icon\": \"mdi:thermometer-water\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Water Temperature Low";
  sprintf(topic, "%s/Set2", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ WTempLow: {{value}} }\", \
  \"min\": \"0\", \
  \"max\": \"40\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.WLT | float | multiply(0.01) | round(2) }}\", \
  \"unique_id\": \"poolmaster_wtemp_low\", \
  \"unit_of_measurement\": \"°C\", \
  \"icon\": \"mdi:thermometer-water\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "pH Set Point";
  sprintf(topic, "%s/Set2", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PhSetPoint: {{value}} }\", \
  \"min\": \"6\", \
  \"max\": \"8\", \
  \"mode\": \"slider\", \
  \"step\": \"0.1\", \
  \"value_template\": \"{{ value_json.pHSP | float | multiply(0.01) | round(2) }}\", \
  \"unique_id\": \"poolmaster_ph_setpoint\", \
  \"icon\": \"mdi:ph\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Orp Set Point";
  sprintf(topic, "%s/Set2", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ OrpSetPoint: {{value}} }\", \
  \"min\": \"600\", \
  \"max\": \"900\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.OrpSP | float }}\", \
  \"unique_id\": \"poolmaster_orp_setpoint\", \
  \"icon\": \"mdi:water-check\", \
  \"unit_of_measurement\": \"mV\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "pH PID Window Size";
  sprintf(topic, "%s/Set2", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ PhPIDWSize: {{value}} }\", \
  \"min\": \"10\", \
  \"max\": \"50\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.pHWS | float }}\", \
  \"unique_id\": \"poolmaster_ph_pid_windowssize\", \
  \"unit_of_measurement\": \"mn\", \
  \"icon\": \"mdi:ph\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "SWG Startup Delay";
  sprintf(topic, "%s/Set5", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ ElectroConfig:[{{states('number.poolmaster_swg_security_temperature')}},{{value}}]}\", \
  \"min\": \"0\", \
  \"max\": \"25\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.SWGDel | int }}\", \
  \"unique_id\": \"poolmaster_swg_startup_delay\", \
  \"unit_of_measurement\": \"mn\", \
  \"icon\": \"mdi:timer-sync\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "SWG Security Temperature";
  sprintf(topic, "%s/Set5", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ ElectroConfig:[{{value}},{{states('number.poolmaster_swg_startup_delay')}}]}\", \
  \"min\": \"1\", \
  \"max\": \"25\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.SWGSec | int }}\", \
  \"unique_id\": \"poolmaster_swg_security_temperature\", \
  \"unit_of_measurement\": \"°C\", \
  \"icon\": \"mdi:thermometer-alert\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
 {char name[] = "Orp PID Window Size";
  sprintf(topic, "%s/Set2", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ OrpPIDWSize: {{value}} }\", \
  \"min\": \"10\", \
  \"max\": \"50\", \
  \"mode\": \"slider\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.ChlWS | float }}\", \
  \"unique_id\": \"poolmaster_orp_pid_windowsize\", \
  \"icon\": \"mdi:water-check\", \
  \"unit_of_measurement\": \"mn\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

sprintf(commandtopic, "%s/SVAPI", roottopic);
 {char name[] = "WaterMeter L";
  sprintf(topic, "%s/WaterMeter", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ 'WaterMeter L': '{{ value }}' }\", \
  \"min\": \"0\", \
  \"max\": \"999999999\", \
  \"mode\": \"box\", \
  \"step\": \"1\", \
  \"value_template\": \"{{ value_json.L | int }}\", \
  \"unique_id\": \"poolmaster_watermeter_l\", \
  \"unit_of_measurement\": \"L\", \
  \"icon\": \"mdi:counter\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "WaterMeter K";
  sprintf(topic, "%s/WaterMeter", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ 'WaterMeter K': '{{ value }}' }\", \
  \"min\": \"0.0\", \
  \"max\": \"99.0\", \
  \"step\": \"0.1\", \
  \"mode\": \"box\", \
  \"value_template\": \"{{ value_json.K | float }}\", \
  \"unique_id\": \"poolmaster_watermeter_k\", \
  \"icon\": \"mdi:multiplication-box\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "WaterMeter Debounce";
  sprintf(topic, "%s/WaterMeter", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ 'WaterMeter D': '{{ value }}' }\", \
  \"min\": \"0\", \
  \"max\": \"9999\", \
  \"mode\": \"box\", \
  \"value_template\": \"{{ value_json.D | float }}\", \
  \"unique_id\": \"poolmaster_watermeter_d\", \
  \"icon\": \"mdi:clock-time-four-outline\", \
  \"unit_of_measurement\": \"ms\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
  // TEXT
  // ***************
  strcpy(entitytype, "text");
  sprintf(commandtopic, "%s/SVAPI", roottopic);

 {char name[] = "Update Host (http)";
  sprintf(topic, "%s/SuperVisor", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ 'Update Host': '{{ value }}' }\", \
  \"value_template\": \"{{ value_json['Update Host'] }}\", \
  \"unique_id\": \"poolmaster_update_host\", \
  \"icon\": \"mdi:server-network-outline\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Update PoolMaster path";
  sprintf(topic, "%s/SuperVisor", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ 'Poolmaster Path': '{{ value }}' }\", \
  \"value_template\": \"{{ value_json['Poolmaster Path'] }}\", \
  \"unique_id\": \"poolmaster_update_pm_path\", \
  \"icon\": \"mdi:server-network-outline\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Update SuperVisor path";
  sprintf(topic, "%s/SuperVisor", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ 'SuperVisor Path': '{{ value }}' }\", \
  \"value_template\": \"{{ value_json['SuperVisor Path'] }}\", \
  \"unique_id\": \"poolmaster_update_sv_path\", \
  \"icon\": \"mdi:server-network-outline\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Update Nextion path";
  sprintf(topic, "%s/SuperVisor", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ 'Nextion Path': '{{ value }}' }\", \
  \"value_template\": \"{{ value_json['Nextion Path'] }}\", \
  \"unique_id\": \"poolmaster_update_nx_path\", \
  \"icon\": \"mdi:server-network-outline\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

  {char name[] = "Papertrail Host";
  sprintf(topic, "%s/SuperVisor", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ 'Papertrail Host': '{{ value }}' }\", \
  \"value_template\": \"{{ value_json['Papertrail Host'] }}\", \
  \"unique_id\": \"poolmaster_papertrail_host\", \
  \"icon\": \"mdi:server-network-outline\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
  
  {char name[] = "Papertrail Port";
  sprintf(topic, "%s/SuperVisor", roottopic);
  char JSEntity[] = " \
  \"command_template\": \"{ 'Papertrail Port': '{{ value }}' }\", \
  \"value_template\": \"{{ value_json['Papertrail Port'] }}\", \
  \"unique_id\": \"poolmaster_papertrail_port\", \
  \"icon\": \"mdi:server-network-outline\", \
  \"entity_category\": \"config\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}

 sprintf(commandtopic, "%s/API", roottopic);
 {char name[] = "JSON Command";
  sprintf(topic, "%s", commandtopic);
  char JSEntity[] = " \
  \"command_template\": \"{{ value }}\", \
  \"unique_id\": \"poolmaster_jsoncommand\", \
  \"icon\": \"mdi:code-json\", \
  ";
  createHAEntitie(entitytype, name, topic, JSEntity, JScommon, hatopic, commandtopic);}
}
