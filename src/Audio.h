#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stddef.h>

// Audio buffer size for transmission (must fit in ESP-NOW packet)
#define AUDIO_CHUNK_SIZE 200  // 200 bytes = 100 samples (16-bit)

// Dual I2S audio setup and control
void setupAudio();
void setupHandsetAudio();  // I2S0 for handset + microphone
void setupRingerAudio();   // I2S1 for base ringer
void setupMicrophone();    // ICS-43434 I2S microphone

// Tone generation and playback
void playDialTone();
void playRingbackTone();
void playRingTone();
void playErrorTone();  // Fast busy tone for invalid number
void playBusyTone();   // Busy tone for when called phone is in use
void playTestRecordedAudio();  // Test mode recorded audio playback
void stopTone();
void updateToneGeneration(); // Call this in loop() to keep tones playing

// Audio transmission functions (ICS-43434 I2S microphone)
bool readMicrophoneBuffer(int16_t* buffer, size_t samples);
void writeAudioBuffer(const int16_t* buffer, size_t samples);

// Test mode functions
void generateTone(int16_t* buffer, size_t samples, float frequency, bool handsetChannel, bool ringerChannel);
void generateTestTone(int16_t* buffer, size_t samples, float frequency, bool handsetChannel, bool ringerChannel);
void writeHandsetAudioBuffer(const int16_t* buffer, size_t samples);
void writeRingerAudioBuffer(const int16_t* buffer, size_t samples);

#endif // AUDIO_H
