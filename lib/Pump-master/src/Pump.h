/*
            Pump - a simple library to handle home-pool filtration and peristaltic pumps
                 (c) Loic74 <loic74650@gmail.com> 2017-2020
Features: 

- keeps track of running time
- keeps track of Tank Levels
- set max running time limit

NB: all timings are in milliseconds
*/

#include "Relay.h"                // To handle underlying pump relays

#ifndef PUMP_h
#define PUMP_h
#define PUMP_VERSION "1.0.2"

//Constants used in some of the functions below
#define TANK_FULL  0
#define TANK_EMPTY 1
#define NO_LEVEL 170          // Pump with tank but without level switch
#define NO_TANK 255           // Pump without tank
#define NO_INTERLOCK 255  
//#define INTERLOCK_REFERENCE 170  //Interlock object was passed as a reference Pump&

#define DEFAULTMAXUPTIME 60*30*1000 //default value is 30 minutes

class Pump : public Relay {
  public:
  //Constructor
  //PumpPin is the Arduino relay output pin number to be switched to start/stop the pump
  //TankLevelPin is the Arduino digital input pin number connected to the tank level switch
  //ActiveLevel is either ACTIVE_HIGH or ACTIVE_LOW depending on underlying relay module
  //Operation mode is either MODE_LATCHING or MODE_MOMENTARY
  //   - MODE_LATCHING when PUMP is ON, underlying relay is ON (idem for OFF)
  //   - MODE_MOMENTARY when PUMP turns ON, underlying relay switches ON for a short period on time then turns back OFF (idem for OFF).
  //     this is used to simulate pressing a button and allows control of a wider variety of devices
  //FlowRate is the flow rate of the pump in Liters/Hour, typically 1.5 or 3.0 L/hour for peristaltic pumps for pools. This is used to compute how much of the tank we have emptied out
  //TankVolume is used here to compute the percentage fill used
  //TankFill current tank fill percentage when object is constructed
    Pump(uint8_t _pin_number, uint8_t _pin_id, uint8_t _tank_level_pin = NO_TANK, uint8_t _active_level = ACTIVE_LOW, bool _operation_mode = MODE_LATCHING, double _flowrate= 0., double _tankvolume= 0., double _tankfill=100.) 
    : Relay(_pin_number, _pin_id, OUTPUT_DIGITAL, _active_level, _operation_mode) 
    {
      tank_level_pin = _tank_level_pin;
      flowrate = _flowrate;
      tankvolume = _tankvolume;
      tankfill = _tankfill;
    }

    void loop();

    bool Start();
    bool Stop(); 

    bool IsRunning();
   
    bool TankLevel();
    void SetTankLevelPIN(uint8_t);
    void SetTankFill(double);
    double GetTankFill();
    void SetTankVolume(double Volume);
    double GetTankUsage();
    void SetFlowRate(double FlowRate);
    void SetMaxUpTime(unsigned long Max);
    void ResetUpTime();
    void ClearErrors();

    void SetInterlock(PIN*);
    void SetInterlock(uint8_t);
    uint8_t GetInterlockId(void);
    bool CheckInterlock();  // Return true if interlock pump running (use only the pointer to the interlock pump so need to call InitInterlock before)
    bool IsRelay(void);

    void SavePreferences(Preferences& prefs) override {
        Relay::SavePreferences(prefs);

        char key[15];

        snprintf(key, sizeof(key), "d%d_fr", GetPinId());  // "device_X_flowrate"
        prefs.putDouble(key, flowrate);

        snprintf(key, sizeof(key), "d%d_tv", GetPinId());  // "device_X_tankvolume"
        prefs.putDouble(key, tankvolume);

        snprintf(key, sizeof(key), "d%d_tf", GetPinId());  // "device_X_tankfill"
        prefs.putDouble(key, tankfill);

        snprintf(key, sizeof(key), "d%d_tl", GetPinId());  // "device_X_tanklevelpin"
        prefs.putUChar(key, tank_level_pin);

        snprintf(key, sizeof(key), "d%d_il", GetPinId());  // "device_X_interlockid"
        prefs.putUChar(key, interlock_pin_id);

        snprintf(key, sizeof(key), "d%d_mu", GetPinId());  // "device_X_interlockid"
        prefs.putULong(key, MaxUpTime/1000);
      }

    void LoadPreferences(Preferences& prefs) override {
        Relay::LoadPreferences(prefs);

        char key[15];

        snprintf(key, sizeof(key), "d%d_fr", GetPinId());
        flowrate = prefs.getDouble(key, flowrate);

        snprintf(key, sizeof(key), "d%d_tv", GetPinId());
        tankvolume = prefs.getDouble(key, tankvolume);

        snprintf(key, sizeof(key), "d%d_tf", GetPinId());
        tankfill = prefs.getDouble(key, tankfill);
  
        snprintf(key, sizeof(key), "d%d_tl", GetPinId());
        tank_level_pin = prefs.getUChar(key, tankfill);

        snprintf(key, sizeof(key), "d%d_il", GetPinId());
        interlock_pin_id = prefs.getUChar(key, interlock_pin_id);

        snprintf(key, sizeof(key), "d%d_mu", GetPinId());
        MaxUpTime = prefs.getULong(key, MaxUpTime);
      }

    unsigned long UpTime        = 0;
    unsigned long MaxUpTime     = DEFAULTMAXUPTIME; // Uptime for a run
    unsigned long CurrMaxUpTime = DEFAULTMAXUPTIME; // Total uptime after error were cleared an another maxuptime was authorized
    bool          UpTimeError   = false;
    unsigned long StartTime = 0;
    unsigned long StopTime      = 0; 
    
  private:
    PIN*  interlock_pump_ = nullptr; // Interlock can be passed 
    uint8_t interlock_pin_id = NO_INTERLOCK; // Used temporarily to store the interlock pin id but converted in pointer by InitInterlock function
    unsigned long LastLoopMillis     = 0;
    uint8_t tank_level_pin;
    double flowrate = 1.5, tankvolume = 20., tankfill = 100.; // Flowrate in L/h, tank volume in Liters, tank fill in %
};
#endif
