#include "stc15.h"
#include <stdint.h>

#define io_led P3_1
#define io_reed P3_2
#define io_btn P3_3
#define io_batt P3_5 // low when battery is low
#define io_ask P3_4
#define io_tx P3_0 // active low, also serial rx

void delay_500us() {
  // clang-format off
  __asm
    push 0x30 // 2
    push 0x31 // 2
    mov 0x30,#2 // 3
    mov 0x31,#240 // 3
NEXT1:
    djnz 0x31,NEXT1 // 5
    djnz 0x30,NEXT1 // 5
    pop 0x31 // 2
    pop 0x30 // 2
  __endasm;
  // clang-format on
}

void serial_tx(char c) {
  P3_1 = 0;
  delay_500us();
  for (int i = 0; i < 8; i++) {
    if (c & (1 << i))
      P3_1 = 1;
    else
      P3_1 = 0;
    delay_500us();
  }
  P3_1 = 1;
  delay_500us();
}

volatile uint8_t state = 0;
uint8_t last_state = 0xff;

#define state_mask 0b111
#define bit_open 0b1
#define bit_retransmit 0b10
#define bit_battery 0b100

static const uint8_t id[3] = {0x10, 0x20, 0x30};

void xmit_byte(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    io_ask = 1;
    if (byte & (1 << i)) {
      delay_500us();
    } else {
      delay_500us();
      delay_500us();
      delay_500us();
    }
    io_ask = 0;
    delay_500us();
  }
}

void xmit(uint8_t state) {
  io_tx = 0;
  delay_500us();

  for (uint8_t i = 0; i < 5; i++) {
    xmit_byte(id[0]);
    xmit_byte(id[1]);
    xmit_byte((id[2] & ~state_mask) | state);

    delay_500us();
    delay_500us();
    delay_500us();
    delay_500us();
  }

  io_tx = 1;
}

// xmit with blink
void do_xmit(uint8_t state) {
  io_led = 0;
  for (unsigned int i = 0; i < 100; i++)
    delay_500us();
  io_led = 1;

  xmit(state);
}

void reed_isr(void) __interrupt(IE0_VECTOR) {
  state &= ~bit_open;
  if (io_reed)
    state |= bit_open;
}

void battery_isr(void) __interrupt(IE3_VECTOR) {
  if (!io_batt)
    state |= bit_battery;

  AUXR2 &= ~EX3; // enable (falling only)
}

#define reset_delay 1
#define max_delay 450
uint16_t current_delay;
uint16_t current_delay_remaining;

void main() {
  // setup pin types
  // input: 10
  // push-pull: 01
  //       76543210
  P3M1 = 0b00100100;
  P3M0 = 0b00010000;

  io_ask = 0;
  io_tx = 1;

  serial_tx('h');
  serial_tx('i');
  serial_tx('\n');

  // setup reed interrupt
  IT0 = 0; // rising or falling
  EX0 = 1; // enable

  // setup battery interrupt
  AUXR2 |= EX3; // enable (falling only)

  // call the ISRs to fill in the state
  reed_isr();
  battery_isr();

  EA = 1; // enable all

  { // setup wakeup timer for 8s intervals
    uint16_t freq = ((uint16_t)WIRC_H << 8) | (uint16_t)WIRC_L;
    freq >>= 1;
    WKTCL = freq;
    WKTCH = 0x80 | (freq >> 8);
  }

  while (1) {
    uint8_t local_state = state;
    if (local_state != last_state) {
      do_xmit(local_state);

      last_state = local_state;

      current_delay = current_delay_remaining = reset_delay;
    } else if (WKTCH == 0xff && WKTCL == 0xff) {
      current_delay_remaining--;

      if (current_delay_remaining == 0) {
        do_xmit(state | bit_retransmit);

        current_delay <<= 1;
        if (current_delay > max_delay)
          current_delay = max_delay;

        current_delay_remaining = current_delay;
      }
    }

    PCON = PD;
    __asm__("nop");
  }
}
