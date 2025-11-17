
Per phone: connections at a glance
- ESP32-S3-DevKitC-1
  - 5V (VBUS) → feeds both MAX98357A VIN pins
  - 3V3 → powers INMP441 VCC
  - GND → common to all modules
  - **I2S0 (Handset + Microphone) - Full Duplex**
    - BCLK: GPIO 8 → Handset MAX98357A BCLK + INMP441 SCK
    - LRCLK/WS: GPIO 9 → Handset MAX98357A LRCLK + INMP441 WS
    - DATA_OUT: GPIO 10 → Handset MAX98357A DIN
    - DATA_IN: GPIO 6 → INMP441 SD (microphone data input)
  - **I2S1 (Base Ringer) - TX Only**
    - BCLK: GPIO 12 → Base MAX98357A BCLK
    - LRCLK/WS: GPIO 13 → Base MAX98357A LRCLK
    - DATA_OUT: GPIO 14 → Base MAX98357A DIN
  - Amp enables (independent control, no channel latching needed)
    - Handset amp SD: GPIO 11
    - Base (ringer) amp SD: GPIO 15
  - Hook switch input
    - HOOK_SW: GPIO 16 (enable internal pull‑up; optional 100 nF to GND for debounce)
  - Rotary Dial input
    - ROTARY_PULSE: GPIO 17 (input with pull-up)
    - ROTARY_ACTIVE: GPIO 18 (input with pull-up)
  - Optional buttons (now available since microphone is digital)
    - VOL_UP: GPIO 4 (pull‑up enabled, button to GND)
    - VOL_DOWN: GPIO 5 (pull‑up enabled, button to GND)
  - Optional status LED: any free GPIO via 330 Ω to LED anode; LED cathode to GND

- **MAX98357A (handset amp) - I2S0**
  - VIN → 5V (from ESP32 VBUS)
  - GND → GND
  - BCLK → GPIO 8 (shared with INMP441)
  - LRCLK/WS → GPIO 9 (shared with INMP441)
  - DIN → GPIO 10 (handset audio output)
  - SD → GPIO 11 (independent enable control)
  - SPK+ / SPK− → handset speaker (two wires through handset cord)

- **MAX98357A (base ringer amp) - I2S1**
  - VIN → 5V (from ESP32 VBUS)
  - GND → GND
  - BCLK → GPIO 12 (dedicated I2S1 bus)
  - LRCLK/WS → GPIO 13 (dedicated I2S1 bus)
  - DIN → GPIO 14 (ringer audio output)
  - SD → GPIO 15 (independent enable control)
  - SPK+ / SPK− → base speaker mounted in the phone base

- **INMP441 Digital Microphone (in handset)**
  - VCC → 3V3 (from ESP32)
  - GND → GND
  - L/R → GND (configures as left channel)
  - WS → GPIO 9 (shared with handset amp LRCLK)
  - SCK → GPIO 8 (shared with handset amp BCLK)
  - SD → GPIO 6 (digital audio data to ESP32)
  - **Benefits**: Crystal clear digital audio, 61dB SNR, no analog noise

**Handset cord (6 conductors required - upgrade from 4-wire)**
- Two wires: handset speaker from base handset amp SPK+ and SPK−
- Four wires: INMP441 digital microphone (VCC, GND, WS, SCK, SD)
- **Suggested cord**: RJ25 6P6C or similar 6-conductor telephone cord
- Keep digital and power pairs separated for best performance

Power distribution and decoupling
- USB 5V → ESP32 USB‑C; tap 5V (VBUS) on the ESP32 header to feed both amps' VIN
- 3V3 from ESP32 → INMP441 VCC (much lower power than MAX9814)
- Add a 470 µF electrolytic across 5V and GND physically near the two MAX98357A boards
- Add 0.1 µF ceramic close to each module's power pin if not already on the breakout

**NEW: Dual I2S Architecture - No Channel Latching!**
- **I2S0**: Handset amplifier + INMP441 microphone (full-duplex)
- **I2S1**: Base ringer amplifier (TX only)
- **Benefits**: 
  - No fragile boot sequence or timing issues
  - Independent control of each amplifier
  - Crystal clear digital microphone path
  - Much more reliable operation
- **Audio routing**: Direct targeting - no stereo channel confusion
  - Dial tone, ringback, call audio → Handset amp (I2S0)
  - Ring tone → Base ringer amp (I2S1)
  - Microphone → Direct I2S digital input (I2S0 RX)

**REMOVED: Channel assignment complexity**
The old stereo channel latching system has been eliminated. Each amplifier now has its own dedicated I2S bus for bulletproof reliability.

Hook switch wiring
- Microswitch COM → GND
- Microswitch NO → ESP32 HOOK_SW (GPIO 16) with internal pull‑up enabled
- Optional RC debounce: 100 nF from GPIO 16 to GND

Rotary Dial wiring

Rotary dials come in two common configurations: **3-wire** (shared common) and **4-wire** (separate returns). Both work with the same ESP32 code - only the physical wiring differs.

**How to identify your dial type:**
Use a multimeter in continuity mode:
- With dial at rest: Find contacts that are **closed** → these are the **shunt/NC contacts**
- Spin the dial: Find contacts that **pulse open/closed** → these are the **pulse contacts**

**3-Wire Rotary Dial** (Most Common)
Three terminals:
1. **Common/Ground** - Shared return for both switches
2. **Pulse Switch** - Opens/closes 1-10 times per digit
3. **Shunt/NC Switch** - Normally closed, opens while dialing

Wiring to ESP32:
```
Common/Ground → GND
Pulse Switch  → GPIO 17 (ROTARY_PULSE with internal pull-up enabled)
Shunt/NC      → GPIO 18 (ROTARY_ACTIVE with internal pull-up enabled)
```

**4-Wire Rotary Dial**
Four terminals (two isolated switch pairs):
1. **Pulse Contact A** - One side of pulse switch
2. **Pulse Contact B** - Other side of pulse switch
3. **Shunt Contact A** - One side of shunt switch
4. **Shunt Contact B** - Other side of shunt switch

Wiring to ESP32:
```
Pulse Contact A → GPIO 17 (ROTARY_PULSE with internal pull-up enabled)
Pulse Contact B → GND
Shunt Contact A → GPIO 18 (ROTARY_ACTIVE with internal pull-up enabled)
Shunt Contact B → GND
```

**How it works:**
- **ROTARY_PULSE (GPIO 17)**: Counts pulses as dial returns to rest
  - With pull-up enabled: normally HIGH, goes LOW on each pulse
  - Each digit generates 1-10 pulses (digit "1" = 1 pulse, "0" = 10 pulses)
  
- **ROTARY_ACTIVE (GPIO 18)**: Detects when dial is being turned
  - With pull-up enabled: normally HIGH (shunt closed to GND)
  - Goes HIGH when you start turning the dial (shunt opens)
  - Returns to HIGH when dial returns to rest
  - Prevents false hook switch triggers during dialing

**Testing your dial:**
1. Connect pulse switch as shown above
2. Enable internal pull-up on GPIO 15
3. Monitor GPIO 17 with Serial.println()
4. Dial "1" → should see 1 pulse (LOW transition)
5. Dial "5" → should see 5 pulses
6. Dial "0" → should see 10 pulses

**Note:** The ESP32 code uses internal pull-up resistors, so **no external resistors needed**. Both GPIO 18 and 17 are configured with `INPUT_PULLUP` in the firmware.

## NEW: INMP441 Digital Microphone Wiring

**INMP441 I2S Digital Microphone → ESP32-S3**
```
INMP441 Pin      ESP32-S3 Pin    Signal
───────────────────────────────────────────
VCC             3.3V             Power
GND             GND              Ground
WS              GPIO 10          I2S0 Word Select (L/R Channel)
SCK             GPIO 9           I2S0 Bit Clock
SD              GPIO 6           I2S0 Serial Data In
L/R             GND              Left Channel (tie to ground)
```

**Handset Cord Upgrade Required:**
- **OLD**: 4-conductor cord (tip, ring, sleeve 1, sleeve 2)
- **NEW**: 7-conductor cord to support INMP441 digital microphone
  - INMP441: 5 wires (VCC, GND, WS, SCK, SD)
  - Speaker: 2 wires (SPK+, SPK-)
  - Total: 7 conductors needed

**Recommended: Standard RJ45 Ethernet Cable (8 conductors)**
```
Twisted Pair Assignment (T568B Standard):
Pair 1 (Blue):     VCC (3.3V) + GND           ← Power pair
Pair 2 (Orange):   WS + SCK                   ← I2S clock signals  
Pair 3 (Green):    SPK+ + SPK-                ← Speaker audio
Pair 4 (Brown):    SD + (spare)               ← I2S data + spare
```

Speaker wiring cautions
- MAX98357A outputs are bridged (BTL). Do not connect SPK+ or SPK− to ground
- Twist each speaker pair; keep them away from digital I2S lines to minimize interference

Grounding and antenna placement
- Common ground for all modules; star or short, thick traces/wires for 5V/GND to the amps
- Keep ESP32 antenna area clear of metal; use an external‑antenna version if the base is metal

## ARCHITECTURE BENEFITS: Dual I2S System

**Eliminated Complexity:**
- ❌ No more channel latching timing issues
- ❌ No more stereo channel confusion
- ❌ No more MAX9814 high-gain noise (60dB → 0dB)
- ❌ No more fragile initialization sequences

**Added Reliability:**
- ✅ Independent amplifier control (handset vs ringer)
- ✅ Pure digital microphone path (61dB SNR)
- ✅ Bulletproof audio routing
- ✅ Professional-grade audio quality

**Hardware Required:**
- ESP32-S3-DevKitC-1 (dual I2S support)
- INMP441 Digital Microphone ($3-5)
- Two MAX98357A I2S Amplifiers
- Standard RJ45 ethernet cable (8-conductor, uses 7)

That's the complete wiring picture for the new dual I2S architecture. The INMP441 provides excellent audio quality at half the cost of alternatives, while the dual I2S system eliminates all the reliability issues of the previous design.