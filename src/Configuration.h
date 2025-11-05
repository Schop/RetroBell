/*
 * Configuration.h - Configuration Management
 * 
 * Handles loading, saving, and managing phone configuration from LittleFS.
 * 
 * Configuration includes:
 * - Phone number (0-999, or -1 for not configured)
 * - Wi-Fi SSID
 * - Wi-Fi password
 * 
 * The configuration is stored in /config.json on the ESP32's flash filesystem.
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <Arduino.h>

// Configuration structure
struct PhoneConfig {
  int phoneNumber;       // This phone's number (-1 = not configured)
  String wifiSsid;       // Wi-Fi network name
  String wifiPassword;   // Wi-Fi password
};

// Initialize configuration system
void setupConfiguration();

// Load configuration from flash
bool loadConfiguration(PhoneConfig& config);

// Save configuration to flash
bool saveConfiguration(const PhoneConfig& config);

// Get the current phone number (for use by other modules)
int getPhoneNumber();

// First-time setup mode
void runSetupMode(PhoneConfig& config);

#endif // CONFIGURATION_H
