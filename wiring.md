Here’s a one‑page wiring “diagram” you can follow. It’s for Variant A with two MAX98357A amps per phone (one for the handset, one in the base for ring), an electret capsule in the handset, and a MAX9814 preamp in the base. Pin map is for ESP32‑S3‑DevKitC‑1. Notes for classic ESP32 are included where it matters.

Per phone: connections at a glance
- ESP32-S3-DevKitC-1
  - 5V (VBUS) → feeds both MAX98357A VIN pins
  - 3V3 → powers MAX9814 VCC
  - GND → common to all modules
  - I2S0 TX (stereo out to both amps)
    - BCLK: GPIO 26 → both MAX98357A BCLK
    - LRCLK/WS: GPIO 25 → both MAX98357A LRCLK
    - DATA_OUT: GPIO 22 → both MAX98357A DIN
  - Amp enables (to latch channels and allow mute)
    - Handset amp SD: GPIO 33
    - Base (ringer) amp SD: GPIO 32
  - Mic input (from MAX9814 OUT)
    - MIC_ADC: GPIO 4 (ADC input on S3)
      - If using classic ESP32 (WROOM-32), use GPIO 34 (ADC1) to avoid Wi‑Fi conflicts
  - Hook switch input
    - HOOK_SW: GPIO 18 (enable internal pull‑up; optional 100 nF to GND for debounce)
  - Rotary Dial input
    - ROTARY_PULSE: GPIO 15 (input with pull-up)
    - ROTARY_ACTIVE: GPIO 14 (input with pull-up)
  - Optional buttons
    - VOL_UP: GPIO 16 (pull‑up enabled, button to GND)
    - VOL_DOWN: GPIO 17 (pull‑up enabled, button to GND)
  - Optional status LED: any free GPIO via 330 Ω to LED anode; LED cathode to GND

- MAX98357A (handset amp)
  - VIN → 5V (from ESP32 VBUS)
  - GND → GND
  - BCLK → GPIO 26
  - LRCLK/WS → GPIO 25
  - DIN → GPIO 22
  - SD → GPIO 33
  - SPK+ / SPK− → handset speaker (two wires through handset cord)
  - Note: This amp will be latched to one stereo channel (see boot sequence)

- MAX98357A (base ringer amp)
  - VIN → 5V (from ESP32 VBUS)
  - GND → GND
  - BCLK → GPIO 26
  - LRCLK/WS → GPIO 25
  - DIN → GPIO 22
  - SD → GPIO 32
  - SPK+ / SPK− → base speaker mounted in the phone base

- MAX9814 mic preamp (in base)
  - VCC → 3V3
  - GND → GND
  - MIC+ / MIC− → two wires from the handset electret capsule (twisted pair through handset cord)
    - If your board has an onboard mic, remove it and use those pads for the handset capsule
  - OUT → ESP32 MIC_ADC (GPIO 4 on S3; GPIO 34 on classic ESP32)
  - (Optional on some boards) GAIN/TH/AR pins → leave at defaults initially (start at 40–50 dB max gain)

Handset cord (4 conductors total)
- Two wires: handset speaker from base handset amp SPK+ and SPK−
- Two wires: handset electret capsule back to MAX9814 MIC+ and MIC−
- Keep the speaker pair and mic pair separated inside the base for noise immunity

Power distribution and decoupling
- USB 5V → ESP32 USB‑C; tap 5V (VBUS) on the ESP32 header to feed both amps’ VIN
- 3V3 from ESP32 → MAX9814 VCC only (don’t power MAX98357A from 3V3)
- Add a 470 µF electrolytic across 5V and GND physically near the two MAX98357A boards
- Add 0.1 µF ceramic close to each module’s power pin if not already on the breakout

Channel assignment (left = base ring, right = handset audio)
- Both amps share the same I2S lines; you assign channels by how you enable them at boot:
  1) Hold both SD pins LOW (both amps off)
  2) Temporarily drive LRCLK (GPIO 25) as a plain GPIO LOW
  3) Set Handset SD (GPIO 33) HIGH → handset amp latches LEFT
  4) Drive LRCLK (GPIO 25) HIGH
  5) Set Base SD (GPIO 32) HIGH → base amp latches RIGHT
  6) Hand GPIO 25 back to the I2S peripheral and start I2S TX
- In your playback:
  - Send base ring tone on the LEFT channel only (base amp hears it)
  - Send dial tone, ringback, and far‑end audio on the RIGHT channel only (handset amp hears it)
- If you prefer the opposite mapping, just swap which amp you enable at LRCLK LOW/HIGH or swap which channel you place which audio

Hook switch wiring
- Microswitch COM → GND
- Microswitch NO → ESP32 HOOK_SW (GPIO 18) with internal pull‑up enabled
- Optional RC debounce: 100 nF from GPIO 18 to GND

Rotary Dial wiring
- Most rotary dials have at least two terminals for pulsing. Connect these to the ESP32 ROTARY_PULSE pin (GPIO 15) and GND.
- Many dials also have an "off-normal" switch with two additional terminals. This switch closes when you begin to turn the dial and opens when it returns to rest. Connect these to ROTARY_ACTIVE (GPIO 14) and GND. This signal is useful for ignoring hook switch activity while dialing.
- Enable internal pull-ups on both GPIO 14 and 15.

Speaker wiring cautions
- MAX98357A outputs are bridged (BTL). Do not connect SPK+ or SPK− to ground
- Twist each speaker pair; keep them away from the mic pair to minimize buzz

Grounding and antenna placement
- Common ground for all modules; star or short, thick traces/wires for 5V/GND to the amps
- Keep ESP32 antenna area clear of metal; use an external‑antenna version if the base is metal

Classic ESP32 (DevKitC‑32E) notes
- Use MIC_ADC = GPIO 34 (ADC1) instead of GPIO 4
- Other pin choices (22/25/26/32/33/16/17/18) work as listed

That’s the full wiring picture you can build from. If you want, share your specific MAX98357A and MAX9814 breakout models, and I’ll confirm any board‑specific pin labels or jumpers before you solder.