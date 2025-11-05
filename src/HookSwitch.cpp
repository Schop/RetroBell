/*
 * Hook Switch - Handset On/Off-Hook Detection
 * 
 * Monitors the hook switch to detect when the handset is lifted or replaced.
 * 
 * Hardware:
 * - Switch is normally closed (LOW) when handset is on the cradle
 * - Switch opens (HIGH) when handset is lifted
 * - Uses internal pull-up resistor
 * 
 * Features:
 * - 50ms debouncing to prevent false triggers
 * - State transitions: IDLE ↔ OFF_HOOK
 * - Answers incoming calls (RINGING → IN_CALL)
 * - Ends calls when hung up (IN_CALL/CALLING → IDLE)
 */

#include "HookSwitch.h"
#include "Pins.h"
#include "State.h"
#include "Network.h"
#include "RotaryDial.h"
#include <Arduino.h>

// Hook Switch Debouncing
int currentHookState = LOW; // LOW = on-hook, HIGH = off-hook
int lastHookState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // 50ms debounce time

/*
 * Setup Hook Switch
 * Configure the hook switch pin with internal pull-up resistor
 */
void setupHookSwitch() {
    pinMode(HOOK_SW_PIN, INPUT_PULLUP);
}

/*
 * Handle Hook Switch
 * 
 * Reads the hook switch state with debouncing and triggers state changes.
 * 
 * State Logic:
 * - Handset lifted (HIGH) while IDLE → Go OFF_HOOK (play dial tone)
 * - Handset lifted (HIGH) while RINGING → Answer call (go IN_CALL)
 * - Handset replaced (LOW) while IN_CALL or CALLING → Hang up (go IDLE)
 * 
 * Debouncing:
 * Only accepts a state change if the switch has been in the new state
 * for at least 50ms continuously. This prevents false triggers from
 * mechanical switch bounce.
 */
void handleHookSwitch() {
  int reading = digitalRead(HOOK_SW_PIN);

  // If the switch state has changed, reset the debounce timer
  if (reading != lastHookState) {
    lastDebounceTime = millis();
  }

  // After the debounce delay, if the state is stable and has changed, update it
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentHookState) {
      currentHookState = reading;

      // A HIGH reading means the handset is OFF the hook (switch is open)
      if (currentHookState == HIGH) {
        // Only transition to OFF_HOOK if we are currently IDLE
        if (getCurrentState() == IDLE) {
          changeState(OFF_HOOK);
          startDialing(); // Initialize the dialing system
        }
        // If we're RINGING and user picks up, answer the call
        else if (getCurrentState() == RINGING) {
          Serial.println("Answering incoming call");
          sendCallAccept(getCurrentCallPeer());
          changeState(IN_CALL);
        }
      }
      // A LOW reading means the handset is ON the hook (switch is closed)
      else {
        // Hanging up should end any active call and return to IDLE
        if (getCurrentState() == IN_CALL || getCurrentState() == CALLING) {
          Serial.println("Hanging up");
          sendCallEnd(getCurrentCallPeer());
        }
        changeState(IDLE);
      }
    }
  }

  lastHookState = reading;
}
