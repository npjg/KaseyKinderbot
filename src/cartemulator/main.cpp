/*
    Kasey the Kinderbot Cartridge Bus - Cartridge Writer Prototype

    Data bus:
        D0-D7 -> pins 22-29 (PORTA)

    Control:
        C0 -> pin 2 (INT4 / PE4)  [driven by console]
        C1 -> pin 3 (INT5 / PE5)  [driven by cartridge]

    Simplified rule set:
        If console sends 0x10, cartridge responds with 0x01.
        If console sends 0x49, cartridge responds with 0x09.
        Otherwise, cartridge does not transmit.

    Cartridge transmit timing constraints:
        - Data must settle >= 5 us before C1 goes high.
        - Data must remain on the bus >= 24 us total.
        - C1 must go low >= 5 us after console acknowledges by taking C0 high.
*/

#include "Arduino.h"
#include <avr/io.h>

static constexpr uint8_t CONSOLE_SEND_TRIGGER_BYTE_1 = 0x10;
static constexpr uint8_t CARTRIDGE_RESPONSE_BYTE_1 = 0x01;

static constexpr uint8_t CONSOLE_SEND_TRIGGER_BYTE_2 = 0x49;
static constexpr uint8_t CARTRIDGE_RESPONSE_BYTE_2 = 0x07;

static constexpr uint16_t DATA_SETTLE_BEFORE_C1_HIGH_US = 5;
static constexpr uint16_t DATA_HOLD_TOTAL_US = 24;
static constexpr uint16_t C1_LOW_AFTER_CONSOLE_ACK_US = 5;
static constexpr uint16_t CONTROL_LINE_TIMEOUT_US = 200;

static inline bool isConsoleC0High() {
    return (PINE & _BV(PE4)) != 0;
}

static inline void driveC1Low() {
    PORTE &= ~_BV(PE5);
}

static inline void driveC1High() {
    PORTE |= _BV(PE5);
}

static inline void configureDataBusAsInput() {
    DDRA = 0x00;
    PORTA = 0x00;
}

static inline void driveDataBus(uint8_t dataByte) {
    DDRA = 0xFF;
    PORTA = dataByte;
}

static bool waitForConsoleC0Level(bool expectedHighLevel) {
    const uint32_t waitStartTimestampUs = micros();

    while (isConsoleC0High() != expectedHighLevel) {
        const uint32_t elapsedWaitDurationUs = micros() - waitStartTimestampUs;
        if (elapsedWaitDurationUs >= CONTROL_LINE_TIMEOUT_US) {
            return false;
        }
    }

    return true;
}

static void acknowledgeConsoleSend() {
    driveC1High();

    const bool hasConsoleReleasedC0 = waitForConsoleC0Level(false);

    if (!hasConsoleReleasedC0) {
        // If edge timing was missed, return C1 to idle and continue.
        driveC1Low();
        return;
    }

    driveC1Low();
}

static bool tryReceiveConsoleByte(uint8_t &receivedByte) {
    static bool wasConsoleC0High = false;

    const bool isConsoleC0CurrentlyHigh = isConsoleC0High();
    const bool hasConsoleRaisedC0 = isConsoleC0CurrentlyHigh && !wasConsoleC0High;

    if (!hasConsoleRaisedC0) {
        wasConsoleC0High = isConsoleC0CurrentlyHigh;
        return false;
    }

    receivedByte = PINA;
    acknowledgeConsoleSend();
    wasConsoleC0High = false;
    return true;
}

static void sendCartridgeByte(uint8_t responseByte) {
    driveDataBus(responseByte);
    const uint32_t dataDriveStartTimestampUs = micros();

    delayMicroseconds(DATA_SETTLE_BEFORE_C1_HIGH_US);
    driveC1High();

    const bool hasConsoleAcknowledged = waitForConsoleC0Level(true);
    if (!hasConsoleAcknowledged) {
        driveC1Low();
        configureDataBusAsInput();
        return;
    }

    delayMicroseconds(C1_LOW_AFTER_CONSOLE_ACK_US);
    driveC1Low();

    const bool hasConsoleReleasedC0 = waitForConsoleC0Level(false);
    if (!hasConsoleReleasedC0) {
        configureDataBusAsInput();
        return;
    }

    const uint32_t elapsedDataDriveDurationUs = micros() - dataDriveStartTimestampUs;
    if (elapsedDataDriveDurationUs < DATA_HOLD_TOTAL_US) {
        delayMicroseconds(DATA_HOLD_TOTAL_US - elapsedDataDriveDurationUs);
    }

    configureDataBusAsInput();
}

void setup() {
    Serial.begin(2000000);

    configureDataBusAsInput();

    // C0 input from console.
    DDRE &= ~_BV(PE4);
    PORTE &= ~_BV(PE4);

    // C1 output from cartridge, idle low.
    DDRE |= _BV(PE5);
    driveC1Low();

    Serial.println(F("Cartridge writer ready (rules: 10->01, 49->09)"));
}

void loop() {
    uint8_t receivedByte;
    const bool hasReceivedConsoleByte = tryReceiveConsoleByte(receivedByte);

    if (!hasReceivedConsoleByte) {
        return;
    }

    uint8_t responseByte = 0;
    bool hasResponseRule = false;

    if (receivedByte == CONSOLE_SEND_TRIGGER_BYTE_1) {
        responseByte = CARTRIDGE_RESPONSE_BYTE_1;
        hasResponseRule = true;
    } else if (receivedByte == CONSOLE_SEND_TRIGGER_BYTE_2) {
        responseByte = CARTRIDGE_RESPONSE_BYTE_2;
        hasResponseRule = true;
    }

    if (hasResponseRule) {
        sendCartridgeByte(responseByte);

        Serial.print(F("rx "));
        if (receivedByte < 16) {
            Serial.print('0');
        }
        Serial.print(receivedByte, HEX);
        Serial.print(F(" -> tx "));
        if (responseByte < 16) {
            Serial.print('0');
        }
        Serial.println(responseByte, HEX);
        return;
    }

    Serial.print(F("rx "));
    if (receivedByte < 16) {
        Serial.print('0');
    }
    Serial.print(receivedByte, HEX);
    Serial.println(F(" -> N"));
}
