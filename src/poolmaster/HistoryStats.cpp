// Task to capture and store long term statistical data
#include "HistoryStats.h"

CircularBuffer<int,NUMBER_OF_HISTORY_SAMPLES> pH_Samples;
CircularBuffer<int,NUMBER_OF_HISTORY_SAMPLES> Orp_Samples;
CircularBuffer<int,NUMBER_OF_HISTORY_SAMPLES> WTemp_Samples;

//Function to update TFT display
//update the global variables of the TFT + the widgets of the active page
//call this function at least every second to ensure fluid display
void HistoryStats(void *pvParameters)
{
  static UBaseType_t hwm=0;     // free stack size

  while(!startTasks);
  vTaskDelay(DT11);                                // Scheduling offset 

  //esp_task_wdt_add(nullptr);
  TickType_t period = PT11;  
  TickType_t ticktime = xTaskGetTickCount(); 

  #ifdef CHRONO
  unsigned long td;
  int t_act=0,t_min=999,t_max=0;
  float t_mean=0.;
  int n=1;
  #endif

  for(;;)
  {
    // reset watchdog
    //esp_task_wdt_reset();

    #ifdef CHRONO
    td = millis();
    #endif    

    // Store History Samples
    pH_Samples.push((int)(PMData.PhValue*100));
    Orp_Samples.push((int)(PMData.OrpValue));
    WTemp_Samples.push((int)(PMData.WaterTemp*10));

    // Check NTP Connection and reconnect if needed

    #ifdef CHRONO
    t_act = millis() - td;
    if(t_act > t_max) t_max = t_act;
    if(t_act < t_min) t_min = t_act;
    t_mean += (t_act - t_mean)/n;
    ++n;
    Debug.print(DBG_INFO,"[PoolMaster] td: %d t_act: %d t_min: %d t_max: %d t_mean: %4.1f",td,t_act,t_min,t_max,t_mean);
    #endif 

    stack_mon(hwm);
    vTaskDelayUntil(&ticktime,period);
  } 
}

