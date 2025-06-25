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

bool FiltrationPump_StartCondition(void) { return false; }
bool FiltrationPump_StopCondition(void) { return false; }
bool RobotPump_StartCondition(void) { return false; }
bool RobotPump_StopCondition(void) { return false; }
bool SWGPump_StartCondition(void) { return false; }
bool SWGPump_StopCondition(void) { return false; }

// What to do when the pump starts or stops (only message displayed on console but could be used to trigger other actions)
void FiltrationPump_StartAction(void) { Debug.print(DBG_INFO,"[LOGIC] Start %s",FiltrationPump.GetName()); }
void FiltrationPump_StopAction(void) { Debug.print(DBG_INFO,"[LOGIC] Stop %s",FiltrationPump.GetName()); }

void RobotPump_StartAction(void) { Debug.print(DBG_INFO,"[LOGIC] Start %s",RobotPump.GetName()); }
void RobotPump_StopAction(void) { Debug.print(DBG_INFO,"[LOGIC] Stop %s",RobotPump.GetName()); }

void SWGPump_StartAction(void) { Debug.print(DBG_INFO,"[LOGIC] Start %s",SWGPump.GetName()); }
void SWGPump_StopAction(void) { Debug.print(DBG_INFO,"[LOGIC] Stop %s",SWGPump.GetName()); }

void FillingPump_StartAction(void) { Debug.print(DBG_INFO,"[LOGIC] Start %s",FillingPump.GetName()); }
void FillingPump_StopAction(void) { Debug.print(DBG_INFO,"[LOGIC] Stop %s",FillingPump.GetName()); }