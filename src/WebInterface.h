/*
 * WebInterface.h - Web-based Debug Interface
 * 
 * Provides a simple web server for monitoring phone status and debugging.
 * Access via phone's IP address in a web browser.
 * 
 * Features:
 * - Current phone state
 * - Connected peer list
 * - Configuration info
 * - System stats (uptime, memory, etc.)
 * - Real-time status updates
 */

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

// Initialize web server on port 80
void setupWebInterface();

// Handle incoming web requests (call in loop)
void handleWebInterface();

#endif // WEB_INTERFACE_H
