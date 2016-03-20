#include <avr/io.h>
#include <avr/sfr_defs.h>
#define F_CPU 8000000UL
#define BAUD   250000UL
#include <util/setbaud.h>

const char cmd_clear = 'c';
const char cmd_write = 'w';
const char cmd_dump = 'd';
const char* error_str = "error";
const char* ack_str = "eepeepack";
const char* cmd_str = "eepeepcmd";

#define SREG_CLK_DIR DDRA
#define SREG_CLK_PORT PORTA
#define SREG_CLK_BIT PORTA0
// Address pins 0-7 are controlled by the low shift register
#define LOW_SREG_DATA_DIR DDRD
#define LOW_SREG_DATA_PORT PORTD
#define LOW_SREG_DATA_BIT PORTD4
// Address pins 11-17 are controlled by the high shift register
#define HIGH_SREG_DATA_DIR DDRD
#define HIGH_SREG_DATA_PORT PORTD
#define HIGH_SREG_DATA_BIT PORTD5

// Address pins 8-10 are controlled by the microcontroller.
// pin 8 is PORTD4
#define ROM_A8_10_DIR DDRD
#define ROM_A8_10_PORT PORTD
#define ROM_A8_10_BITMASK (_BV(PORTD4) | _BV(PORTD5) | _BV(PORTD6))
#define ROM_A8_10_SHIFT PORTD4

#define ROM_DATA_DIR DDRB
#define ROM_DATA_PORT PORTB
#define ROM_DATA_PIN PINB

#define ROM_OUTPUT_ENABLE_DIR DDRD
#define ROM_OUTPUT_ENABLE_PORT PORTD
#define ROM_OUTPUT_ENABLE_BIT PORTD2
#define ROM_WRITE_ENABLE_DIR DDRD
#define ROM_WRITE_ENABLE_PORT PORTD
#define ROM_WRITE_ENABLE_BIT PORTD3

#define CHIP_SIZE 0x40000

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

void usart_write_str(const char* str) {
  if (!str) return;
  while (*str) {
    usart_write(*str);
    ++str;
  }
}

char usart_read() {
  loop_until_bit_is_set(UCSRA, RXC);
  return UDR;
}

void wait1() {
  int times;
  for (times = 0; times < 10; ++times) {
    unsigned int i = 1;
    while (i != 0) ++i;
  }
}

void init_pins() {
  SREG_CLK_DIR |= _BV(SREG_CLK_BIT);
  LOW_SREG_DATA_DIR |= _BV(LOW_SREG_DATA_BIT);
  HIGH_SREG_DATA_DIR |= _BV(HIGH_SREG_DATA_BIT);
  ROM_A8_10_DIR |= ROM_A8_10_BITMASK;
  ROM_OUTPUT_ENABLE_DIR |= _BV(ROM_OUTPUT_ENABLE_BIT);
  ROM_WRITE_ENABLE_DIR |= _BV(ROM_WRITE_ENABLE_BIT);
}

void shift(char high_byte, char low_byte) {
  int bit;
  for (bit = 0; bit < 8; ++bit) {
    // clk low
    SREG_CLK_PORT &= ~_BV(SREG_CLK_BIT);
    if (low_byte & 0x80) {
      LOW_SREG_DATA_PORT |= _BV(LOW_SREG_DATA_BIT);
    } else {
      LOW_SREG_DATA_PORT &= ~_BV(LOW_SREG_DATA_BIT);
    }
    if (high_byte & 0x80) {
      HIGH_SREG_DATA_PORT |= _BV(HIGH_SREG_DATA_BIT);
    } else {
      HIGH_SREG_DATA_PORT &= ~_BV(HIGH_SREG_DATA_BIT);
    }
    // clk high, data shifts out
    SREG_CLK_PORT |= _BV(SREG_CLK_BIT);
    low_byte <<= 1;
    high_byte <<= 1;
  }
  // ♪♫ One last shift, just like the good old times. ♪♫
  // Clocking shifts the /last/ state of the shift register into the output
  // register, so we shift a garbage bit here to get the full bytes out.
  // clk low
  SREG_CLK_PORT &= ~_BV(SREG_CLK_BIT);
  // clk high, all bits are output now
  SREG_CLK_PORT |= _BV(SREG_CLK_BIT);
}

// Clocks the shift registers once. The last bit shifted will be output and
// the MSB of low_next_byte will be loaded into low sreg.
// High sreg is kept at 0.
void shift_clock_with_next_low(char low_next_byte) {
  // clk low
  SREG_CLK_PORT &= ~_BV(SREG_CLK_BIT);
  if (low_next_byte & 0x80) {
    LOW_SREG_DATA_PORT |= _BV(LOW_SREG_DATA_BIT);
  } else {
    LOW_SREG_DATA_PORT &= ~_BV(LOW_SREG_DATA_BIT);
  }
  // High sreg is kept at 0.
  HIGH_SREG_DATA_PORT &= ~_BV(HIGH_SREG_DATA_BIT);
  // clk high, data shifts out
  SREG_CLK_PORT |= _BV(SREG_CLK_BIT);
}

// Shifts low_byte into the low shift register.
// The MSB of low_next_byte is shifted into the low sreg and will be shifted
// out on the next clock. High sreg is kept at 0x00.
void shift_low_with_next_byte(char low_byte, char low_next_byte) {
  // High sreg is kept at 0.
  HIGH_SREG_DATA_PORT &= ~_BV(HIGH_SREG_DATA_BIT);

  int bit;
  for (bit = 0; bit < 8; ++bit) {
    // clk low
    SREG_CLK_PORT &= ~_BV(SREG_CLK_BIT);
    if (low_byte & 0x80) {
      LOW_SREG_DATA_PORT |= _BV(LOW_SREG_DATA_BIT);
    } else {
      LOW_SREG_DATA_PORT &= ~_BV(LOW_SREG_DATA_BIT);
    }
    // clk high, data shifts out
    SREG_CLK_PORT |= _BV(SREG_CLK_BIT);
    low_byte <<= 1;
  }
  // ♪♫ One last shift, just like the good old times. ♪♫
  // Clocking shifts the /last/ state of the shift register into the output
  // register. We shift the next bit here.
  shift_clock_with_next_low(low_next_byte);
}

// Sets address bits 8-10 to the lowest three bits of three_bits.
void set_a8_10(char three_bits) {
  ROM_A8_10_PORT |= (three_bits << ROM_A8_10_SHIFT) & ROM_A8_10_BITMASK;
  ROM_A8_10_PORT &= (three_bits << ROM_A8_10_SHIFT) | (~ROM_A8_10_BITMASK);
}

void set_address(long address) {
  char low_byte = address & 0xFF;
  char mid_three_bits = (address >> 8) & 0x07;
  // Could probably shift ROM_A8_10_SHIFT less here.
  char high_byte = (address >> 11) & 0xFF;

  shift(high_byte, low_byte);
  set_a8_10(mid_three_bits);
}

// All the flash commands involve writing bytes to inverting addresses (0x555
// and 0x2AA)
// This is a bit of a hack to speed up the write process by shifting only one
// address bit per instruction.
void shift_command(char command_byte) {
  // Disable output and write (active low)
  ROM_OUTPUT_ENABLE_PORT |= _BV(ROM_OUTPUT_ENABLE_BIT);
  ROM_WRITE_ENABLE_PORT |= _BV(ROM_WRITE_ENABLE_BIT);
  // Set data register to output
  ROM_DATA_DIR = 0xFF;

  shift_low_with_next_byte(0x55, 0x00); // low sreg = 0x55, next bit = 0
  set_a8_10(0x05);
  ROM_DATA_PORT = 0xAA;
  ROM_WRITE_ENABLE_PORT &= ~_BV(ROM_WRITE_ENABLE_BIT);
  ROM_WRITE_ENABLE_PORT |= _BV(ROM_WRITE_ENABLE_BIT);

  shift_clock_with_next_low(0xFF);  // low sreg = 0xAA, next bit = 1
  set_a8_10(0x02);
  ROM_DATA_PORT = 0x55;
  ROM_WRITE_ENABLE_PORT &= ~_BV(ROM_WRITE_ENABLE_BIT);
  ROM_WRITE_ENABLE_PORT |= _BV(ROM_WRITE_ENABLE_BIT);

  shift_clock_with_next_low(0x00);  // low sreg = 0x55, next bit = 0
  set_a8_10(0x05);
  ROM_DATA_PORT = command_byte;
  ROM_WRITE_ENABLE_PORT &= ~_BV(ROM_WRITE_ENABLE_BIT);
  ROM_WRITE_ENABLE_PORT |= _BV(ROM_WRITE_ENABLE_BIT);
}

void write_at_address(long address, char byte) {
  char check;
  do {
    // This command already disables output and write and sets the data reg to
    // output.
    shift_command(0xA0);
    set_address(address);
    ROM_DATA_PORT = byte;
    ROM_WRITE_ENABLE_PORT &= ~_BV(ROM_WRITE_ENABLE_BIT);
    ROM_WRITE_ENABLE_PORT |= _BV(ROM_WRITE_ENABLE_BIT);
    ROM_DATA_DIR = 0x00;
    check = ROM_DATA_PIN;
    usart_write(check);
  } while (check != byte);
}

void chip_erase() {
  shift_command(0x80);
  // The second set of commands starts over at address 555
  shift_command(0x10);

  usart_write('.');
  wait1();
  usart_write('.');
  wait1();
  usart_write('.');
  wait1();
}

/*
char to_hex(unsigned char nibble) {
  if (nibble < 10) {
    return '0' + nibble;
  }
  return 'A' + nibble - 10;
}

void write_hex(unsigned char byte) {
  usart_write(to_hex(byte >> 4));
  usart_write(to_hex(byte & 0x0F));
}*/

void dump_rom() {
  // Disable write
  ROM_WRITE_ENABLE_PORT |= _BV(ROM_WRITE_ENABLE_BIT);
  // Enable output
  ROM_OUTPUT_ENABLE_PORT &= ~_BV(ROM_OUTPUT_ENABLE_BIT);
  // Set data to read
  ROM_DATA_DIR = 0x00;
  long address = 0;
  for (; address < CHIP_SIZE; ++address) {
    set_address(address);
    usart_write(ROM_DATA_PIN);
  }
}

void write_rom() {
  long address = 0;
  for (; address < CHIP_SIZE; ++address) {
    write_at_address(address, usart_read());
  }
}

int main(void) {
  usart_init();
  init_pins();

  //ROM_DATA_DIR = 0xFF;
  while (1) {
    // TODO: Check to see if OUTPUT_ENABLE and WRITE_ENABLE are working as intended.
    /*
    long address = 0x11111;
    char data = 0x11;
    for (int i = 0; i < 4; ++i) {
      set_address(address);
      ROM_DATA_PORT = data;
      wait1();
      address <<= 1;
      data <<= 1;
    }
    */
    char c = usart_read();
    usart_write_str(cmd_str);
    if (c == cmd_clear) {
      usart_write(cmd_clear);
      chip_erase();
    } else if (c == cmd_write) {
      usart_write(cmd_write);
      write_rom();
    } else if (c == cmd_dump) {
      usart_write(cmd_dump);
      dump_rom();
    } else {
      usart_write_str(error_str);
    }
  }
}
