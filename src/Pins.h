/*
 * Pins.h - GPIO Pin Definitions
 * 
 * Central location for all hardware pin assignments on the ESP32-S3-DevKitC-1.
 * 
 * Pin Categories:
 * - I2S Audio Output: Dual independent I2S buses to MAX98357A amplifiers
 * - I2S Microphone: Digital I2S input from ICS-43434 microphone
 * - Amplifier Control: Shutdown/enable pins for dual amps
 * - Hook Switch: Detects handset on/off cradle
 * - Rotary Dial: Pulse counting and active state detection
 * - Buttons: Optional volume control (not yet implemented)
 * 
 * Audio Architecture:
 * - I2S0: Handset amplifier (mono output) + ICS-43434 microphone (input)
 * - I2S1: Base ringer amplifier (mono output)
 * 
 * Note: Pin numbers match the dual I2S wiring diagram
 */

#ifndef PINS_H
#define PINS_H

// ====== I2S0 Audio (Handset + Microphone) ======
// I2S0 handles both handset output and microphone input (full-duplex)
#define I2S0_BCLK_PIN 8     // Handset amplifier bit clock
#define I2S0_LRCLK_PIN 9    // Handset amplifier word select
#define I2S0_DOUT_PIN 10    // Handset amplifier data output
#define I2S0_DIN_PIN 6      // ICS-43434 microphone data input

// ====== I2S1 Audio (Base Ringer) ======
// I2S1 handles base ringer output only (TX only)
#define I2S1_BCLK_PIN 12    // Base ringer amplifier bit clock
#define I2S1_LRCLK_PIN 13   // Base ringer amplifier word select  
#define I2S1_DOUT_PIN 14    // Base ringer amplifier data output

// ====== Amplifier Control (MAX98357A) ======
// SD pins control shutdown/enable - no more channel latching needed!
#define AMP_HANDSET_SD_PIN 11  // Handset amplifier shutdown (HIGH=enabled, LOW=muted)
#define AMP_RINGER_SD_PIN 15   // Ringer amplifier shutdown (HIGH=enabled, LOW=muted)

// ====== ICS-43434 Microphone I2S ======
// Digital MEMS microphone with I2S output - shares I2S0 clock signals
#define MIC_SCK_PIN I2S0_BCLK_PIN    // Clock shared with handset amplifier
#define MIC_WS_PIN I2S0_LRCLK_PIN    // Word select shared with handset amplifier
#define MIC_SD_PIN I2S0_DIN_PIN      // Serial data input (GPIO 6)

// ====== Hook Switch ======
// Detects when handset is lifted or replaced
#define HOOK_SW_PIN 16     // Hook switch input (LOW=on-hook, HIGH=off-hook)

// ====== Rotary Dial ======
// Two pins for decoding rotary dial pulses
#define ROTARY_PULSE_PIN 17   // Pulse output (generates pulses as dial returns)
#define ROTARY_ACTIVE_PIN 18  // Dial active (LOW=dialing, HIGH=idle)

// ====== Optional Buttons ======
// Reserved for volume control (future enhancement)
#define VOL_UP_PIN 4       // Volume up button (was MIC_ADC_PIN)
#define VOL_DOWN_PIN 5     // Volume down button

#endif // PINS_H
