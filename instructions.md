# RetroBell - ESP32 Rotary VoIP Phone

**"Where vintage meets voice"**

This project turns a classic rotary telephone into a modern, network-connected VoIP phone using an ESP32-S3 microcontroller.

## Features

- **Peer-to-Peer Calling:** Two or more phones can call each other directly over a local Wi-Fi network without needing a central server (using ESP-NOW).
- **Rotary Dialing:** Faithfully interprets the pulses from a vintage rotary dial to "dial" a number.
- **Dual Audio Output:** Uses two amplifiers to drive the handset earpiece and a base-mounted ringer speaker independently.
- **High-Quality Audio:** Employs I2S for digital audio playback and a MAX9814 preamplifier for the handset's microphone.
- **Bidirectional Voice:** Real-time audio streaming between phones during calls.
- **Configurable Identity:** Each phone's "number" is set via a configuration file, allowing the same firmware to run on all devices.
- **Authentic Experience:** Responds to on-hook/off-hook events, plays dial tones, ringback, and generates ringing sounds.
- **Error Handling:** Fast busy tone when dialing non-existent numbers.

## System Overview

1.  **Startup:** The phone powers on, connects to Wi-Fi, and reads its configuration from the onboard flash memory.
2.  **Discovery:** It discovers other phones on the network.
3.  **Idle:** The phone waits for the handset to be lifted or for an incoming call request from a peer.
4.  **Off-Hook & Dialing:** When the handset is lifted, a dial tone is played. The user can then use the rotary dial to enter a number.
5.  **Placing a Call:** The phone sends a call request to the dialed number.
6.  **Receiving a Call:** If the phone receives a call request while on-hook, it rings the base speaker.
7.  **In-Call:** Once the call is answered, a two-way audio stream is established.
8.  **Hang-Up:** The call ends when either party places their handset back on the hook.

---

1) Parts needed (for two complete phones)
- 2× ESP32 dev boards (ESP32-S3-DevKitC-1 or ESP32-DevKitC-32E)
- 4× MAX98357A I2S class-D amp modules (2 per phone: handset + base ringer)
- 2× Slim 8 Ω handset speakers (28–36 mm, 0.5–1 W; choose size to fit your earpiece)
- 2× 8 Ω base speakers (40–50 mm, 1–3 W) for ringing in the base
- 2× MAX9814 electret microphone preamp modules (AGC), mounted in the base
- 2× Electret mic capsules (handset mouthpiece; 9–10 mm or 20–25 mm “telephone insert”)
- 2× Lever microswitches (SPDT) for the hook switch
- 2× 5 V USB wall adapters (1–2 A) and 2× panel-mount USB-C (or micro-USB) feed-throughs
- Passive/support:
  - 4× 10 kΩ resistors (pull-ups if needed)
  - 4× 100 nF capacitors (debounce for hook, optional)
  - 2× 470 µF electrolytic capacitors near the amps (helps with ring transients)
  - Assorted 22–26 AWG wire, small perfboard, JST/Dupont connectors, standoffs, foam/mesh, heat-shrink, adhesive
- Handset cords:
  - Use existing 4-conductor RJ9/RJ10 cords (OK with electret + MAX9814 in base)
  - Optional replacement cords if needed

2) How it’s connected (per phone)

Power and grounds
- USB 5 V → ESP32 board.
- From the ESP32’s 5 V/VBUS header pin → both MAX98357A VIN pins.
- ESP32 3.3 V → MAX9814 VCC. All grounds common.

Speakers (two amps, each bridged/BTL — never tie either speaker lead to ground)
- Handset amp (MAX98357A #1) SPK+/- → handset speaker (in the handset).
- Base ringer amp (MAX98357A #2) SPK+/- → base speaker (inside the phone base).

I2S audio (one stereo TX bus feeding both amps; each amp locked to a different channel)
- ESP32 I2S TX pins (example mapping):
  - BCLK → GPIO 26 → both amps BCLK
  - LRCLK/WS → GPIO 25 → both amps LRCLK
  - DATA_OUT → GPIO 22 → both amps DIN
- Amp SD (enable) pins on separate GPIOs so you can latch channels:
  - Handset amp SD → GPIO 33
  - Base amp SD → GPIO 32
- Channel selection at boot (staggered enable):
  - Drive LRCLK low as GPIO, raise handset SD (it latches LEFT)
  - Drive LRCLK high, raise base SD (it latches RIGHT)
  - Then start the I2S peripheral. In software, send:
    - Left channel = base ring tone only
    - Right channel = handset audio (dial tone, ringback, far-end audio, sidetone)

Microphone (electret capsule in handset, MAX9814 in base)
- Handset mic capsule two wires → through the 4-wire cord → MAX9814 MIC+/MIC− in the base (twisted pair).
- MAX9814 OUT → ESP32 ADC pin (example: GPIO 4).
- MAX9814 VCC → 3.3 V; GND → GND. Start with 40–50 dB gain/AGC.

Hook switch and optional buttons
- Hook switch: COM → GND, NO → GPIO 18 (enable internal pull-up; optional 100 nF to GND for debounce).
- Optional buttons: VOL_UP → GPIO 16, VOL_DOWN → GPIO 17 (pull-ups enabled, button to GND).
- Optional STATUS LED to any free GPIO with a series resistor.

Handset cord wire allocation (4 conductors total)
- 2 wires: handset speaker from base amp SPK+/SPK−
- 2 wires: handset mic capsule back to MAX9814 MIC+/MIC−
- Keep speaker wires and mic pair separated to reduce buzz.

3) How the software should work

Overall approach
- Simple peer-to-peer “hotline” over your home Wi‑Fi. No SIP server.
- Two data planes:
  - Control: small JSON messages over UDP or TCP (e.g., UDP port 15060)
  - Media: audio over UDP (e.g., port 20000), low-latency

Boot/init sequence
- Bring up Wi‑Fi; optionally use static IPs or mDNS.
- Latch amp channels:
  - Hold both SD low; drive LRCLK low; set Handset SD high; delay ~2 ms.
  - Drive LRCLK high; set Base SD high; delay ~2 ms.
  - Start I2S TX (stereo) with your chosen pins.
- Initialize ADC for mic (MAX9814 OUT).
- Start control and media sockets; announce presence via broadcast/mDNS with name, extension, and media port.

Control signaling (minimal JSON messages)
- HELLO: advertises extension/IP/ports; cache peers for “directory”
- INVITE: caller requests callee to ring
- RINGING: callee is ringing
- ANSWER: callee answered; both start audio
- BYE: end call
- KEEPALIVE: periodic idle heartbeat (optional)

Call flows
- Outgoing:
  - Off-hook → start dial tone in handset (Right channel).
  - Send INVITE to target. While waiting, stop dial tone and play ringback in handset (Right).
  - On ANSWER → stop ringback; start full-duplex media.
- Incoming:
  - On INVITE while on-hook → play ring cadence in base only (Left channel).
  - On off-hook → send ANSWER, stop ring, start full-duplex media.

Audio specifics
- Capture: ESP32 ADC samples MAX9814 output at 8 kHz, 16-bit mono.
- Encode: G.711 μ-law (8 kHz), 20 ms frames (160 samples → 160 bytes payload).
- Send: UDP packets every 20 ms with sequence and timestamp.
- Receive: small jitter buffer (about 60 ms = 3 frames). If a packet is missing, repeat last frame or add comfort noise.
- Playback:
  - During call: decode to PCM and play on handset channel (Right).
  - Ringing: synthesize ring tone to base channel (Left) with 2 s on / 4 s off cadence.
  - Outgoing ringback: handset channel (Right) 440+480 Hz, 2 s on / 4 s off.
  - Dial tone: handset channel (Right) 350+440 Hz continuous while idle off-hook.
- Sidetone: mix a small percentage of local mic PCM into the handset playback to feel natural.
- Tone generation: synthesize tones in code (no files needed). Optionally store a short “mechanical bell” sample in flash and loop it for the base ring.

Networking and robustness
- Same LAN makes it straightforward. Optionally tag media packets with DSCP EF (46).
- Disable Wi‑Fi modem sleep during active calls to reduce jitter.
- Send KEEPALIVE every 5–10 seconds when idle to maintain peer info.
- Timeouts: cancel ringing if no answer after e.g., 30–60 seconds.

State machine (per phone)
- Idle: on INVITE → Ringing; on off-hook → Outgoing (dial tone → INVITE).
- Ringing: play base ring; on local off-hook → ANSWER → Active; on BYE/timeout → Idle.
- Outgoing: send INVITE; ringback on handset; on ANSWER → Active; on on-hook/BYE → Idle.
- Active: bidirectional audio; on on-hook/BYE → Idle.

Tuning tips
- MAX9814 at 3.3 V so its output bias matches the ESP32 ADC.
- Software high-pass ~120–150 Hz on mic.
- Start volumes low; ramp after I2S stable.
- Keep class-D speaker wires away from mic pair; twist each pair.