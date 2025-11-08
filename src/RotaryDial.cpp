/*
 * Rotary Dial - Pulse Counting and Digit Recognition
 * 
 * Reliable rotary dial decoding using proven methods from testing.
 * 
 * How This Implementation Works:
 * - Uses interrupt-driven pulse detection for reliability
 * - Counts pulses on HIGH transitions (proven most reliable)
 * - Uses shunt switch for immediate completion detection
 * - Proper debouncing: 20ms pulse, 50ms shunt
 * 
 * Hardware:
 * - ROTARY_PULSE: Pulse switch (counts dial pulses)
 * - ROTARY_ACTIVE: Shunt/off-normal switch (detects dialing state)
 * 
 * Timing:
 * - Pulse debounce: 20ms (proven reliable)
 * - Shunt debounce: 50ms (prevents bounce issues)
 * - Safety timeout: 3 seconds (backup if shunt fails)
 * 
 * Example: Dialing "5"
 * 1. User rotates dial to 5 and releases
 * 2. ROTARY_ACTIVE goes LOW (dialing starts)
 * 3. Dial returns, generating 5 HIGH transitions on ROTARY_PULSE
 * 4. ROTARY_ACTIVE goes HIGH (dialing ends) → digit immediately ready
 * 5. Digit "5" is available instantly
 */

#include "RotaryDial.h"
#include "Pins.h"
#include <Arduino.h>

// Single digit state (interrupt-driven, proven reliable)
volatile int pulseCount = 0;
volatile bool isDialing = false;
volatile bool digitReady = false;
volatile unsigned long dialingTimeout = 0;
int lastDialedDigit = -1;

// State tracking for interrupts
volatile bool lastDialState = HIGH;
volatile bool lastPulseState = HIGH;

// Proven timing constants from testing
const unsigned long PULSE_DEBOUNCE_MS = 20;    // Pulse switch debounce
const unsigned long DIAL_DEBOUNCE_MS = 50;     // Shunt switch debounce
const unsigned long SAFETY_TIMEOUT_MS = 3000;  // Safety backup timeout

// Multi-digit collection state (high-level)
String collectedNumber = "";           // Complete phone number being dialed
unsigned long lastDigitCollectedTime = 0;
bool isCollectingNumber = false;
const unsigned long DIAL_COMPLETE_TIMEOUT = 3000; // 3 seconds after last digit = complete
const int MAX_DIGITS = 3;                         // Maximum digits in a phone number

// Interrupt handlers for proven reliable detection
void IRAM_ATTR onPulseInterrupt() {
    static unsigned long lastPulseTime = 0;
    unsigned long currentTime = millis();
    
    bool currentPulseState = digitalRead(ROTARY_PULSE_PIN);
    
    // Count on HIGH transitions (proven most reliable)
    if (currentPulseState == HIGH && lastPulseState == LOW) {
        if (currentTime - lastPulseTime > PULSE_DEBOUNCE_MS) {
            if (isDialing) {
                pulseCount++;
                lastPulseTime = currentTime;
            }
        }
    }
    lastPulseState = currentPulseState;
}

void IRAM_ATTR onDialInterrupt() {
    static unsigned long lastDialTime = 0;
    unsigned long currentTime = millis();
    
    bool currentDialState = digitalRead(ROTARY_ACTIVE_PIN);
    
    if (currentTime - lastDialTime > DIAL_DEBOUNCE_MS) {
        if (currentDialState == LOW && lastDialState == HIGH) {
            // Dialing started
            isDialing = true;
            pulseCount = 0;
            digitReady = false;
            dialingTimeout = currentTime + SAFETY_TIMEOUT_MS;
        }
        else if (currentDialState == HIGH && lastDialState == LOW) {
            // Dialing ended - digit immediately ready
            if (isDialing && pulseCount > 0) {
                isDialing = false;
                digitReady = true;
                // Convert pulse count to digit (0 = 10 pulses)
                lastDialedDigit = (pulseCount == 10) ? 0 : pulseCount;
            }
        }
        lastDialTime = currentTime;
    }
    lastDialState = currentDialState;
}

/*
 * Setup Rotary Dial
 * Configure pins and attach interrupts for reliable detection
 */
void setupRotaryDial() {
    pinMode(ROTARY_PULSE_PIN, INPUT_PULLUP);
    pinMode(ROTARY_ACTIVE_PIN, INPUT_PULLUP);
    
    // Initialize states
    lastPulseState = digitalRead(ROTARY_PULSE_PIN);
    lastDialState = digitalRead(ROTARY_ACTIVE_PIN);
    
    // Attach interrupts for real-time detection
    attachInterrupt(digitalPinToInterrupt(ROTARY_PULSE_PIN), onPulseInterrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ROTARY_ACTIVE_PIN), onDialInterrupt, CHANGE);
}

/*
 * Handle Rotary Dial
 * 
 * Called continuously from main loop. With interrupt-driven detection,
 * this function only needs to handle safety timeout checks.
 * 
 * The actual pulse counting and digit completion is handled by interrupts
 * for maximum reliability and real-time response.
 */
void handleRotaryDial() {
    unsigned long currentTime = millis();
    
    // Safety timeout check - if we've been dialing too long, force completion
    if (isDialing && currentTime > dialingTimeout) {
        if (pulseCount > 0) {
            isDialing = false;
            digitReady = true;
            lastDialedDigit = (pulseCount == 10) ? 0 : pulseCount;
            Serial.print("Safety timeout - Digit: ");
            Serial.println(lastDialedDigit);
        } else {
            // Reset if no pulses detected
            isDialing = false;
            pulseCount = 0;
        }
    }
}

/*
 * Get Dialed Digit
 * Returns the last dialed digit, or -1 if no digit is ready
 */
int getDialedDigit() {
    if (digitReady) {
        return lastDialedDigit;
    }
    return -1;
}

/*
 * Clear Dialed Digit
 * Marks the digit as consumed so it won't be read again
 */
void clearDialedDigit() {
    digitReady = false;
    lastDialedDigit = -1;
}

// ====== Multi-Digit Collection Functions ======

/*
 * Start Dialing
 * 
 * Initialize the multi-digit collection system.
 * Call this when transitioning to OFF_HOOK state.
 */
void startDialing() {
    collectedNumber = "";
    isCollectingNumber = true;
    lastDigitCollectedTime = millis();
    Serial.println("Started collecting phone number");
}

/*
 * Is Dialing Complete
 * 
 * Checks if the user has finished dialing a complete phone number.
 * 
 * Returns true when:
 * 1. Maximum digits reached (e.g., 3 digits for phone number)
 * 2. Timeout expired (3 seconds since last digit)
 * 
 * Called continuously from main loop to detect completion.
 */
bool isDialingComplete() {
    if (!isCollectingNumber) {
        return false;
    }
    
    // Check for new digit and add to collected number
    int digit = getDialedDigit();
    if (digit >= 0) {
        collectedNumber += String(digit);
        lastDigitCollectedTime = millis();
        clearDialedDigit();
        
        Serial.print("Dialed so far: ");
        Serial.println(collectedNumber);
        
        // Check if we've reached maximum digits
        if (collectedNumber.length() >= MAX_DIGITS) {
            Serial.print("Maximum digits reached. Complete number: ");
            Serial.println(collectedNumber);
            return true;
        }
    }
    
    // Check for timeout (user stopped dialing)
    if (collectedNumber.length() > 0) {
        unsigned long timeSinceLastDigit = millis() - lastDigitCollectedTime;
        if (timeSinceLastDigit >= DIAL_COMPLETE_TIMEOUT) {
            Serial.print("Dial timeout. Complete number: ");
            Serial.println(collectedNumber);
            return true;
        }
    }
    
    return false;
}

/*
 * Has Started Dialing
 * 
 * Returns true if at least one digit has been dialed.
 * Used to detect transition from OFF_HOOK to DIALING state.
 */
bool hasStartedDialing() {
    return isCollectingNumber && collectedNumber.length() > 0;
}

/*
 * Get Dialed Number
 * 
 * Returns the complete phone number that was dialed as an integer.
 * Should only be called after isDialingComplete() returns true.
 * 
 * Example: "102" → 102
 */
int getDialedNumber() {
    if (collectedNumber.length() == 0) {
        return -1;
    }
    return collectedNumber.toInt();
}

/*
 * Reset Dialed Number
 * 
 * Clears the collected phone number and stops collection.
 * Call this after the call is complete or when returning to IDLE.
 */
void resetDialedNumber() {
    collectedNumber = "";
    isCollectingNumber = false;
    lastDigitCollectedTime = 0;
    Serial.println("Dialed number reset");
}
