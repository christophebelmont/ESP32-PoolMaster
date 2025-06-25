#ifndef POOLMASTER_STATEMACHINE_H
#define POOLMASTER_STATEMACHINE_H

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include <map>
#include <functional>

extern DeviceManager PoolDeviceManager;

bool FillingPump_StartCondition(void);
bool FillingPump_StopCondition(void);
void FillingPump_StartAction(void);
void FillingPump_StopAction(void);

bool FiltrationPump_StartCondition(void);
bool FiltrationPump_StopCondition(void);
void FiltrationPump_StartAction(void);
void FiltrationPump_StopAction(void);

bool RobotPump_StartCondition(void);
bool RobotPump_StopCondition(void);
void RobotPump_StartAction(void);
void RobotPump_StopAction(void);

bool SWGPump_StartCondition(void);
bool SWGPump_StopCondition(void);
void SWGPump_StartAction(void);
void SWGPump_StopAction(void);

#endif