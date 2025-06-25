#include "Arduino.h"
#include "Relay.h"

// Initialize function for construction
void Relay::Initialize(bool _operation_mode)
{
  // Change from latching to momentary
  if ((_operation_mode == MODE_MOMENTARY) && (operation_mode == MODE_LATCHING))  
  {
    // Create timer used by momentary switches to switch off after predefined period of time
    tmr = xTimerCreate("MomentaryTimer", pdMS_TO_TICKS(momentary_delay), pdFALSE, static_cast<void*>(this) , CallBackTimer);
    if (tmr == nullptr ) {
        Serial.printf("Unable to create timer for momentary relay");
    }
    this->PIN::Disable();  // Disable the port if it was on before
  } else if ((_operation_mode == MODE_LATCHING) && (operation_mode == MODE_MOMENTARY))  // Move from momentary to standard
  {
    // No need for a timer anymore
    if(tmr != nullptr) {
      xTimerDelete(tmr, 0);
    }
  }
}

//Switch the relay ON
bool Relay::Enable()
{
  // If timer is active, this means that a momentary switch is currently being turned on or off
  // do nothing and return false to indicate that something went wrong
  if (tmr != nullptr )
    if ( xTimerIsTimerActive( tmr ) != pdFALSE )
      return false;

  //Serial.printf("Enable Relay !IsEnab=%d \n\r",!IsEnabled());
  if (!IsEnabled()) 
  {
    if (operation_mode == MODE_MOMENTARY) 
    { // If relay is MOMENTARY type
      if (tmr == nullptr ) { // If timer was not properly initialized
        //Serial.printf("Non existent timer for momentary relay");
        return false;
      }

      // Launch timer to switch the momentary relay back off after the delay
      if( xTimerStart( tmr, 0 ) != pdPASS ) {
        //Serial.printf("Unable to launch timer for momentary relay");
        return false;
      }

      virtual_status = 1;
    }
    this->PIN::Enable();
    return true; 
  } else
    return false;
}

//Switch the relay OFF
bool Relay::Disable()
{
  // If timer is active, this means that a momentary switch is currently being turned on or off
  // do nothing and return false to indicate that something went wrong
  if (tmr != nullptr )
    if ( xTimerIsTimerActive( tmr ) != pdFALSE )
      return false;

  //Serial.printf("Disable Relay IsEnab=%d \n\r",IsEnabled());
  if (IsEnabled()) 
  {
    if (operation_mode == MODE_MOMENTARY) 
    { // If relay is MOMENTARY type
      if (tmr == nullptr ) { // If timer was not properly initialized
        //Serial.printf("Non existent timer for momentary relay");
        return false;
      }
      // Launch timer to switch the momentary relay back off after the delay
      if( xTimerStart( tmr, 0 ) != pdPASS ) {
        //Serial.printf("Unable to launch timer for momentary relay");
        return false;
      }

      virtual_status = 0;
      this->PIN::Enable();
    }else{
      this->PIN::Disable();
    }
    
    return true; 
  } else
    return false;
}

//Toggle switch state
void Relay::Toggle()
{
  // WARNING: Do not call base class toggle since all Enable/Disable logic from upper
  // class would be lost.
  (IsEnabled()) ? Disable() : Enable();  // Invert the position of the relay
}

// Define the bestable delay
bool Relay::SetMomentaryDelay(uint64_t  _momentary_delay)
{
  bool isTimerActive = false;
  momentary_delay = _momentary_delay;

  // Initialize timer to new value
  if(operation_mode == MODE_MOMENTARY)
  {
    // Check if timer is currently active to futher reactivate
    isTimerActive = ( xTimerIsTimerActive( tmr ) != pdFALSE )? true : false;

    if (tmr != nullptr) {
      if (xTimerChangePeriod( tmr, pdMS_TO_TICKS(momentary_delay), 0 ) != pdPASS )
        return false;
    } else {
      return false;
    }
    // xTimerChangePeriod causes the timer to restart. If it was dormant before stop it.
    if (!isTimerActive && tmr != nullptr)
      xTimerStop( tmr, 0 ); 
    return true;
  } else
    return true;
}

// Change relay type MODE_LATCHING or MODE_MOMENTARY
void Relay::SetOperationMode(bool _operation_mode)
{
  Initialize(_operation_mode);
  operation_mode = _operation_mode; 
}

// Return relay type MODE_LATCHING or MODE_MOMENTARY
bool Relay::GetOperationMode(void)
{
  return operation_mode; 
}

//relay status
bool Relay::IsEnabled()
{
  if (operation_mode == MODE_MOMENTARY)
  {
    return (virtual_status == 1);
  }
  else
  {
    return (this->PIN::IsActive());
  }
}


void Relay::SavePreferences(Preferences& prefs, uint8_t pin_id)  
{
    PIN::SavePreferences(prefs, pin_id);

    char key[15];
    snprintf(key, sizeof(key), "d%d_om", pin_id);  // "device_X_operation_mode"
    prefs.putBool(key, operation_mode);
    //Serial.printf("Save preference %s = %d\r\n",key, operation_mode);
}

void Relay::LoadPreferences(Preferences& prefs, uint8_t pin_id)  
{
    PIN::LoadPreferences(prefs,pin_id);

    char key[15];
    bool tmp_operation_mode;
    snprintf(key, sizeof(key), "d%d_om", pin_id);
    tmp_operation_mode = prefs.getBool(key, operation_mode);
    //Serial.printf("Read preference %s = %d\r\n",key, tmp_operation_mode);

    Initialize(tmp_operation_mode);    // Initialize before changing the class variable because
                                    // function needs to know what operation mode we change from
    operation_mode = tmp_operation_mode;
}

void Relay::loop()
{
  if (shouldStartHandler && shouldStartHandler() && !IsEnabled()) {
      Enable();
  }

  if (shouldStopHandler && shouldStopHandler() && IsEnabled()) {
      Disable();
  }
};

//Function overiden for derived class hierarchie but they do nothing
void Relay::SetTankLevelPIN(uint8_t _tank_level) {}
void Relay::SetTankFill(double _tank_fill) {}
void Relay::SetTankVolume(double _tank_vol) {}
void Relay::SetFlowRate(double _flow_rate) {}
void Relay::SetMaxUpTime(unsigned long _max_uptime) {}
double Relay::GetTankFill() {return 100.;}
void Relay::ResetUpTime() {}
void Relay::SetInterlock(PIN* _interlock) {}
void Relay::SetInterlock(uint8_t) {}
uint8_t Relay::GetInterlockId(void) { return NO_INTERLOCK;}
bool Relay::IsRelay(void) {return true;}
