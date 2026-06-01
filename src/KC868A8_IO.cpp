/**
 * @file KC868A8_IO.cpp
 * @brief I/O abstraction implementation for Kincony KC868-A8
 *
 * Uses two PCF8574 I2C port expanders:
 *   - 0x24: 8 relays (hardware active LOW — write 0 to energise)
 *   - 0x22: 8 opto-isolated digital inputs
 *
 * NO SOFTWARE INVERSION. Values are passed through directly:
 *   Write 0 → PCF8574 pin LOW  → relay ON
 *   Write 1 → PCF8574 pin HIGH → relay OFF
 *   Read  0 → input contact CLOSED
 *   Read  1 → input contact OPEN
 *
 * This matches the Pump class default (ACTIVE_LOW = 0) out of the box.
 * Devices using active_level = ACTIVE_HIGH must be reconfigured to ACTIVE_LOW
 * for KC868-A8 (all relays are active LOW on this hardware).
 */

#include "KC868A8_IO.h"
#include "Config.h"
#include <Wire.h>

// ---------- shadow state ----------
static uint8_t  relayShadow = 0xFF;   // all relays OFF (1 = PCF8574 HIGH = de-energised)
static uint8_t  inputCache  = 0xFF;   // last read from input expander

// ---------- helpers ----------

/** Write current relayShadow to the relay PCF8574 */
static void kc868FlushRelays()
{
    Wire.beginTransmission(KC868A8_RELAY_ADDR);
    Wire.write(relayShadow);
    Wire.endTransmission();
}

/** Refresh inputCache from the input PCF8574 */
static void kc868RefreshInputs()
{
    Wire.requestFrom(KC868A8_INPUT_ADDR, (uint8_t)1);
    if (Wire.available()) {
        inputCache = Wire.read();
    }
}

// ---------- public API ----------

void kc868A8InitIO()
{
    // Relay expander: all relays OFF (shadow = 0xFF → PCF8574 all HIGH)
    relayShadow = 0xFF;
    kc868FlushRelays();

    // Input expander: set all pins as inputs (PCF8574 quasi-bidirectional: write 1)
    Wire.beginTransmission(KC868A8_INPUT_ADDR);
    Wire.write(0xFF);
    Wire.endTransmission();
}

void kc868A8DigitalWrite(uint8_t pin, uint8_t val)
{
    if (pin >= KC868A8_RELAY_BASE && pin < KC868A8_RELAY_BASE + KC868A8_RELAY_COUNT) {
        // Direct pass-through: 0 → PCF8574 LOW (relay ON), 1 → PCF8574 HIGH (relay OFF)
        uint8_t bit = pin - KC868A8_RELAY_BASE;
        if (val == LOW) {
            relayShadow &= ~(1 << bit);   // clear bit → PCF8574 LOW  → relay ON
        } else {
            relayShadow |=  (1 << bit);   // set bit   → PCF8574 HIGH → relay OFF
        }
        kc868FlushRelays();
    } else {
        // Native GPIO fallback
        digitalWrite(pin, val);
    }
}

int kc868A8DigitalRead(uint8_t pin)
{
    if (pin >= KC868A8_INPUT_BASE && pin < KC868A8_INPUT_BASE + KC868A8_INPUT_COUNT) {
#ifdef SIMU
        // SIMU: simulate tank level and pool level sensors
        // HIGH = sensor OK (no alarm), LOW = sensor triggered
        if (pin == CHL_LEVEL)  return HIGH;   // Chlor tank OK
        if (pin == PH_LEVEL)   return HIGH;   // Acid tank OK
        if (pin == POOL_LEVEL) return HIGH;   // Pool water level OK
#endif
        // Real hardware — read from PCF8574 input expander
        kc868RefreshInputs();
        uint8_t bit = pin - KC868A8_INPUT_BASE;
        return (inputCache & (1 << bit)) ? HIGH : LOW;
    }
    else if (pin >= KC868A8_RELAY_BASE && pin < KC868A8_RELAY_BASE + KC868A8_RELAY_COUNT) {
        // Read back relay state from shadow — return what was last written
        uint8_t bit = pin - KC868A8_RELAY_BASE;
        return (relayShadow & (1 << bit)) ? HIGH : LOW;
    }
    else {
        // Native GPIO fallback
        return digitalRead(pin);
    }
}

void kc868A8PinMode(uint8_t pin, uint8_t mode)
{
    if (pin >= KC868A8_RELAY_BASE && pin < KC868A8_RELAY_BASE + KC868A8_RELAY_COUNT) {
        // PCF8574 — already configured as output by kc868A8InitIO
        return;
    }
    else if (pin >= KC868A8_INPUT_BASE && pin < KC868A8_INPUT_BASE + KC868A8_INPUT_COUNT) {
        // PCF8574 — already configured as input by kc868A8InitIO
        return;
    }
    else {
        // Native GPIO
        pinMode(pin, mode);
    }
}
