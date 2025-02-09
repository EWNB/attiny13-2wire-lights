#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

const uint8_t OVERFLOW_COUNT_MASK = 0x3F;

volatile uint8_t overflow_count;
volatile uint8_t ocr0a_next;
volatile uint8_t ocr0b_next;
uint8_t fade_pos;
uint8_t fade_dir;
uint8_t mode;

void main() {
  cli();

  overflow_count = 0;
  ocr0a_next = 0;
  ocr0b_next = 0;
  fade_pos = 0;
  fade_dir = 0;
  mode = 0;

  // Disable ADC to save power
  PRR = 0b00000001;
  // Disable comparator to save power
  ACSR = 0b10000000;

  // Timer/Counter 0 Control Register A
  // OC0A set on up-compare, OC0B clear on up-compare, fast PWM
  TCCR0A = 0b11100011;

  // Timer/Counter 0 Control Register B
  // ATTINY13A default clock: 9.6 MHz int-osc & CKDIV8 -> 1.2 MHz
  // 1.2 MHz / (2 kHz * 510) = 1.18 -> no prescaler -> 2.353 kHz
  // No force output compare, 0xFF TOP, no prescaler
  TCCR0B = 0b00000001;

  // Inital compare values - LEDs off
  OCR0A = 0x00;
  OCR0B = 0xFF;

  // Set OC0A + OC0B as outputs, other pins as inputs
  DDRB = 0b00000011;

  // Enable pull-ups on PB3 + PB4
  // PORTB = 0b00011000;

  // Enable timer overflow interrupt
  TIMSK0 = 0b00000010;

  while (1) {
    if (!(overflow_count & OVERFLOW_COUNT_MASK)) {
      // Check mode select inputs
      mode = (PINB >> 3) & 0b11;

      // Update outputs
      if (mode == 0b00) {
        // Static
        ocr0a_next = 0x80;
        ocr0b_next = 0x80;
      } else {
        // Dynamic
        if (mode == 0b01) {
          // Pulse together
          ocr0a_next =        (fade_pos >> 1);
          ocr0b_next = 0xFF - (fade_pos >> 1);
        } else if (mode == 0b10) {
          // Alternate - full
          ocr0a_next = fade_pos;
          ocr0b_next = fade_pos;
        } else /*mode == 0b11*/ {
          // Alternate - pulse
          if (fade_dir == 1) {
            ocr0a_next = (fade_pos < 0x80) ?        (fade_pos << 1)
                                           : 0xFF - (fade_pos << 1);
            ocr0b_next = 0xFF;
          } else {
            ocr0a_next = 0x00;
            ocr0b_next = (fade_pos < 0x80) ? 0xFF - (fade_pos << 1)
                                           :        (fade_pos << 1);
          }
        }
      }
      // Update fade cycle (0x00 -> 0xFF -> 0x00 -> ...)
      fade_pos = fade_dir ? fade_pos + 1 : fade_pos - 1;
      fade_dir = (fade_dir || fade_pos == 0) && fade_pos != 0xFF;
    }

    // https://www.nongnu.org/avr-libc/user-manual/group__avr__sleep.html
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();
    // sleep_bod_disable();
    sei();
    sleep_cpu();
    sleep_disable();
    cli();
  }
}

ISR(TIM0_OVF_vect) {
  // Update timer compare values if count of timer overflows is divisible
  // by target (power-of-2) period.
  if (!(++overflow_count & OVERFLOW_COUNT_MASK)) {
    OCR0A = ocr0a_next;
    OCR0B = ocr0b_next;
  }
}
