/*
 * TestMode.h - Hardware Testing and Diagnostics
 * 
 * Interactive test mode for validating audio, microphone, and other hardware components.
 * Accessible via serial commands for development and troubleshooting.
 * 
 * Features:
 * - Audio output tests (handset speaker, base ringer)
 * - Microphone input monitoring and recording
 * - Component diagnostics
 * - Hardware validation
 * 
 * Commands:
 * - test enter         : Enter test mode
 * - test exit          : Exit test mode
 * - test audio handset : Test handset speaker (right channel)
 * - test audio ringer  : Test base ringer (left channel)
 * - test audio both    : Test both speakers simultaneously
 * - test mic level     : Show microphone input levels
 * - test mic record    : Record and playback microphone audio
 * - test pins          : Show all GPIO pin states
 * - test help          : Show available commands
 */

#ifndef TEST_MODE_H
#define TEST_MODE_H

#include <Arduino.h>

// Test mode state
extern bool testModeActive;

// Core functions
void setupTestMode();
void handleTestMode();
void processTestCommand(String command);

// Internal test handlers (called from handleTestMode)
void handleMicrophoneTest();
void handleAudioTest();

// Audio test functions
void testHandsetAudio();
void testRingerAudio();
void testBothAudio();
void testDialTone();
void stopAudioTest();

// Microphone test functions
void testMicrophoneLevel();
void testMicrophoneRecord();
void testMicrophoneTone();
void stopMicrophoneTest();
void testWAVPlayback();
void testMP3Playback();

// Debug test functions
void testSineWave();

// Diagnostic functions
void testPinStates();
void showTestHelp();

// Utility functions
void enterTestMode();
void exitTestMode();
bool isTestModeActive();

#endif // TEST_MODE_H