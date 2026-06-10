/*
    Kasey the Kinderbot Cartridge Bus

    This seems to be a pure data bus, rather than a data/address bus.
    The console seems to send commands and get responses rather than
    fetching instructions and executing them.

    Data lines:
        D0-D7 -> pins 22-29 (PORTA). Idle high.

    Control:
        C0 -> pin 2 (INT4). Idle low.
        C1 -> pin 3 (INT5). Idle low.

    Protocol:
        C0 rises first  => Console sending, capture byte
        C1 rises second => ACK from cart

        C1 rises first  => Cart sending, capture byte
        C0 rises second => ACK from console
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

// We store captured packets in a ring buffer because
// even a 2 Mbaud serial connection is too slow to print
// all packets in real time. So we store a packet when we get
// an interrupt and print them in the main loop.
//
// This is a reasonable buffer size determined empirically. It
// doesn't seem to ever drop packets under normal bus conditions.
#define BUFFER_SIZE 1024
volatile Packet buffer[BUFFER_SIZE];

volatile uint16_t head = 0;
volatile uint16_t tail = 0;

volatile uint32_t dropped_packet_count = 0;

volatile State state = STATE_IDLE;

volatile uint8_t c0_state = 0;
volatile uint8_t c1_state = 0;

// Read C0 from the ATmega2560 input register directly.
//
// Pin 2 on the Mega maps to PE4 (Port E, bit 4).
// PINE is the live input register for all Port E pins, and
// _BV(PE4) creates a bit mask with only bit 4 set.
//
// We use this instead of digitalRead(2) because this code runs in an ISR,
// where lower overhead gives more accurate timestamps and fewer dropped packets.
// TODO: See if this is truly necessary.
static inline uint8_t readControlLineC0() {
    return (PINE & _BV(PE4)) ? 1 : 0;
}

// Read C1 from pin 3, which maps to PE5 on ATmega2560.
static inline uint8_t readControlLineC1() {
    return (PINE & _BV(PE5)) ? 1 : 0;
}

static inline void pushPacket(uint8_t data, uint8_t dir) {
    uint16_t next = (head + 1) & (BUFFER_SIZE - 1);

    if (next == tail) {
        // Our buffer overflowed!
        dropped_packet_count++;
        return;
    }

    // Capture the byte and direction at interrupt time.
    // This keeps bus sampling in the ISR and serial printing in loop().
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
    c0_state = readControlLineC0();

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
    c1_state = readControlLineC1();

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
    // This is the highest baud rate that the ATmega can support,
    // so we will use it for the best throughput.
    Serial.begin(2000000);

    // This controls direction per Port A bit (0: input, 1: output).
    // We are only sniffing the data bus, so we want all the data lines to be inputs.
    DDRA = 0x00;
    // This controls output level when configured as output, or pull-up when input.
    // We are only sniffing the data bus, so disable internal pull-ups on all data lines.
    PORTA = 0x00;
    // Control lines should be high-impedance inputs since we are only sniffing the bus.
    pinMode(2, INPUT);
    pinMode(3, INPUT);

    // Attach external interrupts to both edges of C0 and C1, so we can detect which
    // control line rose first (start-of-byte direction) and detect when both lines
    // return low (back to idle state).
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

    // Write out all the packets in the buffer.
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