/*
    Kasey the Kinderbot Cartridge Bus

    Data bus:
        D0-D7 -> pins 22-29 (PORTA)

    Control:
        C0 -> pin 2 (INT4)
        C1 -> pin 3 (INT5)

    Protocol:
        C0 rises first  => Console sending
        C1 rises second => ACK, capture byte

        C1 rises first  => Cartridge sending
        C0 rises second => ACK, capture byte
*/

#include "Arduino.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#define BUFFER_SIZE 1024

enum Direction : uint8_t
{
    DIR_CONSOLE_TO_CART = 0,
    DIR_CART_TO_CONSOLE = 1
};

enum State : uint8_t
{
    STATE_IDLE,
    STATE_CONSOLE_SENDING,
    STATE_CART_SENDING
};

struct Packet
{
    uint8_t data;
    uint8_t dir;
};

volatile Packet buffer[BUFFER_SIZE];

volatile uint16_t head = 0;
volatile uint16_t tail = 0;

volatile State state = STATE_IDLE;

volatile uint8_t c0_state = 0;
volatile uint8_t c1_state = 0;

static inline void pushPacket(uint8_t data, uint8_t dir) {
    uint16_t next = (head + 1) & (BUFFER_SIZE - 1);

    if (next == tail)
        return;     // overflow

    buffer[head].data = data;
    buffer[head].dir  = dir;

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
                state = STATE_CONSOLE_SENDING;
                break;

            case STATE_CART_SENDING:
                // ACK from console
                pushPacket(
                    PINA,
                    DIR_CART_TO_CONSOLE
                );
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
                state = STATE_CART_SENDING;
                break;

            case STATE_CONSOLE_SENDING:
                // ACK from cartridge
                pushPacket(
                    PINA,
                    DIR_CONSOLE_TO_CART
                );
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
    while (tail != head)
    {
        Packet pkt;

        noInterrupts();
        pkt.data = buffer[tail].data;
        pkt.dir  = buffer[tail].dir;
        tail = (tail + 1) & (BUFFER_SIZE - 1);
        interrupts();

        if (pkt.dir == DIR_CONSOLE_TO_CART)
            Serial.print("-> ");
        else
            Serial.print("<- ");

        if (pkt.data < 16)
            Serial.print('0');

        Serial.println(pkt.data, HEX);
    }
}