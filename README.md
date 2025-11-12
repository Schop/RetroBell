# RetroBell

A retro rotary phone system using ESP32-S3 microcontrollers with peer-to-peer communication via ESP-NOW.

**"Who you gonna call?"**

## 🎯 Overview

RetroBell transforms vintage rotary phones into a modern VoIP system using ESP32 microcontrollers. Two (or more) phones can discover each other automatically and communicate peer-to-peer without requiring a central server.

## ✨ Features

- **Rotary Dial Input**: Authentic pulse counting from mechanical rotary dials
- **Hook Switch Detection**: Knows when handset is lifted or replaced
- **Dual Channel Audio**: Separate control of handset speaker and base ringer
- **Automatic Discovery**: Phones find each other automatically (no MAC address configuration)
- **Easy Setup**: First-time phone number configuration via serial or rotary dial
- **Authentic Tones**: Dial tone, ringback, and ring tones
- **State Machine**: Proper call flow (IDLE → OFF_HOOK → DIALING → CALLING → IN_CALL)

## 📁 Project Structure

```
RetroBell/
├── src/
│   ├── main.cpp           # Main program & state machine
│   ├── Pins.h             # GPIO pin definitions
│   ├── State.h            # State machine definitions
│   ├── Audio.cpp/h        # I2S audio & tone generation
│   ├── HookSwitch.cpp/h   # Handset on/off-hook detection
│   ├── RotaryDial.cpp/h   # Pulse counting & digit decoding
│   └── Network.cpp/h      # ESP-NOW peer-to-peer communication
├── data/
│   └── config.json        # Phone number & Wi-Fi credentials
├── platformio.ini         # PlatformIO configuration
├── wiring.md              # Hardware wiring diagram
└── instructions.md        # Build instructions

```

## 🔧 Hardware Components

- **ESP32-S3-DevKitC-1**: Main microcontroller
- **MAX98357A** (2x): I2S audio amplifiers (one for handset, one for ringer)
- **MAX9814**: Microphone preamplifier with AGC
- **Rotary Dial**: Vintage phone dial with pulse contacts
- **Hook Switch**: Detects handset position
- **Speakers** (2x): One in handset, one for base ringer

## 🚀 How It Works

### 1. **Audio System** (`Audio.cpp`)
- Uses I2S for digital audio output at 16kHz
- Channel latching ensures LEFT channel = ringer, RIGHT channel = handset
- Generates pure sine wave tones:
  - Dial tone: 350Hz continuous
  - Ringback: 440Hz, 2s on / 4s off
  - Ring: 440Hz, 2s on / 4s off

### 2. **Rotary Dial** (`RotaryDial.cpp`)
- Monitors two pins: ROTARY_ACTIVE (dialing state) and ROTARY_PULSE (pulse count)
- Counts falling edges on pulse pin
- Converts pulse count to digit (10 pulses = 0)
- 150ms timeout after last pulse to finalize digit

### 3. **Hook Switch** (`HookSwitch.cpp`)
- 50ms debouncing to prevent false triggers
- HIGH = handset lifted (off-hook)
- LOW = handset on cradle (on-hook)
- Answers calls when lifted during RINGING
- Ends calls when replaced

### 4. **Network Discovery** (`Network.cpp`)
- ESP-NOW provides low-latency peer-to-peer communication
- Every 10 seconds, broadcasts: "I'm phone #X"
- When broadcast received, sender is added to peer list
- No manual MAC address configuration needed!

### 5. **State Machine** (`main.cpp`)

```
IDLE
  ↓ (lift handset)
OFF_HOOK (play dial tone)
  ↓ (start dialing)
DIALING (stop dial tone)
  ↓ (complete number)
CALLING (play ringback tone)
  ↓ (peer answers)
IN_CALL (audio streaming)
  ↓ (hang up)
IDLE
```

Parallel path for incoming calls:
```
IDLE → RINGING (play ring tone) → IN_CALL
```

## 📝 Configuration

### Initial Setup

**Before first upload:**

1. Copy `data/config.json.template` to `data/config.json`
2. Edit `data/config.json` with your WiFi credentials:

```json
{
  "number": -1,
  "wifi_ssid": "YourWiFiNetwork",
  "wifi_password": "YourPassword"
}
```

3. Keep `"number": -1` for first-time setup

### First-Time Phone Number Setup

When you first power on the phone with `"number": -1` in config.json:

1. **Serial Monitor Method**:
   - Type `1` and press Enter
   - Type your phone number (e.g., `101`)
   - Press Enter

2. **Rotary Dial Method**:
   - Type `2` and press Enter (or just start dialing)
   - Dial your phone number using the rotary dial
   - Hang up the handset to save

### config.json Format

```json
{
  "number": 101,
  "wifi_ssid": "YourWiFiNetwork",
  "wifi_password": "YourPassword"
}
```

- `number`: This phone's number (0-999, or -1 for not configured)
- `wifi_ssid`: Your home Wi-Fi network name
- `wifi_password`: Your Wi-Fi password

**Note**: Wi-Fi connection improves reliability by ensuring phones are on the same channel, but ESP-NOW communication is direct peer-to-peer (doesn't go through the router).

## 🔌 Pin Connections

See `Pins.h` for complete pin definitions:

| Function | ESP32 Pin | Notes |
|----------|-----------|-------|
| I2S BCLK | 26 | Bit clock |
| I2S LRCLK | 25 | Left/Right clock |
| I2S DOUT | 22 | Data output |
| Handset Amp SD | 33 | Shutdown control |
| Ringer Amp SD | 32 | Shutdown control |
| Microphone | 4 | ADC input |
| Hook Switch | 18 | INPUT_PULLUP |
| Rotary Pulse | 15 | INPUT_PULLUP |
| Rotary Active | 14 | INPUT_PULLUP |

## 📞 Making a Call

1. Lift handset → hear dial tone
2. Dial the peer's number (e.g., dial `102`)
3. Hear ringback tone
4. Peer's phone rings
5. Peer lifts handset → both phones go IN_CALL
6. Talk!
7. Hang up to end call

## 🛠️ Building & Uploading

### Prerequisites
- PlatformIO (VS Code extension or CLI)
- ESP32-S3-DevKitC-1 board

### Steps

1. **Build the firmware**:
   ```
   pio run
   ```

2. **Upload firmware**:
   ```
   pio run --target upload
   ```

3. **Upload filesystem** (config.json):
   ```
   pio run --target uploadfs
   ```

4. **Monitor serial output**:
   ```
   pio device monitor
   ```

## 🎓 Code Walkthrough

### Main Loop Flow (`main.cpp`)

```cpp
loop() {
  // 1. Check hardware inputs
  handleHookSwitch();    // Is handset lifted?
  handleRotaryDial();    // Any digits dialed?
  
  // 2. Maintain services
  updateToneGeneration(); // Keep audio playing
  updateNetwork();        // Send discovery broadcasts
  
  // 3. Process dialed digits
  if (digit >= 0) {
    dialedNumber += digit;
    if (complete) {
      sendCallRequest(dialedNumber);
    }
  }
  
  // 4. State-specific actions
  switch (currentState) {
    case IDLE: /* wait */ break;
    case OFF_HOOK: playDialTone(); break;
    case CALLING: playRingbackTone(); break;
    case RINGING: playRingTone(); break;
    case IN_CALL: /* stream audio */ break;
  }
}
```

### Message Flow

**Outgoing Call**:
```
Phone A: sendCallRequest(102)
   → ESP-NOW →
Phone B: receives MSG_CALL_REQUEST → changeState(RINGING)
Phone B: User lifts handset → sendCallAccept(101)
   ← ESP-NOW ←
Phone A: receives MSG_CALL_ACCEPT → changeState(IN_CALL)
```

**Hanging Up**:
```
Phone A: User hangs up → sendCallEnd(102)
   → ESP-NOW →
Phone B: receives MSG_CALL_END → changeState(IDLE)
```

## 🔮 Future Enhancements

- [ ] **Audio Streaming**: Implement actual voice transmission using MSG_AUDIO_DATA
- [ ] **Microphone Input**: Read ADC values from MAX9814 and encode audio
- [ ] **Volume Control**: Use VOL_UP/VOL_DOWN buttons to adjust speaker volume
- [ ] **Call Rejection**: Add button to reject incoming calls
- [ ] **Multiple Calls**: Support call waiting or conference calls
- [ ] **Speed Dial**: Pre-program frequently called numbers
- [ ] **Display**: Add LCD to show caller ID or phone status

## 🐛 Troubleshooting

### Phones don't discover each other
- Check Wi-Fi connection (both phones should connect to same network)
- Verify serial monitor shows "Discovery broadcast sent"
- Wait up to 10 seconds for discovery to complete
- Check that both phones have different phone numbers

### No dial tone when lifting handset
- Check hook switch wiring (HOOK_SW_PIN = 18)
- Verify serial shows "State changed to: OFF_HOOK"
- Check amplifier connections and I2S pins

### Rotary dial doesn't register digits
- Verify ROTARY_PULSE_PIN and ROTARY_ACTIVE_PIN connections
- Check serial monitor for "Dial started" and pulse count messages
- Ensure dial is rotating fully and returning to rest position

### Audio is distorted or quiet
- Check I2S pin connections (BCLK, LRCLK, DOUT)
- Verify both amplifiers are enabled (SD pins HIGH)
- Check speaker connections
- Adjust amplitude in `generateTone()` (currently 8000)

## 📚 Key Concepts

### ESP-NOW
- Layer 2 protocol (uses MAC addresses, not IP)
- No Wi-Fi association required (but benefits from it)
- Max 250-byte messages
- Up to 20 encrypted peers or 6 unencrypted peers per device

### I2S Audio
- Synchronous serial interface for audio
- Three signals: BCLK (bit timing), LRCLK (channel timing), DOUT (data)
- Stereo interleaved: [LEFT sample][RIGHT sample][LEFT sample]...

### Channel Latching (MAX98357A)
- Amplifier reads LRCLK state on first unmute
- LRCLK LOW = become LEFT channel
- LRCLK HIGH = become RIGHT channel
- Must latch before starting I2S driver

## 📄 License

This project is open source. Feel free to modify and share!

## 🙏 Acknowledgments

- ESP-NOW for providing low-latency peer-to-peer communication
- MAX98357A for simple I2S audio output with channel selection
- The Arduino community for excellent ESP32 support

---

**Built with ❤️ using PlatformIO and ESP32**
