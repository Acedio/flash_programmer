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

int main(void) {
  usart_init();
  DDRB |= _BV(DDB0);
  PORTB |= _BV(PB0);
  while (1) {
    char c = usart_read();
    if (c & 1) {
      PORTB &= ~_BV(PB0);
    } else {
      PORTB |= _BV(PB0);
    }
    usart_write(c);
  }
}
