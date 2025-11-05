# RetroBell - Code Architecture Overview

**"Who you gonna call?"**

## 📐 System Design

The RetroBell firmware is organized into modular components, each handling a specific aspect of the phone system.

```
┌─────────────────────────────────────────────────────────────┐
│                         main.cpp                            │
│                  (Central Coordinator)                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         State Machine & Call Logic                   │   │
│  │  IDLE → OFF_HOOK → DIALING → CALLING → IN_CALL       │   │
│  └──────────────────────────────────────────────────────┘   │
└────────┬──────────┬──────────┬──────────┬──────────┬────────┘
         │          │          │          │          │
         ▼          ▼          ▼          ▼          ▼
    ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
    │ Audio  │ │  Hook  │ │Rotary  │ │Network │ │ State  │
    │ Module │ │Switch  │ │ Dial   │ │ Module │ │ Module │
    └────────┘ └────────┘ └────────┘ └────────┘ └────────┘
        │          │          │          │          │
        ▼          ▼          ▼          ▼          │
    ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐   │
    │  I2S   │ │  GPIO  │ │  GPIO  │ │ESP-NOW │   │
    │Hardware│ │Hardware│ │Hardware│ │Protocol│   │
    └────────┘ └────────┘ └────────┘ └────────┘   │
                                                    │
                                                    ▼
                                            ┌──────────────┐
                                            │  Pins.h      │
                                            │(Pin Mapping) │
                                            └──────────────┘
```

---

## 🗂️ Module Responsibilities

### 1. **main.cpp** - The Orchestrator
**Role:** Central coordinator and state machine

**Responsibilities:**
- Boot sequence and initialization
- Configuration management (load/save)
- First-time setup mode
- State machine logic
- Digit collection and timeout
- State-specific actions (play tones, etc.)

**Key Functions:**
```cpp
setup()                  // Initialize all modules
loop()                   // Main execution loop
loadConfiguration()      // Read config from flash
saveConfiguration()      // Write config to flash
runSetupMode()          // Interactive phone number setup
changeState()           // Transition between states
```

**Dependencies:** Everything (it's the coordinator)

---

### 2. **Audio.cpp/h** - Sound Generator
**Role:** I2S audio output and tone generation

**Responsibilities:**
- Initialize I2S peripheral
- Channel latching for dual amplifiers
- Generate sine wave tones
- Manage tone cadences (on/off patterns)
- Output stereo audio (LEFT=ringer, RIGHT=handset)

**Key Functions:**
```cpp
setupAudio()            // Configure I2S and amplifiers
playDialTone()          // Start dial tone (continuous)
playRingbackTone()      // Start ringback (2s on, 4s off)
playRingTone()          // Start ring (2s on, 4s off)
stopTone()              // Stop all audio
updateToneGeneration()  // Called from loop to maintain audio
generateTone()          // Internal: create sine waves
```

**Hardware Interface:**
- I2S pins: BCLK (26), LRCLK (25), DOUT (22)
- Amplifier control: SD_LEFT (32), SD_RIGHT (33)

**Dependencies:** Pins.h

**Design Notes:**
- Uses channel latching trick for dual amp setup
- Generates tones in real-time (no pre-recorded audio)
- Cadence timing handled in updateToneGeneration()

---

### 3. **HookSwitch.cpp/h** - Handset Detection
**Role:** Detect when handset is lifted or replaced

**Responsibilities:**
- Read hook switch state with debouncing
- Trigger state transitions (IDLE ↔ OFF_HOOK)
- Answer incoming calls (RINGING → IN_CALL)
- End calls when hung up

**Key Functions:**
```cpp
setupHookSwitch()       // Configure pin with pull-up
handleHookSwitch()      // Read state and trigger actions
```

**Hardware Interface:**
- Pin 18: Hook switch input (LOW=on-hook, HIGH=off-hook)

**Dependencies:** Pins.h, State.h, Network.h

**Design Notes:**
- 50ms debouncing prevents false triggers
- Different actions based on current state
- Calls Network module to send call accept/end

---

### 4. **RotaryDial.cpp/h** - Digit Decoder
**Role:** Count pulses from rotary dial and decode digits

**Responsibilities:**
- Monitor dial active state
- Count falling edges on pulse pin
- Convert pulse count to digit (10 pulses = 0)
- Implement pulse debouncing
- Provide digit to main loop when ready

**Key Functions:**
```cpp
setupRotaryDial()       // Configure pins with pull-ups
handleRotaryDial()      // Monitor and count pulses
getDialedDigit()        // Return digit if ready
clearDialedDigit()      // Mark digit as consumed
```

**Hardware Interface:**
- Pin 14: ROTARY_ACTIVE (LOW=dialing, HIGH=idle)
- Pin 15: ROTARY_PULSE (generates pulses)

**Dependencies:** Pins.h

**Design Notes:**
- 10ms pulse debouncing
- 150ms timeout after last pulse
- Stateless (main.cpp handles digit collection)

---

### 5. **Network.cpp/h** - Peer Communication
**Role:** ESP-NOW peer-to-peer messaging and discovery

**Responsibilities:**
- Initialize ESP-NOW protocol
- Broadcast discovery every 10 seconds
- Maintain peer directory (phone# → MAC address)
- Send/receive call control messages
- Handle incoming message callbacks

**Key Functions:**
```cpp
setupNetwork()          // Initialize ESP-NOW
broadcastDiscovery()    // Announce presence
updateNetwork()         // Periodic discovery maintenance
addPeerByMac()          // Register discovered peer
sendCallRequest()       // Initiate call
sendCallAccept()        // Answer call
sendCallEnd()           // Hang up call
handleIncomingMessage() // Process received messages
getCurrentCallPeer()    // Get active call peer number
```

**Message Types:**
- `MSG_DISCOVERY`: Broadcast announcement
- `MSG_CALL_REQUEST`: Incoming call
- `MSG_CALL_ACCEPT`: Call answered
- `MSG_CALL_REJECT`: Call declined
- `MSG_CALL_END`: Hang up
- `MSG_AUDIO_DATA`: Voice data (future)

**Dependencies:** State.h

**Design Notes:**
- Uses broadcast address (FF:FF:FF:FF:FF:FF) for discovery
- Maintains array of up to 10 peers
- Messages filtered by toNumber field
- Callback function invoked by ESP-NOW

---

### 6. **State.h** - State Definitions
**Role:** Define phone states and state transition function

**Responsibilities:**
- Define PhoneState enum
- Declare changeState() function
- Break circular dependencies

**States:**
```cpp
IDLE        // Phone at rest
OFF_HOOK    // Handset lifted, dial tone
DIALING     // Actively dialing
CALLING     // Waiting for answer, ringback tone
RINGING     // Incoming call, ring tone
IN_CALL     // Connected call
```

**Dependencies:** None (pure definitions)

**Design Notes:**
- Minimal header to prevent circular includes
- Implementation of changeState() is in main.cpp

---

### 7. **Pins.h** - Hardware Mapping
**Role:** Central location for all GPIO pin assignments

**Responsibilities:**
- Define pin numbers for all peripherals
- Document pin purposes
- Make pin changes easy

**Pin Groups:**
- I2S Audio: 26, 25, 22
- Amplifiers: 33, 32
- Microphone: 4
- Hook Switch: 18
- Rotary Dial: 15, 14
- Buttons: 16, 17

**Dependencies:** None (pure definitions)

---

## 🔄 Data Flow Examples

### Example 1: Making a Call

```
1. User lifts handset
   └→ HookSwitch detects HIGH
      └→ changeState(OFF_HOOK)
         └→ main.cpp: playDialTone()

2. User dials 1-0-2
   └→ RotaryDial counts pulses
      └→ getDialedDigit() returns 1, then 0, then 2
         └→ main.cpp collects: dialedNumber = "102"
            └→ changeState(DIALING), then CALLING
               └→ Network.sendCallRequest(102)

3. Message sent via ESP-NOW
   └→ Peer receives MSG_CALL_REQUEST
      └→ Network.handleIncomingMessage()
         └→ changeState(RINGING)
            └→ main.cpp: playRingTone()

4. Peer lifts handset
   └→ HookSwitch detects RINGING state
      └→ Network.sendCallAccept(101)
         └→ changeState(IN_CALL)

5. Caller receives accept message
   └→ Network.handleIncomingMessage()
      └→ changeState(IN_CALL)
```

### Example 2: Audio Tone Generation

```
1. main.cpp calls playDialTone()
   └→ Audio: currentTone = TONE_DIAL

2. Every loop iteration:
   └→ main.cpp calls updateToneGeneration()
      └→ Audio.cpp checks currentTone
         └→ If TONE_DIAL: generateTone(440Hz, right channel)
            └→ Calculate sine wave samples
               └→ Write to I2S DMA buffer
                  └→ I2S peripheral sends to amplifiers
                     └→ Handset speaker plays tone
```

### Example 3: Peer Discovery

```
1. Every 10 seconds:
   └→ Network.updateNetwork()
      └→ broadcastDiscovery()
         └→ Create MSG_DISCOVERY message
            └→ Set fromNumber = config_myNumber
               └→ esp_now_send(FF:FF:FF:FF:FF:FF)

2. Other phones receive broadcast:
   └→ ESP-NOW triggers callback
      └→ handleIncomingMessage()
         └→ Check type == MSG_DISCOVERY
            └→ addPeerByMac(sender_mac, sender_number)
               └→ Add to peer directory
                  └→ Can now send direct messages
```

---

## 📊 Memory Usage

**Flash:** 770,657 bytes (23.1% of 3,342,336 bytes)
**RAM:** 43,776 bytes (13.4% of 327,680 bytes)

**Breakdown:**
- Framework/Libraries: ~500KB
- Application Code: ~270KB
- Audio Buffers: ~2KB
- Network Peer List: ~600 bytes
- Config & State: <1KB

---

## 🔐 Critical Sections

### 1. Channel Latching (Audio.cpp)
**Why Critical:** Must happen before I2S driver starts
**Sequence:**
1. Both amps OFF
2. LRCLK LOW → Enable ringer amp (latches to LEFT)
3. LRCLK HIGH → Enable handset amp (latches to RIGHT)
4. Start I2S driver (takes over LRCLK)

### 2. Message Filtering (Network.cpp)
**Why Critical:** Prevents phones from processing wrong messages
**Check:** `if (msg->toNumber != config_myNumber) return;`

### 3. Debouncing (HookSwitch.cpp, RotaryDial.cpp)
**Why Critical:** Mechanical switches bounce
**Solution:** Require stable state for 10-50ms

---

## 🎯 Extension Points

### Adding Audio Streaming
**Where:** Network.cpp + main.cpp IN_CALL state
**How:**
1. Read microphone ADC (pin 4)
2. Encode to 8-bit μ-law or 16-bit PCM
3. Send via MSG_AUDIO_DATA messages
4. Decode and output to I2S on receiver

### Adding More Phones
**Where:** No code changes needed!
**How:**
1. Flash same firmware to new ESP32
2. Give it unique phone number (103, 104, etc.)
3. Discovery handles the rest

### Adding Volume Control
**Where:** Audio.cpp
**How:**
1. Read VOL_UP_PIN (16) and VOL_DOWN_PIN (17)
2. Adjust `amplitude` variable in generateTone()
3. Store volume setting in config.json

---

## 🧪 Testing Strategy

### Unit Testing
- Each module is independently testable
- Mock State.h for testing Network.cpp
- Mock Hardware for testing Audio.cpp

### Integration Testing
1. Hook Switch → State changes
2. Rotary Dial → Digit collection
3. Network → Discovery and messaging
4. Audio → Tone generation

### System Testing
1. Full call flow (dial → ring → answer → hang up)
2. Discovery with multiple phones
3. Error cases (peer not found, timeout)

---

## 📝 Code Conventions

- **File naming:** Module.cpp/h (capitalized)
- **Function naming:** camelCase
- **Constants:** UPPER_SNAKE_CASE
- **Pin definitions:** XXX_PIN format
- **Comments:** Block comments for files/functions, inline for complex logic
- **Serial output:** Informative messages for debugging

---

**This architecture provides a solid foundation for a vintage rotary phone VoIP system!**
