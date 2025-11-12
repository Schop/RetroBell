#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stddef.h>

// Audio buffer size for transmission (must fit in ESP-NOW packet)
#define AUDIO_CHUNK_SIZE 200  // 200 bytes = 100 samples (16-bit)

void setupAudio();
void setupMicrophone();
void playDialTone();
void playRingbackTone();
void playRingTone();
void playErrorTone();  // Fast busy tone for invalid number
void playBusyTone();   // Busy tone for when called phone is in use
void stopTone();
void updateToneGeneration(); // Call this in loop() to keep tones playing

// Audio transmission functions
bool readMicrophoneBuffer(int16_t* buffer, size_t samples);
void writeAudioBuffer(const int16_t* buffer, size_t samples);

// Test mode functions
void generateTestTone(int16_t* buffer, size_t samples, float frequency, bool leftChannel, bool rightChannel);
void writeRawAudioBuffer(const int16_t* buffer, size_t samples);

#endif // AUDIO_H
