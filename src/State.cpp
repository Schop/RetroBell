/*
 * State - Phone State Machine Implementation
 * 
 * Manages the phone's state transitions and state variable.
 * 
 * The state machine coordinates all phone behaviors:
 * - IDLE: Waiting for activity
 * - OFF_HOOK: Handset lifted, ready to dial
 * - DIALING: Collecting dialed digits
 * - CALLING: Waiting for peer to answer
 * - RINGING: Receiving incoming call
 * - IN_CALL: Active voice call
 * 
 * State transitions are logged to serial for debugging.
 */

#include "State.h"
#include <Arduino.h>

// Current state of the phone (shared across modules)
PhoneState currentState = IDLE;

/*
 * Change Phone State
 * 
 * Transitions the phone to a new state and logs it to serial monitor.
 * This function is the single point of control for all state changes,
 * making it easy to debug state flow.
 * 
 * States: IDLE, OFF_HOOK, DIALING, CALLING, RINGING, IN_CALL
 * 
 * Called from:
 * - HookSwitch.cpp when handset is lifted/replaced
 * - Network.cpp when call requests are received
 * - main.cpp when dialing completes
 * 
 * Parameters:
 * - newState: The state to transition to
 */
void changeState(PhoneState newState) {
  if (newState == currentState) return; // No change needed
  
  currentState = newState;
  Serial.print("State changed to: ");
  switch (currentState) {
    case IDLE: Serial.println("IDLE"); break;
    case OFF_HOOK: Serial.println("OFF_HOOK"); break;
    case DIALING: Serial.println("DIALING"); break;
    case CALLING: Serial.println("CALLING"); break;
    case RINGING: Serial.println("RINGING"); break;
    case IN_CALL: Serial.println("IN_CALL"); break;
    case CALL_FAILED: Serial.println("CALL_FAILED"); break;
  }
}

/*
 * Get Current State
 * 
 * Returns the current phone state.
 * Used by modules that need to check state without extern variables.
 * 
 * Returns: Current PhoneState value
 */
PhoneState getCurrentState() {
  return currentState;
}
