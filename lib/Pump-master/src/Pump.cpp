#include "Arduino.h"
#include "Pump.h"

//Call this in the main loop, for every loop, as often as possible
void Pump::loop()
{
  u_int8_t bitMaskErrors = 0;
  // Call the loop handler if it exists, if the pump is not running
  // and only if the interlock pump is running (if it exists)
  if (interlock_pump_ == nullptr || (interlock_pump_ != nullptr && interlock_pump_->IsEnabled())) {
    if (shouldStartHandler && shouldStartHandler() && !IsRunning()) {
        // If started successfully, call the onStart handler
        bitMaskErrors = Start(); // Start the pump and reset UpTime
        if(bitMaskErrors == 0) { // No error at startup
            if (onStartHandler) onStartHandler(); // Appel du handler
        }
    }
  }

  if (shouldStopHandler && shouldStopHandler() && IsRunning()) {
      if (Stop() == true) {
          // If stopped successfully, call the onStop handler
          if (onStopHandler) onStopHandler(); // Appel du handler
      }
  }

  if(IsRunning())
  {
    if (loopHandler) loopHandler(); // Appel du handler à chaque boucle

    UpTime += millis() - LastLoopMillis;
    LastLoopMillis = millis();

    if((CurrMaxUpTime > 0) && (UpTime >= CurrMaxUpTime))
    {
        Stop();
        UpTimeError = true;
    }

    if(!TankLevel())
    {
      Stop();
    } 

    // If there is an interlock pump and it stopped. Stop this pump as well
    if ((interlock_pump_!=nullptr) && (interlock_pump_->IsEnabled() == false))
    {
      Stop();
    }
  }
}

//Switch pump ON
u_int8_t Pump::Start(bool _resetUpTime)
{
  u_int8_t bitMaskErrors = 0;

  if (_resetUpTime) {
    ResetUpTime(); // Reset UpTime if requested
  }

  // Check why a Pump would not start
  bitMaskErrors |= (UpTimeError & 1) << 0; // Bit 0: UpTime error
  bitMaskErrors |= (!TankLevel() & 1) << 1; // Bit 1: Tank level error
  bitMaskErrors |= (!CheckInterlock() & 1) << 2; // Bit 2: Interlock error

  if((!IsRunning()) && !UpTimeError && TankLevel() && CheckInterlock())
  {
    if (!this->Relay::Enable()) {
      bitMaskErrors |= (1 << 3); // Bit 3: Relay error
      return bitMaskErrors;
    } else {
      LastLoopMillis = StartTime = millis(); 
    }
  }
  return bitMaskErrors;
}

//Switch pump OFF
bool Pump::Stop()
{
  //Serial.printf("Stopping %s IsRun=%d\n\r",pin_name,IsRunning());
  if(IsRunning())
  {
    if (!this->Relay::Disable())
    {
      return false;
    }
    
    UpTime += millis() - LastLoopMillis; 

    

    return true;
  } else return false;
}

//Pump status
bool Pump::IsRunning()
{
  return (this->Relay::IsEnabled());
}

//tank level status (true = full, false = empty)
bool Pump::TankLevel()
{
  if(tank_level_pin == NO_TANK)
  {
    return true;
  }
  else if (tank_level_pin == NO_LEVEL)
  {
    return (GetTankFill() > 5.); //alert below 5% 
  }
  else
  {
    return (digitalRead(tank_level_pin) == TANK_FULL);
  } 
}

//Set tank fill (percentage of tank volume)
void Pump::SetTankFill(double _tankfill)
{
  tankfill = _tankfill;
}

//Return the remaining quantity in tank in %. When resetting UpTime, SetTankFill must be called accordingly
double Pump::GetTankFill()
{
  return (tankfill - GetTankUsage());
}

//Set Tank volume
//Typically call this function when changing tank and set it to the full volume
void Pump::SetTankVolume(double _tankvolume)
{
  tankvolume = _tankvolume;
}

//Return the percentage used since last reset of UpTime
double Pump::GetTankUsage() 
{
  float PercentageUsed = -1.0;
  if((tankvolume != 0.0) && (flowrate !=0.0))
  {
    double MinutesOfUpTime = (double)UpTime/1000.0/60.0;
    double Consumption = flowrate/60.0*MinutesOfUpTime;
    PercentageUsed = Consumption/tankvolume*100.0;
  }
  return (PercentageUsed);  
}

//Set flow rate of the pump in Liters/hour
void Pump::SetFlowRate(double _flowrate)
{
  flowrate = _flowrate;
}

//Set a maximum running time (in millisecs) per day (in case ResetUpTime() is called once per day)
//Once reached, pump is stopped and "UpTimeError" error flag is raised
//Set "Max" to 0 to disable limit
void Pump::SetMaxUpTime(unsigned long _maxuptime)
{
  MaxUpTime = _maxuptime;
  CurrMaxUpTime = _maxuptime;
}

//Set a minimum running time (in millisecs) 
//Pump can't stop before this time is reached
//Set "Min" to 0 to disable limit
void Pump::SetMinUpTime(unsigned long _minuptime)
{
  MinUpTime = _minuptime;
}


//Reset the tracking of running time
//This is typically called every day at midnight
void Pump::ResetUpTime()
{
  StartTime = LastLoopMillis = 0;
  StopTime = 0;
  UpTime = 0;
  CurrMaxUpTime = MaxUpTime;
}

//Clear "UpTimeError" error flag and allow the pump to run for an extra MaxUpTime
void Pump::ClearErrors()
{
  if(UpTimeError)
  {
    CurrMaxUpTime += MaxUpTime;
    UpTimeError = false;
  }
}

// Set Tank Level PIN
void Pump::SetTankLevelPIN(uint8_t _tank_level_pin)
{
  tank_level_pin = _tank_level_pin;
}

// Initialize the Interlock if needed
void Pump::SetInterlock(PIN* _interlock_pump_)
{
  interlock_pump_ = _interlock_pump_;
}

void Pump::SetInterlock(uint8_t _interlock_pin_id)
{
  interlock_pin_id = _interlock_pin_id;
}

uint8_t Pump::GetInterlockId(void) 
{
/*  if(interlock_pump_ != nullptr)
  {
    return interlock_pump_->GetPinId();
  } else {
    return NO_INTERLOCK;
  }*/
  return interlock_pin_id;
}

//Interlock status
bool Pump::CheckInterlock()
{
  if (interlock_pump_ == nullptr) {
    return true;
  } else {
    return  ((Relay*)interlock_pump_->IsEnabled());
  }
}

bool Pump::IsRelay(void)
{
  return false;
}

void Pump::SavePreferences(Preferences& prefs, uint8_t pin_id)  {
    Relay::SavePreferences(prefs,pin_id);

    char key[15];
    //uint8_t tmp_pin_id = GetPinId();

    snprintf(key, sizeof(key), "d%d_fr", pin_id);  // "device_X_flowrate"
    prefs.putDouble(key, flowrate);
    //Serial.printf("Save preference %s = %d\r\n",key, flowrate);

    snprintf(key, sizeof(key), "d%d_tv", pin_id);  // "device_X_tankvolume"
    prefs.putDouble(key, tankvolume);
    //Serial.printf("Save preference %s = %d\r\n",key, tankvolume);

    snprintf(key, sizeof(key), "d%d_tf", pin_id);  // "device_X_tankfill"
    prefs.putDouble(key, tankfill);
    //Serial.printf("Save preference %s = %d\r\n",key, tankfill);

    snprintf(key, sizeof(key), "d%d_tl", pin_id);  // "device_X_tanklevelpin"
    prefs.putUChar(key, tank_level_pin);
    //Serial.printf("Save preference %s = %d\r\n",key, tank_level_pin);

    snprintf(key, sizeof(key), "d%d_il", pin_id);  // "device_X_interlockid"
    prefs.putUChar(key, interlock_pin_id);
    //Serial.printf("Save preference %s = %d\r\n",key, interlock_pin_id);

    snprintf(key, sizeof(key), "d%d_mu", pin_id);  // "device_X_maxutime"
    prefs.putULong(key, MaxUpTime);
    //Serial.printf("Save preference %s = %d\r\n",key, MaxUpTime);

    snprintf(key, sizeof(key), "d%d_mi", pin_id);  // "device_X_minutime"
    prefs.putULong(key, MinUpTime);
    //Serial.printf("Save preference %s = %d\r\n",key, MinUpTime);

  }

void Pump::LoadPreferences(Preferences& prefs, uint8_t pin_id)  {
    Relay::LoadPreferences(prefs,pin_id);

    char key[15];
    //uint8_t tmp_pin_id = GetPinId();

    snprintf(key, sizeof(key), "d%d_fr", pin_id);
    flowrate = prefs.getDouble(key, flowrate);
    Serial.printf("[%d] %s %s = %lf\r\n",pin_id,pin_name,key, flowrate);

    snprintf(key, sizeof(key), "d%d_tv", pin_id );
    tankvolume = prefs.getDouble(key, tankvolume);
    Serial.printf("[%d] %s %s = %lf\r\n",pin_id,pin_name,key, tankvolume);

    snprintf(key, sizeof(key), "d%d_tf", pin_id);
    tankfill = prefs.getDouble(key, tankfill);
    Serial.printf("[%d] %s %s = %lf\r\n",pin_id,pin_name,key, tankfill);

    snprintf(key, sizeof(key), "d%d_tl", pin_id);
    tank_level_pin = prefs.getUChar(key, tank_level_pin);
    Serial.printf("[%d] %s %s = %d\r\n",pin_id,pin_name,key, tank_level_pin);

    snprintf(key, sizeof(key), "d%d_il", pin_id);
    interlock_pin_id = prefs.getUChar(key, interlock_pin_id);
    Serial.printf("[%d] %s %s = %d\r\n",pin_id,pin_name,key, interlock_pin_id);

    snprintf(key, sizeof(key), "d%d_mu", pin_id);
    MaxUpTime = prefs.getULong(key, MaxUpTime);
    Serial.printf("[%d] %s %s = %lu\r\n",pin_id,pin_name,key, MaxUpTime);

    snprintf(key, sizeof(key), "d%d_mi", pin_id);
    MinUpTime = prefs.getULong(key, MinUpTime);
    Serial.printf("[%d] %s %s = %lu\r\n",pin_id,pin_name,key, MinUpTime);
  }

