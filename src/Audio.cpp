/*
 * Audio System - Dual I2S Output & I2S Microphone Input
 * 
 * Manages dual MAX98357A amplifiers and ICS-43434 microphone:
 * - I2S0: Handset amplifier (mono) + ICS-43434 microphone input (full-duplex)
 * - I2S1: Base ringer amplifier (mono output only)
 * - Sample rate: 16kHz, 16-bit mono per channel
 * - Generates dial tone, ringback tone, and ring tone
 * - Uses sine wave generation for pure tones
 * - Digital microphone input via I2S for crystal-clear voice transmission
 */

#include "Audio.h"
#include "Pins.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

// Audio settings
#define I2S_HANDSET_PORT I2S_NUM_0  // Handset + microphone (full-duplex)
#define I2S_RINGER_PORT I2S_NUM_1   // Base ringer (TX only)
#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define BUFFER_SIZE 256

// Tone generation state
enum ToneType {
  TONE_NONE,
  TONE_DIAL,
  TONE_RINGBACK,
  TONE_RING,
  TONE_ERROR,  // Fast busy tone for errors (250ms cadence)
  TONE_BUSY,   // Normal busy tone (500ms cadence)
  TONE_TEST_RECORDED  // Test mode recorded audio playback
};

ToneType currentTone = TONE_NONE;
unsigned long toneStartTime = 0;
unsigned long lastCadenceTime = 0;
bool cadenceOn = false;

// Test mode recorded audio playback
extern int16_t* testRecordedBuffer;
extern int testRecordedSamples;
static int recordedPlaybackIndex = 0;

// Audio system status
static bool handsetAudioReady = false;
static bool ringerAudioReady = false;
static bool microphoneReady = false;

/*
 * Generate Sine Wave Tone
 * 
 * Creates a pure sine wave and writes it to a mono buffer.
 * 
 * Parameters:
 * - buffer: Array to fill with audio samples (mono)
 * - samples: Number of samples to generate
 * - frequency: Tone frequency in Hz (e.g., 350, 440, 480)
 * - handsetChannel: true = output to handset amplifier (I2S0)
 * - ringerChannel: true = output to ringer amplifier (I2S1)
 * 
 * Sine Wave Formula:
 * sample = sin(phase) * amplitude
 * phase += (2π * frequency) / sampleRate
 */
void generateTone(int16_t* buffer, size_t samples, float frequency, bool handsetChannel, bool ringerChannel) {
  static float phase = 0.0;
  float phaseIncrement = (2.0 * PI * frequency) / SAMPLE_RATE;
  
  for (size_t i = 0; i < samples; i++) {
    int16_t sample = (int16_t)(sin(phase) * 8000); // Amplitude of 8000 (adjust for volume)
    phase += phaseIncrement;
    if (phase >= 2.0 * PI) {
      phase -= 2.0 * PI;
    }
    
    buffer[i] = sample;
  }
  
  // Output to requested channels
  if (handsetChannel && handsetAudioReady) {
    writeHandsetAudioBuffer(buffer, samples);
  }
  if (ringerChannel && ringerAudioReady) {
    writeRingerAudioBuffer(buffer, samples);
  }
}

/*
 * Setup Audio System
 * 
 * Initializes dual I2S audio system with ICS-43434 microphone.
 * This new architecture eliminates the fragile channel latching by using
 * separate I2S buses for each amplifier.
 * 
 * Architecture:
 * - I2S0: Full-duplex for handset amp (TX) + ICS-43434 mic (RX)
 * - I2S1: TX-only for base ringer amp
 * 
 * Benefits:
 * - No channel confusion - each amp has dedicated control
 * - Digital microphone - studio-quality audio, no noise
 * - Independent sample rates possible
 * - Much more reliable than channel latching
 */
void setupAudio() {
  Serial.println("Configuring Dual I2S Audio System...");

  // Initialize amplifier control pins
  pinMode(AMP_HANDSET_SD_PIN, OUTPUT);
  pinMode(AMP_RINGER_SD_PIN, OUTPUT);
  
  // Start with both amps disabled
  digitalWrite(AMP_HANDSET_SD_PIN, LOW);
  digitalWrite(AMP_RINGER_SD_PIN, LOW);
  
  // Setup individual audio subsystems
  setupHandsetAudio();
  setupRingerAudio(); 
  setupMicrophone();
  
  // Enable amplifiers after I2S is configured
  digitalWrite(AMP_HANDSET_SD_PIN, HIGH);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH);
  
  Serial.println("Dual I2S Audio System Ready!");
  Serial.println("  - Handset: I2S0 (GPIO 8,9,10)");
  Serial.println("  - Ringer: I2S1 (GPIO 12,13,14)");
  Serial.println("  - Microphone: ICS-43434 on I2S0 RX (GPIO 6)");
}

/*
 * Setup Handset Audio (I2S0 TX)
 * 
 * Configures I2S0 for handset amplifier output.
 * This will also be used for microphone input (full-duplex).
 */
void setupHandsetAudio() {
  // Configure I2S0 for full-duplex (TX + RX)
  i2s_config_t handset_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX), // Full-duplex
      .sample_rate = SAMPLE_RATE,                           // 16kHz
      .bits_per_sample = BITS_PER_SAMPLE,                  // 16-bit
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,         // Mono (left channel)
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // Standard I2S
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,            // Interrupt priority
      .dma_buf_count = 8,                                   // Number of DMA buffers
      .dma_buf_len = 64,                                    // Samples per buffer
      .use_apll = false,                                   // Don't use Audio PLL
      .tx_desc_auto_clear = true,                          // Auto-clear descriptors
      .fixed_mclk = 0                                      // No fixed MCLK
  };

  // Define I2S0 pin configuration
  i2s_pin_config_t handset_pin_config = {
      .bck_io_num = I2S0_BCLK_PIN,      // Bit clock (GPIO 8)
      .ws_io_num = I2S0_LRCLK_PIN,      // Word select (GPIO 9) 
      .data_out_num = I2S0_DOUT_PIN,    // Data output to handset amp (GPIO 10)
      .data_in_num = I2S0_DIN_PIN       // Data input from ICS-43434 mic (GPIO 6)
  };

  // Install and configure I2S0 driver
  esp_err_t result = i2s_driver_install(I2S_HANDSET_PORT, &handset_config, 0, NULL);
  if (result == ESP_OK) {
    i2s_set_pin(I2S_HANDSET_PORT, &handset_pin_config);
    i2s_zero_dma_buffer(I2S_HANDSET_PORT);
    handsetAudioReady = true;
    Serial.println("I2S0 Handset Audio: Ready");
  } else {
    Serial.printf("I2S0 Handset Audio: Failed (error %d)\n", result);
  }
}

/*
 * Setup Ringer Audio (I2S1 TX)
 * 
 * Configures I2S1 for base ringer amplifier output only.
 */
void setupRingerAudio() {
  // Configure I2S1 for TX only
  i2s_config_t ringer_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // TX only
      .sample_rate = SAMPLE_RATE,                           // 16kHz (could be different)
      .bits_per_sample = BITS_PER_SAMPLE,                  // 16-bit
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,         // Mono (left channel)
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // Standard I2S
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,            // Interrupt priority
      .dma_buf_count = 8,                                   // Number of DMA buffers
      .dma_buf_len = 64,                                    // Samples per buffer
      .use_apll = false,                                   // Don't use Audio PLL
      .tx_desc_auto_clear = true,                          // Auto-clear descriptors
      .fixed_mclk = 0                                      // No fixed MCLK
  };

  // Define I2S1 pin configuration
  i2s_pin_config_t ringer_pin_config = {
      .bck_io_num = I2S1_BCLK_PIN,      // Bit clock (GPIO 12)
      .ws_io_num = I2S1_LRCLK_PIN,      // Word select (GPIO 13)
      .data_out_num = I2S1_DOUT_PIN,    // Data output to ringer amp (GPIO 14)
      .data_in_num = I2S_PIN_NO_CHANGE  // No input on I2S1
  };

  // Install and configure I2S1 driver
  esp_err_t result = i2s_driver_install(I2S_RINGER_PORT, &ringer_config, 0, NULL);
  if (result == ESP_OK) {
    i2s_set_pin(I2S_RINGER_PORT, &ringer_pin_config);
    i2s_zero_dma_buffer(I2S_RINGER_PORT);
    ringerAudioReady = true;
    Serial.println("I2S1 Ringer Audio: Ready");
  } else {
    Serial.printf("I2S1 Ringer Audio: Failed (error %d)\n", result);
  }
}

/*
 * Setup Microphone Input (ICS-43434)
 * 
 * Configures the ICS-43434 digital MEMS microphone for I2S input.
 * The microphone shares clock signals with the handset amplifier (I2S0)
 * but uses a separate data input pin.
 * 
 * ICS-43434 Features:
 * - Digital I2S output - no analog noise
 * - 16-bit, up to 48kHz sample rate
 * - -26 dBFS sensitivity, 65 dB SNR
 * - Built-in decimation filter
 * 
 * Audio Pipeline:
 * Sound → ICS-43434 MEMS → I2S digital → ESP32 → ESP-NOW → Peer ESP32 → I2S → Speaker
 */
void setupMicrophone() {
  // ICS-43434 microphone configuration is handled by setupHandsetAudio()
  // since it shares the I2S0 bus for full-duplex operation
  
  if (handsetAudioReady) {
    microphoneReady = true;
    Serial.println("ICS-43434 Microphone: Ready (I2S0 RX)");
    Serial.println("  - Digital MEMS microphone on GPIO 6");
    Serial.println("  - Shares clock with handset amplifier");
    Serial.println("  - 16-bit, 16kHz, crystal-clear audio");
  } else {
    Serial.println("ICS-43434 Microphone: Failed (I2S0 not ready)");
  }
}

/*
 * Read Microphone Buffer (ICS-43434 I2S)
 * 
 * Reads digital audio samples from the ICS-43434 microphone via I2S.
 * The microphone provides clean 16-bit digital samples with no need for 
 * ADC conversion, DC offset removal, or analog noise filtering.
 * 
 * Parameters:
 * - buffer: Array to fill with audio samples
 * - samples: Number of samples to read
 * 
 * Returns: true if successful, false if error
 * 
 * Note: Non-blocking read with timeout. For 100 samples at 16kHz, this takes ~6ms.
 */
bool readMicrophoneBuffer(int16_t* buffer, size_t samples) {
  if (!buffer || !microphoneReady) return false;
  
  size_t bytes_read = 0;
  size_t bytes_to_read = samples * sizeof(int16_t);
  
  // Read digital audio samples directly from ICS-43434 via I2S0 RX
  esp_err_t result = i2s_read(I2S_HANDSET_PORT, buffer, bytes_to_read, &bytes_read, pdMS_TO_TICKS(100));
  
  if (result == ESP_OK && bytes_read == bytes_to_read) {
    // ICS-43434 provides clean digital audio - no processing needed!
    // The microphone has built-in:
    // - Automatic gain control
    // - Digital filtering 
    // - Noise reduction
    // - DC offset removal
    return true;
  } else {
    // Fill buffer with silence on read error
    memset(buffer, 0, bytes_to_read);
    return false;
  }
}

/*
 * Write Audio Buffer (Handset Output)
 * 
 * Plays received audio samples through the handset amplifier (I2S0).
 * Used for incoming call audio - voice from the remote phone.
 * 
 * Parameters:
 * - buffer: Array of audio samples to play
 * - samples: Number of samples in buffer
 * 
 * This is called when MSG_AUDIO_DATA packets are received from the peer phone.
 */
void writeAudioBuffer(const int16_t* buffer, size_t samples) {
  writeHandsetAudioBuffer(buffer, samples);
}

/*
 * Write Handset Audio Buffer (I2S0 TX)
 * 
 * Writes mono audio samples directly to the handset amplifier.
 */
void writeHandsetAudioBuffer(const int16_t* buffer, size_t samples) {
  if (!buffer || !handsetAudioReady) return;
  
  size_t bytes_written;
  i2s_write(I2S_HANDSET_PORT, buffer, samples * sizeof(int16_t), &bytes_written, pdMS_TO_TICKS(100));
}

/*
 * Write Ringer Audio Buffer (I2S1 TX)
 * 
 * Writes mono audio samples directly to the base ringer amplifier.
 */
void writeRingerAudioBuffer(const int16_t* buffer, size_t samples) {
  if (!buffer || !ringerAudioReady) return;
  
  size_t bytes_written;
  i2s_write(I2S_RINGER_PORT, buffer, samples * sizeof(int16_t), &bytes_written, pdMS_TO_TICKS(100));
}

/*
 * Play Dial Tone
 * Continuous 350Hz tone on handset amplifier (I2S0)
 * Standard North American dial tone
 */
void playDialTone() {
  if (currentTone != TONE_DIAL) {
    currentTone = TONE_DIAL;
    toneStartTime = millis();
    Serial.println("Playing dial tone (350Hz on handset)");
  }
}

/*
 * Play Ringback Tone
 * What you hear when calling someone - indicates their phone is ringing
 * Pattern: 440Hz, 2 seconds ON, 4 seconds OFF
 * Output: RIGHT channel (handset)
 */
void playRingbackTone() {
  if (currentTone != TONE_RINGBACK) {
    currentTone = TONE_RINGBACK;
    toneStartTime = millis();
    lastCadenceTime = millis();
    cadenceOn = true;
    Serial.println("Playing ringback tone");
  }
}

/*
 * Play Test Recorded Audio
 * Plays back recorded microphone audio from test mode
 */
void playTestRecordedAudio() {
  if (currentTone != TONE_TEST_RECORDED) {
    currentTone = TONE_TEST_RECORDED;
    toneStartTime = millis();
    recordedPlaybackIndex = 0;
    Serial.print("Playing recorded audio - samples: ");
    Serial.print(testRecordedSamples);
    Serial.print(", buffer: 0x");
    Serial.println((unsigned long)testRecordedBuffer, HEX);
  }
}

/*
 * Play Ring Tone
 * Sound when receiving an incoming call
 * Pattern: 440Hz, 2 seconds ON, 4 seconds OFF
 * Output: Base ringer amplifier (I2S1)
 */
void playRingTone() {
  if (currentTone != TONE_RING) {
    currentTone = TONE_RING;
    toneStartTime = millis();
    lastCadenceTime = millis();
    cadenceOn = true;
    Serial.println("Playing ring tone (440Hz on base ringer)");
  }
}

/*
 * Play Error Tone
 * Fast busy signal for invalid number / call failed
 * Pattern: 480Hz + 620Hz dual tone, 250ms ON, 250ms OFF (fast busy)
 * Output: Handset amplifier (I2S0)
 * 
 * This is the North American "reorder tone" or "fast busy signal"
 * indicating the call cannot be completed (number not found, network error, etc.)
 */
void playErrorTone() {
  if (currentTone != TONE_ERROR) {
    currentTone = TONE_ERROR;
    toneStartTime = millis();
    lastCadenceTime = millis();
    cadenceOn = true;
    Serial.println("Playing error tone (fast busy on handset)");
  }
}

/*
 * Play Busy Tone
 * Normal busy signal for when called party is already in a call
 * Pattern: 480Hz + 620Hz dual tone, 500ms ON, 500ms OFF (normal busy)
 * Output: RIGHT channel (handset)
 * 
 * This is the standard "busy signal" indicating the called phone
 * is currently in use (off-hook or already in another call).
 */
void playBusyTone() {
  if (currentTone != TONE_BUSY) {
    currentTone = TONE_BUSY;
    toneStartTime = millis();
    lastCadenceTime = millis();
    cadenceOn = true;
    Serial.println("Playing busy tone");
  }
}

/*
 * Stop All Tones
 * Clears audio buffers and stops tone generation
 */
void stopTone() {
  if (currentTone != TONE_NONE) {
    currentTone = TONE_NONE;
    // Clear both I2S buffers to stop audio
    if (handsetAudioReady) i2s_zero_dma_buffer(I2S_HANDSET_PORT);
    if (ringerAudioReady) i2s_zero_dma_buffer(I2S_RINGER_PORT);
    Serial.println("All tones stopped");
  }
}

/*
 * Update Tone Generation
 * 
 * Called continuously from main loop to generate audio output.
 * Handles:
 * - TONE_DIAL: Continuous tone on handset (I2S0)
 * - TONE_RINGBACK: Cadenced tone on handset (I2S0) - 2s on, 4s off
 * - TONE_RING: Cadenced tone on ringer (I2S1) - 2s on, 4s off  
 * - TONE_ERROR/BUSY: Fast/normal busy on handset (I2S0)
 * - TONE_NONE: Silence
 * 
 * The cadence pattern matches standard telephone ring patterns.
 */
void updateToneGeneration() {
  if (currentTone == TONE_NONE) {
    return;
  }
  
  int16_t buffer[BUFFER_SIZE];
  unsigned long currentTime = millis();
  
  switch (currentTone) {
    case TONE_DIAL:
      // Continuous 350Hz dial tone on handset amplifier (I2S0)
      generateTone(buffer, BUFFER_SIZE, 350.0, true, false); // handset=true, ringer=false
      break;
      
    case TONE_RINGBACK:
      // Ringback: 440Hz, 2 seconds on, 4 seconds off, on handset amplifier (I2S0)
      // Manages cadence timing
      if (cadenceOn && (currentTime - lastCadenceTime > 2000)) {
        // Been on for 2 seconds, switch to off
        cadenceOn = false;
        lastCadenceTime = currentTime;
      } else if (!cadenceOn && (currentTime - lastCadenceTime > 4000)) {
        // Been off for 4 seconds, switch to on
        cadenceOn = true;
        lastCadenceTime = currentTime;
      }
      
      if (cadenceOn) {
        generateTone(buffer, BUFFER_SIZE, 440.0, true, false); // handset only
      } else {
        // Silence period - no output needed
      }
      break;
      
    case TONE_RING:
      // Ring tone: 440Hz, 2 seconds on, 4 seconds off, on base ringer (I2S1)
      // Manages cadence timing
      if (cadenceOn && (currentTime - lastCadenceTime > 2000)) {
        // Been on for 2 seconds, switch to off
        cadenceOn = false;
        lastCadenceTime = currentTime;
      } else if (!cadenceOn && (currentTime - lastCadenceTime > 4000)) {
        // Been off for 4 seconds, switch to on
        cadenceOn = true;
        lastCadenceTime = currentTime;
      }
      
      if (cadenceOn) {
        generateTone(buffer, BUFFER_SIZE, 440.0, false, true); // ringer only
      } else {
        // Silence period - no output needed
      }
      break;
      
    case TONE_ERROR:
      // Error/Fast Busy: 480Hz, 250ms on, 250ms off, on handset amplifier (I2S0)
      // Fast cadence indicates call failed / number not found
      if (cadenceOn && (currentTime - lastCadenceTime > 250)) {
        // Been on for 250ms, switch to off
        cadenceOn = false;
        lastCadenceTime = currentTime;
      } else if (!cadenceOn && (currentTime - lastCadenceTime > 250)) {
        // Been off for 250ms, switch to on
        cadenceOn = true;
        lastCadenceTime = currentTime;
      }
      
      if (cadenceOn) {
        generateTone(buffer, BUFFER_SIZE, 480.0, true, false); // handset only
      } else {
        // Silence period - no output needed
      }
      break;
      
    case TONE_BUSY:
      // Normal Busy: 480Hz + 620Hz, 500ms on, 500ms off, on handset amplifier (I2S0)
      // Indicates called party is already in use
      if (cadenceOn && (currentTime - lastCadenceTime > 500)) {
        // Been on for 500ms, switch to off
        cadenceOn = false;
        lastCadenceTime = currentTime;
      } else if (!cadenceOn && (currentTime - lastCadenceTime > 500)) {
        // Been off for 500ms, switch to on
        cadenceOn = true;
        lastCadenceTime = currentTime;
      }
      
      if (cadenceOn) {
        // Generate dual tone: 480Hz + 620Hz mixed
        int16_t buffer1[BUFFER_SIZE];
        int16_t buffer2[BUFFER_SIZE];
        
        // Generate both frequencies separately
        static float phase1 = 0.0, phase2 = 0.0;
        float phaseIncrement1 = (2.0 * PI * 480.0) / SAMPLE_RATE;
        float phaseIncrement2 = (2.0 * PI * 620.0) / SAMPLE_RATE;
        
        for (size_t i = 0; i < BUFFER_SIZE; i++) {
          int16_t sample1 = (int16_t)(sin(phase1) * 4000);
          int16_t sample2 = (int16_t)(sin(phase2) * 4000);
          buffer[i] = sample1 + sample2; // Mix the tones
          
          phase1 += phaseIncrement1;
          phase2 += phaseIncrement2;
          if (phase1 >= 2.0 * PI) phase1 -= 2.0 * PI;
          if (phase2 >= 2.0 * PI) phase2 -= 2.0 * PI;
        }
        
        writeHandsetAudioBuffer(buffer, BUFFER_SIZE);
      } else {
        // Silence period - no output needed
      }
      break;
      
    case TONE_TEST_RECORDED:
      // Recorded audio playback - plays back test recording
      if (testRecordedBuffer && testRecordedSamples > 0) {
        // Debug: Show progress every 1000 samples
        static int lastDebugIndex = -1000;
        if (recordedPlaybackIndex - lastDebugIndex >= 1000) {
          Serial.print("Playback progress: ");
          Serial.print(recordedPlaybackIndex);
          Serial.print("/");
          Serial.println(testRecordedSamples);
          lastDebugIndex = recordedPlaybackIndex;
        }
        
        // Fill buffer with recorded samples
        // Since we recorded at ~500Hz but play at 16kHz, repeat each sample 32 times
        static int sampleRepeatCount = 0;
        static int16_t currentSample = 0;
        static int16_t previousSample = 0;
        const int REPEAT_FACTOR = 32; // 16000Hz / 500Hz = 32
        
        for (size_t i = 0; i < BUFFER_SIZE; i++) {
          // Get new sample if we've repeated the current one enough times
          if (sampleRepeatCount == 0) {
            if (recordedPlaybackIndex < testRecordedSamples) {
              int16_t rawSample = testRecordedBuffer[recordedPlaybackIndex];
              
              // Light smoothing filter - less aggressive than before
              currentSample = (rawSample * 3 + previousSample) / 4;
              
              // Moderate amplitude limiting - allow louder signals
              if (currentSample > 8000) currentSample = 8000;
              if (currentSample < -8000) currentSample = -8000;
              
              previousSample = rawSample;
              
              recordedPlaybackIndex++;
            } else {
              currentSample = 0; // Silence when done
            }
          }
          
          buffer[i] = currentSample;
          
          // Track repeat count
          sampleRepeatCount++;
          if (sampleRepeatCount >= REPEAT_FACTOR) {
            sampleRepeatCount = 0;
          }
        }
        
        // Play on handset amplifier (where we know it works)
        writeHandsetAudioBuffer(buffer, BUFFER_SIZE);
        
        // Stop when we've played all samples
        if (recordedPlaybackIndex >= testRecordedSamples && sampleRepeatCount == 0) {
          currentTone = TONE_NONE;
          Serial.println("Recorded audio playback complete - all samples played");
        }
      } else {
        // No recorded data available
        Serial.println("ERROR: No recorded data available for playback!");
      }
      break;
      
    case TONE_NONE:
    default:
      break;
  }
}

/*
 * Generate Test Tone  
 * 
 * Creates a test tone for hardware validation with dual I2S control.
 * Used by test mode to verify individual amplifier channels.
 * 
 * Parameters:
 * - buffer: Output buffer for mono samples
 * - samples: Number of samples to generate
 * - frequency: Tone frequency in Hz
 * - handsetChannel: Output on handset amplifier (I2S0)
 * - ringerChannel: Output on ringer amplifier (I2S1)
 */
void generateTestTone(int16_t* buffer, size_t samples, float frequency, bool handsetChannel, bool ringerChannel) {
  static float phase = 0.0;
  float phaseIncrement = (2.0 * PI * frequency) / SAMPLE_RATE;
  
  for (size_t i = 0; i < samples; i++) {
    int16_t sample = (int16_t)(sin(phase) * 12000); // Higher amplitude for test
    phase += phaseIncrement;
    if (phase >= 2.0 * PI) {
      phase -= 2.0 * PI;
    }
    
    buffer[i] = sample;
  }
  
  // Output to requested channels
  if (handsetChannel && handsetAudioReady) {
    writeHandsetAudioBuffer(buffer, samples);
  }
  if (ringerChannel && ringerAudioReady) {
    writeRingerAudioBuffer(buffer, samples);
  }
}
