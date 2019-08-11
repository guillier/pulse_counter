/*
 * The MIT License (MIT)
 * Copyright (c) 2019 François GUILLIER <dev @ guillier . org>
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
// ATMEL ATTINY 85
//
//                   +-\/-+
// Reset (D 5) PB5  1|    |8  Vcc
//       (D 3) PB3  2|    |7  PB2 (D 2) PCINT2 
//       (D 4) PB4  3|    |6  PB1 (D 1) RX
//             GND  4|    |5  PB0 (D 0) TX
//                   +----+

#include <avr/sleep.h>    // Sleep Modes
#include <avr/power.h>    // Power management


const bool debug = true;
const byte SWITCH = 2;  // physical pin 7 / PCINT2
bool tick = true;
bool relative = true;
unsigned long next_sleep_ts;
bool new_value_from_serial = false;
uint32_t cnt;
uint32_t new_value;


void setup ()
{ 
    pinMode(SWITCH, INPUT_PULLUP);  // Internal pull-up
    bitSet(PCMSK, PCINT2);  // Counting & waking-up
    bitSet(PCMSK, PCINT1);  // Waking-up
    bitSet(GIMSK, PCIE);    // General Interrupt

    Serial.begin(1200);  // Slow Software Serial
    if (debug)
        Serial.println("#Starting");  // Debug
    cnt = 0;
    next_sleep_ts = millis();
}  // end of setup

// Interrupt Vector (both for counting & waking-up)
ISR(PCINT0_vect)
{
}

void send_value_serial()
{
    // Software serial means the main loop is blocked until all characters are sent
    // which is a bonus in our case
    Serial.print((relative)? "*" : "<");
    Serial.println(cnt);  
}


void go_to_sleep ()
{
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    bitClear(ADCSRA, ADEN);  // turn off ADC
    power_all_disable();
    sleep_enable();
    sleep_cpu();
    // Zzzzzz

    // Back in business
    sleep_disable();   
    power_all_enable();    // power everything back on
    if (debug)
        Serial.println("#Awake");
}


void loop ()
{
    if (millis() > next_sleep_ts)
    {
        if (debug)
            Serial.println("#Going to sleep");
        go_to_sleep();
        // Here ==> Woken-up either by counter or serial
        
        new_value_from_serial = false;
        if (Serial.available () > 0)  // If woken-up by serial, we grant 5s to send the whole value
            next_sleep_ts = millis() + 5000;
        else
            next_sleep_ts = millis() + 500;
    }

    if (Serial.available() > 0)
    {
        byte c = Serial.read();
        if (new_value_from_serial)
        {
            if (isDigit(c))
            {
                new_value *= 10;
                new_value += c - '0';
            }
            if (c == '\r' or c == '\n')
            {
                cnt = new_value;
                new_value_from_serial = false;
                relative = false;
                send_value_serial();
            }
        } else
            if (c == '>')
            {
                new_value_from_serial = true;
                new_value = 0;
            } else
                if (c == '?')
                    send_value_serial();
    } else
    {
        if ((PINB >> PINB2) & 1)
            tick = true;  // High level seen
        else
            if (tick)  // Counting at low level only if high level seen
            {
                tick = false;  // Low level dealt with
                cnt++;  // Counter
                send_value_serial();
            }
        delay(100);
    }
}  // end of loop
