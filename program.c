#include <avr/io.h>
#include <avr/sfr_defs.h>
#define F_CPU 8000000UL
#define BAUD 9600
#include <util/setbaud.h>

void usart_init() {
  UBRRH = UBRRH_VALUE;
  UBRRL = UBRRL_VALUE;
#if USE_2X
  UCSRA |= _BV(U2X);
#else
  UCSRA &= ~_BV(U2X);
#endif
  UCSRB = _BV(RXEN) | _BV(TXEN);
  UCSRC = _BV(UCSZ1) | _BV(UCSZ0);
}

void usart_write(char c) {
  loop_until_bit_is_set(UCSRA, UDRE);
  UDR = c;
}

char usart_read() {
  loop_until_bit_is_set(UCSRA, RXC);
  return UDR;
}

void wait1() {
  int times;
  for (times = 0; times < 100; ++times) {
    unsigned int i = 1;
    while (i != 0) ++i;
  }
}

void init_shift() {
  // PA0 is clock, PD5 is data
  DDRA |= _BV(DDRA0);
  DDRD |= _BV(DDD5);
}

void shift(char byte) {
  int bit;
  for (bit = 0; bit < 8; ++bit) {
    PORTA &= ~_BV(PORTA0);
    if (byte & 0x80) {
      PORTD |= _BV(PORTD5);
    } else {
      PORTD &= ~_BV(PORTD5);
    }
    PORTA |= _BV(PORTA0);
    byte <<= 1;
  }
  // ♪♫ One last shift, just like the good old times. ♪♫
  // Clocking shifts the /last/ state of the shift register into the output
  // register. We'll need to actually shift a bit here if we want cleverness.
  PORTA &= ~_BV(PORTA0);
  PORTA |= _BV(PORTA0);
}

int main(void) {
  usart_init();
  init_shift();
  // Set up the LED
  DDRB |= _BV(DDB0);
  PORTB |= _BV(PB0);
  while (1) {
    char c = usart_read();
    shift(c);
    if (c & 1) {
      PORTB &= ~_BV(PB0);
    } else {
      PORTB |= _BV(PB0);
    }
    usart_write(c);
  }
}
