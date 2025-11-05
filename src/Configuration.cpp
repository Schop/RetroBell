/*
 * Configuration.cpp - Configuration Management Implementation
 * 
 * Manages persistent storage of phone configuration using LittleFS and JSON.
 */

#include "Configuration.h"
#include "Pins.h"
#include "RotaryDial.h"
#include "HookSwitch.h"
#include "State.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Current configuration (cached in memory)
static PhoneConfig currentConfig;

/*
 * Setup Configuration System
 * 
 * Initializes the LittleFS filesystem for configuration storage.
 * Must be called once during startup before any load/save operations.
 */
void setupConfiguration() {
  Serial.println("Initializing configuration system...");
  
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR: Failed to mount LittleFS filesystem");
    return;
  }
  
  Serial.println("LittleFS filesystem mounted successfully");
}

/*
 * Load Configuration from Flash Storage
 * 
 * Reads config.json from the ESP32's LittleFS filesystem.
 * 
 * Expected JSON format:
 * {
 *   "number": 101,
 *   "wifi_ssid": "YourNetwork",
 *   "wifi_password": "YourPassword"
 * }
 * 
 * Returns:
 * - true if configuration loaded successfully
 * - false if file not found or parse error
 */
bool loadConfiguration(PhoneConfig& config) {
  Serial.println("Loading configuration from /config.json...");
  
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Config file not found - first time setup required");
    config.phoneNumber = -1; // Indicates not configured
    return false;
  }

  // Parse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();
  
  if (error) {
    Serial.print("Failed to parse config file: ");
    Serial.println(error.c_str());
    config.phoneNumber = -1;
    return false;
  }

  // Load values from JSON
  config.phoneNumber = doc["number"] | -1; // Default to -1 if not present
  config.wifiSsid = doc["wifi_ssid"].as<String>();
  config.wifiPassword = doc["wifi_password"].as<String>();
  
  // Cache in memory
  currentConfig = config;
  
  // Display loaded configuration
  if (config.phoneNumber >= 0) {
    Serial.print("✓ Loaded phone number: ");
    Serial.println(config.phoneNumber);
  } else {
    Serial.println("⚠ Phone number not configured (value: -1)");
  }
  
  if (config.wifiSsid.length() > 0) {
    Serial.print("✓ Loaded Wi-Fi SSID: ");
    Serial.println(config.wifiSsid);
  } else {
    Serial.println("⚠ No Wi-Fi credentials found");
  }
  
  return true;
}

/*
 * Save Configuration to Flash Storage
 * 
 * Writes the current configuration to config.json so it persists across reboots.
 * 
 * Returns:
 * - true if saved successfully
 * - false if write failed
 */
bool saveConfiguration(const PhoneConfig& config) {
  Serial.println("Saving configuration to /config.json...");
  
  // Create JSON document
  StaticJsonDocument<256> doc;
  doc["number"] = config.phoneNumber;
  doc["wifi_ssid"] = config.wifiSsid;
  doc["wifi_password"] = config.wifiPassword;
  
  // Open file for writing
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("ERROR: Failed to open config file for writing");
    return false;
  }
  
  // Write JSON to file
  if (serializeJson(doc, configFile) == 0) {
    Serial.println("ERROR: Failed to write config file");
    configFile.close();
    return false;
  }
  
  configFile.close();
  
  // Update cached config
  currentConfig = config;
  
  Serial.println("✓ Configuration saved successfully!");
  return true;
}

/*
 * Get Phone Number
 * 
 * Returns the currently configured phone number.
 * Used by Network module to identify this device.
 * 
 * Returns -1 if not configured.
 */
int getPhoneNumber() {
  return currentConfig.phoneNumber;
}

/*
 * Run Setup Mode
 * 
 * Interactive first-time setup that allows the user to assign a phone number via:
 * - Method 1: Serial monitor - type '1', then enter the number and press Enter
 * - Method 2: Rotary dial - type '2' or just start dialing, then hang up when done
 * 
 * If no input is received for 60 seconds, a number is auto-assigned based on MAC address.
 * 
 * This function blocks until setup is complete.
 */
void runSetupMode(PhoneConfig& config) {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║    FIRST TIME SETUP - PHONE NUMBER    ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("\nChoose setup method:");
  Serial.println("  1. Enter number via Serial Monitor");
  Serial.println("  2. Dial number using Rotary Dial");
  Serial.println("\nWaiting for input...\n");
  
  unsigned long startTime = millis();
  String dialedNumber = "";
  bool serialMode = false;
  bool dialMode = false;
  
  // Wait for user to choose method or start dialing
  while (config.phoneNumber == -1) {
    // ====== Check Serial Input ======
    if (Serial.available() > 0) {
      char c = Serial.read();
      
      // User selects serial input mode
      if (c == '1') {
        serialMode = true;
        Serial.println("\n>> Serial input mode selected");
        Serial.println("Enter your phone number (0-999) and press Enter:");
      }
      // User selects rotary dial mode
      else if (c == '2') {
        dialMode = true;
        Serial.println("\n>> Rotary dial mode selected");
        Serial.println("Dial your phone number now, then hang up the handset.");
      }
      // User is entering digits via serial
      else if (serialMode && c >= '0' && c <= '9') {
        dialedNumber += c;
        Serial.print(c);
      }
      // User pressed Enter - save the number
      else if (serialMode && (c == '\n' || c == '\r')) {
        if (dialedNumber.length() > 0) {
          config.phoneNumber = dialedNumber.toInt();
          Serial.print("\n\n✓ Phone number set to: ");
          Serial.println(config.phoneNumber);
          break;
        }
      }
    }
    
    // ====== Check Rotary Dial Input ======
    handleRotaryDial();
    int digit = getDialedDigit();
    if (digit >= 0) {
      dialMode = true; // Automatically enter dial mode if user starts dialing
      dialedNumber += String(digit);
      Serial.print("Dialed: ");
      Serial.println(dialedNumber);
      clearDialedDigit();
    }
    
    // ====== Check if Handset is Hung Up (Dial Complete) ======
    // Read hook switch directly (can't use handleHookSwitch without state)
    if (dialMode && dialedNumber.length() > 0 && digitalRead(HOOK_SW_PIN) == LOW) {
      config.phoneNumber = dialedNumber.toInt();
      Serial.print("\n✓ Phone number set to: ");
      Serial.println(config.phoneNumber);
      delay(1000); // Debounce
      break;
    }
    
    // ====== Timeout After 60 Seconds ======
    // Auto-assign a number based on MAC address to prevent indefinite waiting
    if (millis() - startTime > 60000) {
      uint8_t mac[6];
      WiFi.macAddress(mac);
      config.phoneNumber = 100 + (mac[5] % 156); // Number between 100-255
      Serial.print("\n⚠ Timeout! Auto-assigned phone number: ");
      Serial.println(config.phoneNumber);
      break;
    }
    
    delay(50); // Small delay to prevent busy-waiting
  }
  
  Serial.println("\n✓ Setup complete!\n");
}
