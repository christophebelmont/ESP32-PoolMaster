#include "Arduino.h"
#include "Pump.h"

//Call this in the main loop, for every loop, as often as possible
void Pump::loop()
{
  if (shouldStartHandler && shouldStartHandler() && !IsRunning()) {
      Start();
  }

  if (shouldStopHandler && shouldStopHandler() && IsRunning()) {
      Stop();
  }

  if(IsRunning())
  {
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
bool Pump::Start(bool _resetUpTime)
{
  if (_resetUpTime) {
    ResetUpTime(); // Reset UpTime if requested
  }

  if((!IsRunning()) && !UpTimeError && TankLevel() && CheckInterlock())
  {
    if (!this->Relay::Enable())
      return false;
    
    LastLoopMillis = StartTime = millis(); 

    if (onStartHandler) onStartHandler(); // Appel du handler

    return true; 
  } else return false;
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

    if (onStopHandler) onStopHandler(); // Appel du handler

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
  LastLoopMillis = 0;
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

void Pump::SavePreferences(Preferences& prefs)  {
    Relay::SavePreferences(prefs);

    char key[15];
    uint8_t tmp_pin_id = GetPinId();

    snprintf(key, sizeof(key), "d%d_fr", tmp_pin_id);  // "device_X_flowrate"
    prefs.putDouble(key, flowrate);
    //Serial.printf("Save preference %s = %d\r\n",key, flowrate);

    snprintf(key, sizeof(key), "d%d_tv", tmp_pin_id);  // "device_X_tankvolume"
    prefs.putDouble(key, tankvolume);
    //Serial.printf("Save preference %s = %d\r\n",key, tankvolume);

    snprintf(key, sizeof(key), "d%d_tf", tmp_pin_id);  // "device_X_tankfill"
    prefs.putDouble(key, tankfill);
    //Serial.printf("Save preference %s = %d\r\n",key, tankfill);

    snprintf(key, sizeof(key), "d%d_tl", tmp_pin_id);  // "device_X_tanklevelpin"
    prefs.putUChar(key, tank_level_pin);
    //Serial.printf("Save preference %s = %d\r\n",key, tank_level_pin);

    snprintf(key, sizeof(key), "d%d_il", tmp_pin_id);  // "device_X_interlockid"
    prefs.putUChar(key, interlock_pin_id);
    //Serial.printf("Save preference %s = %d\r\n",key, interlock_pin_id);

    snprintf(key, sizeof(key), "d%d_mu", tmp_pin_id);  // "device_X_maxutime"
    prefs.putULong(key, MaxUpTime);
    //Serial.printf("Save preference %s = %d\r\n",key, MaxUpTime);

    snprintf(key, sizeof(key), "d%d_mi", tmp_pin_id);  // "device_X_minutime"
    prefs.putULong(key, MinUpTime);
    //Serial.printf("Save preference %s = %d\r\n",key, MinUpTime);

  }

void Pump::LoadPreferences(Preferences& prefs)  {
    Relay::LoadPreferences(prefs);

    char key[15];
    uint8_t tmp_pin_id = GetPinId();

    snprintf(key, sizeof(key), "d%d_fr", tmp_pin_id);
    flowrate = prefs.getDouble(key, flowrate);
    //Serial.printf("Read preference %s = %d\r\n",key, flowrate);

    snprintf(key, sizeof(key), "d%d_tv", tmp_pin_id );
    tankvolume = prefs.getDouble(key, tankvolume);
    //Serial.printf("Read preference %s = %d\r\n",key, tankvolume);

    snprintf(key, sizeof(key), "d%d_tf", tmp_pin_id);
    tankfill = prefs.getDouble(key, tankfill);
    //Serial.printf("Read preference %s = %d\r\n",key, tankfill);

    snprintf(key, sizeof(key), "d%d_tl", tmp_pin_id);
    tank_level_pin = prefs.getUChar(key, tank_level_pin);
    //Serial.printf("Read preference %s = %d\r\n",key, tank_level_pin);

    snprintf(key, sizeof(key), "d%d_il", tmp_pin_id);
    interlock_pin_id = prefs.getUChar(key, interlock_pin_id);
    //Serial.printf("Read preference %s = %d\r\n",key, interlock_pin_id);

    snprintf(key, sizeof(key), "d%d_mu", tmp_pin_id);
    MaxUpTime = prefs.getULong(key, MaxUpTime);
    //Serial.printf("Read preference %s = %d\r\n",key, MaxUpTime);

    snprintf(key, sizeof(key), "d%d_mi", tmp_pin_id);
    MinUpTime = prefs.getULong(key, MinUpTime);
    //Serial.printf("Read preference %s = %d\r\n",key, MinUpTime);
  }

