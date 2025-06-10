/*
            PINs - a simple library to handle ESP ports
                 (c) Christophe <christophe.belmont@gmail.com> 2024
Features: 

- TBD

NB: all timings are in milliseconds
*/
#include <Preferences.h>

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
    PIN(uint8_t, uint8_t, uint8_t = OUTPUT_DIGITAL, bool = ACTIVE_LOW);

    void Enable();
    void Disable();
    void Toggle();

    void Begin();

    bool IsActive();
    void SetActiveLevel(bool);    // ACTIVE_HIGH or ACTIVE_LOW
    bool GetActiveLevel(void);    // ACTIVE_HIGH or ACTIVE_LOW

    uint8_t GetPinId();
    uint8_t GetPinNumber();
    void SetPinNumber(uint8_t,uint8_t = OUTPUT_DIGITAL,bool = ACTIVE_LOW);
    void ResetPinLevel(void); // Reset to inactive level for momentary callback function

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
    virtual void loop() = 0;
    virtual bool IsEnabled() = 0;
    virtual bool IsRelay() = 0;

    virtual void SavePreferences(Preferences& prefs) {
        char key[15]; 
        
        snprintf(key, sizeof(key), "d%d_pn", pin_id);  // "device_X_pin_number"
        prefs.putUChar(key, pin_number);

        snprintf(key, sizeof(key), "d%d_pd", pin_id);  // "device_X_pin_direction"
        prefs.putBool(key, pin_direction);

        snprintf(key, sizeof(key), "d%d_id", pin_id);  // "device_X_pin_id"
        prefs.putUChar(key, pin_id);

        snprintf(key, sizeof(key), "d%d_al", pin_id);  // "device_X_active_level"
        prefs.putBool(key, active_level);
    }

    virtual void LoadPreferences(Preferences& prefs) {
      char key[15];

      snprintf(key, sizeof(key), "d%d_pn", pin_id);
      pin_number = prefs.getUChar(key, pin_number);

      snprintf(key, sizeof(key), "d%d_pd", pin_id);
      pin_direction = prefs.getBool(key, pin_id);

      snprintf(key, sizeof(key), "d%d_id", pin_id);
      pin_id = prefs.putUChar(key, pin_id);

      snprintf(key, sizeof(key), "d%d_al", pin_id);
      active_level = prefs.getBool(key, active_level);
  }


  private:
    void Initialize(uint8_t, uint8_t = OUTPUT_DIGITAL, bool = ACTIVE_LOW);

    uint8_t pin_number;
    bool pin_direction;
    bool active_level;
    uint8_t pin_id;
};

#endif
