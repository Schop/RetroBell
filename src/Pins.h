/*
 * Pins.h - GPIO Pin Definitions
 * 
 * Central location for all hardware pin assignments on the ESP32-S3-DevKitC-1.
 * 
 * Pin Categories:
 * - I2S Audio: Digital audio output to MAX98357A amplifiers
 * - Amplifier Control: Shutdown/enable pins for dual amps
 * - Microphone: ADC input for MAX9814 preamp (not yet implemented)
 * - Hook Switch: Detects handset on/off cradle
 * - Rotary Dial: Pulse counting and active state detection
 * - Buttons: Optional volume control (not yet implemented)
 * 
 * Note: Pin numbers match the wiring diagram in wiring.md
 */

#ifndef PINS_H
#define PINS_H

// ====== I2S Audio Output ======
// Standard I2S interface for stereo audio output
#define I2S_BCLK_PIN 8     // Bit Clock (BCLK) - timing for bits (was 26)
#define I2S_LRCLK_PIN 9    // Left/Right Clock (WS) - timing for channels (was 25)
#define I2S_DOUT_PIN 10    // Data Output (DOUT) - actual audio data (was 22)

// ====== Amplifier Control (MAX98357A) ======
// SD pins control shutdown/enable and channel latching
#define AMP_HANDSET_SD_PIN 11  // Handset amplifier shutdown (HIGH=enabled, LOW=muted) (was 33)
#define AMP_RINGER_SD_PIN 12   // Ringer amplifier shutdown (HIGH=enabled, LOW=muted) (was 32)

// ====== Microphone Input ======
// ADC input for reading microphone level
#define MIC_ADC_PIN 4      // Analog input from MAX9814 microphone preamp

// ====== Hook Switch ======
// Detects when handset is lifted or replaced
#define HOOK_SW_PIN 18     // Hook switch input (LOW=on-hook, HIGH=off-hook)

// ====== Rotary Dial ======
// Two pins for decoding rotary dial pulses
#define ROTARY_PULSE_PIN 15   // Pulse output (generates pulses as dial returns)
#define ROTARY_ACTIVE_PIN 14  // Dial active (LOW=dialing, HIGH=idle)

// ====== Optional Buttons ======
// Reserved for volume control (future enhancement)
#define VOL_UP_PIN 16      // Volume up button
#define VOL_DOWN_PIN 17    // Volume down button

#endif // PINS_H
