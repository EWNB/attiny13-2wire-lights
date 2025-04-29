// Copyright Elliot Baptist.
// Licensed under the MIT License, see LICENSE for details.
// SPDX-License-Identifier: MIT

// tiny_fairy.c
// Use an ATTINY13 to drive a simple two-wire two-group string of fairy lights.

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// Clock prescaler mappings.
// Divisors 16, 32, 64, 128, & 256 have been omitted due to causing flickering.
#define CLKDIV_1 ((0<<CLKPS3) | (0<<CLKPS2) | (0<<CLKPS1) | (0<<CLKPS0))
#define CLKDIV_2 ((0<<CLKPS3) | (0<<CLKPS2) | (0<<CLKPS1) | (1<<CLKPS0))
#define CLKDIV_4 ((0<<CLKPS3) | (0<<CLKPS2) | (1<<CLKPS1) | (0<<CLKPS0))
#define CLKDIV_8 ((0<<CLKPS3) | (0<<CLKPS2) | (1<<CLKPS1) | (1<<CLKPS0))

// SYS_CLK_PRESCALE: System clock prescaler
// Increase for power savings. Decrease for shorter PWM cycles (less flicker).
// Will also affect FADE_PERIOD, which is specified in PWM cycles.
// - Units: prescaler divisor
// - Range: 1, 2, 4, 8, 16, 32, 64, 128, 256
#define SYS_CLK_PRESCALE CLKDIV_8

// PWM cycle period = 9.6 MHz / (SYS_CLK_PRESCALE * 510)

// FADE_PERIOD: Fade animation time period
// Increase value for slower LED fade animations. Decrease for faster.
// - Units: PWM cycles (hardware Timer/Counter 0 overflows)
// - Range: 1-255
#define FADE_PERIOD 64

uint8_t ucCounter;
uint8_t ucCompareA;
uint8_t ucCompareB;
uint8_t ucFadePos;
uint8_t ucFadeDir;
uint8_t ucMode;

void main() {
  ucCounter = FADE_PERIOD;
  ucCompareA = 0;
  ucCompareB = 0xFF;
  ucFadePos = 0;
  ucFadeDir = 0;

  // Set global clock prescaler to desired balance between power and flicker.
  // This two-step sequence must be followed or the write will have no effect.
  CLKPR = (1<<CLKPCE);      // step 1
  CLKPR = SYS_CLK_PRESCALE; // step 2
  // Disable ADC to save power
  PRR = (1<<PRADC);
  // Disable comparator to save power
  ACSR = (1<<ACD);
  // Timer/Counter 0 Control Register A
  // OC0A *set* on up-compare, OC0B *clear* on up-compare, fast PWM mode
  TCCR0A = ((1<<COM0A1) | (1<<COM0A0) | (1<<COM0B1) | (0<<COM0B0) |
            (1<<WGM01) | (1<<WGM00));
  // Timer/Counter 0 Control Register B
  // ATTINY13A default clock: 9.6 MHz int-osc & CKDIV8 -> 1.2 MHz
  // 1.2 MHz / (2 kHz * 510) = 1.18 -> no prescaler -> 2.353 kHz
  // No force output compare, 0xFF TOP, no additional clock prescaling
  TCCR0B = ((0<<FOC0B) | (0<<FOC0A) | (0<<WGM02) |
            (0<<CS02) | (0<<CS01) | (1<<CS00));
  // Inital compare values - LEDs off
  OCR0A = ucCompareA;
  OCR0B = ucCompareB;
  // Set PMW pins OC0A + OC0B as outputs, other pins as inputs
  DDRB = (1<<DDB1) | (1<<DDB0);
  // // Enable pull-ups on PB3 + PB4
  // PORTB = (1<<PORTB4) | (1<<PORTB3);
  // Enable timer overflow interrupt
  TIMSK0 = (1<<TOIE0);

  while (1) {
    if (ucCounter == 0) {
      // Check mode select inputs
      ucMode = (PINB >> 3) & 0b11;

      // Update outputs
      if (ucMode == 0b00) {
        // Static
        ucCompareA = 0x80;
        ucCompareB = 0x80;
      } else {
        // Dynamic
        if (ucMode == 0b01) {
          // Pulse together
          ucCompareA =        (ucFadePos>>1);
          ucCompareB = 0xFF - (ucFadePos>>1);
        } else if (ucMode == 0b10) {
          // Alternate - full
          ucCompareA = ucFadePos;
          ucCompareB = ucFadePos;
        } else /*ucMode == 0b11*/ {
          // Alternate - pulse
          uint8_t val = ucFadePos<<1;
          if (ucFadeDir == 1) {
            ucCompareA = (ucFadePos < 0x80) ? val : 0xFF - val;
            ucCompareB = 0xFF;
          } else {
            ucCompareA = 0x00;
            ucCompareB = (ucFadePos < 0x80) ? 0xFF - val : val;
          }
        }
      }
      // Update fade cycle (0x00 -> 0xFF -> 0x00 -> ...)
      ucFadePos = ucFadeDir ? ucFadePos + 1 : ucFadePos - 1;
      ucFadeDir = (ucFadeDir || ucFadePos == 0) && ucFadePos != 0xFF;
    }

    // https://www.nongnu.org/avr-libc/user-manual/group__avr__sleep.html
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();
    sei();
    sleep_cpu();
    // ... processor sleeps until awoken by timer interrupt ...
    sleep_disable();
    cli();
  }
}

ISR(TIM0_OVF_vect) {
  // Update timer compare values if enough
  if (ucCounter == 0) {
    OCR0A = ucCompareA;
    OCR0B = ucCompareB;
    ucCounter = FADE_PERIOD;
  }
  ucCounter--;
}
