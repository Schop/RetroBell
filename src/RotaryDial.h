#ifndef ROTARY_DIAL_H
#define ROTARY_DIAL_H

// Single digit functions (low-level)
void setupRotaryDial();
void handleRotaryDial();
int getDialedDigit(); // Returns -1 if no digit is ready, otherwise 0-9
void clearDialedDigit();

// Multi-digit collection functions (high-level)
void startDialing();           // Start collecting a phone number
bool isDialingComplete();      // Check if complete number is ready (timeout or max digits)
bool hasStartedDialing();      // Check if user has dialed at least one digit
int getDialedNumber();         // Get the complete dialed number as integer
void resetDialedNumber();      // Clear for next call

#endif // ROTARY_DIAL_H
