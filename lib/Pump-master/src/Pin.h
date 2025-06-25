/*
            PINs - a simple library to handle ESP ports
                 (c) Christophe <christophe.belmont@gmail.com> 2024
Features: 

- TBD

NB: all timings are in milliseconds
*/
#include <Preferences.h>
#include <functional>


#ifndef PIN_h
#define PIN_h
#define PIN_VERSION "1.0.1"

//Class constants
#define OUTPUT_DIGITAL 0
#define OUTPUT_PWM     1
#define INPUT_DIGITAL  2         

#define ACTIVE_HIGH 1
#define ACTIVE_LOW  0

class PIN {
  public:
    PIN(uint8_t, uint8_t = OUTPUT_DIGITAL, bool = ACTIVE_LOW);

    void Enable();
    void Disable();
    void Toggle();

    void Begin();

    bool IsActive();
    void SetActiveLevel(bool);    // ACTIVE_HIGH or ACTIVE_LOW
    bool GetActiveLevel(void);    // ACTIVE_HIGH or ACTIVE_LOW

    //uint8_t GetPinId();
    uint8_t GetPinNumber();
    uint8_t GetPinDirection();
    void SetPinNumber(uint8_t,uint8_t = OUTPUT_DIGITAL,bool = ACTIVE_LOW);
    void ResetPinLevel(void); // Reset to inactive level for momentary callback function

    void SetName(const char* _name) {
        strncpy(pin_name, _name, sizeof(pin_name) - 1);
        pin_name[sizeof(pin_name) - 1] = '\0'; // Ensure null termination
    }
    const char* GetName(void) {return pin_name;}

    virtual bool GetOperationMode(void) = 0;
    virtual void SetOperationMode(bool) = 0;
    virtual void SetTankLevelPIN(uint8_t) = 0;
    virtual void SetTankFill(double) = 0;
    virtual void SetTankVolume(double) = 0;
    virtual void SetFlowRate(double) = 0;
    virtual void SetMaxUpTime(unsigned long) = 0;
    virtual void SetInterlock(PIN*) = 0;
    virtual void SetInterlock(uint8_t) = 0;
    virtual uint8_t GetInterlockId(void) = 0;
    virtual double GetTankFill() = 0;
    virtual void ResetUpTime() = 0;
    virtual bool IsEnabled() = 0;
    virtual bool IsRelay() = 0;
    virtual void loop() = 0; // Loop function to be called periodically for upper classes

    virtual void SavePreferences(Preferences& prefs, uint8_t pin_id);
    virtual void LoadPreferences(Preferences& prefs, uint8_t pin_id);

    virtual void SetHandlers(std::function<bool()> _handler1,std::function<bool()> _handler2,std::function<void()> _handler3,std::function<void()> _handler4) = 0;
    virtual void SetShouldStartHandler(std::function<bool()> _handler) = 0;
    virtual void SetShouldStopHandler(std::function<bool()> _handler) = 0;
    virtual void SetOnStartHandler(std::function<void()> _handler) = 0;
    virtual void SetOnStopHandler(std::function<void()> _handler) = 0;

  protected:
    // State Machine like handlers
    // These handlers are used to control the pump state machine
    std::function<bool()> shouldStartHandler = nullptr;
    std::function<bool()> shouldStopHandler = nullptr;
    std::function<void()> onStartHandler = nullptr;
    std::function<void()> onStopHandler = nullptr;
    char pin_name[30] = "\0"; // Name of the pin, used for logs

  private:
    void Initialize(uint8_t, uint8_t = OUTPUT_DIGITAL, bool = ACTIVE_LOW);

    uint8_t pin_number;
    uint8_t pin_direction;
    bool active_level;
  //  uint8_t pin_id;
};

#endif
