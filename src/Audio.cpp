/*
 * Audio System - I2S Output & Tone Generation
 * 
 * Manages the MAX98357A amplifiers and generates audio tones:
 * - Dual channel output: LEFT=ringer (base), RIGHT=handset
 * - I2S sample rate: 16kHz, 16-bit stereo
 * - Generates dial tone, ringback tone, and ring tone
 * - Uses sine wave generation for pure tones
 * - Microphone input via ADC for voice transmission
 */

#include "Audio.h"
#include "Pins.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <driver/adc.h>
#include <math.h>

// Audio settings
#define I2S_PORT I2S_NUM_0
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

/*
 * Generate Sine Wave Tone
 * 
 * Creates a pure sine wave and writes it to a stereo buffer.
 * 
 * Parameters:
 * - buffer: Array to fill with audio samples (stereo interleaved)
 * - samples: Number of samples to generate (must be even for stereo)
 * - frequency: Tone frequency in Hz (e.g., 350, 440, 480)
 * - leftChannel: true = output on LEFT (ringer), false = silence on LEFT
 * - rightChannel: true = output on RIGHT (handset), false = silence on RIGHT
 * 
 * Sine Wave Formula:
 * sample = sin(phase) * amplitude
 * phase += (2π * frequency) / sampleRate
 */
void generateTone(int16_t* buffer, size_t samples, float frequency, bool leftChannel, bool rightChannel) {
  static float phase = 0.0;
  float phaseIncrement = (2.0 * PI * frequency) / SAMPLE_RATE;
  
  for (size_t i = 0; i < samples; i += 2) {
    int16_t sample = (int16_t)(sin(phase) * 8000); // Amplitude of 8000 (adjust for volume)
    phase += phaseIncrement;
    if (phase >= 2.0 * PI) {
      phase -= 2.0 * PI;
    }
    
    // Stereo: left channel first, then right channel
    buffer[i] = leftChannel ? sample : 0;
    buffer[i + 1] = rightChannel ? sample : 0;
  }
}

/*
 * Setup Audio System
 * 
 * Configures I2S peripheral and initializes the MAX98357A amplifiers with channel latching.
 * 
 * Critical Channel Latching Sequence:
 * The MAX98357A determines LEFT/RIGHT channel based on LRCLK state during its first unmute:
 * - LRCLK LOW when SD goes HIGH = device becomes LEFT channel
 * - LRCLK HIGH when SD goes HIGH = device becomes RIGHT channel
 * 
 * Steps:
 * 1. Mute both amps (SD pins LOW)
 * 2. Take manual control of LRCLK pin
 * 3. Set LRCLK LOW, unmute ringer amp → ringer latches to LEFT channel
 * 4. Set LRCLK HIGH, unmute handset amp → handset latches to RIGHT channel
 * 5. Configure I2S driver (takes over LRCLK pin)
 * 6. Both amps now respond to their respective channels on the shared I2S bus
 * 
 * Result: Ringer = LEFT, Handset = RIGHT
 */
void setupAudio() {
  Serial.println("Configuring Audio...");

  // 1. Mute both amps
  digitalWrite(AMP_HANDSET_SD_PIN, LOW);
  digitalWrite(AMP_RINGER_SD_PIN, LOW);

  // 2. Temporarily take control of the LRCLK pin
  pinMode(I2S_LRCLK_PIN, OUTPUT);

  // --- Assign channels based on wiring.md playback instructions ---
  // Ringer Amp -> LEFT Channel
  // Handset Amp -> RIGHT Channel

  // 3. Latch Ringer amp to LEFT channel
  digitalWrite(I2S_LRCLK_PIN, LOW);
  delay(1);
  digitalWrite(AMP_RINGER_SD_PIN, HIGH); // Enable ringer amp while LRCLK is LOW
  delay(1);

  // 4. Latch Handset amp to RIGHT channel
  digitalWrite(I2S_LRCLK_PIN, HIGH);
  delay(1);
  digitalWrite(AMP_HANDSET_SD_PIN, HIGH); // Enable handset amp while LRCLK is HIGH
  delay(1);

  Serial.println("Amplifier channels latched. Ringer=LEFT, Handset=RIGHT.");

  // 5. Configure I2S peripheral
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // Master mode, transmit only
      .sample_rate = SAMPLE_RATE,                           // 16kHz
      .bits_per_sample = BITS_PER_SAMPLE,                  // 16-bit
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,        // Stereo (LEFT + RIGHT)
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // Standard I2S
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,            // Interrupt priority
      .dma_buf_count = 8,                                   // Number of DMA buffers
      .dma_buf_len = 64,                                    // Samples per buffer
      .use_apll = false,                                   // Don't use Audio PLL
      .tx_desc_auto_clear = true                           // Auto-clear descriptors
  };

  // 6. Define I2S pin configuration
  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCLK_PIN,      // Bit clock
      .ws_io_num = I2S_LRCLK_PIN,      // Word select (L/R clock)
      .data_out_num = I2S_DOUT_PIN,    // Data output
      .data_in_num = I2S_PIN_NO_CHANGE // No input (playback only)
  };

  // 7. Install and start I2S driver
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT); // Clear buffer to start with silence

  Serial.println("I2S Driver installed.");
}

/*
 * Setup Microphone Input
 * 
 * Configures the ADC to continuously sample from the MAX9814 microphone preamp.
 * The MAX9814 outputs an analog signal (0-3.3V) that represents audio amplitude.
 * 
 * ADC Configuration:
 * - 12-bit resolution (0-4095)
 * - Attenuation: 11dB (reads 0-3.3V range)
 * - Pin: MIC_ADC_PIN (GPIO 4)
 * 
 * Audio Pipeline:
 * Electret mic → MAX9814 preamp (AGC) → ADC → ESP32 → ESP-NOW → Peer ESP32 → I2S → Speaker
 */
void setupMicrophone() {
  // Configure ADC for microphone input
  adc1_config_width(ADC_WIDTH_BIT_12);  // 12-bit resolution (0-4095)
  adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_12);  // GPIO 4 = ADC1_CHANNEL_3
  
  Serial.println("Microphone ADC configured on GPIO 4");
}

/*
 * Read Microphone Buffer
 * 
 * Samples the microphone ADC and fills a buffer with audio data.
 * Converts 12-bit ADC readings (0-4095) to 16-bit signed audio samples (-32768 to +32767).
 * 
 * Parameters:
 * - buffer: Array to fill with audio samples
 * - samples: Number of samples to read
 * 
 * Returns: true if successful, false if error
 * 
 * Note: Sampling is blocking. For 100 samples at 16kHz, this takes ~6ms.
 */
bool readMicrophoneBuffer(int16_t* buffer, size_t samples) {
  if (!buffer) return false;
  
  static int32_t dcOffset = 2048;  // Running average for DC removal
  static bool dcCalibrated = false;
  
  for (size_t i = 0; i < samples; i++) {
    // Read 12-bit ADC value (0-4095)  
    int adcValue = adc1_get_raw(ADC1_CHANNEL_3);  // GPIO 4
    
    // Update DC offset (slow adaptation for DC removal)
    if (!dcCalibrated) {
      dcOffset = adcValue;  // Quick initial calibration
      dcCalibrated = true;
    } else {
      dcOffset = (dcOffset * 1023 + adcValue) / 1024;  // Very slow adaptation
    }
    
    // Remove DC component and scale to 16-bit audio
    int32_t acSignal = adcValue - dcOffset;
    
    // Amplify and convert to signed 16-bit (with clipping protection)
    int32_t amplified = acSignal * 128;  // Higher amplification for better signal
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;
    
    buffer[i] = (int16_t)amplified;
  }
  
  return true;
}

/*
 * Write Audio Buffer
 * 
 * Plays received audio samples through the I2S output.
 * Converts mono audio to stereo by duplicating to RIGHT channel (handset).
 * LEFT channel (ringer) is kept silent during calls.
 * 
 * Parameters:
 * - buffer: Array of audio samples to play
 * - samples: Number of samples in buffer
 * 
 * This is called when MSG_AUDIO_DATA packets are received from the peer phone.
 */
void writeAudioBuffer(const int16_t* buffer, size_t samples) {
  if (!buffer) return;
  
  // Create stereo buffer (LEFT + RIGHT interleaved)
  int16_t stereoBuffer[samples * 2];
  
  for (size_t i = 0; i < samples; i++) {
    stereoBuffer[i * 2] = 0;           // LEFT channel (ringer) = silence
    stereoBuffer[i * 2 + 1] = buffer[i];  // RIGHT channel (handset) = audio
  }
  
  // Write to I2S
  size_t bytes_written;
  i2s_write(I2S_PORT, stereoBuffer, samples * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
}

/*
 * Play Dial Tone
 * Continuous 350Hz tone on RIGHT channel (handset)
 * Standard North American dial tone
 */
void playDialTone() {
  if (currentTone != TONE_DIAL) {
    currentTone = TONE_DIAL;
    toneStartTime = millis();
    Serial.println("Playing dial tone");
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
 * Output: LEFT channel (base ringer)
 */
void playRingTone() {
  if (currentTone != TONE_RING) {
    currentTone = TONE_RING;
    toneStartTime = millis();
    lastCadenceTime = millis();
    cadenceOn = true;
    Serial.println("Playing ring tone");
  }
}

/*
 * Play Error Tone
 * Fast busy signal for invalid number / call failed
 * Pattern: 480Hz + 620Hz dual tone, 250ms ON, 250ms OFF (fast busy)
 * Output: RIGHT channel (handset)
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
    Serial.println("Playing error tone (fast busy)");
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
    i2s_zero_dma_buffer(I2S_PORT); // Clear the buffer to stop audio
    Serial.println("Tone stopped");
  }
}

/*
 * Update Tone Generation
 * 
 * Called continuously from main loop to generate audio output.
 * Handles:
 * - TONE_DIAL: Continuous tone on handset
 * - TONE_RINGBACK: Cadenced tone on handset (2s on, 4s off)
 * - TONE_RING: Cadenced tone on ringer (2s on, 4s off)
 * - TONE_NONE: Silence
 * 
 * The cadence pattern matches standard telephone ring patterns:
 * Ring for 2 seconds, silent for 4 seconds, repeat
 */
void updateToneGeneration() {
  if (currentTone == TONE_NONE) {
    return;
  }
  
  int16_t buffer[BUFFER_SIZE];
  size_t bytes_written;
  unsigned long currentTime = millis();
  
  switch (currentTone) {
    case TONE_DIAL:
      // Continuous 350Hz + 440Hz dial tone on RIGHT channel (handset)
      // Simplified to 350Hz for now (could mix both frequencies)
      generateTone(buffer, BUFFER_SIZE, 350.0, false, true);
      i2s_write(I2S_PORT, buffer, BUFFER_SIZE * sizeof(int16_t), &bytes_written, portMAX_DELAY);
      break;
      
    case TONE_RINGBACK:
      // Ringback: 440Hz, 2 seconds on, 4 seconds off, on RIGHT channel (handset)
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
        generateTone(buffer, BUFFER_SIZE, 440.0, false, true);
      } else {
        memset(buffer, 0, BUFFER_SIZE * sizeof(int16_t)); // Silence
      }
      i2s_write(I2S_PORT, buffer, BUFFER_SIZE * sizeof(int16_t), &bytes_written, portMAX_DELAY);
      break;
      
    case TONE_RING:
      // Ring tone: 440Hz, 2 seconds on, 4 seconds off, on LEFT channel (base ringer)
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
        generateTone(buffer, BUFFER_SIZE, 440.0, true, false);
      } else {
        memset(buffer, 0, BUFFER_SIZE * sizeof(int16_t)); // Silence
      }
      i2s_write(I2S_PORT, buffer, BUFFER_SIZE * sizeof(int16_t), &bytes_written, portMAX_DELAY);
      break;
      
    case TONE_ERROR:
      // Error/Fast Busy: 480Hz, 250ms on, 250ms off, on RIGHT channel (handset)
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
        generateTone(buffer, BUFFER_SIZE, 480.0, false, true);
      } else {
        memset(buffer, 0, BUFFER_SIZE * sizeof(int16_t)); // Silence
      }
      i2s_write(I2S_PORT, buffer, BUFFER_SIZE * sizeof(int16_t), &bytes_written, portMAX_DELAY);
      break;
      
    case TONE_BUSY:
      // Normal Busy: 480Hz + 620Hz, 500ms on, 500ms off, on RIGHT channel (handset)
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
        // Generate dual tone: 480Hz + 620Hz
        int16_t buffer1[BUFFER_SIZE];
        int16_t buffer2[BUFFER_SIZE];
        generateTone(buffer1, BUFFER_SIZE, 480.0, false, true);
        generateTone(buffer2, BUFFER_SIZE, 620.0, false, true);
        // Mix the two tones
        for (size_t i = 0; i < BUFFER_SIZE; i++) {
          buffer[i] = (buffer1[i] + buffer2[i]) / 2;
        }
      } else {
        memset(buffer, 0, BUFFER_SIZE * sizeof(int16_t)); // Silence
      }
      i2s_write(I2S_PORT, buffer, BUFFER_SIZE * sizeof(int16_t), &bytes_written, portMAX_DELAY);
      break;
      
    case TONE_TEST_RECORDED:
      // Recorded audio playback - plays for exactly the length of the recording, then stops
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
        
        for (size_t i = 0; i < BUFFER_SIZE; i += 2) {
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
          
          // Output on LEFT channel (ringer) - we know this works
          buffer[i] = currentSample;   // Left channel
          buffer[i + 1] = 0;          // Right channel (silent)
          
          // Track repeat count
          sampleRepeatCount++;
          if (sampleRepeatCount >= REPEAT_FACTOR) {
            sampleRepeatCount = 0;
          }
        }
        
        // Stop when we've played all samples
        if (recordedPlaybackIndex >= testRecordedSamples && sampleRepeatCount == 0) {
          currentTone = TONE_NONE;
          Serial.println("Recorded audio playback complete - all samples played");
        }
      } else {
        // No recorded data available
        memset(buffer, 0, BUFFER_SIZE * sizeof(int16_t));
        Serial.println("ERROR: No recorded data available for playback!");
      }
      i2s_write(I2S_PORT, buffer, BUFFER_SIZE * sizeof(int16_t), &bytes_written, portMAX_DELAY);
      break;
      
    case TONE_NONE:
    default:
      break;
  }
}

/*
 * Write Raw Audio Buffer
 * 
 * Directly writes audio samples to I2S output without channel conversion.
 * Used by test mode for precise control of stereo channels.
 * 
 * Parameters:
 * - buffer: Stereo interleaved samples (LEFT, RIGHT, LEFT, RIGHT...)
 * - samples: Total number of samples (must be even for stereo)
 */
void writeRawAudioBuffer(const int16_t* buffer, size_t samples) {
  if (!buffer) return;
  
  size_t bytes_written;
  i2s_write(I2S_PORT, buffer, samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);
}

/*
 * Generate Test Tone
 * 
 * Creates a test tone for hardware validation with precise channel control.
 * Used by test mode to verify individual speaker channels.
 * 
 * Parameters:
 * - buffer: Output buffer for stereo samples
 * - samples: Number of samples to generate (must be even)
 * - frequency: Tone frequency in Hz
 * - leftChannel: Output on LEFT channel (ringer)
 * - rightChannel: Output on RIGHT channel (handset)
 */
void generateTestTone(int16_t* buffer, size_t samples, float frequency, bool leftChannel, bool rightChannel) {
  static float phase = 0.0;
  float phaseIncrement = (2.0 * PI * frequency) / SAMPLE_RATE;
  
  for (size_t i = 0; i < samples; i += 2) {
    int16_t sample = (int16_t)(sin(phase) * 8000); // Test tone at 50% volume
    phase += phaseIncrement;
    if (phase >= 2.0 * PI) {
      phase -= 2.0 * PI;
    }
    
    // Stereo: left channel first, then right channel
    buffer[i] = leftChannel ? sample : 0;
    buffer[i + 1] = rightChannel ? sample : 0;
  }
}
