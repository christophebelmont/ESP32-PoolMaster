#include "Arduino.h"
#include "Pin.h"


//Constructor
PIN::PIN(uint8_t _pin_number, uint8_t _pin_id, uint8_t _pin_direction, bool _active_level)
{
  Initialize(_pin_number, _pin_direction,_active_level);
  pin_id = _pin_id;
}

void PIN::Initialize(uint8_t _pin_number, uint8_t _pin_direction, bool _active_level)
{
  // Set variables
  if((_pin_number != pin_number) && (_pin_direction != INPUT_DIGITAL) && ((pin_direction==OUTPUT_DIGITAL)||(pin_direction==OUTPUT_PWM))) { // If we changed port
    if(_pin_number != 0) {
      digitalWrite(pin_number,!active_level); // Inactivate the previous port
      // IMPORTANT NOTE: Always change pin number in following order
      //  ->SetPinNumber
      //  ->SetActiveLevel
      //  ->Begin
    }
  }

  pin_number = _pin_number;
  pin_direction = _pin_direction;
  active_level = _active_level;
}

//Open the port
void PIN::Begin()
{
  if(pin_number!=0) {
    // Open the port for OUTPUT and set it to down state
    if(pin_direction==OUTPUT_DIGITAL) {  
      pinMode(pin_number, OUTPUT);
      digitalWrite(pin_number,!active_level); // Initialize the port to inactive
    } else if(pin_direction==INPUT_DIGITAL) {
      pinMode(pin_number, INPUT);
    }
  }
}

//Switch the PIN ON
void PIN::Enable()
{
  if (pin_number != 0)
  {
    digitalWrite(pin_number, active_level);
  }
}

//Switch the PIN OFF
void PIN::Disable()
{
  //Serial.printf("Entered PIN Disable %d\r\n",pin_number);
  if (pin_number != 0)
  {
    //Serial.printf("Disabling PIN %d %d\r\n",pin_number, !active_level);
    digitalWrite(pin_number, !active_level);
  }
}

//Toggle switch state
void PIN::Toggle()
{
  (IsActive()) ? Disable() : Enable();  // Invert the position of the relay
}

//Set pump relay to be active on HIGH or LOW state of the corresponding pin
void PIN::SetActiveLevel(bool _active_level)
{
  active_level = _active_level;
}

//Set pump relay to be active on HIGH or LOW state of the corresponding pin
bool PIN::GetActiveLevel(void)
{
  return active_level;
}

// Return pin id related to this relay
uint8_t PIN::GetPinId()
{
  return pin_id;
}

// Return pin number related to this relay
uint8_t PIN::GetPinNumber()
{
  return pin_number;
}

// Return pin direction related to this relay
uint8_t PIN::GetPinDirection()
{
  return pin_direction;
}

// Set the pin number for this relay
void PIN::SetPinNumber(uint8_t _pin_number, uint8_t _pin_direction, bool _active_level)
{
  // Open the port
  Initialize(_pin_number,_pin_direction,_active_level);
}

//relay status
bool PIN::IsActive()
{
  if (pin_number != 0) {
    return (digitalRead(pin_number) == active_level);
  } else {
    return false;
  }
}