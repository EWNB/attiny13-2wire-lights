// Copyright EWNB.
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
// - Range: 2-255
#define FADE_PERIOD 64

// EEPROM addresses
#define FLAG_ADDR 0
#define MODE_ADDR 1

enum mode {MODE_0, MODE_1, MODE_2, MODE_3, MAX_MODE};

uint8_t ucCounter;
uint8_t ucCompareA;
uint8_t ucCompareB;
uint8_t ucFadePos;
uint8_t ucFadeDir;
uint8_t ucMode;
uint8_t ucModeFixed;
void (*pfModeFunc)(uint8_t, uint8_t);

// Various different fade animations in the form of functions.
// Inputs (explicit): ucFadeDir, ucFadePos
// Outputs (implicit): ucCompareA, ucCompareB
void fadeStatic(uint8_t dir, uint8_t pos) {
  ucCompareA = 0x80;
  ucCompareB = 0x80;
}
void fadePulseTogether(uint8_t dir, uint8_t pos) {
  ucCompareA =        (pos>>1);
  ucCompareB = 0xFF - (pos>>1);
}
void fadeAlternateFull(uint8_t dir, uint8_t pos) {
  ucCompareA = pos;
  ucCompareB = pos;
}
void fadeAlternatePulse(uint8_t dir, uint8_t pos) {
  uint8_t val = pos<<1;
  if (dir == 1) {
    ucCompareA = (pos < 0x80) ? val : 0xFF - val;
    ucCompareB = 0xFF;
  } else {
    ucCompareA = 0x00;
    ucCompareB = (pos < 0x80) ? 0xFF - val : val;
  }
}

// Based on code snippet in Microchip ATtiny13A Datasheet (DS40002307A)
uint8_t eeprom_read(uint8_t ucAddress) {
  // Wait for completion of previous write
  while (EECR & (1<<EEPE)) { ; }
  // Set up address register
  EEARL = ucAddress;
  // Start EEPROM read by writing EERE
  EECR |= (1<<EERE);
  // Return data from data register
  return EEDR;
}

// Based on code snippet in Microchip ATtiny13A Datasheet (DS40002307A)
void eeprom_write(uint8_t ucAddress, uint8_t ucData) {
  // Wait for completion of previous write
  while (EECR & (1<<EEPE)) { ; }
  // Set Programming Mode to erase and write (atomic)
  EECR = (0<<EEPM1) | (0<<EEPM0);
  // Set up address and data registers
  EEARL = ucAddress;
  EEDR = ucData;
  // Write logical one to EEMPE
  EECR |= (1<<EEMPE);
  // Start EEPROM write by setting EEPE
  EECR |= (1<<EEPE);
}

void main() {
  ucCounter = FADE_PERIOD;
  ucCompareA = 0;
  ucCompareB = 0xFF;
  ucFadePos = 0;
  ucFadeDir = 1;
  ucModeFixed = 0;

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
  // Enable timer overflow interrupt
  TIMSK0 = (1<<TOIE0);

  // Fade mode - EEPROM reads
  ucMode = eeprom_read(MODE_ADDR);
  if (eeprom_read(FLAG_ADDR) == 1) {
    if (++ucMode >= MAX_MODE) ucMode = MODE_0;
  }

  if (ucMode == MODE_0) {
    pfModeFunc = fadeStatic;
  } else if (ucMode == MODE_1) {
    pfModeFunc = fadePulseTogether;
  } else if (ucMode == MODE_2) {
    pfModeFunc = fadeAlternateFull;
  } else /*ucMode == MODE_3*/ {
    pfModeFunc = fadeAlternatePulse;
  }

  while (1) {
    if (ucCounter == 1 && ucModeFixed == 0) {
      // Fade mode - EEPROM writes (can take time)
      if (ucFadePos == 0x08) {
        // Start of mode increment period
        eeprom_write(FLAG_ADDR, 1);
        eeprom_write(MODE_ADDR, ucMode);
      } else if (ucFadePos == 0xFF) {
        // End of mode increment period
        eeprom_write(FLAG_ADDR, 0);
        ucModeFixed = 1;
        if (ucMode == MODE_0) TIMSK0 = (0<<TOIE0); // never wake from sleep
      }
    } else if (ucCounter == 0) {
      // Fade animation - calculate next PWM values
      pfModeFunc(ucFadeDir, ucFadePos);
      // Fade animation - advance position (0x00 -> 0xFF -> 0x00 -> ...)
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
