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
    {"PumpMaxUp",       p_PumpMaxUp      },
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
    {"FillAutoMode",    p_FillAutoMode   },
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
    PMConfig.put<bool>(BUZZERON, (bool)_jsonsdoc[F("Buzzer")]);
}

void p_Lang(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<uint8_t>(LANG_LOCALE, (uint8_t)_jsonsdoc[F("Lang_Locale")]);
}

void p_TempExt(StaticJsonDocument<250>  &_jsonsdoc) {
    PMData.AirTemp = _jsonsdoc[F("TempExt")].as<float>();
}

//{"pHCalib":[4.02,3.8,9.0,9.11]}  -> multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
void p_pHCalib(StaticJsonDocument<250>  &_jsonsdoc) {
    float CalibPoints[12]; //Max six calibration point-couples! Should be plenty enough
    int NbPoints = (int)copyArray(_jsonsdoc[F("pHCalib")].as<JsonArray>(),CalibPoints);        
    Debug.print(DBG_DEBUG,"pHCalib command - %d points received",NbPoints);
    for (int i = 0; i < NbPoints; i += 2)
      Debug.print(DBG_DEBUG,"%10.2f - %10.2f",CalibPoints[i],CalibPoints[i + 1]);

    double _pHCalibCoeffs0, _pHCalibCoeffs1;    // Temporary variables for linear regression coefficients
    _pHCalibCoeffs0 = PMConfig.get<double>(PHCALIBCOEFFS0);
    _pHCalibCoeffs1 = PMConfig.get<double>(PHCALIBCOEFFS1);

    if (NbPoints == 2) //Only one pair of points. Perform a simple offset calibration
    {
      Debug.print(DBG_DEBUG,"2 points. Performing a simple offset calibration");

      //compute offset correction
      _pHCalibCoeffs1 += CalibPoints[1] - CalibPoints[0];

      //Set slope back to default value
      _pHCalibCoeffs0 = 3.76;

      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",_pHCalibCoeffs0,_pHCalibCoeffs1);
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
        xCalibPoints[i / 2] = (CalibPoints[i] - _pHCalibCoeffs1) / _pHCalibCoeffs0;
        yCalibPoints[i / 2] = CalibPoints[i + 1];
      }

      //Compute linear regression coefficients
      simpLinReg(xCalibPoints, yCalibPoints, _pHCalibCoeffs0, _pHCalibCoeffs1, NbPoints / 2);

      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",_pHCalibCoeffs0 ,_pHCalibCoeffs1);
    }

    PMConfig.put<double>(PHCALIBCOEFFS0, _pHCalibCoeffs0);
    PMConfig.put<double>(PHCALIBCOEFFS1, _pHCalibCoeffs1);

    //Store the new coefficients in eeprom
    PublishSettings();
}

//{"OrpCalib":[450,465,750,784]}   -> multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
void p_OrpCalib(StaticJsonDocument<250>  &_jsonsdoc) {
    float CalibPoints[12]; //Max six calibration point-couples! Should be plenty enough
    int NbPoints = (int)copyArray(_jsonsdoc[F("OrpCalib")].as<JsonArray>(),CalibPoints);
    Debug.print(DBG_DEBUG,"OrpCalib command - %d points received",NbPoints);
    for (int i = 0; i < NbPoints; i += 2)
      Debug.print(DBG_DEBUG,"%10.2f - %10.2f",CalibPoints[i],CalibPoints[i + 1]);

    double _OrpCalibCoeffs0, _OrpCalibCoeffs1;    // Temporary variables for linear regression coefficients
    _OrpCalibCoeffs0 = PMConfig.get<double>(ORPCALIBCOEFFS0);
    _OrpCalibCoeffs1 = PMConfig.get<double>(ORPCALIBCOEFFS1);

    if (NbPoints == 2) //Only one pair of points. Perform a simple offset calibration
    {
      Debug.print(DBG_DEBUG,"2 points. Performing a simple offset calibration");

      //compute offset correction
      _OrpCalibCoeffs1 += CalibPoints[1] - CalibPoints[0];

      //Set slope back to default value
      _OrpCalibCoeffs0 = -1000.0; 

      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",_OrpCalibCoeffs0,_OrpCalibCoeffs1);
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
        xCalibPoints[i / 2] = (CalibPoints[i] - _OrpCalibCoeffs1) / _OrpCalibCoeffs0;
        yCalibPoints[i / 2] = CalibPoints[i + 1];
      }

      //Compute linear regression coefficients
      simpLinReg(xCalibPoints, yCalibPoints, _OrpCalibCoeffs0, _OrpCalibCoeffs1, NbPoints / 2);

      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",_OrpCalibCoeffs0,_OrpCalibCoeffs1);
    }

    PMConfig.put<double>(ORPCALIBCOEFFS0, _OrpCalibCoeffs0);
    PMConfig.put<double>(ORPCALIBCOEFFS1, _OrpCalibCoeffs1);

    PublishSettings();
}

//{"PSICalib":[0,0,0.71,0.6]}   -> multi-point linear regression calibration (minimum 2 point-couple, 6 max.) in the form [ElectronicPressureSensorReading_0, MechanicalPressureSensorReading_0, xx, xx, ElectronicPressureSensorReading_n, ElectronicPressureSensorReading_n]
void p_PSICalib(StaticJsonDocument<250>  &_jsonsdoc) {
    float CalibPoints[12];//Max six calibration point-couples! Should be plenty enough, typically use two point-couples (filtration ON and filtration OFF)
    int NbPoints = (int)copyArray(_jsonsdoc[F("PSICalib")].as<JsonArray>(),CalibPoints);
    Debug.print(DBG_DEBUG,"PSICalib command - %d points received",NbPoints);
    for (int i = 0; i < NbPoints; i += 2)
      Debug.print(DBG_DEBUG,"%10.2f, %10.2f",CalibPoints[i],CalibPoints[i + 1]);

    double _PSICalibCoeffs0, _PSICalibCoeffs1;    // Temporary variables for linear regression coefficients
    _PSICalibCoeffs0 = PMConfig.get<double>(PSICALIBCOEFFS0);
    _PSICalibCoeffs1 = PMConfig.get<double>(PSICALIBCOEFFS1);

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
        xCalibPoints[i / 2] = (CalibPoints[i] - _PSICalibCoeffs1) / _PSICalibCoeffs0;
        yCalibPoints[i / 2] = CalibPoints[i + 1];
      }

      //Compute linear regression coefficients
      simpLinReg(xCalibPoints, yCalibPoints, _PSICalibCoeffs0, _PSICalibCoeffs1, NbPoints / 2);

    PMConfig.put<double>(PSICALIBCOEFFS0, _PSICalibCoeffs0);
    PMConfig.put<double>(PSICALIBCOEFFS1, _PSICalibCoeffs1);

      PublishSettings();
      Debug.print(DBG_DEBUG,"Calibration completed. Coeffs are: %10.2f, %10.2f",_PSICalibCoeffs0,_PSICalibCoeffs1);
    }
}

void p_Mode(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<bool>(AUTOMODE, (bool)_jsonsdoc[F("Mode")]);
    //storage.AutoMode = (bool)_jsonsdoc[F("Mode")];
    if (!PMConfig.get<bool>(AUTOMODE)) // Stop PIDs if manual mode
    {
      SetPhPID(false);
      SetOrpPID(false);
    }
    //saveParam("AutoMode",storage.AutoMode);
    PublishSettings();
}
void p_Electrolyse(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((int)_jsonsdoc[F("Electrolyse")] == 1)  // activate electrolyse
    {
      // start electrolyse if not below minimum temperature
      // do not take care of minimum filtering time as it 
      // was forced on.
      if (PMData.WaterTemp >= (double)PMConfig.get<double>(SECUREELECTRO))
        if (!SWGPump.Start())
          Debug.print(DBG_WARNING,"Problem starting SWG");   
    } else {
      if (!SWGPump.Stop())
        Debug.print(DBG_WARNING,"Problem stopping SWG");   
    }
    // Direct action on Electrolyse will exit the automatic Electro Regulation Mode
    PMConfig.put<bool>(ELECTROLYSEMODE, false);
    PublishSettings();
}
void p_ElectrolyseMode(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<bool>(ELECTROLYSEMODE, (bool)_jsonsdoc[F("ElectrolyseMode")]);
    PublishSettings();
}
void p_Winter(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<bool>(WINTERMODE, (bool)_jsonsdoc[F("Winter")]);
    PublishSettings();
}
void p_PhSetPoint(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(PH_SETPOINT, _jsonsdoc[F("PhSetPoint")].as<double>());
    PMData.Ph_SetPoint = PMConfig.get<double>(PH_SETPOINT);
    PublishSettings();
}
void p_OrpSetPoint(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(ORP_SETPOINT, _jsonsdoc[F("OrpSetPoint")].as<double>());
    PMData.Orp_SetPoint = PMConfig.get<double>(ORP_SETPOINT);
    PublishSettings();
}
void p_WSetPoint(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(WATERTEMP_SETPOINT, _jsonsdoc[F("WSetPoint")].as<double>());
    PublishSettings();
}
//"pHTank" command which is called when the pH tank is changed or refilled
//First parameter is volume of tank in Liters, second parameter is percentage Fill of the tank (typically 100% when new)

void p_pHTank(StaticJsonDocument<250>  &_jsonsdoc) {
    PhPump.SetTankVolume((double)_jsonsdoc[F("pHTank")][0]);
    PhPump.SetTankFill((double)_jsonsdoc[F("pHTank")][1]);
    PoolDeviceManager.SavePreferences(DEVICE_PH_PUMP);
    PublishSettings();
}
void p_ChlTank(StaticJsonDocument<250>  &_jsonsdoc) {
    ChlPump.SetTankVolume((double)_jsonsdoc[F("ChlTank")][0]);
    ChlPump.SetTankFill((double)_jsonsdoc[F("ChlTank")][1]);
    PoolDeviceManager.SavePreferences(DEVICE_CHL_PUMP);
    PublishSettings();
}
void p_WTempLow(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(WATERTEMPLOWTHRESHOLD, (double)_jsonsdoc[F("WTempLow")]);
    PublishSettings();
}
void p_PumpsMaxUp(StaticJsonDocument<250>  &_jsonsdoc) {
    PhPump.SetMaxUpTime((unsigned int)_jsonsdoc[F("PumpsMaxUp")]*60*1000);
    ChlPump.SetMaxUpTime((unsigned int)_jsonsdoc[F("PumpsMaxUp")]*60*1000);
    PoolDeviceManager.SavePreferences(DEVICE_PH_PUMP);
    PoolDeviceManager.SavePreferences(DEVICE_CHL_PUMP);
    PublishSettings();
}
void p_PumpMaxUp(StaticJsonDocument<250>  &_jsonsdoc) {
    u_int8_t device_index = (u_int8_t)_jsonsdoc[F("PumpMaxUp")][0];
    PoolDeviceManager.GetDevice(device_index)->SetMaxUpTime((unsigned int)_jsonsdoc[F("PumpMaxUp")][1]*60*1000);
    PoolDeviceManager.SavePreferences(device_index);
    PublishSettings();
}
void p_FillMinUpTime(StaticJsonDocument<250>  &_jsonsdoc) {
    FillingPump.SetMinUpTime((unsigned int)_jsonsdoc[F("FillMinUpTime")]*60*1000);
    PoolDeviceManager.SavePreferences(DEVICE_FILLING_PUMP);
    PublishSettings();
}
void p_FillMaxUpTime(StaticJsonDocument<250>  &_jsonsdoc) {
    FillingPump.SetMaxUpTime((unsigned int)_jsonsdoc[F("FillMaxUpTime")]*60*1000);
    PoolDeviceManager.SavePreferences(DEVICE_FILLING_PUMP);
    PublishSettings();
}
void p_OrpPIDParams(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(ORP_KP, (double)_jsonsdoc[F("OrpPIDParams")][0]);
    PMConfig.put<double>(ORP_KI, (double)_jsonsdoc[F("OrpPIDParams")][1]);
    PMConfig.put<double>(ORP_KD, (double)_jsonsdoc[F("OrpPIDParams")][2]);
    OrpPID.SetTunings(PMConfig.get<double>(ORP_KP), PMConfig.get<double>(ORP_KI), PMConfig.get<double>(ORP_KD));
    PublishSettings();
}
void p_PhPIDParams(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(PH_KP, (double)_jsonsdoc[F("PhPIDParams")][0]);
    PMConfig.put<double>(PH_KI, (double)_jsonsdoc[F("PhPIDParams")][1]);
    PMConfig.put<double>(PH_KD, (double)_jsonsdoc[F("PhPIDParams")][2]);
    PhPID.SetTunings(PMConfig.get<double>(PH_KP), PMConfig.get<double>(PH_KI), PMConfig.get<double>(PH_KD));
    PublishSettings();
}
void p_OrpPIDWSize(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<unsigned long>(ORPPIDWINDOWSIZE, (unsigned long)_jsonsdoc[F("OrpPIDWSize")]*60*1000); //in millisecs
    OrpPID.SetSampleTime((int)PMConfig.get<unsigned long>(ORPPIDWINDOWSIZE));
    OrpPID.SetOutputLimits(0, PMConfig.get<unsigned long>(ORPPIDWINDOWSIZE));  //Whatever happens, don't allow continuous injection of Chl for more than a PID Window
    PublishSettings();
}
void p_PhPIDWSize(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<unsigned long>(PHPIDWINDOWSIZE, (unsigned long)_jsonsdoc[F("PhPIDWSize")]*60*1000); //in millisecs
    PhPID.SetSampleTime((int)PMConfig.get<unsigned long>(PHPIDWINDOWSIZE));
    PhPID.SetOutputLimits(0, PMConfig.get<unsigned long>(PHPIDWINDOWSIZE));    //Whatever happens, don't allow continuous injection of Acid for more than a PID Window
    PublishSettings();
}
void p_Date(StaticJsonDocument<250>  &_jsonsdoc) {
    setTime((uint8_t)_jsonsdoc[F("Date")][4], (uint8_t)_jsonsdoc[F("Date")][5], (uint8_t)_jsonsdoc[F("Date")][6], (uint8_t)_jsonsdoc[F("Date")][0], (uint8_t)_jsonsdoc[F("Date")][2], (uint8_t)_jsonsdoc[F("Date")][3]); //(Day of the month, Day of the week, Month, Year, Hour, Minute, Second)
}
void p_FiltT0(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<uint8_t>(FILTRATIONSTARTMIN, (uint8_t)_jsonsdoc[F("FiltT0")]);
    PublishSettings();
}
void p_FiltT1(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<uint8_t>(FILTRATIONSTOPMAX, (uint8_t)_jsonsdoc[F("FiltT1")]);
    PublishSettings();
}
void p_PubPeriod(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<unsigned long>(PUBLISHPERIOD, (unsigned long)_jsonsdoc[F("PubPeriod")] * 1000); //in millisecs
    PublishSettings();
}
void p_DelayPID(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<uint8_t>(DELAYPIDS, (uint8_t)_jsonsdoc[F("DelayPID")]);
    PublishSettings();
}
void p_PSIHigh(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(PSI_HIGHTHRESHOLD, (double)_jsonsdoc[F("PSIHigh")]);
    PublishSettings();
}
void p_PSILow(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(PSI_MEDTHRESHOLD, (double)_jsonsdoc[F("PSILow")]);
    PublishSettings();
}
void p_pHPumpFR(StaticJsonDocument<250>  &_jsonsdoc) {
    PhPump.SetFlowRate((double)_jsonsdoc[F("pHPumpFR")] * 1000);
    PoolDeviceManager.SavePreferences(DEVICE_PH_PUMP);
    PublishSettings();
}
void p_ChlPumpFR(StaticJsonDocument<250>  &_jsonsdoc) {
    PhPump.SetFlowRate((double)_jsonsdoc[F("ChlPumpFR")] * 1000);
    PoolDeviceManager.SavePreferences(DEVICE_CHL_PUMP);
    PublishSettings();
}
void p_RstpHCal(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(PHCALIBCOEFFS0, (double)-2.50133333);
    PMConfig.put<double>(PHCALIBCOEFFS1, (double)6.9);
    PublishSettings();
}
void p_RstOrpCal(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(ORPCALIBCOEFFS0, (double)431.03);
    PMConfig.put<double>(ORPCALIBCOEFFS1, (double)0.0);
    PublishSettings();
}
void p_RstPSICal(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<double>(PSICALIBCOEFFS0, (double)0.377923399);
    PMConfig.put<double>(PSICALIBCOEFFS1, (double)-0.17634473);
    PublishSettings();
}
void p_Settings(StaticJsonDocument<250>  &_jsonsdoc) {
    PublishSettings();
}
void p_FiltPump(StaticJsonDocument<250>  &_jsonsdoc) {
    u_int8_t ErrorCode = 0;
    if ((bool)_jsonsdoc[F("FiltPump")])
    {
        ErrorCode = FiltrationPump.Start();   //start filtration pump

        // Manage Starting problems to show in the logs
        if(ErrorCode & (1<<0) != 0) //if error code bit 0 is set, it means the pump is already running
            Debug.print(DBG_WARNING,"%s UpTime prevented start. No action taken",FiltrationPump.GetName());
        else if(ErrorCode & (1<<1) != 0) //if error code bit 1 is set, it means the pump has a problem starting
            Debug.print(DBG_WARNING,"%s TankLevel prevented start. No action taken",FiltrationPump.GetName());
        else if(ErrorCode & (1<<2) != 0) //if error code bit 2 is set, it means the pump is not running
            Debug.print(DBG_WARNING,"%s Interlock prevented start. No action taken",FiltrationPump.GetName());
        else if(ErrorCode & (1<<3) != 0) //if error code bit 3 is set, it means the pump is running but has a problem
            Debug.print(DBG_WARNING,"%s Underlying Relay prevented start. No action taken",FiltrationPump.GetName());
    }
    else
    {
        FiltrationPump.Stop();  //stop filtration pump

        //Stop PIDs
        SetPhPID(false);
        SetOrpPID(false);
    }

    PMConfig.put<bool>(AUTOMODE, false);   // Manually changing pump operation disables the automatic mode
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
    
    PMConfig.put<bool>(PHAUTOMODE, false);   // Manually changing pump operation disables the automatic mode
    if (PMConfig.get<bool>(PHAUTOMODE) == 0) SetPhPID(false);

    PublishSettings();
}
void p_FillPump(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("FillPump")])
        FillingPump.Start();      //start swimming pool filling pump
    else
        FillingPump.Stop();       //stop swimming pool filling pump

    PMConfig.put<bool>(FILLAUTOMODE, false);   // Manually changing pump operation disables the automatic mode
    PublishSettings();
 }
void p_ChlPump(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("ChlPump")])
        ChlPump.Start();     //start Chl pump  
    else
        ChlPump.Stop();      //stop Chl pump      

    PMConfig.put<bool>(ORPAUTOMODE, false);
    if (PMConfig.get<bool>(ORPAUTOMODE) == 0) SetOrpPID(false);
    PublishSettings();
}
void p_PhPID(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("PhPID")])
        SetPhPID(true);
    else
        SetPhPID(false);
}
void p_PhAutoMode(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<bool>(PHAUTOMODE, (bool)_jsonsdoc[F("PhAutoMode")]);
    if (PMConfig.get<bool>(PHAUTOMODE) == 0) SetPhPID(false);
    PublishSettings();
}
void p_OrpPID(StaticJsonDocument<250>  &_jsonsdoc) {
    if ((bool)_jsonsdoc[F("OrpPID")])
        SetOrpPID(true);
    else
        SetOrpPID(false);
}
void p_OrpAutoMode(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<bool>(ORPAUTOMODE, (bool)_jsonsdoc[F("OrpAutoMode")]);
    if (PMConfig.get<bool>(ORPAUTOMODE) == 0) SetOrpPID(false);
    PublishSettings();
}
void p_FillAutoMode(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<bool>(FILLAUTOMODE, (bool)_jsonsdoc[F("FillAutoMode")]);
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
    PMConfig.put<uint8_t>(SECUREELECTRO, (uint8_t)_jsonsdoc[F("ElectroConfig")][0]);
    PMConfig.put<uint8_t>(DELAYELECTRO, (uint8_t)_jsonsdoc[F("ElectroConfig")][1]);
    PublishSettings();
}
void p_SecureElectro(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<uint8_t>(SECUREELECTRO, (uint8_t)_jsonsdoc[F("SecureElectro")]);
    PublishSettings();
}
void p_DelayElectro(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<uint8_t>(DELAYELECTRO, (uint8_t)_jsonsdoc[F("DelayElectro")]);
    PublishSettings();
}
void p_ElectroRunMode(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<bool>(ELECTRORUNMODE, (bool)_jsonsdoc[F("ElectroRunMode")]);
    PublishSettings();
}
void p_ElectroRuntime(StaticJsonDocument<250>  &_jsonsdoc) {
    PMConfig.put<uint8_t>(ELECTRORUNTIME, (uint8_t)_jsonsdoc[F("ElectroRuntime")]);
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
    IPAddress _mqtt_ip;
    _mqtt_ip.fromString((const char*)_jsonsdoc[F("MQTTConfig")][0]);
    PMConfig.put<uint32_t>(MQTT_IP, _mqtt_ip); // Save the IP address in PMConfig

    PMConfig.put<uint32_t>(MQTT_PORT, (uint32_t)_jsonsdoc[F("MQTTConfig")][1]);
    PMConfig.put<const char*>(MQTT_LOGIN, (const char*)_jsonsdoc[F("MQTTConfig")][2]);
    PMConfig.put<const char*>(MQTT_PASS, (const char*)_jsonsdoc[F("MQTTConfig")][3]);
    PMConfig.put<const char*>(MQTT_ID, (const char*)_jsonsdoc[F("MQTTConfig")][4]);
    PMConfig.put<const char*>(MQTT_TOPIC, (const char*)_jsonsdoc[F("MQTTConfig")][5]);

    // Connect to new MQTT Credentials
    mqttDisconnect();
    mqttInit();
    // It automatically tries to reconnect using a timer
}
void p_SMTPConfig(StaticJsonDocument<250>  &_jsonsdoc) {
    // Format of message is {"SMTPConfig":[SMTP_SERVER, SMTP_PORT, SMTP_LOGIN, SMTP_PASS, SMTP_SENDER, SMTP_RECIPIENT]}
    // Save SMTP configuration
    PMConfig.put<const char*>(SMTP_SERVER, (const char*)_jsonsdoc[F("SMTPConfig")][0]);
    PMConfig.put<uint32_t>(SMTP_PORT, (uint32_t)_jsonsdoc[F("SMTPConfig")][1]);
    PMConfig.put<const char*>(SMTP_LOGIN, (const char*)_jsonsdoc[F("SMTPConfig")][2]);
    PMConfig.put<const char*>(SMTP_PASS, (const char*)_jsonsdoc[F("SMTPConfig")][3]);
    PMConfig.put<const char*>(SMTP_SENDER, (const char*)_jsonsdoc[F("SMTPConfig")][4]);
    PMConfig.put<const char*>(SMTP_RECIPIENT, (const char*)_jsonsdoc[F("SMTPConfig")][5]);
    /*strcpy(storage.SMTP_SERVER,_jsonsdoc[F("SMTPConfig")][0]);
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
    saveParam("SMTP_RECIPIENT",storage.SMTP_RECIPIENT);*/
}
void p_PINConfig(StaticJsonDocument<250>  &_jsonsdoc) {
    // Format of message is {"PINConfig":[index, pin_number, active_level, relay_operation_mode, interlock_id]}
    uint8_t temp_index = (uint8_t)_jsonsdoc[F("PINConfig")][0];

    uint8_t lock_id = (uint8_t)_jsonsdoc[F("PINConfig")][4];
    lock_id = ((lock_id == NO_INTERLOCK)?NO_INTERLOCK:lock_id-1); // Nextion counts from 1 to 8 but GetInterlockId return from 0 to 7 (except NO_INTERLOCK which does not move)

    PIN *tmp_device = PoolDeviceManager.GetDevice(temp_index);
    // Apply changes
    tmp_device->SetPinNumber((uint8_t)_jsonsdoc[F("PINConfig")][1]);
    tmp_device->SetActiveLevel((bool)_jsonsdoc[F("PINConfig")][2]);
    tmp_device->SetOperationMode((bool)_jsonsdoc[F("PINConfig")][3]);
    tmp_device->SetInterlock((uint8_t)lock_id);
    PoolDeviceManager.InitDevicesInterlock(temp_index);
    tmp_device->Begin();
    PoolDeviceManager.SavePreferences(temp_index);
}

