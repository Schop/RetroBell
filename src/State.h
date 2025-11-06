/*
 * State.h - Phone State Machine Definition
 * 
 * Defines the possible states of the phone and provides state transition function.
 * 
 * State Flow:
 * 
 * IDLE → User lifts handset → OFF_HOOK → User dials → DIALING → Complete dial → CALLING
 *   ↑                                                                               ↓
 *   └─────────────────── Peer answers ────────────────────────────────────── IN_CALL
 * 
 * IDLE ← Incoming call ← RINGING → User lifts handset → IN_CALL
 * 
 * Any active state → User hangs up → IDLE
 * 
 * States:
 * - IDLE: Phone at rest, waiting for activity
 * - OFF_HOOK: Handset lifted, dial tone playing, ready to dial
 * - DIALING: Actively dialing a number with rotary dial
 * - CALLING: Dialed a complete number, ringing remote phone
 * - RINGING: Incoming call, playing ring tone
 * - IN_CALL: Connected call, audio streaming active
 */

#ifndef STATE_H
#define STATE_H

// Phone State Machine
enum PhoneState {
  IDLE,       // Phone idle, waiting for action
  OFF_HOOK,   // Handset lifted, dial tone playing
  DIALING,    // User is dialing a number
  CALLING,    // Waiting for peer to answer
  RINGING,    // Incoming call, ringing
  IN_CALL,    // Active call in progress
  CALL_FAILED,// Call failed (number not found, etc.)
  CALL_BUSY   // Called phone is busy (already in a call)
};

// Change phone state and log to serial
void changeState(PhoneState newState);

// Get current phone state
PhoneState getCurrentState();

#endif // STATE_H
