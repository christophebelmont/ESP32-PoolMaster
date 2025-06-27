// Task to process JSON commands received via MQTT.
// If the comand modify a setting parameter, it is saved in NVS and published back
// for other MQTT clients (dashboards)

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include "PoolServer_Commands.h"

// Functions prototypes
extern void PublishMeasures();
extern void stack_mon(UBaseType_t&);

void ProcessCommand(void *pvParameters)
{
  //Json Document
  StaticJsonDocument<250> command;
  char JSONCommand[200] = "";                         // JSON command to process  
  
  while (!startTasks) ;
  vTaskDelay(DT2);                                // Scheduling offset   

  TickType_t period = PT2;  
  static UBaseType_t hwm = 0;
  TickType_t ticktime = xTaskGetTickCount(); 

  #ifdef CHRONO
  unsigned long td;
  int t_act=0,t_min=999,t_max=0;
  float t_mean=0.;
  int n=1;
  #endif

  for(;;) {
    #ifdef CHRONO
    td = millis();
    #endif
    //Is there any incoming JSON commands
    if (uxQueueMessagesWaiting(queueIn) != 0)
    {  
      xQueueReceive(queueIn,&JSONCommand,0);
      //Parse Json object and find which command it is
      DeserializationError error = deserializeJson(command,JSONCommand);

      // Test if parsing succeeds.
      if (error)
      {
        Debug.print(DBG_WARNING,"Json parseObject() failed");
      }
      else
      {
        Debug.print(DBG_DEBUG,"Json parseObject() success: %s",JSONCommand);

        //Find command keyword in list and execute corresponding function handler
        JsonObject::iterator it = command.as<JsonObject>().begin();
        auto it_find = server_handlers.find(it->key().c_str());
        if (it_find != server_handlers.end()) {
          it_find->second(command); // Call the function associated with the key
        } else {
          Debug.print(DBG_WARNING,"Command not found: %s", it->key().c_str());
        }

        // Bip the buzzer to show good execution of the command
        if(PMConfig.get<bool>(BUZZERON))
        {
          // Sound configuration applied
          digitalWrite(BUZZER,HIGH);
          delay(30);
          digitalWrite(BUZZER,LOW);
          delay(40);
          digitalWrite(BUZZER,HIGH);
          delay(30);
          digitalWrite(BUZZER,LOW);
        }
        // Publish Update on the MQTT broker the status of our variables
        PublishMeasures();
      }
    }
    #ifdef CHRONO
    t_act = millis() - td;
    if(t_act > t_max) t_max = t_act;
    if(t_act < t_min) t_min = t_act;
    t_mean += (t_act - t_mean)/n;
    ++n;
    Debug.print(DBG_INFO,"[PoolServer] td: %d t_act: %d t_min: %d t_max: %d t_mean: %4.1f",td,t_act,t_min,t_max,t_mean);
    #endif 
    stack_mon(hwm); 
    vTaskDelayUntil(&ticktime,period);
  }  
}
