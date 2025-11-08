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
volatile unsigned long lastPulseTime = 0;
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
    unsigned long now = millis();
    
    // Debounce
    static unsigned long lastPulseDebounce = 0;
    if (now - lastPulseDebounce < PULSE_DEBOUNCE_MS) {
        return;
    }
    
    bool currentPulseState = digitalRead(ROTARY_PULSE_PIN);
    if (currentPulseState != lastPulseState) {
        lastPulseDebounce = now;
        
        // Count on HIGH transitions (like working Arduino sketch)
        if (isDialing && currentPulseState == HIGH) {
            pulseCount++;
            lastPulseTime = now;
            dialingTimeout = now;  // Reset timeout on each pulse
        }
        
        lastPulseState = currentPulseState;
    }
}

void IRAM_ATTR onDialInterrupt() {
    unsigned long now = millis();
    
    // Debounce
    static unsigned long lastDialDebounce = 0;
    if (now - lastDialDebounce < DIAL_DEBOUNCE_MS) {
        return;
    }
    
    bool currentDialState = digitalRead(ROTARY_ACTIVE_PIN);
    if (currentDialState != lastDialState) {
        lastDialDebounce = now;
        
        // Start dialing when shunt goes LOW
        if (!isDialing && currentDialState == LOW) {
            isDialing = true;
            pulseCount = 0;
            digitReady = false;
            dialingTimeout = now;
            // Remove Serial.println from ISR - causes watchdog timeout
        }
        // End dialing when shunt goes HIGH (dial returned to rest)
        else if (isDialing && currentDialState == HIGH) {
            isDialing = false;
            
            // Process the digit immediately when dial returns to rest
            if (pulseCount > 0) {
                digitReady = true;
                // Convert pulse count to digit (10 pulses = 0)
                lastDialedDigit = (pulseCount == 10) ? 0 : pulseCount;
                // Remove Serial.println from ISR - causes watchdog timeout
            }
        }
        
        lastDialState = currentDialState;
    }
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
    
    // Show initial switch states for debugging
    Serial.println("Initial rotary dial switch states:");
    Serial.print("  Pulse switch (GPIO 15): ");
    Serial.println(digitalRead(ROTARY_PULSE_PIN) ? "HIGH" : "LOW");
    Serial.print("  Shunt switch (GPIO 14): ");
    Serial.println(digitalRead(ROTARY_ACTIVE_PIN) ? "HIGH" : "LOW");
}

/*
 * Handle Rotary Dial
 * 
 * Called continuously from main loop. With interrupt-driven detection,
 * this function provides visual feedback and safety timeout.
 * 
 * The actual pulse counting and digit completion is handled by interrupts
 * for maximum reliability and real-time response.
 */
void handleRotaryDial() {
    unsigned long now = millis();
    
    // Handle dial state messages (moved from ISR to avoid watchdog timeout)
    static bool lastReportedDialing = false;
    static bool lastReportedDigitReady = false;
    
    if (isDialing && !lastReportedDialing) {
        Serial.println("[Dial started turning]");
        lastReportedDialing = true;
    }
    
    if (!isDialing && lastReportedDialing) {
        Serial.println("[Dial returned to rest]");
        lastReportedDialing = false;
    }
    
    if (digitReady && !lastReportedDigitReady) {
        Serial.print("✓ Digit dialed: ");
        Serial.print(lastDialedDigit);
        Serial.print(" (");
        Serial.print(pulseCount);
        Serial.println(" pulses)");
        lastReportedDigitReady = true;
    }
    
    if (!digitReady) {
        lastReportedDigitReady = false;
    }
    
    // Handle pulse display (show dots for visual feedback)
    static int lastDisplayedCount = 0;
    if (isDialing && pulseCount > lastDisplayedCount) {
        Serial.print(".");
        Serial.print("[");
        Serial.print(pulseCount);
        Serial.print("]");
        lastDisplayedCount = pulseCount;
    }
    
    // Reset display counter when not dialing
    if (!isDialing) {
        lastDisplayedCount = 0;
    }
    
    // Safety timeout check - if we've been dialing too long, force completion
    if (isDialing && (now - dialingTimeout) > (SAFETY_TIMEOUT_MS * 2)) {
        // Safety timeout reached - something went wrong
        isDialing = false;
        
        Serial.println("\n[Safety timeout - dial may be stuck]");
        
        if (pulseCount > 0) {
            digitReady = true;
            lastDialedDigit = (pulseCount == 10) ? 0 : pulseCount;
            
            Serial.print("✓ Digit dialed: ");
            Serial.print(lastDialedDigit);
            Serial.print(" (");
            Serial.print(pulseCount);
            Serial.println(" pulses)");
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
