/*
 * WebInterface - Web-based Debug Interface
 * 
 * Simple HTTP server that displays phone status and debug information.
 * Useful for monitoring system state, viewing peer list, and troubleshooting.
 */

#include "WebInterface.h"
#include "State.h"
#include "Configuration.h"
#include "Network.h"
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

// Web server instance on port 80
WebServer server(80);

/*
 * Get State Name as String
 * Converts PhoneState enum to human-readable string
 */
const char* getStateName(PhoneState state) {
  switch (state) {
    case IDLE: return "IDLE";
    case OFF_HOOK: return "OFF_HOOK";
    case DIALING: return "DIALING";
    case CALLING: return "CALLING";
    case RINGING: return "RINGING";
    case IN_CALL: return "IN_CALL";
    case CALL_FAILED: return "CALL_FAILED";
    default: return "UNKNOWN";
  }
}

/*
 * Root Page Handler
 * Displays main status page with phone info
 */
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='5'>"; // Auto-refresh every 5 seconds
  html += "<title>RetroBell Status</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; border-bottom: 2px solid #007bff; padding-bottom: 10px; }";
  html += "h2 { color: #555; margin-top: 20px; }";
  html += ".status { font-size: 24px; font-weight: bold; color: #007bff; padding: 10px; background: #e7f3ff; border-radius: 5px; }";
  html += ".info-row { display: flex; justify-content: space-between; padding: 8px; border-bottom: 1px solid #eee; }";
  html += ".label { font-weight: bold; color: #666; }";
  html += ".value { color: #333; }";
  html += ".peer { background: #f8f9fa; padding: 10px; margin: 5px 0; border-radius: 5px; border-left: 4px solid #28a745; }";
  html += ".footer { margin-top: 20px; text-align: center; color: #999; font-size: 12px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  
  // Header
  html += "<h1>🔔 RetroBell Status</h1>";
  html += "<p style='color: #666; font-style: italic;'>Who you gonna call?</p>";
  
  // Current State
  html += "<h2>Current State</h2>";
  html += "<div class='status'>" + String(getStateName(getCurrentState())) + "</div>";
  
  // Phone Configuration
  html += "<h2>Phone Configuration</h2>";
  html += "<div class='info-row'><span class='label'>Phone Number:</span><span class='value'>" + String(getPhoneNumber()) + "</span></div>";
  html += "<div class='info-row'><span class='label'>MAC Address:</span><span class='value'>" + WiFi.macAddress() + "</span></div>";
  html += "<div class='info-row'><span class='label'>IP Address:</span><span class='value'>" + WiFi.localIP().toString() + "</span></div>";
  html += "<div class='info-row'><span class='label'>WiFi SSID:</span><span class='value'>" + WiFi.SSID() + "</span></div>";
  html += "<div class='info-row'><span class='label'>WiFi Channel:</span><span class='value'>" + String(WiFi.channel()) + "</span></div>";
  html += "<div class='info-row'><span class='label'>Signal Strength:</span><span class='value'>" + String(WiFi.RSSI()) + " dBm</span></div>";
  
  // System Info
  html += "<h2>System Information</h2>";
  html += "<div class='info-row'><span class='label'>Uptime:</span><span class='value'>" + String(millis() / 1000) + " seconds</span></div>";
  html += "<div class='info-row'><span class='label'>Free Heap:</span><span class='value'>" + String(ESP.getFreeHeap() / 1024) + " KB</span></div>";
  html += "<div class='info-row'><span class='label'>CPU Frequency:</span><span class='value'>" + String(ESP.getCpuFreqMHz()) + " MHz</span></div>";
  html += "<div class='info-row'><span class='label'>Flash Size:</span><span class='value'>" + String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB</span></div>";
  
  // Call Info
  html += "<h2>Call Status</h2>";
  int callPeer = getCurrentCallPeer();
  if (callPeer >= 0) {
    html += "<div class='info-row'><span class='label'>Connected to:</span><span class='value'>Phone #" + String(callPeer) + "</span></div>";
  } else {
    html += "<div class='info-row'><span class='label'>Call Status:</span><span class='value'>No active call</span></div>";
  }
  
  // Discovered Peers
  html += "<h2>Discovered Peers</h2>";
  html += "<p style='color: #666; font-size: 14px;'>Phones discovered on the network:</p>";
  // Note: We'll need to add a function to get peer list from Network module
  html += "<p style='color: #999; font-style: italic;'>Peer list display coming soon...</p>";
  
  // Footer
  html += "<div class='footer'>";
  html += "Page auto-refreshes every 5 seconds<br>";
  html += "RetroBell &copy; 2025";
  html += "</div>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

/*
 * 404 Not Found Handler
 */
void handleNotFound() {
  String message = "404: Not Found\n\n";
  message += "URI: " + server.uri() + "\n";
  message += "Method: " + String((server.method() == HTTP_GET) ? "GET" : "POST") + "\n";
  server.send(404, "text/plain", message);
}

/*
 * Setup Web Interface
 * Initializes the web server and registers route handlers
 */
void setupWebInterface() {
  // Register route handlers
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  
  // Start the server
  server.begin();
  
  Serial.println("Web interface started!");
  Serial.print("Access at: http://");
  Serial.println(WiFi.localIP());
}

/*
 * Handle Web Interface
 * Process incoming HTTP requests
 * Must be called repeatedly in main loop
 */
void handleWebInterface() {
  server.handleClient();
}
