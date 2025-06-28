// File to store all states
#include "State_Machine.h"

bool FillingPump_StartCondition()
{
  // Check water level (HIGH means that jumper is opened)
  // So if IsActive is true, this means that the water level is OK
  InputSensor *PoolWaterLevelSensor_ = static_cast<InputSensor*>(PoolDeviceManager.GetDevice(DEVICE_POOL_LEVEL));
  return (!PoolWaterLevelSensor_->IsActive());
}

bool FillingPump_StopCondition()
{
  // Check water level (HIGH means that jumper is opened)
  // So if IsActive is true, this means that the water level is OK
  InputSensor *PoolWaterLevelSensor_ = static_cast<InputSensor*>(PoolDeviceManager.GetDevice(DEVICE_POOL_LEVEL));
  Pump *FillingPump_ = static_cast<Pump*>(PoolDeviceManager.GetDevice(DEVICE_FILLING_PUMP));
  return (PoolWaterLevelSensor_->IsActive() && FillingPump_->MinUpTimeReached());
}

bool FiltrationPump_StartCondition() { 
  // If no PSI error, 
  // Either
  // - Auto mode AND filtration time is between start and stop time
  // - Winter mode AND air temperature is below -2°C
  if (PSIError) return false; // If PSI error, should never start

  return (((PMConfig.get<bool>(AUTOMODE) && hour() >= PMConfig.get<uint8_t>(FILTRATIONSTART) && hour() < PMConfig.get<uint8_t>(FILTRATIONSTOP)) ||
          (PMConfig.get<bool>(WINTERMODE) && PMData.AirTemp < -2.0)));
}


void FiltrationPump_LoopActions() {
  // Check over and under pressure alarms
  if ((((millis() - FiltrationPump.StartTime) > 180000) && (PMData.PSIValue < PMConfig.get<double>(PSI_MEDTHRESHOLD))) ||
      (PMData.PSIValue > PMConfig.get<double>(PSI_HIGHTHRESHOLD)))
  {
    PSIError = true;
    Debug.print(DBG_INFO,"[LOGIC] PSI Changed to Error");
    mqttErrorPublish("{\"PSI Error\":1}");
  }

  // Check outside Winter Mode tasks
  if (!PMConfig.get<bool>(WINTERMODE)) // NOT Winter Mode
  {
    // pH Regulation tasks
    if (PMConfig.get<bool>(PHAUTOMODE))
    {
      if ((PhPID.GetMode()==MANUAL) && ((millis() - FiltrationPump.StartTime) / 1000 / 60 >= PMConfig.get<uint8_t>(DELAYPIDS))) {
        SetPhPID(true);
        Debug.print(DBG_INFO,"[LOGIC] Activate pH PID (delay %dmn)",(millis() - FiltrationPump.StartTime) / 1000 / 60 );
      }
    }

    // Orp Regulation tasks
    if (PMConfig.get<bool>(ORPAUTOMODE))
    {
      if ((OrpPID.GetMode()==MANUAL) && ((millis() - FiltrationPump.StartTime) / 1000 / 60 >= PMConfig.get<uint8_t>(DELAYPIDS))) {
        SetOrpPID(true);
        Debug.print(DBG_INFO,"[LOGIC] Activate Orp PID (delay %dmn)",(millis() - FiltrationPump.StartTime) / 1000 / 60 );
      }
    }
  }
}

bool FiltrationPump_StopCondition() { 
  // If temp <= +2°C, never stop
  // If PSI error, should stop immediately
  // If Auto mode AND filtration time is outside start and stop time

  if(PMData.AirTemp <= 2.0) return false;

  if (PSIError) return true;

  return (PMConfig.get<bool>(AUTOMODE) && (hour() >= PMConfig.get<uint8_t>(FILTRATIONSTOP) || hour() < PMConfig.get<uint8_t>(FILTRATIONSTART)));
}

bool RobotPump_StartCondition(void) { 
  // Robot Pump should start if:
  // - Auto mode is ON
  // - Filtration Pump has been running for at least ROBOT_DELAY minutes
  // - Cleaning is not done for the day yet
  return (PMConfig.get<bool>(AUTOMODE) && ((millis() - FiltrationPump.StartTime) / 1000 / 60) >= ROBOT_DELAY && !cleaning_done);
}

bool RobotPump_StopCondition(void) { 
  // Robot Pump should stop if:
  // - It has been running for at least ROBOT_DURATION minutes
  return (PMConfig.get<bool>(AUTOMODE) && (RobotPump.GetUpTime() / 60) >= ROBOT_DURATION);
}

bool SWGPump_StartCondition() {
  // SWG Pump should start if:
  // - Electrolyse mode is ON AND
  // - if ElectroRunMode is ON
  //    - Water temperature is above SecureElectro threshold
  //    - DelayElectro minutes have passed since Filtration Pump started
  //    - ORP value is below 90% of the set point
  // - if ElectroRunMode is OFF
  //    - Water temperature is above SecureElectro threshold
  //    - DelayElectro minutes have passed since Filtration Pump started
  //      If ElectroRunMode is OFF, it will be switched on automatically with filtration and off by the Filtration Pump interlock
  if (PMConfig.get<bool>(ELECTROLYSEMODE)) {
    if (PMConfig.get<bool>(ELECTRORUNMODE)) { //SWG Run Mode is ADJUSTED
      return (PMData.OrpValue <= PMData.Orp_SetPoint * 0.9 && PMData.WaterTemp >= PMConfig.get<uint8_t>(SECUREELECTRO) &&
              (millis() - FiltrationPump.StartTime) / 1000 / 60 >= PMConfig.get<uint8_t>(DELAYELECTRO));
    } else {                      //SWG Run Mode is FIXED
      return (PMData.WaterTemp >= PMConfig.get<uint8_t>(SECUREELECTRO) &&
              (millis() - FiltrationPump.StartTime) / 1000 / 60 >= PMConfig.get<uint8_t>(DELAYELECTRO));
    }
  }
  return false; // SWG Pump should not start if Electrolyse mode is OFF
}

bool SWGPump_StopCondition() {
  // SWG Pump should stop if:
  // - Electrolyse mode is ON AND
  // - if ElectroRunMode is ON
  //    - ORP value is above the set point
  if (PMConfig.get<bool>(ELECTROLYSEMODE)) {
    if (PMConfig.get<bool>(ELECTRORUNMODE)) { //SWG Run Mode is ADJUSTED
      return (PMData.OrpValue > PMData.Orp_SetPoint);
    } else {                      //SWG Run Mode is FIXED
      // SWG Pump will be stopped by the Filtration Pump interlock
      return false; // Nothing to do, will be stopped automatically
    }
  }
  return false; // SWG Pump should not start if Electrolyse mode is OFF
}

// What to do when the pump starts or stops (only message displayed on console but could be used to trigger other actions)
void FiltrationPump_StartAction(void) { Debug.print(DBG_INFO,"[LOGIC] Start %s",FiltrationPump.GetName()); }
void FiltrationPump_StopAction(void) { Debug.print(DBG_INFO,"[LOGIC] Stop %s",FiltrationPump.GetName()); }

void RobotPump_StartAction(void) { Debug.print(DBG_INFO,"[LOGIC] Start %s",RobotPump.GetName()); }
void RobotPump_StopAction(void) { Debug.print(DBG_INFO,"[LOGIC] Stop %s",RobotPump.GetName()); RobotPump.ResetUpTime(); cleaning_done = true; }

void SWGPump_StartAction(void) { Debug.print(DBG_INFO,"[LOGIC] Start %s",SWGPump.GetName()); }
void SWGPump_StopAction(void) { Debug.print(DBG_INFO,"[LOGIC] Stop %s",SWGPump.GetName()); }

void FillingPump_StartAction(void) { Debug.print(DBG_INFO,"[LOGIC] Start %s",FillingPump.GetName()); }
void FillingPump_StopAction(void) { Debug.print(DBG_INFO,"[LOGIC] Stop %s",FillingPump.GetName()); }