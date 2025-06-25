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

#define DEFAULT_FLOWRATE 1.5 //default flow rate in Liters/hour
#define DEFAULT_TANK_VOLUME 20.0 //default tank volume in Liters
#define DEFAULT_TANK_FILL 100.0 //default tank fill in percentage (100% means full tank)

#define DEFAULTMAXUPTIME 60*30*1000 //default value is 30 minutes
#define DEFAULTMINUPTIME 0 //default is 0, meanning no minimum uptime

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
    Pump(uint8_t _pin_number, uint8_t _tank_level_pin = NO_TANK, uint8_t _active_level = ACTIVE_LOW, bool _operation_mode = MODE_LATCHING, double _flowrate= DEFAULT_FLOWRATE, double _tankvolume= DEFAULT_TANK_VOLUME, double _tankfill=DEFAULT_TANK_FILL)
    : Relay(_pin_number, OUTPUT_DIGITAL, _active_level, _operation_mode) 
    {
      tank_level_pin = _tank_level_pin;
      flowrate = _flowrate;
      tankvolume = _tankvolume;
      tankfill = _tankfill;
      CurrMaxUpTime  = MaxUpTime; // Set the current max uptime to the default value
    }

    void loop();

    bool Start(bool _resetUpTime = false); // Start the pump, reset UpTime if ResetUpTime is true
    bool Stop(); 

    bool IsRunning();
   
    bool TankLevel();
    void SetTankLevelPIN(uint8_t);
    void SetTankFill(double);
    double GetTankFill();
    void SetTankVolume(double Volume);
    double GetTankVolume() { return tankvolume; } // Return the tank volume in Liters
    double GetTankUsage();
    void SetFlowRate(double FlowRate);
    double GetFlowRate() { return flowrate; } // Return the flow rate of the pump in Liters/hour
    void SetMaxUpTime(unsigned long);
    void SetMinUpTime(unsigned long);
    void ResetUpTime();
    void ClearErrors();
    bool MinUpTimeReached() { return (UpTime >= MinUpTime); } // Return true if minimum uptime reached

    void SetInterlock(PIN*);
    void SetInterlock(uint8_t);
    uint8_t GetInterlockId(void);
    bool CheckInterlock();  // Return true if interlock pump running (use only the pointer to the interlock pump so need to call InitInterlock before)
    bool IsRelay(void);

    void SavePreferences(Preferences& prefs, uint8_t pin_id);
    void LoadPreferences(Preferences& prefs, uint8_t pin_id);

    unsigned long UpTime        = 0;
    unsigned long MaxUpTime     = DEFAULTMAXUPTIME; // Maximum Uptime for a run
    unsigned long MinUpTime     = DEFAULTMINUPTIME; // Minimum Uptime for a run (just used as storage for the moment, not used by the class)
    unsigned long CurrMaxUpTime; // Total uptime after error were cleared an another maxuptime was authorized
    bool          UpTimeError   = false;
    unsigned long StartTime = 0;
    unsigned long StopTime      = 0; 
    
  private:
    PIN*  interlock_pump_     = nullptr; // Interlock can be passed 
    uint8_t interlock_pin_id  = NO_INTERLOCK; // Used temporarily to store the interlock pin id but converted in pointer by InitInterlock function
    unsigned long LastLoopMillis     = 0;
    uint8_t tank_level_pin;
    double flowrate, tankvolume, tankfill; // Flowrate in L/h, tank volume in Liters, tank fill in %
};
#endif
