
# ESP32 PoolMaster

## About the Project
PoolMaster is a pool management system which automates water maintenance and control.

### Measurement and control
System continuously monitors various metrics and periodically reports them over MQTT and touchscreen:
 - Water and Air Temperature
 - Water pressure
 - pH (with temperature compensation)
 - Orp
 - Chlorine tank level
 - pH tank level
 - Water Level

pH and Orp are continuously adjusted thanks to two peristaltic pumps controlled by two PID regulation loops.

Below a short video presentation (french):

[![DESCRIPTION VIDEO](https://img.youtube.com/vi/WqgkL-VFOhA/0.jpg)](https://www.youtube.com/watch?v=WqgkL-VFOhA) 

### Support many maintenance methods
 - Liquid Chlore
 - SWG (Salt Water Chlorine Generator) through external command and control
 - Liquid active Oxygen

Tank-levels are estimated based on the running-time and flow-rate of each pump. Additionnally a low level contact can be plugged into the system.
 
### Control (local and remote)
 - 3.5'' Nextion TFT touchscreen for local information and control
 - MQTT integration with home automation systems (Home Assistant, Jeedom, NodeRed, InfluxDB, Grafana, etc.)
 - Long term statistics
 - Phone App templates for home automation systems

### Automation
 - Filtration pump automation reduces energy consumption and adapt filtering time slots according to water temperature (70% of swimming pool energy consumption is for filter pump)
 - Thanks to its embedded RTC (Real Time Clock) the system keeps track of precise date/time even when powered off (Adjusted automatically via NTP or manually)
 - Winter mode reduces filtering at its minimum and ensure freezing protection (perfect for active filtering during winter)
 - 2 spare relay outputs for controlling external equipment (heating, lighting, etc.)
   
![Ecosystem](https://github.com/christophebelmont/ESP32-PoolMaster/blob/main/docs/PoolMaster%20Ecosystem.png)

##  Project Details
### Features
This project is a fork of ESP32 Poolmaster by Gixy ([https://github.com/Gixy31/ESP32-PoolMaster](https://github.com/Gixy31/ESP32-PoolMaster)).

Gixy's project is a redesign for ESP32 of an initial project by Loic74650 [(https://github.com/Loic74650/PoolMaster](https://github.com/Loic74650/PoolMaster)) which was intended for Atmega2560 MCUs.

Project includes ESP code and three PCB designs for:
- The main control board
- The pH/Orp acquisition board
- The main control board (extended interface version)

The main unit interfaces with a 3.5'' Nextion touchscreen for local information and control.
It includes Wifi connectivity to communicate via MQTT and SMTP. Project includes configuration files for various home automation systems such as Home Assistant, Jeedom, NodeRed, Grafana, etc.

Finally code and PCB are made with high flexibility to precisely tailor the features to the user.

### Work in Progress
Below the main features to be implemented:
- currently developping support for higher end 800x480 intelligent series screen from Nextion. for better looking and interface and invreased readability.

### Hardware
A dedicated board has been designed to host all components. There are 8 LEDs at the bottom to display status, warnings and alarms.
Below two examples of live deployments:

![Project Hardware](https://github.com/christophebelmont/ESP32-PoolMaster/blob/main/docs/Hardware2.png)

An extended board is also available which includes additional interfaces as well as a second ESP32 slot for watchdog and remote maintenance. This is particularly useful for remote pool management features such as:
- Logs sent to cloud (and long term storage & statistics)
- ESP32 power toggle switch (if reboot needed)
- Over-the-Internet software upgrades (for PoolMaster and Nextion screen)
- Alarm Buzzer included onto the board
- External 5V output to power additional device

![Project Extended Hardware](https://github.com/christophebelmont/ESP32-PoolMaster/blob/main/docs/ExtendedBoard_PCB.png)

### Software
The ESP32 project isn't a fork of the ATmega2560 original one due to the different structure of source files with PlatformIO ((.cpp, .h). 
It includes:
 - Management of ESP32 and Wifi
 - ESP32-Arduino framework with PlatformIO IDE
 - Async MQTT Client
 - JSON upgrade to version 6
 - Async Analog measurement via external I2C ADS115

The version V6, (aka ESP-2.0) implement direct usage of FreeRTOS functions for managing tasks and queues. There are 10 tasks sharing the app_CPU :

| Ref| Task|  Description                | Run every|Offset|
|--| :-------- |  :------------------------- |------|----|
|| `Arduino Loop` |  with only the setup() function. When the setup is finished, the task deletes itself to recover memory  ||
|T1| `AnalogPoll` |  acquire analog measurements of pH, ORP and Pressure with an ADS115 sensor on an I2C bus |`125ms`|`0`
|T2| `PoolServer` |  process commands received on API MQTT Topic |`500ms`|`190`
|T3| `PoolMaster` |  mainly supervises the overal timing of the system  |`500ms`|`310`
|T4| `GetTemp` |  acquire water and air temperatures with DS18B20 sensors on two 1Wire busses |`1000ms`|`440`
|T5| `ORPRegulation` |  manage Chlorine pump |`1000ms`|`560`
|T6| `pHRegulation` |  manage Acid/Soda pump |`1000ms`|`920`
|T7| `StatusLights` | display a row of 8 status LEDs on the mother board, through a PCF8574A on the I2C bus |`3000ms`|`100`
|T8| `PublishMeasures` | publish measurement on the MQTT topic (Meas1 - Meas2) |`30s`|`570`
|T9| `PublishSettings` | publish settings on the MQTT topic (Set1 - Set5) |`When notifed`|`940`
|T10| `UpdateTFT` | update Screen |`1s screen off, 500ms screen on, 200ms menu page`|`50`
|T11| `DataHistory` | store last 12h statistics of pH, Orp and Temp |`2 mn`|`2000`

[https://github.com/christophebelmont/ESP32-PoolMaster/blob/main/docs/PoolMaster_FullLogic.pdf](https://github.com/christophebelmont/ESP32-PoolMaster/blob/main/docs/PoolMaster_FullLogic.pdf)

### 3.5'' TouchScreen control
Below a mosaïc of 3.5'' 480x320 control touchscreen:
![Nextion TouchScreen HMI](https://github.com/christophebelmont/ESP32-PoolMaster/blob/main/docs/Nextion_Screens_v5.jpg)
### Home Automation Integration
Example of integration in Grafana and Home Assistant
![Home Automation Integration](https://github.com/christophebelmont/ESP32-PoolMaster/blob/main/docs/Grafana%20and%20App.png)
###  Tips
Before attempting to regulate your pool water with this automated system, it is essential that you start with:
 1. testing your water quality (using liquid kits and/or test strips for instance) and balancing it properly (pH, Chlorine, Alkalinity, Hardness). Proper water balancing will greatly ease the pH stability and regulation
 2. calibrating the pH probe using calibrated buffer solutions (pay attention to the water temperature which plays a big role in pH readings)
 3. adjusting pH to 7.4
 4. once above steps 1 to 3 are ok, you can start regulating ORP

Notes: 
 - the ORP sensor should theoretically not be calibrated nor temperature compensated (by nature its 0mV pivot point cannot shift) 
  - the ORP reading is strongly affected by the pH value and the water temperature. Make sure pH is adjusted at 7.4 
   - prefer platinium ORP probes for setpoints >500mV (ie. Pools and spas)
   - the response time of ORP sensors can be fast in reference buffer solutions (10 secs) and yet very slow in pool water (minutes or more) as it depends on the water composition

###  Details on the PID regulation
In this project the Arduino PID library is used to start/stop the chemicals pumps in a cyclic manner, similar to a "PWM" signal.
The period of the cycle is defined by the WINDOW SIZE parameter which is fixed (in Milliseconds). What the PID library does is vary the duty cycle of the cycle.
If the computed error in the PID loop is null or negative, the duty cycle is set to zero (the ouput of the function PID.Compute()) and the pump is never actuated.
If the error is positive, the duty cycle is set to a value between 0 and a max value (equal to the WINDOW SIZE, ie. the pump is running full time).

So in practice the ouput of the PID.Compute() function is a duration in milliseconds during which the pump will be activated for every cycle.
If for instance, the WINDOW SIZE is set to 3600000ms (ie. one hour) and the output of the PID is 600000ms (ie. 10mins), the pump will be activated for 10mins at the begining of every hour cycle.

On the default Kp,Ki,Kd parameter values of the PID:
By default in this project, Ki and Kd are null for stability reasons and so the PID loop is only a P loop, ie. a proportional loop.
Adding some Ki and Kd to the PID loop may theoretically increase regulation performance but is also more complex to adjust and could result in instabilities. Since a P-only loop worked well enough and that safety considerations should be taken seriously in this project, I left it as is.

For my 50m3 pool the Kp default values are 2000000 for the pH loop and 4500 for the Orp loop. They were chosen experimentally in the following way:

I experimentally checked how much chemical was required to change the measured parameter (pH or Orp) by a certain amount. For instance I determined that 83ml of acid changed the pH by 0.1 for my 50m3 pool. The flow rate of the acid pump being 1.5L/hour, we can then determine for how many minutes the pump should be activated if the pH error is 0.1, which are (0.083*60/1.5) = 3.3minutes or roughly 200000ms.
And so for an error of 1 in the pH PID loop, the pump needs to be activated 10 times longer, ie. during 2000000ms, which should be taken as the Kp value. The same reasoning goes for the Kp value of the Orp PID loop.

On the WINDOW SIZE:
Various parameters influence the speed at which an injected chemical in the pool water will result in a variation in the measured pH or Orp.
Experimentally I measured that in my case it can take up to 30minutes and therefore the injection cycle period should be at least 30mins or longer in order not to inject more chemical over the following cycles thinking that it required more when in fact the chemical reactions simply needed more time to take effect, which would eventually result in overshooting. So in my case I setlled for a safe one hour WINDOW SIZE (ie. 3600000ms)

### MQTT API
#### Get Information
Every 30 seconds (configurable), the system will publish the following information:
```http
POOLTOPIC/Meas1
```
| Parameter |  Description                | Unit |
| :-------- |  :------------------------- |------|
| `TE` |  Measured air temperature |°C (x100)|
| `Tmp` | Measured water temperature |°C (x100)|
| `pH` |  pH measurement |(x100)|
| `PSI` | Pump pressure PSI  |b (x100)|
| `Orp` |  Orp measurement|mV|
| `PhUpT` |  pH peristaltic pump uptime  |mn (x100)|
| `ChlUpT` |  Chlorine peristaltic pump uptime  |mn (x100)|


```http
POOLTOPIC/Meas2
```
| Parameter |  Description                |Unit|
| :-------- |  :------------------------- |----|
| `AcidF` |  pH tank fill percentage|%|
| `ChlF` | Chlorine tank fill percentage|%|
| `IO` |  Bitmap1 (see below)||
| `IO2` | Bitmap2 (see below)||
| `IO3` |  Bitmap3 (see below)||

Bitmap are on/off states of various elements in the system:
```http
IO
```
|Bit| Parameter |  Description                |
|-| :-------- |  :------------------------- |
|7|  FiltPump| current state of Filtration Pump (1=on, 0=off)
|6|   PhPump| current state of Ph Pump (1=on, 0=off)
|5|   ChlPump| current state of Chl Pump (1=on, 0=off)
|4|   PhlLevel| current state of Acid tank level (0=ok, 1=low level)
|3|   ChlLevel| current state of Chl tank level (0=ok, 1=low level)
|2|   PSIError| over-pressure error
|1|   pHErr| pH pump overtime error flag
|0|   ChlErr| Chl pump overtime error flag
```http
IO2
```
|Bit| Parameter |  Description                |
|-| :-------- |  :------------------------- |
|7|  pHPID| current state of pH PID regulation loop (1=on, 0=off)
|6|   OrpPID| current state of Orp PID regulation loop (1=on, 0=off)
|5|   Mode| filtration pump automation mode (0=manual, 1=automatic)
|4|   RobotPump| current state of Robot Pump (1=on, 0=off)
|3|   RELAYR0| current state of spare Relay0 (1=on, 0=off)
|2|   RELAYR1|current state of spare Relay1 (1=on, 0=off)
|1|   Winter| current state of winter mode (0=summer, 1=winter)
|0|   Water Level| Current state of pool level (0=ok, 1=low level)
```http
IO3
```
|Bit| Parameter |  Description                |
|-| :-------- |  :------------------------- |
|7|  | Unused
|6|   | Unused
|5|   FillErr| Pool Filling pump overtime error flag
|4|   Filling Pump| current state of Pool Filling Pump (1=on, 0=off)
|3|   SWGMode| current mode of Salt Water Chlorine Generator (0=no SWG, 1=SWG active)
|2|   SWGState|current state of Salt Water Chlorine Generator (0=off, 1=on)
|1|   pHAutoMode| pH automation and regulation mode (0=manual, 1=automatic)
|0|   OrpAutoMode| Orp automation and regulation mode (0=manual, 1=PID active)

In addition the system published on-demand settings :
```http
POOLTOPIC/Set1
```
| Parameter |  Description                |
| :-------- |  :------------------------- |
| `FW` |  Firmware Version |
| `FSta` | Computed filtration start hour, in the morning (hours) |
| `FStaM` |  Earliest Filtration start hour, in the morning (hours) |
| `FDu` | Computed filtration duration based on water temperature (hours) |
| `FStoM` |  Latest hour for the filtration to run. Whatever happens, filtration won't run later than this hour |
| `FSto` |  Computed filtration stop hour, equal to FSta + FDu (hour) |
| `pHUTL` |  Max allowed daily run time for the pH pump (mins) |
| `ChlUTL` |  Max allowed daily run time for the Chl pump (mins) |
| `FilUTL` |  Max allowed daily run time for the Pool Filling pump (mins) |


```http
POOLTOPIC/Set2
```
| Parameter |  Description                |
| :-------- |  :------------------------- |
| `pHWS` |  pH PID window size (mins) |
| `ChlWS` | Orp PID window size (mins) |
| `pHSP` |  pH setpoint (x100) |
| `OrpSP` | Orp setpoint |
| `WSP` |  Water temperature setpoint (x100) |
| `WLT` |  Water temperature low threshold to activate anti-freeze mode (x100) |
| `PSIHT` |  Water pressure high threshold to trigger error (x100) |
| `PSIMT` |  Water pressure medium threshold (unused yet) (x100) |
 
```http
POOLTOPIC/Set3
```
| Parameter |  Description                |
| :-------- |  :------------------------- |
| `pHC0` | pH sensor calibration coefficient C0 |
| `pHC1` | pH sensor calibration coefficient C1 |
| `OrpC0` |  Orp sensor calibration coefficient C0 |
| `OrpC1` | Orp sensor calibration coefficient C1 |
| `PSIC0` |  Pressure sensor calibration coefficient C0 |
| `PSIC1` |  Pressure sensor calibration coefficient C1 |

```http
POOLTOPIC/Set4
```
| Parameter |  Description                |
| :-------- |  :------------------------- |
| `pHKp` | pH PID coeffcicient Kp |
| `pHKi` | pH PID coeffcicient Ki|
| `pHKd` |  pH PID coeffcicient Kd |
| `OrpKp` | Orp PID coeffcicient Kp |
| `OrpKi` | Orp PID coeffcicient Ki |
| `OrpKd` | Orp PID coeffcicient Kd |
| `Dpid` |  Delay from FSta for the water regulation/PIDs to start (mins) |
| `PubP` |  Settings publish period in sec |

```http
POOLTOPIC/Set5
```
| Parameter |  Description                |
| :-------- |  :------------------------- |
| `pHTV` | Acid tank nominal volume (Liters) |
| `ChlTV` | Chl tank nominal volume (Liters)|
| `pHFR` |  Acid pump flow rate (L/hour) |
| `OrpFR` | Chl pump flow rate (L/hour) |
| `SWGSec` | SWG Chlorine Generator Secure Temperature (°C)|
| `SWGDel` |SWG Chlorine Generator Delay before start after pump (mn) |

#### Push Information
Below are the Payloads/commands to publish on the "PoolTopicAPI" topic in Json format in order to launch actions :
| Parameter |  Description                |
| :-------- |  :------------------------- |
|{"Mode":1} or {"Mode":0} | set "Mode" to manual (0) or Auto (1). In Auto, filtration starts/stops at set times of the day and pH and Orp are regulated (if pH/Orp PID Mode activated see below)
|{"FiltPump":1} or {"FiltPump":0} | manually start/stop the filtration pump
|{"pHAutoMode":1} or {"pHAutoMode":0} | set " Automatic Mode" for pH regulation to manual (0) or Auto (1). In Auto mode the pH PID regulation loop (see below) turns on and off automatically. 
|{"PhPID":1} or {"PhPID":0} | start/stop the Ph PID regulation loop which directly controls the pH pump.
|{"PhPump":1} or {"PhPump":0} | manually start/stop the Acid pump to lower the Ph
|{"OrpAutoMode":1} or {"OrpAutoMode":0} | set " Automatic Mode" for Orp regulation to manual (0) or Auto (1). In Auto mode the Orp PID regulation loop or SWG device (see below) turn on and off automatically. 
|{"OrpPID":1} or {"OrpPID":0} | start/stop the Orp PID regulation loop which directly controls the Chlorine pump.
|{"ChlPump":1} or {"ChlPump":0} | manually start/stop the Chl pump to add more Chlorine
|{"ElectrolyseMode":1} or {"ElectrolyseMode":0} | set auto mode for Chlorine production via Salt Water Chlorine Generation. In Auto mode the Orp regulates itself (when filtration pump on) with Salt Water Chlorine Generator.
|{"Electrolyse":1} or {"Electrolyse":0} | start/stop the Salt Water Chlorine Generator device (if exists)
|{"ElectroConfig":[15, 2]} | set the minimum temperature for Salt Water Chlorine Generation system to operate and the delay between filtration pump start and Salt Water Chlorine Generation system start.
|{"pHCalib":[4.02,3.8,9.0,9.11]} | multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
|{"OrpCalib":[450,465,750,784]} | multi-point linear regression calibration (minimum 1 point-couple, 6 max.) in the form [ProbeReading_0, BufferRating_0, xx, xx, ProbeReading_n, BufferRating_n]
|{"PhSetPoint":7.4} | set the Ph setpoint, 7.4 in this example
|{"OrpSetPoint":750.0} | set the Orp setpoint, 750mV in this example
|{"WSetPoint":27.0} | set the water temperature setpoint, 27.0deg in this example
|{"Winter":1} or {"Winter":0} | set "Winter" mode to On or Off. Mainly deactivate all the regulation. Only pump continues to operate. Pump will start operating when temperature reaches -2°C and stop if temperature rises back to +2°C.
|{"WTempLow":10.0} | set the water low-temperature threshold below which there is no need to regulate Orp and Ph (ie. in winter)
|{"OrpPIDParams":[2857,0,0]} | respectively set Kp,Ki,Kd parameters of the Orp PID loop. In this example they are set to 2857, 0 and 0
|{"PhPIDParams":[1330000,0,0.0]} | respectively set Kp,Ki,Kd parameters of the Ph PID loop. In this example they are set to 1330000, 0 and 0
|{"OrpPIDWSize":3600000} | set the window size of the Orp PID loop in msec, 60mins in this example
|{"PhPIDWSize":1200000} | set the window size of the Ph PID loop in msec, 20mins in this example
|{"FiltT0":9} | set the earliest hour (9:00 in this example) to run filtration pump. Filtration pump will not run beofre that hour
|{"FiltT1":20} | set the latest hour (20:00 in this example) to run filtration pump. Filtration pump will not run after that hour
|{"PubPeriod":30} | set the periodicity (in seconds) at which the system measurement (pumps states, tank levels states, measured values, etc) will be published to the MQTT broker
|{"PumpsMaxUp":1800} | set the Max Uptime (in secs) for the Ph and Chl pumps over a 24h period. If over, PID regulation is stopped and a warning flag is raised
|{"Clear":1} | reset the pH and Orp pumps overtime error flags in order to let the regulation loops continue. "Mode" also needs to be switched back to Auto (1) after an error flag was raised
|{"DelayPID":30} | Delay (in mins) after FiltT0 before the PID regulation loops will start. This is to let the Orp and pH readings stabilize first. 30mins in this example. Should not be > 59mins
|{"TempExt":4.2} | Provide the system with the external temperature. Should be updated regularly and will be used to start filtration when temperature is less than 2°C. 4.2deg in this example
|{"PSIHigh":1.0} | set the water high-pressure threshold (1.0bar in this example). When water pressure is over that threshold, an error flag is set
|{"pHTank":[20,100]} | call this function when the Acid tank is replaced or refilled. First parameter is the tank volume in Liters, second parameter is its percentage fill (100% when full)
|{"ChlTank":[20,100]} | call this function when the Chlorine tank is replaced or refilled. First parameter is the tank volume in Liters, second parameter is its percentage fill (100% when full)
|{"Relay":[1,1]} | call this generic command to actuate spare relays. Parameter 1 is the relay number (R1 in this example), parameter 2 is the relay state (ON in this example). This command is useful to use spare relays for additional features (lighting, etc). Available relay numbers are 1 and 2
|{"pHPumpFR":1.5} | call this command to set pH pump flow rate un L/s. In this example 1.5L/s
|{"ChlPumpFR":3} | call this command to set Chl pump flow rate un L/s. In this example 3L/s
|{"RstpHCal":1} | call this command to reset the calibration coefficients of the pH probe
|{"RstOrpCal":1} | call this command to reset the calibration coefficients of the Orp probe
|{"RstPSICal":1} | call this command to reset the calibration coefficients of the pressure sensor
|{"Date":[1,1,1,18,13,32,0]} | set date/time of RTC module in the following format: (Day of the month, Day of the week, Month, Year, Hour, Minute, Seconds), in this example: Monday 1st January 2018 - 13h32mn00secs
|{"SetDateTime":[11,30,15,3,6,2025]} | set Date/Time with format setTime(rtc_hour,rtc_min,rtc_sec,rtc_mday,rtc_mon+1,rtc_year) in order of appearance in JSON table.
|{"WifiConfig":["SSID","PASSWORD"]} | configure WIFI Network
|{"Lang_Locale":1} | configure PoolMaster language (0=English, 1=French)
|{"MQTTConfig":["IP",PORT,"LOGIN","PASSWORD","SERVER_ID","TOPIC"]} | configure MQTT connection
|{"Reboot":1} | call this command to reboot the controller (after 8 seconds from calling this command)
|{"Settings":1} | force settings (Set1-Set5) publication to MQTT to refresh values
<!--stackedit_data:
eyJoaXN0b3J5IjpbLTQwNjYwOTEyNSwxOTU3MjAzMDc5XX0=
-->
