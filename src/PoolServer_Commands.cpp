// File to store all commands
#include "PoolServer_Commands.h"

// Map key -> handler function
const std::map<std::string, std::function<void(StaticJsonDocument<250> &_jsonsdoc)>> server_handlers = {
    {"Buzzer",          p_Buzzer         },
    {"Lang_Locale",     p_Lang           },
    {"TempExt",         p_TempExt        },
    {"pHCalib",         p_pHCalib        },
    {"OrpCalib",        p_OrpCalib       },
    {"PSICalib",        p_PSICalib       },
    {"Mode",            p_Mode           },
    {"Electrolyse",     p_Electrolyse    },
    {"ElectrolyseMode", p_ElectrolyseMode},
    {"Winter",          p_Winter         },
    {"PhSetPoint",      p_PhSetPoint     },
    {"OrpSetPoint",     p_OrpSetPoint    },
    {"WSetPoint",       p_WSetPoint      },
    {"pHTank",          p_pHTank         },
    {"ChlTank",         p_ChlTank        },
    {"WTempLow",        p_WTempLow       },
    {"PumpsMaxUp",      p_PumpsMaxUp     },
    {"FillMinUpTime",   p_FillMinUpTime  },
    {"FillMaxUpTime",   p_FillMaxUpTime  },
    {"OrpPIDParams",    p_OrpPIDParams   },
    {"PhPIDParams",     p_PhPIDParams    },
    {"OrpPIDWSize",     p_OrpPIDWSize    },
    {"PhPIDWSize",      p_PhPIDWSize     },
    {"Date",            p_Date           },
    {"FiltT0",          p_FiltT0         },
    {"FiltT1",          p_FiltT1         },
    {"PubPeriod",       p_PubPeriod      },
    {"DelayPID",        p_DelayPID       },
    {"PSIHigh",         p_PSIHigh        },
    {"PSILow",          p_PSILow         },
    {"pHPumpFR",        p_pHPumpFR       },
    {"ChlPumpFR",       p_ChlPumpFR      },
    {"RstpHCal",        p_RstpHCal       },
    {"RstOrpCal",       p_RstOrpCal      },
    {"RstPSICal",       p_RstPSICal      },
    {"Settings",        p_Settings       },
    {"FiltPump",        p_FiltPump       },
    {"RobotPump",       p_RobotPump      },
    {"PhPump",          p_PhPump         },
    {"FillPump",        p_FillPump       },
    {"ChlPump",         p_ChlPump        },
    {"PhPID",           p_PhPID          },
    {"PhAutoMode",      p_PhAutoMode     },
    {"OrpPID",          p_OrpPID         },
    {"OrpAutoMode",     p_OrpAutoMode    },
    {"Relay",           p_Relay          },
    {"Reboot",          p_Reboot         },
    {"Clear",           p_Clear          },
    {"ElectroConfig",   p_ElectroConfig  },
    {"SecureElectro",   p_SecureElectro  },
    {"DelayElectro",    p_DelayElectro   },
    {"ElectroRunMode",  p_ElectroRunMode },
    {"ElectroRuntime",  p_ElectroRuntime },
    {"SetDateTime",     p_SetDateTime    },
    {"WifiConfig",      p_WifiConfig     },
    {"MQTTConfig",      p_MQTTConfig     },
    {"SMTPConfig",      p_SMTPConfig     },
    {"PINConfig",       p_PINConfig      }
};

/* All JSON commands functions definition */
void p_Buzzer(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.BuzzerOn = (bool)_jsonsdoc[F("Buzzer")];
    saveParam("BuzzerOn",storage.BuzzerOn);
}

void p_Lang(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.Lang_Locale = (uint8_t)_jsonsdoc[F("Lang_Locale")];
    saveParam("Lang_Locale",storage.Lang_Locale);
}

void p_TempExt(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.AirTemp = _jsonsdoc[F("TempExt")].as<float>();
}

//{"pHCalib":[4.02,3.8,9.0,9.11]}  -> multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
void p_pHCalib(StaticJsonDocument<250>  &_jsonsdoc) {
    float CalibPoints[12]; //Max six calibration point-couples! Should be plenty enough
    int NbPoints = (int)copyArray(_jsonsdoc[F("pHCalib")].as<JsonArray>(),CalibPoints);        
    Debug.print(DBG_DEBUG,"pHCalib command - %d points received",NbPoints);
    for (int i = 0; i < NbPoints; i += 2)
      Debug.print(DBG_DEBUG,"%10.2f - %10.2f",CalibPoints[i],CalibPoints[i + 1]);

    if (NbPoints == 2) //Only one pair of points. Perform a simple offset calibration
    {
      Debug.print(DBG_DEBUG,"2 points. Performing a simple offset calibration");

      //compute offset correction
      storage.pHCalibCoeffs1 += CalibPoints[1] - CalibPoints[0];

      //Set slope back to default value
      storage.pHCalibCoeffs0 = 3.76;

      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",storage.pHCalibCoeffs0,storage.pHCalibCoeffs1);
    }
    else if ((NbPoints > 3) && (NbPoints % 2 == 0)) //we have at least 4 points as well as an even number of points. Perform a linear regression calibration
    {
      Debug.print(DBG_DEBUG,"%d points. Performing a linear regression calibration",NbPoints / 2);

      float xCalibPoints[NbPoints / 2];
      float yCalibPoints[NbPoints / 2];

      //generate array of x sensor values (in volts) and y rated buffer values
      //storage.PhValue = (storage.pHCalibCoeffs0 * ph_sensor_value) + storage.pHCalibCoeffs1;
      for (int i = 0; i < NbPoints; i += 2)
      {
        xCalibPoints[i / 2] = (CalibPoints[i] - storage.pHCalibCoeffs1) / storage.pHCalibCoeffs0;
        yCalibPoints[i / 2] = CalibPoints[i + 1];
      }

      //Compute linear regression coefficients
      simpLinReg(xCalibPoints, yCalibPoints, storage.pHCalibCoeffs0, storage.pHCalibCoeffs1, NbPoints / 2);

      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",storage.pHCalibCoeffs0 ,storage.pHCalibCoeffs1);
    }
    //Store the new coefficients in eeprom
    saveParam("pHCalibCoeffs0",storage.pHCalibCoeffs0);
    saveParam("pHCalibCoeffs1",storage.pHCalibCoeffs1);          
    PublishSettings();
}

//{"OrpCalib":[450,465,750,784]}   -> multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
void p_OrpCalib(StaticJsonDocument<250>  &_jsonsdoc) {
    float CalibPoints[12]; //Max six calibration point-couples! Should be plenty enough
    int NbPoints = (int)copyArray(_jsonsdoc[F("OrpCalib")].as<JsonArray>(),CalibPoints);
    Debug.print(DBG_DEBUG,"OrpCalib command - %d points received",NbPoints);
    for (int i = 0; i < NbPoints; i += 2)
      Debug.print(DBG_DEBUG,"%10.2f - %10.2f",CalibPoints[i],CalibPoints[i + 1]);        
    if (NbPoints == 2) //Only one pair of points. Perform a simple offset calibration
    {
      Debug.print(DBG_DEBUG,"2 points. Performing a simple offset calibration");

      //compute offset correction
      storage.OrpCalibCoeffs1 += CalibPoints[1] - CalibPoints[0];

      //Set slope back to default value
      storage.OrpCalibCoeffs0 = -1000;

      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",storage.OrpCalibCoeffs0,storage.OrpCalibCoeffs1);
    }
    else if ((NbPoints > 3) && (NbPoints % 2 == 0)) //we have at least 4 points as well as an even number of points. Perform a linear regression calibration
    {
      Debug.print(DBG_DEBUG,"%d points. Performing a linear regression calibration",NbPoints / 2);

      float xCalibPoints[NbPoints / 2];
      float yCalibPoints[NbPoints / 2];

      //generate array of x sensor values (in volts) and y rated buffer values
      //storage.OrpValue = (storage.OrpCalibCoeffs0 * orp_sensor_value) + storage.OrpCalibCoeffs1;
      for (int i = 0; i < NbPoints; i += 2)
      {
        xCalibPoints[i / 2] = (CalibPoints[i] - storage.OrpCalibCoeffs1) / storage.OrpCalibCoeffs0;
        yCalibPoints[i / 2] = CalibPoints[i + 1];
      }

      //Compute linear regression coefficients
      simpLinReg(xCalibPoints, yCalibPoints, storage.OrpCalibCoeffs0, storage.OrpCalibCoeffs1, NbPoints / 2);

      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",storage.OrpCalibCoeffs0,storage.OrpCalibCoeffs1);
    }
    //Store the new coefficients in eeprom
    saveParam("OrpCalibCoeffs0",storage.OrpCalibCoeffs0);
    saveParam("OrpCalibCoeffs1",storage.OrpCalibCoeffs1);          
    PublishSettings();
}

//{"PSICalib":[0,0,0.71,0.6]}   -> multi-point linear regression calibration (minimum 2 point-couple, 6 max.) in the form [ElectronicPressureSensorReading_0, MechanicalPressureSensorReading_0, xx, xx, ElectronicPressureSensorReading_n, ElectronicPressureSensorReading_n]
void p_PSICalib(StaticJsonDocument<250>  &_jsonsdoc) {
    float CalibPoints[12];//Max six calibration point-couples! Should be plenty enough, typically use two point-couples (filtration ON and filtration OFF)
    int NbPoints = (int)copyArray(_jsonsdoc[F("PSICalib")].as<JsonArray>(),CalibPoints);
    Debug.print(DBG_DEBUG,"PSICalib command - %d points received",NbPoints);
    for (int i = 0; i < NbPoints; i += 2)
      Debug.print(DBG_DEBUG,"%10.2f, %10.2f",CalibPoints[i],CalibPoints[i + 1]);

    if ((NbPoints > 3) && (NbPoints % 2 == 0)) //we have at least 4 points as well as an even number of points. Perform a linear regression calibration
    {
      Debug.print(DBG_DEBUG,"%d points. Performing a linear regression calibration",NbPoints / 2);

      float xCalibPoints[NbPoints / 2];
      float yCalibPoints[NbPoints / 2];

      //generate array of x sensor values (in volts) and y rated buffer values
      //storage.OrpValue = (storage.OrpCalibCoeffs0 * orp_sensor_value) + storage.OrpCalibCoeffs1;
      //storage.PSIValue = (storage.PSICalibCoeffs0 * psi_sensor_value) + storage.PSICalibCoeffs1;
      for (int i = 0; i < NbPoints; i += 2)
      {
        xCalibPoints[i / 2] = (CalibPoints[i] - storage.PSICalibCoeffs1) / storage.PSICalibCoeffs0;
        yCalibPoints[i / 2] = CalibPoints[i + 1];
      }

      //Compute linear regression coefficients
      simpLinReg(xCalibPoints, yCalibPoints, storage.PSICalibCoeffs0, storage.PSICalibCoeffs1, NbPoints / 2);

      //Store the new coefficients in eeprom
      saveParam("PSICalibCoeffs0",storage.PSICalibCoeffs0);
      saveParam("PSICalibCoeffs1",storage.PSICalibCoeffs1);          
      PublishSettings();
      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",storage.PSICalibCoeffs0,storage.PSICalibCoeffs1);
    }
}

void p_Mode(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.AutoMode = (bool)_jsonsdoc[F("Mode")];
    if (!storage.AutoMode) // Stop PIDs if manual mode
    {
      SetPhPID(false);
      SetOrpPID(false);
    }
    saveParam("AutoMode",storage.AutoMode);
    PublishSettings();
}
void p_Electrolyse(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((int)_jsonsdoc[F("Electrolyse")] == 1)  // activate electrolyse
    {
      // start electrolyse if not below minimum temperature
      // do not take care of minimum filtering time as it 
      // was forced on.
      if (storage.WaterTemp >= (double)storage.SecureElectro)
        if (!SWGPump.Start())
          Debug.print(DBG_WARNING,"Problem starting SWG");   
    } else {
      if (!SWGPump.Stop())
        Debug.print(DBG_WARNING,"Problem stopping SWG");   
    }
    // Direct action on Electrolyse will exit the automatic Electro Regulation Mode
    storage.ElectrolyseMode = 0;
    saveParam("ElectrolyseMode",storage.ElectrolyseMode);
    PublishSettings();
}
void p_ElectrolyseMode(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.ElectrolyseMode = (int)_jsonsdoc[F("ElectrolyseMode")];
    saveParam("ElectrolyseMode",storage.ElectrolyseMode);
    PublishSettings();
}
void p_Winter(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.WinterMode = (bool)_jsonsdoc[F("Winter")];
    saveParam("WinterMode",storage.WinterMode);
    PublishSettings();
}
void p_PhSetPoint(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.Ph_SetPoint = _jsonsdoc[F("PhSetPoint")].as<double>();
    saveParam("Ph_SetPoint",storage.Ph_SetPoint);
    PublishSettings();
}
void p_OrpSetPoint(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.Orp_SetPoint = _jsonsdoc[F("OrpSetPoint")].as<double>();
    saveParam("Orp_SetPoint",storage.Orp_SetPoint);
    PublishSettings();
}
void p_WSetPoint(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.WaterTemp_SetPoint = (double)_jsonsdoc[F("WSetPoint")];
    saveParam("WaterTempSet",storage.WaterTemp_SetPoint);
    PublishSettings();
}
//"pHTank" command which is called when the pH tank is changed or refilled
//First parameter is volume of tank in Liters, second parameter is percentage Fill of the tank (typically 100% when new)

void p_pHTank(StaticJsonDocument<250>  &_jsonsdoc) {
    //storage.PumpsConfig[PUMP_PH].tank_vol = (double)_jsonsdoc[F("pHTank")][0];
    //storage.PumpsConfig[PUMP_PH].tank_fill = (double)_jsonsdoc[F("pHTank")][1];
    PhPump.SetTankVolume((double)_jsonsdoc[F("pHTank")][0]);
    PhPump.SetTankFill((double)_jsonsdoc[F("pHTank")][1]);
    PoolDeviceManager.SavePreferences(DEVICE_PH_PUMP);
    PublishSettings();
}
void p_ChlTank(StaticJsonDocument<250>  &_jsonsdoc) {
    //storage.PumpsConfig[PUMP_CHL].tank_vol = (double)_jsonsdoc[F("ChlTank")][0];
    //storage.PumpsConfig[PUMP_CHL].tank_fill = (double)_jsonsdoc[F("ChlTank")][1];
    ChlPump.SetTankVolume((double)_jsonsdoc[F("ChlTank")][0]);
    ChlPump.SetTankFill((double)_jsonsdoc[F("ChlTank")][1]);
    PoolDeviceManager.SavePreferences(DEVICE_CHL_PUMP);
    PublishSettings();
}
void p_WTempLow(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.WaterTempLowThreshold = (double)_jsonsdoc[F("WTempLow")];
    saveParam("WaterTempLowThreshold",storage.WaterTempLowThreshold);
    PublishSettings();
}
void p_PumpsMaxUp(StaticJsonDocument<250>  &_jsonsdoc) {
    //storage.PumpsConfig[PUMP_PH].pump_max_uptime = (unsigned int)_jsonsdoc[F("PumpsMaxUp")];
    //storage.PumpsConfig[PUMP_CHL].pump_max_uptime = (unsigned int)_jsonsdoc[F("PumpsMaxUp")];
    PhPump.SetMaxUpTime((unsigned int)_jsonsdoc[F("PumpsMaxUp")]*1000);
    ChlPump.SetMaxUpTime((unsigned int)_jsonsdoc[F("PumpsMaxUp")]*1000);
    PoolDeviceManager.SavePreferences(DEVICE_PH_PUMP);
    PoolDeviceManager.SavePreferences(DEVICE_CHL_PUMP);
//    savePumpsConf();

    // Apply changes
  //  PhPump.SetMaxUpTime(storage.PumpsConfig[PUMP_PH].pump_max_uptime * 1000);
    //ChlPump.SetMaxUpTime(storage.PumpsConfig[PUMP_CHL].pump_max_uptime * 1000);
    PublishSettings();
}
void p_FillMinUpTime(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.PumpsConfig[PUMP_FILL].pump_min_uptime = (unsigned int)_jsonsdoc[F("FillMinUpTime")] * 60;
    // MinUptime is not a member of pump class, this is used in main logic to prevent pump from stopping too fast
    //savePumpsConf();
    PublishSettings();
}
void p_FillMaxUpTime(StaticJsonDocument<250>  &_jsonsdoc) {
    //storage.PumpsConfig[PUMP_FILL].pump_max_uptime = (unsigned int)_jsonsdoc[F("FillMaxUpTime")] * 60; 
    FillingPump.SetMaxUpTime((unsigned int)_jsonsdoc[F("FillMaxUpTime")] * 60 * 1000);
    PoolDeviceManager.SavePreferences(DEVICE_FILLING_PUMP);
    //savePumpsConf();
    PublishSettings();
}
void p_OrpPIDParams(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.Orp_Kp = (double)_jsonsdoc[F("OrpPIDParams")][0];
    storage.Orp_Ki = (double)_jsonsdoc[F("OrpPIDParams")][1];
    storage.Orp_Kd = (double)_jsonsdoc[F("OrpPIDParams")][2];
    saveParam("Orp_Kp",storage.Orp_Kp);
    saveParam("Orp_Ki",storage.Orp_Ki);
    saveParam("Orp_Kd",storage.Orp_Kd);
    OrpPID.SetTunings(storage.Orp_Kp, storage.Orp_Ki, storage.Orp_Kd);
    PublishSettings();
}
void p_PhPIDParams(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.Ph_Kp = (double)_jsonsdoc[F("PhPIDParams")][0];
    storage.Ph_Ki = (double)_jsonsdoc[F("PhPIDParams")][1];
    storage.Ph_Kd = (double)_jsonsdoc[F("PhPIDParams")][2];
    saveParam("Ph_Kp",storage.Ph_Kp);
    saveParam("Ph_Ki",storage.Ph_Ki);
    saveParam("Ph_Kd",storage.Ph_Kd);
    PhPID.SetTunings(storage.Ph_Kp, storage.Ph_Ki, storage.Ph_Kd);
    PublishSettings();
}
void p_OrpPIDWSize(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.OrpPIDWindowSize = (unsigned long)_jsonsdoc[F("OrpPIDWSize")]*60*1000;
    saveParam("OrpPIDWSize",storage.OrpPIDWindowSize);
    OrpPID.SetSampleTime((int)storage.OrpPIDWindowSize);
    OrpPID.SetOutputLimits(0, storage.OrpPIDWindowSize);  //Whatever happens, don't allow continuous injection of Chl for more than a PID Window
    PublishSettings();
}
void p_PhPIDWSize(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.PhPIDWindowSize = (unsigned long)_jsonsdoc[F("PhPIDWSize")]*60*1000;
    saveParam("PhPIDWSize",storage.PhPIDWindowSize);
    PhPID.SetSampleTime((int)storage.PhPIDWindowSize);
    PhPID.SetOutputLimits(0, storage.PhPIDWindowSize);    //Whatever happens, don't allow continuous injection of Acid for more than a PID Window
    PublishSettings();
}
void p_Date(StaticJsonDocument<250>  &_jsonsdoc) {
    setTime((uint8_t)_jsonsdoc[F("Date")][4], (uint8_t)_jsonsdoc[F("Date")][5], (uint8_t)_jsonsdoc[F("Date")][6], (uint8_t)_jsonsdoc[F("Date")][0], (uint8_t)_jsonsdoc[F("Date")][2], (uint8_t)_jsonsdoc[F("Date")][3]); //(Day of the month, Day of the week, Month, Year, Hour, Minute, Second)
}
void p_FiltT0(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.FiltrationStartMin = (unsigned int)_jsonsdoc[F("FiltT0")];
    saveParam("FiltrStartMin",storage.FiltrationStartMin);
    PublishSettings();
}
void p_FiltT1(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.FiltrationStopMax = (unsigned int)_jsonsdoc[F("FiltT1")];
    saveParam("FiltrStopMax",storage.FiltrationStopMax);
    PublishSettings();
}
void p_PubPeriod(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.PublishPeriod = (unsigned long)_jsonsdoc[F("PubPeriod")] * 1000; //in secs
    saveParam("PublishPeriod",storage.PublishPeriod);
    PublishSettings();
}
void p_DelayPID(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.DelayPIDs = (unsigned int)_jsonsdoc[F("DelayPID")];
    saveParam("DelayPIDs",storage.DelayPIDs);
    PublishSettings();
}
void p_PSIHigh(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.PSI_HighThreshold = (double)_jsonsdoc[F("PSIHigh")];
    saveParam("PSI_High",storage.PSI_HighThreshold);
    PublishSettings();
}
void p_PSILow(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.PSI_MedThreshold = (double)_jsonsdoc[F("PSILow")];
    saveParam("PSI_Med",storage.PSI_MedThreshold);
    PublishSettings();
}
void p_pHPumpFR(StaticJsonDocument<250>  &_jsonsdoc) {
    //storage.PumpsConfig[PUMP_PH].pump_flow_rate = (double)_jsonsdoc[F("pHPumpFR")];
    //savePumpsConf();
    PhPump.SetFlowRate((double)_jsonsdoc[F("pHPumpFR")] * 1000);
    PoolDeviceManager.SavePreferences(DEVICE_PH_PUMP);
    PublishSettings();
}
void p_ChlPumpFR(StaticJsonDocument<250>  &_jsonsdoc) {
    //storage.PumpsConfig[PUMP_CHL].pump_flow_rate = (double)_jsonsdoc[F("ChlPumpFR")];
    //savePumpsConf();
    PhPump.SetFlowRate((double)_jsonsdoc[F("ChlPumpFR")] * 1000);
    PoolDeviceManager.SavePreferences(DEVICE_CHL_PUMP);
    PublishSettings();
}
void p_RstpHCal(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.pHCalibCoeffs0 = (double)-2.50133333;
    storage.pHCalibCoeffs1 = (double)6.9;
    saveParam("pHCalibCoeffs0",storage.pHCalibCoeffs0);
    saveParam("pHCalibCoeffs1",storage.pHCalibCoeffs1);
    PublishSettings();
}
void p_RstOrpCal(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.OrpCalibCoeffs0 = (double)431.03;
    storage.OrpCalibCoeffs1 = (double)0.0;
    saveParam("OrpCalibCoeffs0",storage.OrpCalibCoeffs0);
    saveParam("OrpCalibCoeffs1",storage.OrpCalibCoeffs1);          
    PublishSettings();
}
void p_RstPSICal(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.PSICalibCoeffs0 = (double)0.377923399;
    storage.PSICalibCoeffs1 = (double)-0.17634473;
    saveParam("PSICalibCoeffs0",storage.PSICalibCoeffs0);
    saveParam("PSICalibCoeffs1",storage.PSICalibCoeffs1);          
    PublishSettings();
}
void p_Settings(StaticJsonDocument<250>  &_jsonsdoc) {
    PublishSettings();
}
void p_FiltPump(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("FiltPump")])
    {
        FiltrationPump.Start();   //start filtration pump
    }
    else
    {
        FiltrationPump.Stop();  //stop filtration pump

        //Stop PIDs
        SetPhPID(false);
        SetOrpPID(false);
    }
    storage.AutoMode = 0;   // Manually changing pump operation disables the automatic mode
    saveParam("AutoMode",storage.AutoMode);
    PublishSettings();
}
void p_RobotPump(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("RobotPump")]){
        RobotPump.Start();   //start robot pump
        cleaning_done = false;
    } else {
        RobotPump.Stop();    //stop robot pump
        cleaning_done = true;
    }  
}
void p_PhPump(StaticJsonDocument<250>  &_jsonsdoc) {
    // If pH Pump commanded manually, stop the automode
    if ((bool)_jsonsdoc[F("PhPump")])
        PhPump.Start();      //start Acid pump
    else
        PhPump.Stop();       //stop Acid pump
    
    storage.pHAutoMode = 0;
    saveParam("pHAutoMode",storage.pHAutoMode);
    if (storage.pHAutoMode == 0) SetPhPID(false);

    PublishSettings();
}
void p_FillPump(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("FillPump")])
        FillingPump.Start();      //start swimming pool filling pump
    else
        FillingPump.Stop();       //stop swimming pool filling pump

}
void p_ChlPump(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("ChlPump")])
        ChlPump.Start();     //start Chl pump  
    else
        ChlPump.Stop();      //stop Chl pump      

    storage.OrpAutoMode = 0;
    saveParam("OrpAutoMode",storage.OrpAutoMode);
    if (storage.OrpAutoMode == 0) SetOrpPID(false);
    PublishSettings();
}
void p_PhPID(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("PhPID")])
        SetPhPID(true);
    else
        SetPhPID(false);
}
void p_PhAutoMode(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.pHAutoMode = (int)_jsonsdoc[F("PhAutoMode")];
    saveParam("pHAutoMode",storage.pHAutoMode);
    if (storage.pHAutoMode == 0) SetPhPID(false);
    PublishSettings();
}
void p_OrpPID(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("OrpPID")])
        SetOrpPID(true);
    else
        SetOrpPID(false);
}
void p_OrpAutoMode(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.OrpAutoMode = (bool)_jsonsdoc[F("OrpAutoMode")];
    saveParam("OrpAutoMode",storage.OrpAutoMode);
    if (storage.OrpAutoMode == 0) SetOrpPID(false);
    PublishSettings();
}
//"Relay" command which is called to actuate relays
//Parameter 1 is the relay number (R0 in this example), parameter 2 is the relay state (ON in this example).
void p_Relay(StaticJsonDocument<250>  &_jsonsdoc) {
    switch ((int)_jsonsdoc[F("Relay")][0])
    {
      case 0:
        (bool)_jsonsdoc[F("Relay")][1] ? RELAYR0.Enable() : RELAYR0.Disable();
        break;
      case 1:
        (bool)_jsonsdoc[F("Relay")][1] ? RELAYR1.Enable()  : RELAYR1.Disable();
        break;
    }
}
void p_Reboot(StaticJsonDocument<250>  &_jsonsdoc) {
    delay(REBOOT_DELAY); // wait 10s then restart. Other tasks continue.
    esp_restart();
}
void p_Clear(StaticJsonDocument<250>  &_jsonsdoc) {
    if (PSIError)
        PSIError = false;

    if (PhPump.UpTimeError)
        PhPump.ClearErrors();

    if (ChlPump.UpTimeError)
        ChlPump.ClearErrors();

    if (FillingPump.UpTimeError)
        FillingPump.ClearErrors();

    mqttErrorPublish(""); // publish clearing of error(s)
}
//"ElectroConfig" command which is called when the Electrolyser is configured
//First parameter is minimum temperature to use the Electrolyser, second is the delay after pump start
void p_ElectroConfig(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.SecureElectro = (uint8_t)_jsonsdoc[F("ElectroConfig")][0];
    storage.DelayElectro = (uint8_t)_jsonsdoc[F("ElectroConfig")][1];
    saveParam("SecureElectro",storage.SecureElectro);
    saveParam("DelayElectro",storage.DelayElectro);               
    PublishSettings();
}
void p_SecureElectro(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.SecureElectro = (uint8_t)_jsonsdoc[F("SecureElectro")];
    saveParam("SecureElectro",storage.SecureElectro);
    PublishSettings();
}
void p_DelayElectro(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.DelayElectro = (uint8_t)_jsonsdoc[F("DelayElectro")];
    saveParam("DelayElectro",storage.DelayElectro);
    PublishSettings();
}
void p_ElectroRunMode(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.ElectroRunMode = (bool)_jsonsdoc[F("ElectroRunMode")];
    saveParam("ElectroRunMode",storage.ElectroRunMode);
    PublishSettings();
}
void p_ElectroRuntime(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.ElectroRuntime = (int)_jsonsdoc[F("ElectroRuntime")];
    saveParam("ElectroRuntime",storage.ElectroRuntime);
    PublishSettings();
}
void p_SetDateTime(StaticJsonDocument<250>  &_jsonsdoc) {
    int	rtc_hour = (int)_jsonsdoc[F("SetDateTime")][0];
    int	rtc_min = (int)_jsonsdoc[F("SetDateTime")][1];
    int	rtc_sec = (int)_jsonsdoc[F("SetDateTime")][2];
    int	rtc_mday = (int)_jsonsdoc[F("SetDateTime")][3];
    int	rtc_mon = (int)_jsonsdoc[F("SetDateTime")][4];
    int	rtc_year = (int)_jsonsdoc[F("SetDateTime")][5];
    setTime(rtc_hour,rtc_min,rtc_sec,rtc_mday,rtc_mon+1,rtc_year);
}
void p_WifiConfig(StaticJsonDocument<250>  &_jsonsdoc) {
    const char* WIFI_SSID = _jsonsdoc[F("WifiConfig")][0];
    const char* WIFI_PASS = _jsonsdoc[F("WifiConfig")][1];
    WiFi.begin(WIFI_SSID, WIFI_PASS);
}
void p_MQTTConfig(StaticJsonDocument<250>  &_jsonsdoc) {
    storage.MQTT_IP.fromString((const char*)_jsonsdoc[F("MQTTConfig")][0]);
    storage.MQTT_PORT = (uint)_jsonsdoc[F("MQTTConfig")][1];
    strcpy(storage.MQTT_LOGIN,_jsonsdoc[F("MQTTConfig")][2]);
    strcpy(storage.MQTT_PASS,_jsonsdoc[F("MQTTConfig")][3]);
    strcpy(storage.MQTT_ID,_jsonsdoc[F("MQTTConfig")][4]);
    strcpy(storage.MQTT_TOPIC,_jsonsdoc[F("MQTTConfig")][5]);
    saveParam("MQTT_IP",storage.MQTT_IP);
    saveParam("MQTT_PORT",storage.MQTT_PORT);
    saveParam("MQTT_LOGIN",storage.MQTT_LOGIN);
    saveParam("MQTT_PASS",storage.MQTT_PASS);
    saveParam("MQTT_ID",storage.MQTT_ID);
    saveParam("MQTT_TOPIC",storage.MQTT_TOPIC);

    // Connect to new MQTT Credentials
    mqttDisconnect();
    mqttInit();
    // It automatically tries to reconnect using a timer
}
void p_SMTPConfig(StaticJsonDocument<250>  &_jsonsdoc) {
    strcpy(storage.SMTP_SERVER,_jsonsdoc[F("SMTPConfig")][0]);
    storage.SMTP_PORT = (uint)_jsonsdoc[F("SMTPConfig")][1];
    strcpy(storage.SMTP_LOGIN,_jsonsdoc[F("SMTPConfig")][2]);
    strcpy(storage.SMTP_PASS,_jsonsdoc[F("SMTPConfig")][3]);
    strcpy(storage.SMTP_SENDER,_jsonsdoc[F("SMTPConfig")][4]);
    strcpy(storage.SMTP_RECIPIENT,_jsonsdoc[F("SMTPConfig")][5]);
    saveParam("SMTP_SERVER",storage.SMTP_SERVER);
    saveParam("SMTP_PORT",storage.SMTP_PORT);
    saveParam("SMTP_LOGIN",storage.SMTP_LOGIN);
    saveParam("SMTP_PASS",storage.SMTP_PASS);
    saveParam("SMTP_SENDER",storage.SMTP_SENDER);
    saveParam("SMTP_RECIPIENT",storage.SMTP_RECIPIENT);
}
void p_PINConfig(StaticJsonDocument<250>  &_jsonsdoc) {
    uint8_t temp_index = (uint8_t)_jsonsdoc[F("PINConfig")][0];

    // Save changes
    storage.PumpsConfig[temp_index].pin_number = (uint8_t)_jsonsdoc[F("PINConfig")][1]; // PIN Number
    storage.PumpsConfig[temp_index].pin_active_level = (bool)_jsonsdoc[F("PINConfig")][2]; // LEVEL HIGH or LOW
    storage.PumpsConfig[temp_index].relay_operation_mode = (bool)_jsonsdoc[F("PINConfig")][3]; // LATCH or MOMENTARY
    uint8_t lock_id = (uint8_t)_jsonsdoc[F("PINConfig")][4];
    lock_id = ((lock_id == NO_INTERLOCK)?NO_INTERLOCK:lock_id-1); // Nextion counts from 1 to 8 but GetInterlockId return from 0 to 7 (except NO_INTERLOCK which does not move)
    storage.PumpsConfig[temp_index].pin_interlock = lock_id ; // INTERLOCK PIN INDEX (Nextion counts 1 to 8 so substract 1)
    //savePumpsConf();

    // Apply changes
    //Pool_Equipment[temp_index]->SetPinNumber((uint8_t)_jsonsdoc[F("PINConfig")][1]);
    //Pool_Equipment[temp_index]->SetActiveLevel((bool)_jsonsdoc[F("PINConfig")][2]);
    //Pool_Equipment[temp_index]->SetOperationMode((bool)_jsonsdoc[F("PINConfig")][3]);
    PoolDeviceManager.GetDevice(temp_index)->SetPinNumber((uint8_t)_jsonsdoc[F("PINConfig")][1]);
    PoolDeviceManager.GetDevice(temp_index)->SetActiveLevel((uint8_t)_jsonsdoc[F("PINConfig")][2]);
    PoolDeviceManager.GetDevice(temp_index)->SetOperationMode((uint8_t)_jsonsdoc[F("PINConfig")][3]);
    PoolDeviceManager.GetDevice(temp_index)->SetInterlock((uint8_t)lock_id);
    PoolDeviceManager.InitDevicesInterlock(temp_index);
    PoolDeviceManager.GetDevice(temp_index)->Begin();
    PoolDeviceManager.SavePreferences(temp_index);

    // If an interlock is requested, loop through the equipment list to find its reference and assign the pointer
    /*if(lock_id != NO_INTERLOCK)
    {
        PoolDeviceManager.Ini
      for(auto equi_lock: Pool_Equipment)
      {
        if(equi_lock->GetPinId() == storage.PumpsConfig[temp_index].pin_interlock)
        {
          Pool_Equipment[temp_index]->SetInterlock(equi_lock);
        }
      }
    }else
    {
      Pool_Equipment[temp_index]->SetInterlock(nullptr);
    }
    Pool_Equipment[temp_index]->Begin();*/
}

