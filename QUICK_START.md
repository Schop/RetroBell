# RetroBell - Quick Start Guide

**"Where vintage meets voice"**

Get your RetroBell system up and running in minutes!

## 📋 Prerequisites

- 2x ESP32-S3-DevKitC-1 boards
- Hardware assembled per `wiring.md`
- PlatformIO installed in VS Code

## 🚀 Quick Setup (5 Minutes)

### Step 1: Edit Configuration

Open `data/config.json` and update with your Wi-Fi credentials:

```json
{
  "number": -1,
  "wifi_ssid": "YOUR_WIFI_NAME",
  "wifi_password": "YOUR_WIFI_PASSWORD"
}
```

**Keep `"number": -1` for first-time setup!**

### Step 2: Build and Upload (First Phone)

```bash
# Build the firmware
pio run

# Upload firmware to ESP32
pio run --target upload

# Upload config file to ESP32
pio run --target uploadfs

# Open serial monitor
pio device monitor
```

### Step 3: Configure Phone Number

You'll see:
```
*** FIRST TIME SETUP ***
Choose setup method:
  1. Enter number via Serial Monitor
  2. Dial number using Rotary Dial
```

**Option A - Serial Method:**
1. Type `1` and press Enter
2. Type `101` and press Enter
3. Done!

**Option B - Rotary Dial Method:**
1. Type `2` and press Enter
2. Dial `1-0-1` on the rotary dial
3. Hang up the handset
4. Done!

### Step 4: Repeat for Second Phone

1. Close serial monitor
2. Connect second ESP32
3. Repeat Step 2 (upload firmware & config)
4. In setup, choose number `102` instead of `101`

### Step 5: Make Your First Call!

Within 10 seconds, phones will discover each other automatically.

**On Phone #101:**
1. Lift handset → hear dial tone
2. Dial `1-0-2` on rotary dial
3. Hear ringback tone

**On Phone #102:**
1. Hear ringing
2. Lift handset
3. You're connected!

## 🎯 That's It!

Your vintage rotary phones are now a working VoIP system!

---

## 📞 Usage Cheat Sheet

### Making a Call
1. **Lift handset** → Dial tone plays
2. **Dial number** → Use rotary dial (e.g., 1-0-2)
3. **Wait** → Ringback tone plays
4. **Peer answers** → Connected!
5. **Hang up** → End call

### Receiving a Call
1. **Phone rings** → Base speaker makes ringing sound
2. **Lift handset** → Answer call
3. **Talk**
4. **Hang up** → End call

### Call States (shown in serial monitor)
- `IDLE` - Waiting for activity
- `OFF_HOOK` - Handset lifted, ready to dial
- `DIALING` - Actively dialing a number
- `CALLING` - Waiting for peer to answer
- `RINGING` - Incoming call
- `IN_CALL` - Active call

---

## 🔧 Troubleshooting

### Problem: Phone doesn't start
**Solution:** Check serial monitor for errors. Verify config.json was uploaded.

### Problem: No dial tone
**Solution:** 
- Check hook switch wiring (pin 18)
- Verify amplifier connections
- Check I2S pins (26, 25, 22)

### Problem: Rotary dial doesn't work
**Solution:**
- Check pins 14 (active) and 15 (pulse)
- Watch serial monitor for "Dial started" message
- Ensure dial fully rotates and returns

### Problem: Phones don't discover each other
**Solution:**
- Wait 10-20 seconds for discovery
- Check Wi-Fi connection (both phones must connect)
- Verify different phone numbers (101 vs 102)
- Check serial monitor for "Discovered phone #X"

### Problem: Call doesn't connect
**Solution:**
- Ensure discovery completed first
- Check serial monitor for "Call request sent"
- Verify both phones are powered on

---

## 💡 Pro Tips

1. **Serial Monitor is Your Friend**
   - Always keep it open during testing
   - Shows all state changes and network activity
   - Displays discovery and call messages

2. **Phone Numbers**
   - Can be any number 0-999
   - Different phones must have different numbers
   - Use -1 to reset and re-run setup mode

3. **Wi-Fi Connection**
   - Required for reliable operation
   - Phones communicate directly (not through router)
   - Wi-Fi just ensures same channel

4. **Discovery**
   - Happens automatically every 10 seconds
   - Survives phone reboots
   - Scales to more than 2 phones (up to 10)

5. **Adding More Phones**
   - Just assign different numbers (103, 104, etc.)
   - All phones discover each other automatically
   - No additional configuration needed

---

## 📚 Next Steps

- Read `README.md` for detailed system overview
- Check `wiring.md` for hardware connections
- Explore commented source code in `src/`
- Experiment with different phone numbers
- Add more phones to create a house network!

---

## 🆘 Still Having Issues?

1. Check wiring against `wiring.md`
2. Verify all connections with multimeter
3. Test individual components (hook switch, rotary dial)
4. Review serial monitor output carefully
5. Compare both phones' serial output

**Remember:** The serial monitor shows you exactly what the phone is thinking!

---

**Enjoy your retro VoIP system! 📞**
