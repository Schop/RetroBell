/*
 * Network - ESP-NOW Peer-to-Peer Communication
 * 
 * Manages communication between phones using ESP-NOW protocol.
 * 
 * ESP-NOW Features:
 * - Direct peer-to-peer communication (no router needed)
 * - Low latency (faster than Wi-Fi/TCP)
 * - Works on Wi-Fi channel (benefits from router connection)
 * - Uses MAC addresses for device identification
 * 
 * Automatic Discovery:
 * - Each phone broadcasts its number every 10 seconds
 * - When a broadcast is received, the sender is added as a peer
 * - No manual MAC address configuration needed!
 * 
 * Message Types:
 * - MSG_DISCOVERY: "I exist, my number is X"
 * - MSG_CALL_REQUEST: "I'm calling you"
 * - MSG_CALL_ACCEPT: "I answered your call"
 * - MSG_CALL_REJECT: "I rejected your call"
 * - MSG_CALL_END: "I'm hanging up"
 * - MSG_AUDIO_DATA: Audio stream for voice calls
 */

#include "Network.h"
#include "State.h"
#include "Configuration.h"
#include "Audio.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// External references
extern String dialedNumber;

// Peer information
struct PeerInfo {
  int number;
  uint8_t macAddress[6];
  bool registered;
};

#define MAX_PEERS 10
PeerInfo peers[MAX_PEERS];
int peerCount = 0;

// Current call state
int currentCallPeer = -1;

// Discovery state
unsigned long lastDiscoveryTime = 0;
const unsigned long DISCOVERY_INTERVAL = 10000; // Broadcast every 10 seconds

/*
 * Setup Wi-Fi Connection
 * 
 * Connects the ESP32 to your home Wi-Fi network. This ensures:
 * - Both phones are on the same Wi-Fi channel (required for ESP-NOW)
 * - Better signal reliability (via router) even if phones can't directly reach each other
 * 
 * Note: ESP-NOW communication is direct peer-to-peer and does NOT go through the router
 * 
 * Parameters:
 * - ssid: Wi-Fi network name
 * - password: Wi-Fi password
 */
void setupWifi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA); // Station mode (client)
  
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  // Try to connect for 10 seconds
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Channel: ");
    Serial.println(WiFi.channel());
  } else {
    Serial.println("\nWi-Fi connection failed! Will try ESP-NOW anyway...");
  }
}

/*
 * Print MAC Address
 * 
 * Displays the device's unique MAC address for debugging.
 * This MAC address is used by other phones to identify this device on the network.
 */
void printMacAddress() {
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
}

/*
 * Setup Network
 * 
 * Initializes ESP-NOW and prepares for peer-to-peer communication.
 * 
 * Steps:
 * 1. Ensure WiFi is in STA (station) mode
 * 2. Initialize ESP-NOW protocol
 * 3. Register callback for incoming messages
 * 4. Add broadcast peer (FF:FF:FF:FF:FF:FF) for discovery
 * 5. Send initial discovery broadcast
 * 
 * Why Broadcast Peer?
 * The broadcast address allows sending to all nearby devices without
 * knowing their MAC addresses in advance. Perfect for automatic discovery!
 */
void setupNetwork() {
  // WiFi must be in STA mode for ESP-NOW
  WiFi.mode(WIFI_STA);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Register callback for received data
  esp_now_register_recv_cb(handleIncomingMessage);
  
  // Add broadcast peer for discovery
  esp_now_peer_info_t broadcastPeer;
  memset(&broadcastPeer, 0, sizeof(broadcastPeer));
  for (int i = 0; i < 6; i++) {
    broadcastPeer.peer_addr[i] = 0xFF; // Broadcast address
  }
  broadcastPeer.channel = 0;
  broadcastPeer.encrypt = false;
  esp_now_add_peer(&broadcastPeer);
  
  Serial.println("Network initialized");
  
  // Send initial discovery broadcast
  broadcastDiscovery();
}

/*
 * Add Peer by MAC Address
 * 
 * Registers a discovered peer for future communication.
 * 
 * Process:
 * 1. Check if peer already exists (by MAC address)
 * 2. If exists, update phone number if changed
 * 3. If new, add to ESP-NOW peer list
 * 4. Store in our local peer directory
 * 
 * Why track peers?
 * ESP-NOW requires peers to be registered before sending messages.
 * We maintain a directory mapping phone numbers to MAC addresses.
 */
void addPeerByMac(const uint8_t* macAddress, int phoneNumber) {
  // Check if peer already exists
  for (int i = 0; i < peerCount; i++) {
    if (memcmp(peers[i].macAddress, macAddress, 6) == 0) {
      // Peer already exists, just update the number if needed
      if (peers[i].number != phoneNumber) {
        peers[i].number = phoneNumber;
        Serial.print("Updated peer number to: ");
        Serial.println(phoneNumber);
      }
      return;
    }
  }
  
  if (peerCount >= MAX_PEERS) {
    Serial.println("Max peers reached!");
    return;
  }
  
  // Add peer to ESP-NOW
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, macAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  
  // Store in our peer list
  memcpy(peers[peerCount].macAddress, macAddress, 6);
  peers[peerCount].number = phoneNumber;
  peers[peerCount].registered = true;
  peerCount++;
  
  Serial.print("Added peer #");
  Serial.print(phoneNumber);
  Serial.print(" with MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", macAddress[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}

/*
 * Broadcast Discovery
 * 
 * Announces this phone's presence to all nearby devices.
 * 
 * Message contents:
 * - type: MSG_DISCOVERY
 * - fromNumber: This phone's number
 * - toNumber: -1 (broadcast, not directed)
 * 
 * Sent to: FF:FF:FF:FF:FF:FF (broadcast address)
 * Frequency: Every 10 seconds
 */
void broadcastDiscovery() {
  Message msg;
  msg.type = MSG_DISCOVERY;
  msg.fromNumber = getPhoneNumber();
  msg.toNumber = -1; // Broadcast to all
  
  uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_err_t result = esp_now_send(broadcastAddr, (uint8_t*)&msg, sizeof(msg));
  
  if (result == ESP_OK) {
    Serial.print("Discovery broadcast sent. I am phone #");
    Serial.println(getPhoneNumber());
  } else {
    Serial.println("Error sending discovery broadcast");
  }
  
  lastDiscoveryTime = millis();
}

/*
 * Update Network
 * 
 * Called continuously from main loop to maintain network presence.
 * Sends periodic discovery broadcasts so other phones know we're alive.
 */
void updateNetwork() {
  // Periodically broadcast our presence
  if (millis() - lastDiscoveryTime > DISCOVERY_INTERVAL) {
    broadcastDiscovery();
  }
}

/*
 * Send Call Request
 * 
 * Initiates a call to another phone.
 * 
 * Process:
 * 1. Look up the target phone number in our peer directory
 * 2. Create a MSG_CALL_REQUEST message
 * 3. Send directly to the peer's MAC address
 * 4. Store target as currentCallPeer for future messages
 * 
 * Returns:
 * - true: Peer found and call request sent successfully
 * - false: Peer not found or send failed
 * 
 * Note: If peer isn't found, call fails. Discovery must happen first!
 */
bool sendCallRequest(int targetNumber) {
  Serial.print("Sending call request to: ");
  Serial.println(targetNumber);
  
  // Find the peer with this number
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].registered && peers[i].number == targetNumber) {
      Message msg;
      msg.type = MSG_CALL_REQUEST;
      msg.fromNumber = getPhoneNumber();
      msg.toNumber = targetNumber;
      
      esp_err_t result = esp_now_send(peers[i].macAddress, (uint8_t*)&msg, sizeof(msg));
      if (result == ESP_OK) {
        Serial.println("Call request sent");
        currentCallPeer = targetNumber;
        return true;
      } else {
        Serial.println("Error sending call request");
        return false;
      }
    }
  }
  
  Serial.print("Peer #");
  Serial.print(targetNumber);
  Serial.println(" not found!");
  return false;
}

/*
 * Send Call Accept
 * 
 * Answers an incoming call.
 * Sent in response to MSG_CALL_REQUEST when user lifts handset.
 */
void sendCallAccept(int targetNumber) {
  Serial.print("Sending call accept to: ");
  Serial.println(targetNumber);
  
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].registered && peers[i].number == targetNumber) {
      Message msg;
      msg.type = MSG_CALL_ACCEPT;
      msg.fromNumber = getPhoneNumber();
      msg.toNumber = targetNumber;
      
      esp_now_send(peers[i].macAddress, (uint8_t*)&msg, sizeof(msg));
      currentCallPeer = targetNumber;
      return;
    }
  }
}

/*
 * Send Call Busy
 * 
 * Responds to incoming call with busy signal.
 * Sent when receiving a call while already in another call.
 */
void sendCallBusy(int targetNumber) {
  Serial.print("Sending busy signal to: ");
  Serial.println(targetNumber);
  
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].registered && peers[i].number == targetNumber) {
      Message msg;
      msg.type = MSG_CALL_BUSY;
      msg.fromNumber = getPhoneNumber();
      msg.toNumber = targetNumber;
      
      esp_now_send(peers[i].macAddress, (uint8_t*)&msg, sizeof(msg));
      return;
    }
  }
}

/*
 * Send Call End
 * 
 * Terminates an active call.
 * Sent when user hangs up the handset.
 */
void sendCallEnd(int targetNumber) {
  Serial.print("Sending call end to: ");
  Serial.println(targetNumber);
  
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].registered && peers[i].number == targetNumber) {
      Message msg;
      msg.type = MSG_CALL_END;
      msg.fromNumber = getPhoneNumber();
      msg.toNumber = targetNumber;
      
      esp_now_send(peers[i].macAddress, (uint8_t*)&msg, sizeof(msg));
      currentCallPeer = -1;
      return;
    }
  }
}

/*
 * Send Audio Data
 * 
 * Transmits audio samples to the peer phone during an active call.
 * Audio data is sent in the Message.data field (200 bytes = 100 samples).
 * 
 * Parameters:
 * - audioBuffer: Array of 16-bit audio samples from microphone
 * - samples: Number of samples to send (typically 100)
 * 
 * This function is called continuously during IN_CALL state to stream audio.
 */
void sendAudioData(const int16_t* audioBuffer, size_t samples) {
  // Only send if we're in a call
  if (currentCallPeer == -1) return;
  
  // Find peer MAC address
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].registered && peers[i].number == currentCallPeer) {
      Message msg;
      msg.type = MSG_AUDIO_DATA;
      msg.fromNumber = getPhoneNumber();
      msg.toNumber = currentCallPeer;
      
      // Copy audio samples to message data field
      // Each sample is 2 bytes (16-bit), max 100 samples = 200 bytes
      size_t bytesToCopy = samples * sizeof(int16_t);
      if (bytesToCopy > sizeof(msg.data)) {
        bytesToCopy = sizeof(msg.data);
      }
      memcpy(msg.data, audioBuffer, bytesToCopy);
      
      // Send via ESP-NOW (no error checking for speed)
      esp_now_send(peers[i].macAddress, (uint8_t*)&msg, sizeof(msg));
      return;
    }
  }
}

/*
 * Handle Incoming Message
 * 
 * Callback function invoked by ESP-NOW when a message is received.
 * 
 * Parameters:
 * - mac: Sender's MAC address (used for discovery)
 * - data: Message payload
 * - len: Message length (validated against expected size)
 * 
 * Message Processing:
 * - MSG_DISCOVERY: Add sender to peer list
 * - MSG_CALL_REQUEST: Incoming call → start RINGING
 * - MSG_CALL_ACCEPT: Call answered → go IN_CALL
 * - MSG_CALL_REJECT: Call declined → return to IDLE
 * - MSG_CALL_END: Peer hung up → return to IDLE
 * - MSG_AUDIO_DATA: Voice data (TODO: implement streaming)
 * 
 * Security Note:
 * Messages check toNumber to ensure they're intended for this phone.
 * Discovery broadcasts have toNumber=-1 (everyone processes them).
 */
void handleIncomingMessage(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(Message)) {
    Serial.println("Invalid message size");
    return;
  }
  
  Message* msg = (Message*)data;
  
  Serial.print("Message received. Type: ");
  Serial.print(msg->type);
  Serial.print(" From: ");
  Serial.print(msg->fromNumber);
  Serial.print(" To: ");
  Serial.println(msg->toNumber);
  
  switch (msg->type) {
    case MSG_DISCOVERY:
      // Another phone is announcing its presence
      Serial.print("Discovered phone #");
      Serial.println(msg->fromNumber);
      addPeerByMac(mac, msg->fromNumber);
      break;
      
    case MSG_CALL_REQUEST:
      // Only process if message is for us
      if (msg->toNumber != getPhoneNumber()) {
        return;
      }
      Serial.print("Incoming call from: ");
      Serial.println(msg->fromNumber);
      
      // Check if we're already in a call
      if (getCurrentState() == IN_CALL || getCurrentState() == RINGING) {
        Serial.println("Already busy, sending busy signal");
        sendCallBusy(msg->fromNumber);
        return;
      }
      
      currentCallPeer = msg->fromNumber;
      changeState(RINGING);
      break;
      
    case MSG_CALL_ACCEPT:
      if (msg->toNumber != getPhoneNumber()) return;
      Serial.println("Call accepted!");
      changeState(IN_CALL);
      break;
      
    case MSG_CALL_BUSY:
      if (msg->toNumber != getPhoneNumber()) return;
      Serial.println("Called party is busy");
      currentCallPeer = -1;
      changeState(CALL_BUSY);
      break;
      
    case MSG_CALL_REJECT:
      if (msg->toNumber != getPhoneNumber()) return;
      Serial.println("Call rejected");
      changeState(IDLE);
      break;
      
    case MSG_CALL_END:
      if (msg->toNumber != getPhoneNumber()) return;
      Serial.println("Call ended by peer");
      currentCallPeer = -1;
      changeState(IDLE);
      break;
      
    case MSG_AUDIO_DATA:
      if (msg->toNumber != getPhoneNumber()) return;
      // Extract audio samples from message and play through speaker
      // msg->data contains up to 100 samples (200 bytes) of 16-bit audio
      int16_t* audioSamples = (int16_t*)msg->data;
      size_t sampleCount = sizeof(msg->data) / sizeof(int16_t);
      writeAudioBuffer(audioSamples, sampleCount);
      break;
  }
}

/*
 * Get Current Call Peer
 * 
 * Returns the phone number of the device we're currently in a call with.
 * Returns -1 if no call is active.
 * 
 * Used by HookSwitch.cpp to know who to send hangup messages to.
 */
int getCurrentCallPeer() {
  return currentCallPeer;
}
