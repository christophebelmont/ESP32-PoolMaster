/*
            Relay - a simple library to handle relays
                 (c) Christophe <christophe.belmont@gmail.com> 2024
Features: 

- handle high level and low level active relays
- handle momentary relay operation or standard relay operation
  In case of momentary relays, please note that there is no possibility to know the real underlying state of the relay
  more specifically in case of system restart. This must be dealt with at higher level in your code.

NB: all timings are in milliseconds
*/

#ifndef RELAY_h
#define RELAY_h
#include "Pin.h"

#define RELAY_VERSION "1.0.1"

//Class constants
#define MODE_LATCHING    0    // Underlying relay turns ON and OFF equipment by switching between LOW/HIGH states (depending on level parameters)
#define MODE_MOMENTARY   1    // Underlying relay turns ON and OFF equipment by simulating a button press 
                               // switch to HIGH then LOW after a short period of time to switch ON and switch to HIGH then LOW again to switch OFF)
#define MOMENTARY_SHORT_DELAY  500 // In Milliseconds
#define NO_INTERLOCK 255  

class Relay : public PIN {
  public:
    Relay(uint8_t _pin_number, uint8_t _pin_direction = OUTPUT_DIGITAL, bool _active_level = ACTIVE_LOW, bool _operation_mode = MODE_LATCHING) 
      : PIN(_pin_number, _pin_direction, _active_level) 
    {
      Initialize(_operation_mode);    // Initialize before changing the class variable because
                                      // function needs to know what operation mode we change from
      operation_mode = _operation_mode; 
    };

    bool Enable();
    bool Disable();
    void Toggle();

    void loop();

    bool IsEnabled();
    bool SetMomentaryDelay(uint64_t);
    void SetOperationMode(bool); // MODE_LATCHING or MODE_MOMENTARY
    bool GetOperationMode(void);

    // Functions which does nothing for Relays
    // but needs to define it for derived class
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
    void SavePreferences(Preferences& prefs, uint8_t pin_id);
    void LoadPreferences(Preferences& prefs, uint8_t pin_id);

    void SetHandlers(std::function<bool()> _handler1, std::function<bool()> _handler2, std::function<void()> _handler3, std::function<void()> _handler4) override {
        shouldStartHandler = _handler1;
        shouldStopHandler = _handler2;
        onStartHandler = _handler3;
        onStopHandler = _handler4;
    }
    void SetShouldStartHandler(std::function<bool()> _handler) override { shouldStartHandler = _handler; }
    void SetShouldStopHandler(std::function<bool()> _handler) override { shouldStopHandler = _handler; }
    void SetOnStartHandler(std::function<void()> _handler) override { onStartHandler  = _handler; }
    void SetOnStopHandler(std::function<void()> _handler) override { onStopHandler  = _handler; }

  private:
    void Initialize(bool = MODE_LATCHING);

    TimerHandle_t tmr = nullptr; // Used for MODE_MOMENTARY pin callback function timer

    bool operation_mode = MODE_LATCHING;  // MODE_LATCHING or MODE_MOMENTARY
    uint64_t  momentary_delay = MOMENTARY_SHORT_DELAY;
    bool virtual_status = 0; // Retain status in case of momentary relay
};

// Timer Callback function for momentary relay operations
auto CallBackTimer = []( TimerHandle_t xTimer )
{
    Relay* rr = static_cast<Relay*>(pvTimerGetTimerID(xTimer));      // Get the relay related to this timer
    assert(rr); // Sanity check
    rr->PIN::Disable();
};
#endif
