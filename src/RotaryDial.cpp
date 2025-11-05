/*
 * Rotary Dial - Pulse Counting and Digit Recognition
 * 
 * Decodes the mechanical rotary dial into digits.
 * 
 * How Rotary Dials Work:
 * - User rotates dial to desired number and releases
 * - ROTARY_ACTIVE goes LOW when dial is rotating, HIGH when at rest
 * - ROTARY_PULSE generates pulses (LOW pulses) as dial returns to rest
 * - Number of pulses = digit dialed (1 pulse = 1, 10 pulses = 0)
 * 
 * Timing:
 * - Pulse debounce: 10ms (ignore bounces shorter than this)
 * - Digit timeout: 150ms (digit complete 150ms after last pulse)
 * 
 * Example: Dialing "5"
 * 1. User rotates dial to 5 and releases
 * 2. ROTARY_ACTIVE goes LOW (dialing starts)
 * 3. Dial returns, generating 5 pulses on ROTARY_PULSE
 * 4. ROTARY_ACTIVE goes HIGH (dialing ends)
 * 5. After 150ms, digit "5" is ready
 */

#include "RotaryDial.h"
#include "Pins.h"
#include <Arduino.h>

// Single digit state (low-level)
volatile int pulseCount = 0;
volatile bool isDialing = false;
volatile bool digitReady = false;
int lastDialedDigit = -1;

// Debouncing and timing
int lastPulseState = HIGH;
int lastActiveState = HIGH;
unsigned long lastPulseTime = 0;
unsigned long dialStartTime = 0;
const unsigned long PULSE_DEBOUNCE = 10; // 10ms debounce for pulse pin
const unsigned long DIGIT_TIMEOUT = 150; // 150ms after last pulse to consider digit complete

// Multi-digit collection state (high-level)
String collectedNumber = "";           // Complete phone number being dialed
unsigned long lastDigitCollectedTime = 0;
bool isCollectingNumber = false;
const unsigned long DIAL_COMPLETE_TIMEOUT = 3000; // 3 seconds after last digit = complete
const int MAX_DIGITS = 3;                         // Maximum digits in a phone number

/*
 * Setup Rotary Dial
 * Configure both rotary dial pins with internal pull-up resistors
 */
void setupRotaryDial() {
    pinMode(ROTARY_PULSE_PIN, INPUT_PULLUP);
    pinMode(ROTARY_ACTIVE_PIN, INPUT_PULLUP);
}

/*
 * Handle Rotary Dial
 * 
 * Called continuously from main loop to monitor dial state and count pulses.
 * 
 * Process:
 * 1. Detect dial start (ROTARY_ACTIVE goes LOW)
 * 2. Count falling edges on ROTARY_PULSE while dialing
 * 3. Detect dial end (ROTARY_ACTIVE goes HIGH)
 * 4. After 150ms timeout, convert pulse count to digit
 * 
 * Special Cases:
 * - 10 pulses = digit 0 (zero is at the end of the dial)
 * - Pulses are debounced with 10ms minimum spacing
 */
void handleRotaryDial() {
    int activeState = digitalRead(ROTARY_ACTIVE_PIN);
    int pulseState = digitalRead(ROTARY_PULSE_PIN);
    unsigned long currentTime = millis();

    // Detect when dialing starts (ROTARY_ACTIVE goes LOW)
    if (activeState == LOW && lastActiveState == HIGH) {
        isDialing = true;
        pulseCount = 0;
        digitReady = false;
        dialStartTime = currentTime;
        Serial.println("Dial started");
    }

    // Detect when dialing ends (ROTARY_ACTIVE goes HIGH)
    if (activeState == HIGH && lastActiveState == LOW) {
        isDialing = false;
        Serial.print("Dial ended. Pulse count: ");
        Serial.println(pulseCount);
    }

    // Count pulses while dialing
    if (isDialing) {
        // Detect falling edge on pulse pin (pulse start)
        if (pulseState == LOW && lastPulseState == HIGH) {
            if (currentTime - lastPulseTime > PULSE_DEBOUNCE) {
                pulseCount++;
                lastPulseTime = currentTime;
            }
        }
    }

    // Check if digit is complete (dial has stopped and enough time has passed)
    if (!isDialing && pulseCount > 0 && !digitReady) {
        if (currentTime - lastPulseTime > DIGIT_TIMEOUT) {
            // Convert pulse count to digit (0 = 10 pulses)
            lastDialedDigit = (pulseCount == 10) ? 0 : pulseCount;
            digitReady = true;
            pulseCount = 0;
            
            Serial.print("Digit dialed: ");
            Serial.println(lastDialedDigit);
        }
    }

    lastPulseState = pulseState;
    lastActiveState = activeState;
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
