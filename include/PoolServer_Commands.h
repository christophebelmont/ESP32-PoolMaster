#ifndef POOLMASTER_SRVCOMMANDS_H
#define POOLMASTER_SRVCOMMANDS_H

// File to store all commands processed via JSON
#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include <map>
#include <functional>

// Save Param prototypes
extern bool savePumpsConf(void);
extern bool saveParam(const char*,uint8_t );
extern bool saveParam(const char*,bool );
extern bool saveParam(const char*,unsigned long );
extern bool saveParam(const char*,double );
extern bool saveParam(const char*,u_int);
extern bool saveParam(const char*,char*);
extern bool saveParam(const char*,IPAddress);
extern void PublishSettings(void);
extern void mqttErrorPublish(const char*);
extern void SetPhPID(bool);
extern void SetOrpPID(bool);
extern void mqttInit(void);
extern void mqttDisconnect(void);

extern const std::map<std::string, std::function<void(StaticJsonDocument<250> &_jsonsdoc)>> server_handlers;

void p_Buzzer(StaticJsonDocument<250>  &_jsonsdoc);
void p_Lang(StaticJsonDocument<250>  &_jsonsdoc);
void p_TempExt(StaticJsonDocument<250>  &_jsonsdoc);
void p_pHCalib(StaticJsonDocument<250>  &_jsonsdoc);
void p_OrpCalib(StaticJsonDocument<250>  &_jsonsdoc);
void p_PSICalib(StaticJsonDocument<250>  &_jsonsdoc);
void p_Mode(StaticJsonDocument<250>  &_jsonsdoc);
void p_Electrolyse(StaticJsonDocument<250>  &_jsonsdoc);
void p_ElectrolyseMode(StaticJsonDocument<250>  &_jsonsdoc);
void p_Winter(StaticJsonDocument<250>  &_jsonsdoc);
void p_PhSetPoint(StaticJsonDocument<250>  &_jsonsdoc);
void p_OrpSetPoint(StaticJsonDocument<250>  &_jsonsdoc);
void p_WSetPoint(StaticJsonDocument<250>  &_jsonsdoc);
void p_pHTank(StaticJsonDocument<250>  &_jsonsdoc);
void p_ChlTank(StaticJsonDocument<250>  &_jsonsdoc);
void p_WTempLow(StaticJsonDocument<250>  &_jsonsdoc);
void p_PumpsMaxUp(StaticJsonDocument<250>  &_jsonsdoc);
void p_PumpMaxUp(StaticJsonDocument<250>  &_jsonsdoc);
void p_FillMinUpTime(StaticJsonDocument<250>  &_jsonsdoc);
void p_FillMaxUpTime(StaticJsonDocument<250>  &_jsonsdoc);
void p_OrpPIDParams(StaticJsonDocument<250>  &_jsonsdoc);
void p_PhPIDParams(StaticJsonDocument<250>  &_jsonsdoc);
void p_OrpPIDWSize(StaticJsonDocument<250>  &_jsonsdoc);
void p_PhPIDWSize(StaticJsonDocument<250>  &_jsonsdoc);
void p_Date(StaticJsonDocument<250>  &_jsonsdoc);
void p_FiltT0(StaticJsonDocument<250>  &_jsonsdoc);
void p_FiltT1(StaticJsonDocument<250>  &_jsonsdoc);
void p_PubPeriod(StaticJsonDocument<250>  &_jsonsdoc);
void p_DelayPID(StaticJsonDocument<250>  &_jsonsdoc);
void p_PSIHigh(StaticJsonDocument<250>  &_jsonsdoc);
void p_PSILow(StaticJsonDocument<250>  &_jsonsdoc);
void p_pHPumpFR(StaticJsonDocument<250>  &_jsonsdoc);
void p_ChlPumpFR(StaticJsonDocument<250>  &_jsonsdoc);
void p_RstpHCal(StaticJsonDocument<250>  &_jsonsdoc);
void p_RstOrpCal(StaticJsonDocument<250>  &_jsonsdoc);
void p_RstPSICal(StaticJsonDocument<250>  &_jsonsdoc);
void p_Settings(StaticJsonDocument<250>  &_jsonsdoc);
void p_FiltPump(StaticJsonDocument<250>  &_jsonsdoc);
void p_RobotPump(StaticJsonDocument<250>  &_jsonsdoc);
void p_PhPump(StaticJsonDocument<250>  &_jsonsdoc);
void p_FillPump(StaticJsonDocument<250>  &_jsonsdoc);
void p_ChlPump(StaticJsonDocument<250>  &_jsonsdoc);
void p_PhPID(StaticJsonDocument<250>  &_jsonsdoc);
void p_PhAutoMode(StaticJsonDocument<250>  &_jsonsdoc);
void p_OrpPID(StaticJsonDocument<250>  &_jsonsdoc);
void p_OrpAutoMode(StaticJsonDocument<250>  &_jsonsdoc);
void p_FillAutoMode(StaticJsonDocument<250>  &_jsonsdoc);
void p_Relay(StaticJsonDocument<250>  &_jsonsdoc);
void p_Reboot(StaticJsonDocument<250>  &_jsonsdoc);
void p_Clear(StaticJsonDocument<250>  &_jsonsdoc);
void p_ElectroConfig(StaticJsonDocument<250>  &_jsonsdoc);
void p_SecureElectro(StaticJsonDocument<250>  &_jsonsdoc);
void p_DelayElectro(StaticJsonDocument<250>  &_jsonsdoc);
void p_ElectroRunMode(StaticJsonDocument<250>  &_jsonsdoc);
void p_ElectroRuntime(StaticJsonDocument<250>  &_jsonsdoc);
void p_SetDateTime(StaticJsonDocument<250>  &_jsonsdoc);
void p_WifiConfig(StaticJsonDocument<250>  &_jsonsdoc);
void p_MQTTConfig(StaticJsonDocument<250>  &_jsonsdoc);
void p_SMTPConfig(StaticJsonDocument<250>  &_jsonsdoc);
void p_PINConfig(StaticJsonDocument<250>  &_jsonsdoc);


#endif