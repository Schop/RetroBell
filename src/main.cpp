/*
 * RetroBell - Main Controller
 * 
 * A retro rotary phone system using ESP32-S3 with ESP-NOW for peer-to-peer communication.
 * 
 * Features:
 * - Rotary dial input
 * - Hook switch detection
 * - I2S audio output (dual channel for handset + ringer)
 * - Bidirectional voice streaming
 * - Automatic peer discovery
 * - Interactive first-time setup
 * 
 */

#include "State.h"
#include "Pins.h"
#include "Audio.h"
#include "HookSwitch.h"
#include "RotaryDial.h"
#include "Network.h"
#include "Configuration.h"
#include "WebInterface.h"
#include <Arduino.h>

// Configuration
PhoneConfig config;

// --- Function Prototypes ---
// (None - all functions moved to appropriate modules)

/*
 * Setup - Runs once at boot
 * 
 * Initializes all hardware and software components:
 * 1. Hardware setup (pins, audio, etc.)
 * 2. Load configuration from file
 * 3. Run first-time setup if needed (number not configured)
 * 4. Connect to Wi-Fi
 * 5. Initialize network discovery
 */
void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial monitor time to connect
  Serial.println("\n\n=================================");
  Serial.println("      RetroBell Starting Up");
  Serial.println("=================================");

  // Setup hardware components
  setupHookSwitch();   // Initialize hook switch with pull-up resistor
  setupRotaryDial();   // Initialize rotary dial pins
  setupAudio();        // Configure I2S and amplifiers
  setupMicrophone();   // Configure ADC for microphone input

  // Setup configuration system
  setupConfiguration();    // Initialize LittleFS filesystem
  loadConfiguration(config); // Load phone number and Wi-Fi credentials
  
  // Check if phone number is configured (-1 means not set up yet)
  if (config.phoneNumber == -1) {
    Serial.println("\n*** FIRST TIME SETUP ***");
    runSetupMode(config);      // Interactive setup via serial or rotary dial
    saveConfiguration(config); // Save the chosen number to config file
  }
  
  setupWifi(config.wifiSsid.c_str(), config.wifiPassword.c_str()); // Connect to Wi-Fi router
  printMacAddress();   // Display MAC address for debugging
  setupNetwork();      // Initialize ESP-NOW and start discovery
  setupWebInterface(); // Start web server for debug interface

  Serial.println("\n=================================");
  Serial.print("Phone #");
  Serial.print(config.phoneNumber);
  Serial.println(" is ready!");
  Serial.println("=================================\n");
  changeState(IDLE);
}

/*
 * Main Loop - Runs continuously
 * 
 * This is the heart of the phone system. It:
 * 1. Monitors hardware inputs (hook switch, rotary dial)
 * 2. Maintains audio tone generation
 * 3. Handles network discovery broadcasts
 * 4. Manages state transitions (IDLE -> OFF_HOOK -> DIALING -> CALLING -> IN_CALL)
 */
void loop() {
  // Poll hardware inputs
  handleHookSwitch();              // Check if handset is lifted/replaced
  handleRotaryDial();              // Check for rotary dial pulses
  
  // Maintain ongoing services
  updateToneGeneration();          // Keep audio tones playing (dial tone, ringback, etc.)
  updateNetwork();                 // Send periodic discovery broadcasts
  handleWebInterface();            // Process web server requests

  // ====== Dialing Logic ======
  // Check if user has completed dialing a phone number
  if (getCurrentState() == OFF_HOOK || getCurrentState() == DIALING) {
    // Check if user started dialing (transition OFF_HOOK → DIALING)
    if (getCurrentState() == OFF_HOOK && hasStartedDialing()) {
      changeState(DIALING);
    }
    
    // Check if dialing is complete
    if (isDialingComplete()) {
      int targetNumber = getDialedNumber();
      
      Serial.print("Calling number: ");
      Serial.println(targetNumber);
      
      // Try to send call request
      if (sendCallRequest(targetNumber)) {
        // Peer found, waiting for answer
        changeState(CALLING);
      } else {
        // Peer not found, play error tone
        changeState(CALL_FAILED);
      }
      resetDialedNumber(); // Clear for next call
    }
  }

  // ====== Main State Machine ======
  // Each state handles different phone behaviors
  switch (getCurrentState()) {
    case IDLE:
      // Waiting for handset to be lifted or for an incoming call
      resetDialedNumber(); // Ensure dialing system is reset
      stopTone();          // Ensure no tones are playing
      break;
      
    case OFF_HOOK:
      // Handset is lifted - play dial tone to indicate ready to dial
      playDialTone();
      break;
      
    case DIALING:
      stopTone(); // Stop dial tone when dialing starts
      // Dialing logic is handled above in the dialing section
      break;
      
    case CALLING:
      // We've sent a call request - play ringback tone while waiting for answer
      playRingbackTone();
      // Wait for peer to answer (handled by network callbacks in Network.cpp)
      break;
      
    case RINGING:
      // Incoming call - ring the base speaker
      playRingTone();
      // If user picks up handset, HookSwitch.cpp will send call accept
      break;
      
    case CALL_FAILED:
      // Call failed (number not found) - play fast busy/error tone
      playErrorTone();
      // User must hang up to return to IDLE
      break;
      
    case IN_CALL:
      stopTone(); // Stop any tones when in call
      
      // Stream audio bidirectionally during call
      // Read from microphone and send to peer
      int16_t audioBuffer[AUDIO_SAMPLES_PER_PACKET];
      if (readMicrophoneBuffer(audioBuffer, AUDIO_SAMPLES_PER_PACKET)) {
        sendAudioData(audioBuffer, AUDIO_SAMPLES_PER_PACKET);
      }
      // Receiving audio is handled automatically in Network.cpp callback
      break;
  }
}
