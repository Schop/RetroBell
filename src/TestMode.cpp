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
#include <driver/i2s.h>
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#define I2S_PORT I2S_NUM_0

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
  AUDIO_TEST_BOTH,
  AUDIO_TEST_DIAL_TONE
};
AudioTestType currentAudioTest = AUDIO_TEST_NONE;

// Test mode recorded audio data (shared with Audio.cpp)
int16_t* testRecordedBuffer = nullptr;
int testRecordedSamples = 0;
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
    
    // CRITICAL: Update audio system for recorded playback and other audio tests
    updateToneGeneration();
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
  } else if (command == "test dial tone") {
    testDialTone();
  } else if (command == "test mic level") {
    testMicrophoneLevel();
  } else if (command == "test wav") {
    testWAVPlayback();
  } else if (command == "test mp3") {
    testMP3Playback();
  } else if (command == "test mic record") {
    testMicrophoneRecord();
  } else if (command == "test mic tone") {
    testMicrophoneTone();
  } else if (command == "test mic stop") {
    stopMicrophoneTest();
  } else if (command == "test sine") {
    testSineWave();
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
  
  // Configure and enable handset amplifier
  pinMode(AMP_HANDSET_SD_PIN, OUTPUT);
  digitalWrite(AMP_HANDSET_SD_PIN, HIGH);
  Serial.println("Handset amplifier enabled.");
  
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
  
  // Configure and enable ringer amplifier
  pinMode(AMP_RINGER_SD_PIN, OUTPUT);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH);
  Serial.println("Ringer amplifier enabled.");
  
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
  
  // Configure and enable both amplifiers
  pinMode(AMP_HANDSET_SD_PIN, OUTPUT);
  pinMode(AMP_RINGER_SD_PIN, OUTPUT);
  digitalWrite(AMP_HANDSET_SD_PIN, HIGH);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH);
  Serial.println("Both amplifiers enabled.");
  
  currentAudioTest = AUDIO_TEST_BOTH;
  audioTestStartTime = millis();
  stopTone(); // Stop any existing tones
}

/*
 * Test Normal Dial Tone
 * Uses the built-in dial tone system but routed to ringer speaker
 */
void testDialTone() {
  Serial.println("Testing normal dial tone system...");
  Serial.println("Generating 350Hz dial tone through ringer speaker.");
  Serial.println("Type 'test audio stop' to stop the test.");
  
  // Enable ringer amplifier to hear the dial tone
  pinMode(AMP_RINGER_SD_PIN, OUTPUT);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH);
  Serial.println("Ringer amplifier enabled for dial tone test.");
  
  // Start a custom dial tone that goes to ringer (LEFT channel)
  currentAudioTest = AUDIO_TEST_DIAL_TONE; // Use custom dial tone test
  audioTestStartTime = millis();
  
  Serial.println("350Hz dial tone started on ringer speaker.");
}

/*
 * Stop Audio Test
 */
void stopAudioTest() {
  if (currentAudioTest != AUDIO_TEST_NONE) {
    Serial.println("Audio test stopped.");
    currentAudioTest = AUDIO_TEST_NONE;
    stopTone();
    
    // Disable amplifiers to save power and prevent noise
    digitalWrite(AMP_HANDSET_SD_PIN, LOW);
    digitalWrite(AMP_RINGER_SD_PIN, LOW);
    Serial.println("Amplifiers disabled.");
  } else {
    Serial.println("Audio test stopped (including any dial tone).");
    stopTone(); // Stop any normal system tones too
  }
}

/*
 * Handle Audio Test (called from main test loop)
 */
void handleAudioTest() {
  if (currentAudioTest == AUDIO_TEST_NONE) return;
  
  // Generate test tone based on current test type
  static unsigned long lastToneUpdate = 0;
  if (millis() - lastToneUpdate > 10) { // Update every 10ms for smoother audio
    
    int16_t buffer[512];  // Larger buffer for smoother audio
    size_t samples = 512;
    
    // Generate test tone for specific channels
    switch (currentAudioTest) {
      case AUDIO_TEST_HANDSET:
        generateTone(buffer, samples, 1000.0, false, true); // Right channel only - use working function
        break;
      case AUDIO_TEST_RINGER:
        generateTone(buffer, samples, 440.0, true, false); // Left channel only - 440Hz for mic tone test
        break;
      case AUDIO_TEST_BOTH:
        generateTone(buffer, samples, 1000.0, true, true); // Both channels - use working function
        break;
      case AUDIO_TEST_DIAL_TONE:
        generateTone(buffer, samples, 350.0, true, false); // 350Hz dial tone on left channel (ringer)
        break;
      default:
        generateTone(buffer, samples, 1000.0, false, false); // Silence - use working function
        break;
    }
    
    // Write directly to I2S using same method as working audio
    size_t bytes_written;
    i2s_write(I2S_PORT, buffer, samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);
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
  Serial.println("Microphone record test (recording only)...");
  Serial.println("Testing MAX9814 microphone input - no playback.");
  
  // Calculate dynamic bias from quiet samples with better filtering
  Serial.println("Measuring quiet baseline for 2 seconds...");
  int32_t biasSum = 0;
  int silentMin = 4095, silentMax = 0;
  int validSamples = 0;
  
  // Take more samples for better noise floor measurement
  for (int i = 0; i < 200; i++) {
    int adcValue = analogRead(4);
    
    // Only include samples within reasonable range (filter out spikes)
    if (adcValue > 500 && adcValue < 3500) {
      biasSum += adcValue;
      validSamples++;
      if (adcValue < silentMin) silentMin = adcValue;
      if (adcValue > silentMax) silentMax = adcValue;
    }
    
    delay(10); // 2 seconds total
  }
  
  int adcBias = validSamples > 0 ? biasSum / validSamples : 1500; // Fallback to mid-range
  int silentRange = silentMax - silentMin;
  
  Serial.print("Silent baseline: ");
  Serial.print(adcBias);
  Serial.print(" (from ");
  Serial.print(validSamples);
  Serial.print(" valid samples), range: ");
  Serial.println(silentRange);
  
  if (silentRange > 200) {
    Serial.println("WARNING: High noise level detected - MAX9814 gain may be too high");
    Serial.println("ADVICE: Try reducing microphone gain or improve shielding");
    Serial.println("HARDWARE NOTE: MAX9814 gain can be adjusted by connecting GAIN pin to:");
    Serial.println("  - VCC = 60dB gain (maximum, very sensitive)");
    Serial.println("  - VCC via 100k = 50dB gain (medium)");
    Serial.println("  - GND = 40dB gain (minimum, less sensitive)");
  } else {
    Serial.println("Good: Low noise baseline");
  }
  
  // Show some actual ADC values to diagnose noise source
  Serial.println("Sample of raw ADC values (0-4095):");
  for (int i = 0; i < 10; i++) {
    int rawADC = analogRead(4);
    Serial.print(rawADC);
    Serial.print(" ");
    delay(50);
  }
  Serial.println();
  
  Serial.println();
  Serial.println("Recording for 2 seconds - speak now!");
  
  micRecordActive = true;
  micRecordIndex = 0;
  
  // Record at proper rate - aim for 16kHz but with realistic timing
  // Target: 1000 samples over 2000ms = 1 sample every 2ms = 500Hz
  // This matches our buffer size limitation better
  unsigned long startTime = millis();
  unsigned long nextSampleTime = startTime;
  int voiceMin = 4095, voiceMax = 0;
  
  while (millis() - startTime < 2000 && micRecordIndex < 1000) {
    // Only sample when it's time for the next sample
    if (millis() >= nextSampleTime) {
      int adcValue = analogRead(4);
      
      // Track voice range
      if (adcValue < voiceMin) voiceMin = adcValue;
      if (adcValue > voiceMax) voiceMax = adcValue;
      
      // Balanced conversion - reduce noise but keep signal audible
      int16_t audioSample = (int16_t)((adcValue - adcBias) * 3); // Moderate gain
      
      // Moderate noise gate - filter noise but allow voice through
      if (abs(audioSample) < 300) {
        audioSample = 0; // Silence quiet noise
      }
      // Keep valid signals at full strength
      
      micRecordBuffer[micRecordIndex] = audioSample;
      micRecordIndex++;
      
      // Schedule next sample (2ms later for 500Hz rate)
      nextSampleTime += 2;
    }
    
    // Small delay to prevent busy-waiting
    delayMicroseconds(100);
  }
  
  unsigned long actualTime = millis() - startTime;
  int voiceRange = voiceMax - voiceMin;
  
  Serial.println();
  Serial.print("Recording complete: ");
  Serial.print(actualTime);
  Serial.print("ms, ");
  Serial.print(micRecordIndex);
  Serial.println(" samples");
  
  Serial.print("Voice range: ");
  Serial.print(voiceMin);
  Serial.print(" to ");
  Serial.print(voiceMax);
  Serial.print(" (");
  Serial.print(voiceRange);
  Serial.println(" total)");
  
  // Analyze recording quality
  if (voiceRange > 500) {
    Serial.println("GOOD: Strong voice signal detected!");
  } else if (voiceRange > 100) {
    Serial.println("OK: Some voice signal detected");
  } else {
    Serial.println("WEAK: Very little voice signal - check microphone connection");
  }
  
  // Show sample statistics
  if (micRecordIndex > 0) {
    int16_t minSample = 32767, maxSample = -32768;
    int32_t avgSample = 0;
    
    for (int i = 0; i < micRecordIndex; i++) {
      if (micRecordBuffer[i] < minSample) minSample = micRecordBuffer[i];
      if (micRecordBuffer[i] > maxSample) maxSample = micRecordBuffer[i];
      avgSample += micRecordBuffer[i];
    }
    avgSample /= micRecordIndex;
    
    Serial.print("Processed samples - Min: ");
    Serial.print(minSample);
    Serial.print(", Max: ");
    Serial.print(maxSample);
    Serial.print(", Avg: ");
    Serial.println(avgSample);
  }
  
  Serial.println();
  Serial.println("Microphone recording test complete.");
  
  // Set up the recorded audio for the proper audio system
  testRecordedBuffer = micRecordBuffer;
  testRecordedSamples = micRecordIndex;
  
  Serial.println();
  Serial.print("DEBUG: Set testRecordedBuffer = ");
  Serial.print((unsigned long)testRecordedBuffer, HEX);
  Serial.print(", testRecordedSamples = ");
  Serial.println(testRecordedSamples);
  
  Serial.println();
  Serial.println("=== RECORDED AUDIO PLAYBACK TEST ===");
  Serial.println("Now testing playback using the WORKING audio system...");
  Serial.print("Playing ");
  Serial.print(micRecordIndex);
  Serial.println(" recorded samples through proper audio pipeline.");
  
  // Enable ringer amplifier for playback
  pinMode(AMP_RINGER_SD_PIN, OUTPUT);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH);
  Serial.println("Ringer amplifier enabled for playback.");
  
  // Use the WORKING audio system - same as dial tone!
  playTestRecordedAudio();
  
  Serial.println("Playing recorded audio using SAME system as working dial tone!");
  Serial.println("Audio will play once and automatically stop.");
  Serial.println("This tests the complete microphone → recording → playback chain.");
}

/*
 * Test Microphone with Synthetic Tone
 * Generate a known good audio pattern using the working audio path
 */
void testMicrophoneTone() {
  Serial.println("Microphone tone test...");
  Serial.println("Generating synthetic 440Hz tone using working audio path...");
  Serial.println("Type 'test audio stop' to stop the test.");
  
  // Enable ringer amplifier
  pinMode(AMP_RINGER_SD_PIN, OUTPUT);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH);
  Serial.println("Ringer amplifier enabled for tone test.");
  
  // Use the WORKING audio test mechanism with 440Hz
  currentAudioTest = AUDIO_TEST_RINGER; // Use working ringer test path
  audioTestStartTime = millis();
  
  Serial.println("440Hz synthetic tone started using working audio path.");
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
    
    // Disable amplifiers if they were enabled for playback
    if (micPlaybackActive) {
      digitalWrite(AMP_HANDSET_SD_PIN, LOW);
      digitalWrite(AMP_RINGER_SD_PIN, LOW);
      Serial.println("Amplifiers disabled.");
    }
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
    if (millis() - lastPlaybackUpdate > 31) { // 31ms = ~32Hz for slower, clearer playback
      
      // Play back multiple samples at once for efficiency
      int16_t stereoBuffer[32]; // 16 stereo pairs
      int samplesToPlay = 16;
      
      if (micPlaybackIndex + samplesToPlay > micRecordIndex) {
        samplesToPlay = micRecordIndex - micPlaybackIndex;
      }
      
      if (samplesToPlay > 0) {
        // Convert mono recording to stereo for playback with amplification
        for (int i = 0; i < samplesToPlay; i++) {
          // Amplify the signal by 4x for better audibility
          int32_t amplifiedSample = (int32_t)micRecordBuffer[micPlaybackIndex + i] * 4;
          if (amplifiedSample > 32767) amplifiedSample = 32767;
          if (amplifiedSample < -32768) amplifiedSample = -32768;
          
          int16_t finalSample = (int16_t)amplifiedSample;
          stereoBuffer[i * 2] = finalSample;     // Left channel
          stereoBuffer[i * 2 + 1] = finalSample; // Right channel (same as left)
        }
        
        // Play back recorded samples using same method as working audio
        size_t bytes_written;
        i2s_write(I2S_PORT, stereoBuffer, samplesToPlay * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
        micPlaybackIndex += samplesToPlay;
        
        // Show playback progress
        if (micPlaybackIndex % 100 == 0) {
          Serial.print("Playback: ");
          Serial.print(micPlaybackIndex);
          Serial.print("/");
          Serial.println(micRecordIndex);
        }
      } else {
        Serial.println("Playback complete!");
        micPlaybackActive = false;
        
        // Disable amplifiers after playback
        digitalWrite(AMP_HANDSET_SD_PIN, LOW);
        digitalWrite(AMP_RINGER_SD_PIN, LOW);
        Serial.println("Amplifiers disabled after playback.");
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
  
  // Audio pins - Dual I2S System
  Serial.println("I2S0 Audio Pins (Handset):");
  Serial.print("  BCLK (GPIO ");
  Serial.print(I2S0_BCLK_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S0_BCLK_PIN) ? "HIGH" : "LOW");
  
  Serial.print("  LRCLK (GPIO ");
  Serial.print(I2S0_LRCLK_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S0_LRCLK_PIN) ? "HIGH" : "LOW");
  
  Serial.print("  DOUT (GPIO ");
  Serial.print(I2S0_DOUT_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S0_DOUT_PIN) ? "HIGH" : "LOW");
  
  Serial.print("  DIN (GPIO ");
  Serial.print(I2S0_DIN_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S0_DIN_PIN) ? "HIGH" : "LOW");

  Serial.println("I2S1 Audio Pins (Ringer):");
  Serial.print("  BCLK (GPIO ");
  Serial.print(I2S1_BCLK_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S1_BCLK_PIN) ? "HIGH" : "LOW");
  
  Serial.print("  LRCLK (GPIO ");
  Serial.print(I2S1_LRCLK_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S1_LRCLK_PIN) ? "HIGH" : "LOW");
  
  Serial.print("  DOUT (GPIO ");
  Serial.print(I2S1_DOUT_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(I2S1_DOUT_PIN) ? "HIGH" : "LOW");
  
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
  
  // ICS-43434 Digital Microphone
  Serial.println("Digital Microphone:");
  Serial.print("  ICS-43434 Data (GPIO ");
  Serial.print(MIC_SD_PIN);
  Serial.print("): ");
  Serial.println(digitalRead(MIC_SD_PIN) ? "HIGH" : "LOW");
  Serial.print("  Shares I2S0 Clock (GPIO ");
  Serial.print(MIC_SCK_PIN);
  Serial.print(", ");
  Serial.print(MIC_WS_PIN);
  Serial.println(")");
  Serial.println("  Digital MEMS microphone - no ADC needed!");
  
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
  Serial.println("  test dial tone      - Test normal dial tone system");
  Serial.println("  test audio stop     - Stop audio test");
  Serial.println();
  Serial.println("Microphone Tests:");
  Serial.println("  test mic level      - Monitor microphone input levels");
  Serial.println("  test mic record     - Record and playback test (FULL TEST)");
  Serial.println("  test mic tone       - Test playback with synthetic tone");
  Serial.println("  test wav            - Test WAV-like audio playback");
  Serial.println("  test mp3            - Test MP3-like audio playback");
  Serial.println("  test mic stop       - Stop microphone test");
  Serial.println();
  Serial.println("Debug Tests:");
  Serial.println("  test sine           - Generate pure 440Hz sine wave");
  Serial.println();
  Serial.println("Hardware Diagnostics:");
  Serial.println("  test pins           - Show all GPIO pin states");
  Serial.println("=============================================");
}

/*
 * Test WAV File Playback
 * Tests playing back pre-recorded audio data through the audio system
 */
void testWAVPlayback() {
  Serial.println("WAV playback test...");
  Serial.println("Playing synthetic audio pattern through audio system.");
  
  // Create a synthetic audio pattern (simulates WAV file data)
  const int PATTERN_SIZE = 800;
  static int16_t testPattern[PATTERN_SIZE];
  
  // Generate a test pattern: 440Hz sine wave with amplitude modulation
  for (int i = 0; i < PATTERN_SIZE; i++) {
    float t = (float)i / 500.0f; // 500Hz sample rate simulation
    float sine440 = sin(2.0 * PI * 440.0 * t);
    float envelope = sin(2.0 * PI * 2.0 * t); // 2Hz amplitude modulation
    testPattern[i] = (int16_t)(sine440 * envelope * 4000);
  }
  
  Serial.print("Generated test pattern: ");
  Serial.print(PATTERN_SIZE);
  Serial.println(" samples");
  
  // Set up the test pattern for the audio system
  testRecordedBuffer = testPattern;
  testRecordedSamples = PATTERN_SIZE;
  
  Serial.println("Pattern loaded into audio system.");
  
  // Enable ringer amplifier for playback
  pinMode(AMP_RINGER_SD_PIN, OUTPUT);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH);
  Serial.println("Ringer amplifier enabled for WAV test.");
  
  // Use the working audio system to play the pattern
  playTestRecordedAudio();
  
  Serial.println("Playing synthetic WAV pattern...");
  Serial.println("You should hear a 440Hz tone with amplitude modulation (warbling).");
  Serial.println("This tests the same audio path as WAV file playback would use.");
}

/*
 * Test MP3 File Playback
 * Tests playing back MP3 files from LittleFS through the audio system
 */
void testMP3Playback() {
  Serial.println("MP3 playback test...");
  Serial.println("Looking for test MP3 files in filesystem...");
  
  // Enable ringer amplifier for playback
  pinMode(AMP_RINGER_SD_PIN, OUTPUT);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH);
  Serial.println("Ringer amplifier enabled for MP3 test.");
  
  // For now, let's create a simple tone-based test that simulates MP3 playback
  // In a real implementation, you would:
  // 1. Create AudioFileSourceLittleFS source("/test.mp3");
  // 2. Create AudioGeneratorMP3 mp3;
  // 3. Create AudioOutputI2S out;
  // 4. mp3.begin(&source, &out);
  // 5. Loop mp3.loop() until !mp3.isRunning();
  
  Serial.println("NOTE: For real MP3 playback, you would need:");
  Serial.println("1. An MP3 file uploaded to LittleFS (/test.mp3)");
  Serial.println("2. ESP8266Audio library properly configured");
  Serial.println("3. Sufficient heap memory for MP3 decoding");
  
  Serial.println();
  Serial.println("For now, playing MP3-like test pattern...");
  
  // Create a more complex test pattern (simulates compressed audio)
  const int PATTERN_SIZE = 1000;
  static int16_t mp3Pattern[PATTERN_SIZE];
  
  // Generate a more complex pattern: multiple frequencies mixed
  for (int i = 0; i < PATTERN_SIZE; i++) {
    float t = (float)i / 500.0f; // 500Hz sample rate simulation
    
    // Mix multiple frequencies (simulates MP3 complexity)
    float sine440 = sin(2.0 * PI * 440.0 * t) * 0.4;     // Base tone
    float sine880 = sin(2.0 * PI * 880.0 * t) * 0.3;     // Octave
    float sine220 = sin(2.0 * PI * 220.0 * t) * 0.3;     // Sub-octave
    
    float envelope = 1.0 + sin(2.0 * PI * 1.0 * t) * 0.5; // 1Hz amplitude modulation
    float mixed = (sine440 + sine880 + sine220) * envelope;
    
    mp3Pattern[i] = (int16_t)(mixed * 3000);
  }
  
  Serial.print("Generated MP3-like test pattern: ");
  Serial.print(PATTERN_SIZE);
  Serial.println(" samples");
  
  // Set up the test pattern for the audio system
  testRecordedBuffer = mp3Pattern;
  testRecordedSamples = PATTERN_SIZE;
  
  Serial.println("Pattern loaded into audio system.");
  
  // Use the working audio system to play the pattern
  playTestRecordedAudio();
  
  Serial.println("Playing MP3-like test pattern...");
  Serial.println("You should hear a rich harmonic sound with slow amplitude modulation.");
  Serial.println("This demonstrates the audio path that MP3 files would use.");
}

/*
 * Test Sine Wave Generation
 * Use the SAME audio system path as working dial tone
 */
void testSineWave() {
  Serial.println("Generating 440Hz sine wave for 3 seconds...");
  Serial.println("Using the EXACT SAME audio system as working dial tone.");
  
  // Enable ringer amplifier 
  pinMode(AMP_RINGER_SD_PIN, OUTPUT);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH);
  Serial.println("Ringer amplifier enabled for sine wave test.");
  
  // Use the SAME audio test system as working dial tone
  // This will go through the normal audio pipeline
  currentAudioTest = AUDIO_TEST_RINGER;  // This generates tones through working system
  audioTestStartTime = millis();
  
  Serial.println("440Hz test tone started using WORKING audio system.");
  Serial.println("This should sound identical to the clear dial tone!");
  Serial.println("Type 'test audio stop' to stop the test.");
}