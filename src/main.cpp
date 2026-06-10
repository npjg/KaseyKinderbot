/*
    Kasey the Kinderbot Cartridge Bus

    Data bus:
        D0-D7 -> pins 22-29 (PORTA)

    Control:
        C0 -> pin 2 (INT4)
        C1 -> pin 3 (INT5)

    Protocol:
        C0 rises first  => Console sending, capture byte
        C1 rises second => ACK

        C1 rises first  => Cartridge sending, capture byte
        C0 rises second => ACK
*/

#include "Arduino.h"
#include <avr/io.h>
#include <avr/interrupt.h>

enum Direction : uint8_t {
    DIR_CONSOLE_TO_CART = 0,
    DIR_CART_TO_CONSOLE = 1
};

enum State : uint8_t {
    STATE_IDLE,
    STATE_CONSOLE_SENDING,
    STATE_CART_SENDING
};

struct Packet {
    uint8_t  data;
    uint8_t  dir;
    uint32_t timestamp_us;
};

// This is a reasonable buffer size that doesn't seem to ever drop packets
// under normal bus conditions.
#define BUFFER_SIZE 1024
volatile Packet buffer[BUFFER_SIZE];

volatile uint16_t head = 0;
volatile uint16_t tail = 0;

volatile uint32_t dropped_packet_count = 0;

volatile State state = STATE_IDLE;

volatile uint8_t c0_state = 0;
volatile uint8_t c1_state = 0;

static inline void pushPacket(uint8_t data, uint8_t dir) {
    uint16_t next = (head + 1) & (BUFFER_SIZE - 1);

    if (next == tail) {
        // Our buffer overflowed!
        dropped_packet_count++;
        return;
    }

    buffer[head].data         = data;
    buffer[head].dir          = dir;
    buffer[head].timestamp_us = micros();

    head = next;
}

static inline void checkForIdle() {
    if (!c0_state && !c1_state)
        state = STATE_IDLE;
}

void handleC0Change() {
    c0_state = (PINE & _BV(PE4)) ? 1 : 0;

    if (c0_state) {
        switch(state) {
            case STATE_IDLE:
                // C0 rising first indicates console is attempting to send.
                // Data should be valid once C0 rises.
                pushPacket(
                    PINA,
                    DIR_CONSOLE_TO_CART
                );
                state = STATE_CONSOLE_SENDING;
                break;

            case STATE_CART_SENDING:
                // ACK from cart.
                break;

            default:
                break;
        }
    } else {
        checkForIdle();
    }
}

void handleC1Change() {
    c1_state = (PINE & _BV(PE5)) ? 1 : 0;

    if (c1_state) {
        switch(state) {
            case STATE_IDLE:
                // C1 rising first indicates cartridge is attempting to send.
                // Data should be valid once C1 rises.
                pushPacket(
                    PINA,
                    DIR_CART_TO_CONSOLE
                );
                state = STATE_CART_SENDING;
                break;

            case STATE_CONSOLE_SENDING:
                // ACK from console.
                break;

            default:
                break;
        }
    } else {
        checkForIdle();
    }
}

void ISR_C0() {
    handleC0Change();
}

void ISR_C1() {
    handleC1Change();
}

void setup() {
    Serial.begin(2000000);

    // Data bus inputs
    DDRA = 0x00;
    PORTA = 0x00;

    // Control lines inputs
    pinMode(2, INPUT);
    pinMode(3, INPUT);

    attachInterrupt(
        digitalPinToInterrupt(2),
        ISR_C0,
        CHANGE
    );

    attachInterrupt(
        digitalPinToInterrupt(3),
        ISR_C1,
        CHANGE
    );

    Serial.println(F("Sniffer ready"));
}

void loop() {
    // Report drop count every few seconds so the display doesn't flood.
    // TODO: It might be better to only report this when something actually drops?
    static uint32_t last_report_ms = 0;
    uint32_t now_ms = millis();
    if (now_ms - last_report_ms >= 1000) {
        last_report_ms = now_ms;
        uint32_t snapshot;
        noInterrupts();
        snapshot = dropped_packet_count;
        interrupts();
        Serial.print(F("[dropped: "));
        Serial.print(snapshot);
        Serial.println(F("]"));
    }

    while (tail != head)
    {
        Packet pkt;

        // C++ will not implicitly discard volatile qualifiers via a struct
        // assignment operator, so we must copy each member explicitly.
        noInterrupts();
        pkt.data         = buffer[tail].data;
        pkt.dir          = buffer[tail].dir;
        pkt.timestamp_us = buffer[tail].timestamp_us;
        tail = (tail + 1) & (BUFFER_SIZE - 1);
        interrupts();

        // Print timestamp in microseconds, padded to 10 digits for alignment.
        char timestamp_str[11];
        snprintf(timestamp_str, sizeof(timestamp_str), "%10lu", pkt.timestamp_us);
        Serial.print(timestamp_str);
        Serial.print(" us  ");

        if (pkt.dir == DIR_CONSOLE_TO_CART)
            Serial.print("-> ");
        else
            Serial.print("<- ");

        if (pkt.data < 16)
            Serial.print('0');

        Serial.println(pkt.data, HEX);
    }
}