/*
 * Network.h - ESP-NOW Communication Interface
 * 
 * Provides peer-to-peer communication between phones using ESP-NOW protocol.
 * 
 * Message Types:
 * - MSG_DISCOVERY: Broadcast announcement of phone number and presence
 * - MSG_CALL_REQUEST: Initiate a call to specific phone
 * - MSG_CALL_ACCEPT: Accept an incoming call
 * - MSG_CALL_REJECT: Decline an incoming call
 * - MSG_CALL_END: Hang up an active call
 * - MSG_AUDIO_DATA: Voice data packet (future implementation)
 * 
 * Message Structure:
 * - type: One of the MessageType enums
 * - fromNumber: Sender's phone number
 * - toNumber: Recipient's phone number (-1 for broadcast)
 * - data: Payload (200 bytes, used for audio or other data)
 * 
 * Key Features:
 * - Automatic peer discovery (no manual MAC configuration)
 * - Direct peer-to-peer communication (low latency)
 * - Supports up to 10 peers
 * - Periodic presence broadcasts every 10 seconds
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <esp_now.h>
#include <stdint.h>
#include <stddef.h>

// Audio packet configuration
#define AUDIO_SAMPLES_PER_PACKET 100  // 100 samples at 16-bit = 200 bytes

// Message types for communication between phones
enum MessageType {
  MSG_DISCOVERY,      // Broadcast to announce presence and phone number
  MSG_CALL_REQUEST,   // "I'm calling you"
  MSG_CALL_ACCEPT,    // "I answered your call"
  MSG_CALL_REJECT,    // "I declined your call"
  MSG_CALL_END,       // "I'm hanging up"
  MSG_AUDIO_DATA      // Voice data packet (for future audio streaming)
};

// Message structure (sent via ESP-NOW)
struct Message {
  MessageType type;
  int fromNumber;     // Sender's phone number
  int toNumber;       // Recipient's phone number (-1 = broadcast)
  uint8_t data[200];  // Payload for audio or other data
};

// Initialize ESP-NOW and start discovery
void setupNetwork();

// Setup Wi-Fi connection (required for ESP-NOW to work reliably)
void setupWifi(const char* ssid, const char* password);

// Print MAC address for debugging
void printMacAddress();

// Add a discovered peer to the peer list
void addPeerByMac(const uint8_t* macAddress, int phoneNumber);

// Broadcast our phone number to all nearby devices
void broadcastDiscovery();

// Send call request to a specific phone number
// Returns true if peer found and message sent, false if peer not found
bool sendCallRequest(int targetNumber);

// Accept an incoming call
void sendCallAccept(int targetNumber);

// End an active call
void sendCallEnd(int targetNumber);

// Send audio data to peer during call
void sendAudioData(const int16_t* audioBuffer, size_t samples);

// Callback for incoming ESP-NOW messages
void handleIncomingMessage(const uint8_t *mac, const uint8_t *data, int len);

// Get the phone number we're currently in a call with
int getCurrentCallPeer();

// Maintain network presence (call periodically from main loop)
void updateNetwork();

#endif // NETWORK_H
