# RetroBell Test Mode

## Overview
The RetroBell test mode provides comprehensive hardware validation and diagnostics via serial commands. This is essential for debugging audio issues, validating wiring, and ensuring all components are working correctly.

## Entering Test Mode

Connect to the serial monitor at 115200 baud and type:
```
test help
```

To enter test mode:
```
test enter
```

## Available Commands

### General Commands
- `test enter` - Enter test mode (suspends normal phone operation)
- `test exit` - Exit test mode (resume normal operation)  
- `test help` - Show all available commands

### Audio Testing Commands
- `test audio handset` - Test handset speaker (RIGHT channel only)
- `test audio ringer` - Test base ringer (LEFT channel only)
- `test audio both` - Test both speakers simultaneously
- `test audio stop` - Stop any running audio test

### Microphone Testing Commands
- `test mic level` - Monitor microphone input levels (real-time display)
- `test mic record` - Record 1 second of audio and play it back
- `test mic stop` - Stop any microphone test

### Hardware Diagnostics
- `test pins` - Show current state of all GPIO pins

## Audio Test Details

### Test Tones
All audio tests use a **1000 Hz sine wave** at 50% volume for clear identification.

### Channel Assignment
- **LEFT Channel (Base Ringer):** Connected to GPIO 12 (AMP_RINGER_SD_PIN)
- **RIGHT Channel (Handset Speaker):** Connected to GPIO 11 (AMP_HANDSET_SD_PIN)

### Expected Behavior
1. **Handset Test:** You should hear tone ONLY in the handset speaker
2. **Ringer Test:** You should hear tone ONLY in the base ringer
3. **Both Test:** You should hear tone in BOTH speakers simultaneously

## Microphone Testing

### Level Monitoring
The `test mic level` command shows real-time audio levels:
```
Mic Level: [----||||++++] 75%
```
- `-` = Low levels (quiet)
- `|` = Medium levels (normal speech)
- `+` = High levels (loud/clipping)

### Record & Playback
The `test mic record` command:
1. Records 1 second of audio from the microphone
2. Automatically plays it back through the handset speaker
3. Useful for testing the complete audio chain

## Hardware Diagnostics

### Pin State Display
The `test pins` command shows:
- **I2S Audio Pins:** Current digital states
- **Amplifier Control:** Enable/disable status  
- **Input Pins:** Hook switch and rotary dial states
- **Analog Input:** Microphone ADC value and voltage

Example output:
```
========== GPIO PIN STATES ==========
I2S Audio Pins:
  BCLK (GPIO 8): HIGH
  LRCLK (GPIO 9): HIGH  
  DOUT (GPIO 10): HIGH
Amplifier Control:
  Handset SD (GPIO 11): ENABLED
  Ringer SD (GPIO 12): ENABLED
Input Pins:
  Hook Switch (GPIO 18): ON_HOOK
  Rotary Pulse (GPIO 15): HIGH
  Rotary Active (GPIO 14): IDLE
Analog Input:
  Microphone ADC (GPIO 4): 2048 (1.65V)
=====================================
```

## Troubleshooting Guide

### No Audio Output
1. Check amplifier enable pins are HIGH
2. Verify I2S pins are toggling during test
3. Check power supply (5V for amps, 3.3V for mic)
4. Verify speaker wiring polarity

### Weak/Distorted Audio  
1. Check power supply voltage under load
2. Verify ground connections
3. Check for loose connections
4. Test with different speakers

### No Microphone Input
1. Check ADC reading (should vary with sound)
2. Verify MAX9814 power (3.3V)
3. Check microphone capsule wiring
4. Test microphone capsule with multimeter

### I2S Issues
1. All I2S pins should show activity during audio tests
2. BCLK should be fastest (bit clock)
3. LRCLK should toggle at ~16kHz (sample rate)
4. DOUT should show data activity

## Safety Notes

- **Test mode suspends normal phone operation** - no calls can be made/received
- **Always exit test mode** when finished to resume normal operation
- **Test tones are 50% volume** but can still be loud with sensitive speakers
- **Stop tests before changing hardware** to avoid damage

## Integration with Normal Operation

Test mode is completely isolated from normal phone functionality:
- Entering test mode automatically stops all phone operations
- Normal state machine is suspended while in test mode
- All hardware interrupts continue to function normally
- Exiting test mode immediately resumes normal operation

This ensures you can safely test hardware without interfering with the phone system's operation.