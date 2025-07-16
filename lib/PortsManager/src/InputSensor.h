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
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"

//#define NO_INTERLOCK 255 

class InputSensor : public PIN {
  public:
    InputSensor(uint8_t _pin_number, uint8_t _active_level = ACTIVE_LOW, 
      uint8_t _highThreshold = 10, uint16_t _timeWindow = 5000, uint16_t _readInterval = 200)
      : PIN(_pin_number, (uint8_t)INPUT_DIGITAL, _active_level),
      highThreshold(_highThreshold), timeWindow(_timeWindow), readIntervalMs(_readInterval) {}

    void begin();
    void loop() override {}; // No need for a loop in InputSensor. All is done with FreeRTOS tasks
    bool getState();
    
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
    bool IsEnabled() {return getState();} // InputSensor is enabled if it is active
    bool IsRelay(void);

    void SetHandlers(std::function<bool()> _handler1, std::function<bool()> _handler2, std::function<void()> _handler3, std::function<void()> _handler4) override {
        shouldStartHandler = nullptr;
        shouldStopHandler = nullptr;
        onStartHandler = _handler3;
        onStopHandler = _handler4;
    }
    void SetShouldStartHandler(std::function<bool()> _handler) {};
    void SetShouldStopHandler(std::function<bool()> _handler) {};
    void SetOnStartHandler(std::function<void()> _handler) {onStartHandler = _handler;};
    void SetOnStopHandler(std::function<void()> _handler) {onStopHandler = _handler;};
    void SetLoopHandler(std::function<void()> _handler) {};


    void setHighThreshold(uint8_t _highThreshold) {highThreshold = _highThreshold;}
    void setTimeWindow(uint16_t _timeWindow) {timeWindow = _timeWindow;}
    void setReadInterval(uint16_t _readInterval) {
      readIntervalMs = _readInterval;
      if (localTimerHandle != nullptr) {
          xTimerChangePeriod(localTimerHandle, pdMS_TO_TICKS(readIntervalMs), 0);
     }
    }
    void setAnalysisFrequency(uint16_t _analysisFrequency) {
      analysisFrequency = _analysisFrequency;
    }

  private:
    static const uint8_t HISTO_SIZE = 50;
    unsigned long tableDetections[HISTO_SIZE];
    volatile int tableHead = 0;
    volatile int tableTotal = 0;

    uint8_t highThreshold;
    uint16_t timeWindow;
    uint16_t readIntervalMs;
    uint16_t analysisFrequency = 200;

    portMUX_TYPE localMutex;
    TimerHandle_t localTimerHandle = nullptr;
    TaskHandle_t localTaskHandle = nullptr;

    static void timerCallbackStatic(TimerHandle_t xTimer);
    static void taskDetectStatic(void* param);

    void timerCallback();
    void taskDetection();

    bool currentState = false; // Current state of the input sensor (avoid reading every time)
    bool previous_state = false; // Used to detect changes in state
  };

#endif