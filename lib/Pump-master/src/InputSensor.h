/*
            INPUT Sensor - a simple library to handle ESP ports
                 (c) Christophe <christophe.belmont@gmail.com> 2024
Features: 

- handle get state and debouncing
- compatible with PIN base class

NB: all timings are in milliseconds
*/

#ifndef INPUTSENSOR_h
#define INPUTSENSOR_h
#include "Pin.h"

#include <Arduino.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/timers.h"

#define NO_INTERLOCK 255 

class InputSensor : public PIN {
  public:
    InputSensor(uint8_t _pin_number, uint8_t _pin_id, uint8_t _active_level = ACTIVE_LOW) 
      : PIN(_pin_number, _pin_id, (uint8_t)INPUT_DIGITAL, _active_level) {
        //debounceTimer = xTimerCreate("DebounceTimer", pdMS_TO_TICKS(50), pdFALSE, this, debounceCallback);
    }

    bool getState();

    void loop() override;

    bool IsEnabled() override;

    // Functions which does nothing for InputSensor
    // but needs to define it for derived class
    bool GetOperationMode(void);
    void SetOperationMode(bool);
    void SetTankLevelPIN(uint8_t);
    void SetTankFill(double);
    void SetTankVolume(double);
    void SetFlowRate(double);
    void SetMaxUpTime(unsigned long);
    double GetTankFill();
    void ResetUpTime();
    void SetInterlock(PIN*);
    void SetInterlock(uint8_t);
    uint8_t GetInterlockId(void);
    bool IsRelay(void);

  private:
    bool lastState;
    //TimerHandle_t debounceTimer;


    //static void debounceCallback(TimerHandle_t xTimer) {
    //    InputSensor* instance = static_cast<InputSensor*>(pvTimerGetTimerID(xTimer));
    //    instance->lastState = instance->IsActive(); // Utilise IsActive() pour obtenir l'état après debounce
    //}
};

#endif