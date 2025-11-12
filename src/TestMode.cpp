/*
 * TestMode.cpp - Hardware Testing and Diagnostics Implementation
 * 
 * Interactive test mode for validating RetroBell hardware components.
 * Provides comprehensive testing capabilities via serial interface.
 */

#include "TestMode.h"
#include "Audio.h"
#include "Pins.h"
#include "State.h"
#include <Arduino.h>

// Test mode state
bool testModeActive = false;
String testCommandBuffer = "";

// Microphone test state
bool micTestActive = false;
bool micRecordActive = false;
unsigned long micTestStartTime = 0;
int16_t micRecordBuffer[1000]; // Buffer for recording ~62ms at 16kHz
int micRecordIndex = 0;
bool micPlaybackActive = false;
int micPlaybackIndex = 0;

// Audio test state
enum AudioTestType {
  AUDIO_TEST_NONE,
  AUDIO_TEST_HANDSET,
  AUDIO_TEST_RINGER,
  AUDIO_TEST_BOTH
};
AudioTestType currentAudioTest = AUDIO_TEST_NONE;
unsigned long audioTestStartTime = 0;

/*
 * Setup Test Mode
 * Initialize test mode system (called from main setup)
 */
void setupTestMode() {
  testModeActive = false;
  testCommandBuffer = "";
  Serial.println("Test mode initialized. Type 'test help' for commands.");
}

/*
 * Handle Test Mode
 * Called from main loop to process test mode functionality
 */
void handleTestMode() {
  // Process serial commands
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (testCommandBuffer.length() > 0) {
        processTestCommand(testCommandBuffer);
        testCommandBuffer = "";
      }
    } else if (c >= ' ') { // Printable characters only
      testCommandBuffer += c;
    }
  }
  
  // Handle active tests
  if (testModeActive) {
    handleMicrophoneTest();
    handleAudioTest();
  }
}

/*
 * Process Test Command
 * Parse and execute test commands from serial input
 */
void processTestCommand(String command) {
  command.trim();
  command.toLowerCase();
  
  // Always allow help and enter commands
  if (command == "test help") {
    showTestHelp();
    return;
  }
  
  if (command == "test enter") {
    enterTestMode();
    return;
  }
  
  if (!testModeActive) {
    Serial.println("Test mode not active. Type 'test enter' first.");
    return;
  }
  
  // Test mode commands
  if (command == "test exit") {
    exitTestMode();
  } else if (command == "test audio handset") {
    testHandsetAudio();
  } else if (command == "test audio ringer") {
    testRingerAudio();
  } else if (command == "test audio both") {
    testBothAudio();
  } else if (command == "test audio stop") {
    stopAudioTest();
  } else if (command == "test mic level") {
    testMicrophoneLevel();
  } else if (command == "test mic record") {
    testMicrophoneRecord();
  } else if (command == "test mic stop") {
    stopMicrophoneTest();
  } else if (command == "test pins") {
    testPinStates();
  } else {
    Serial.println("Unknown command. Type 'test help' for available commands.");
  }
}

/*
 * Enter Test Mode
 */
void enterTestMode() {
  testModeActive = true;
  
  // Stop normal phone operations
  stopTone();
  changeState(IDLE);
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("         RETROBELL TEST MODE");
  Serial.println("========================================");
  Serial.println("Normal phone operations suspended.");
  Serial.println("Type 'test help' for available commands.");
  Serial.println("Type 'test exit' to return to normal mode.");
  Serial.println("========================================");
}

/*
 * Exit Test Mode
 */
void exitTestMode() {
  testModeActive = false;
  
  // Stop all tests
  stopAudioTest();
  stopMicrophoneTest();
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("Exiting test mode...");
  Serial.println("Normal phone operations resumed.");
  Serial.println("========================================");
  
  // Resume normal operation
  changeState(IDLE);
}

/*
 * Is Test Mode Active
 */
bool isTestModeActive() {
  return testModeActive;
}

// ====== Audio Test Functions ======

/*
 * Test Handset Audio (Right Channel)
 */
void testHandsetAudio() {
  Serial.println("Testing handset speaker (RIGHT channel)...");
  Serial.println("You should hear a 1kHz tone in the handset.");
  Serial.println("Type 'test audio stop' to stop the test.");
  
  currentAudioTest = AUDIO_TEST_HANDSET;
  audioTestStartTime = millis();
  stopTone(); // Stop any existing tones
}

/*
 * Test Ringer Audio (Left Channel)
 */
void testRingerAudio() {
  Serial.println("Testing base ringer (LEFT channel)...");
  Serial.println("You should hear a 1kHz tone in the base ringer.");
  Serial.println("Type 'test audio stop' to stop the test.");
  
  currentAudioTest = AUDIO_TEST_RINGER;
  audioTestStartTime = millis();
  stopTone(); // Stop any existing tones
}

/*
 * Test Both Audio Channels
 */
void testBothAudio() {
  Serial.println("Testing both speakers simultaneously...");
  Serial.println("You should hear a 1kHz tone in both handset AND base ringer.");
  Serial.println("Type 'test audio stop' to stop the test.");
  
  currentAudioTest = AUDIO_TEST_BOTH;
  audioTestStartTime = millis();
  stopTone(); // Stop any existing tones
}

/*
 * Stop Audio Test
 */
void stopAudioTest() {
  if (currentAudioTest != AUDIO_TEST_NONE) {
    Serial.println("Audio test stopped.");
    currentAudioTest = AUDIO_TEST_NONE;
    stopTone();
  } else {
    Serial.println("No audio test is currently running.");
  }
}

/*
 * Handle Audio Test (called from main test loop)
 */
void handleAudioTest() {
  if (currentAudioTest == AUDIO_TEST_NONE) return;
  
  // Generate test tone based on current test type
  static unsigned long lastToneUpdate = 0;
  if (millis() - lastToneUpdate > 50) { // Update every 50ms
    
    int16_t buffer[256];
    size_t samples = 256;
    
    // Generate 1kHz test tone for specific channels
    switch (currentAudioTest) {
      case AUDIO_TEST_HANDSET:
        generateTestTone(buffer, samples, 1000.0, false, true); // Right channel only
        break;
      case AUDIO_TEST_RINGER:
        generateTestTone(buffer, samples, 1000.0, true, false); // Left channel only
        break;
      case AUDIO_TEST_BOTH:
        generateTestTone(buffer, samples, 1000.0, true, true); // Both channels
        break;
      default:
        generateTestTone(buffer, samples, 1000.0, false, false); // Silence
        break;
    }
    
    // Write directly to I2S
    writeRawAudioBuffer(buffer, samples);
    lastToneUpdate = millis();
  }
  
  // Show test duration every 5 seconds
  static unsigned long lastDurationReport = 0;
  unsigned long testDuration = millis() - audioTestStartTime;
  if (testDuration > 5000 && millis() - lastDurationReport > 5000) {
    Serial.print("Audio test running for ");
    Serial.print(testDuration / 1000);
    Serial.println(" seconds... (type 'test audio stop' to stop)");
    lastDurationReport = millis();
  }
}

// ====== Microphone Test Functions ======

/*
 * Test Microphone Level
 */
void testMicrophoneLevel() {
  Serial.println("Testing microphone input levels...");
  Serial.println("Speak into the microphone. Press 'test mic stop' to stop.");
  Serial.println("Level display: [----||||++++] (- = low, | = medium, + = high)");
  
  micTestActive = true;
  micTestStartTime = millis();
}

/*
 * Test Microphone Record
 */
void testMicrophoneRecord() {
  Serial.println("Microphone record test...");
  Serial.println("Recording 1 second of audio in 3... 2... 1...");
  delay(3000);
  
  Serial.println("RECORDING NOW! Speak into the microphone...");
  
  micRecordActive = true;
  micRecordIndex = 0;
  micPlaybackActive = false;
  
  // Record for ~62ms (1000 samples at 16kHz)
  for (int i = 0; i < 1000; i++) {
    if (readMicrophoneBuffer(&micRecordBuffer[i], 1)) {
      micRecordIndex++;
    }
    delayMicroseconds(62); // ~16kHz sampling rate
  }
  
  Serial.println("Recording complete! Playing back...");
  
  // Start playback
  micRecordActive = false;
  micPlaybackActive = true;
  micPlaybackIndex = 0;
}

/*
 * Stop Microphone Test
 */
void stopMicrophoneTest() {
  if (micTestActive || micRecordActive || micPlaybackActive) {
    Serial.println("Microphone test stopped.");
    micTestActive = false;
    micRecordActive = false;
    micPlaybackActive = false;
  } else {
    Serial.println("No microphone test is currently running.");
  }
}

/*
 * Handle Microphone Test (called from main test loop)
 */
void handleMicrophoneTest() {
  static unsigned long lastMicUpdate = 0;
  
  if (micTestActive && millis() - lastMicUpdate > 100) { // Update every 100ms
    // Read microphone level
    int16_t sample;
    if (readMicrophoneBuffer(&sample, 1)) {
      // Convert to level display
      int level = abs(sample) / 2048; // Scale 0-32767 to 0-16
      if (level > 16) level = 16;
      
      // Create visual level meter
      Serial.print("Mic Level: [");
      for (int i = 0; i < 16; i++) {
        if (i < level) {
          if (i < 5) Serial.print("-");      // Low levels
          else if (i < 10) Serial.print("|"); // Medium levels
          else Serial.print("+");             // High levels
        } else {
          Serial.print(" ");
        }
      }
      Serial.print("] ");
      Serial.print(level * 100 / 16);
      Serial.println("%");
    }
    
    lastMicUpdate = millis();
  }
  
  // Handle microphone playback
  if (micPlaybackActive) {
    static unsigned long lastPlaybackUpdate = 0;
    if (millis() - lastPlaybackUpdate > 1) { // ~1kHz playback rate
      if (micPlaybackIndex < micRecordIndex) {
        // Play back recorded sample (this would need writeAudioBuffer function)
        writeAudioBuffer(&micRecordBuffer[micPlaybackIndex], 1);
        micPlaybackIndex++;
      } else {
        Serial.println("Playback complete!");
        micPlaybackActive = false;
      }
      lastPlaybackUpdate = millis();
    }
  }
}

// ====== Diagnostic Functions ======

/*
 * Test Pin States
 */
void testPinStates() {
  Serial.println();
  Serial.println("========== GPIO PIN STATES ==========");
  
  // Audio pins
  Serial.println("I2S Audio Pins:");
  Serial.print("  BCLK (GPIO ");
  Serial.print(I2S_BCLK_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S_BCLK_PIN) ? "HIGH" : "LOW");
  
  Serial.print("  LRCLK (GPIO ");
  Serial.print(I2S_LRCLK_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S_LRCLK_PIN) ? "HIGH" : "LOW");
  
  Serial.print("  DOUT (GPIO ");
  Serial.print(I2S_DOUT_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S_DOUT_PIN) ? "HIGH" : "LOW");
  
  // Amplifier control pins
  Serial.println("Amplifier Control:");
  Serial.print("  Handset SD (GPIO ");
  Serial.print(AMP_HANDSET_SD_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(AMP_HANDSET_SD_PIN) ? "ENABLED" : "DISABLED");
  
  Serial.print("  Ringer SD (GPIO ");
  Serial.print(AMP_RINGER_SD_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(AMP_RINGER_SD_PIN) ? "ENABLED" : "DISABLED");
  
  // Input pins
  Serial.println("Input Pins:");
  Serial.print("  Hook Switch (GPIO ");
  Serial.print(HOOK_SW_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(HOOK_SW_PIN) ? "OFF_HOOK" : "ON_HOOK");
  
  Serial.print("  Rotary Pulse (GPIO ");
  Serial.print(ROTARY_PULSE_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(ROTARY_PULSE_PIN) ? "HIGH" : "LOW");
  
  Serial.print("  Rotary Active (GPIO ");
  Serial.print(ROTARY_ACTIVE_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(ROTARY_ACTIVE_PIN) ? "IDLE" : "DIALING");
  
  // ADC pin (microphone)
  Serial.println("Analog Input:");
  Serial.print("  Microphone ADC (GPIO ");
  Serial.print(MIC_ADC_PIN);
  Serial.print("): ");
  int micValue = analogRead(MIC_ADC_PIN);
  Serial.print(micValue);
  Serial.print(" (");
  Serial.print((micValue * 3.3) / 4095.0, 2);
  Serial.println("V)");
  
  Serial.println("=====================================");
}

/*
 * Show Test Help
 */
void showTestHelp() {
  Serial.println();
  Serial.println("========== RETROBELL TEST COMMANDS ==========");
  Serial.println("General:");
  Serial.println("  test enter          - Enter test mode");
  Serial.println("  test exit           - Exit test mode");
  Serial.println("  test help           - Show this help");
  Serial.println();
  Serial.println("Audio Tests:");
  Serial.println("  test audio handset  - Test handset speaker (right channel)");
  Serial.println("  test audio ringer   - Test base ringer (left channel)");  
  Serial.println("  test audio both     - Test both speakers");
  Serial.println("  test audio stop     - Stop audio test");
  Serial.println();
  Serial.println("Microphone Tests:");
  Serial.println("  test mic level      - Monitor microphone input levels");
  Serial.println("  test mic record     - Record and playback test");
  Serial.println("  test mic stop       - Stop microphone test");
  Serial.println();
  Serial.println("Hardware Diagnostics:");
  Serial.println("  test pins           - Show all GPIO pin states");
  Serial.println("=============================================");
}