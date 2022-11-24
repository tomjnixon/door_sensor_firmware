#include "config.h"
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
#define bit_battery 0b10
#define bit_retransmit 0b100

// PWM transmission using timer 2 with interrupts
//
// - the state stores the current part being transmitted (mark/space), and an
//   index into the buffer
// - the timer auto-reloads, the timer is set up for the spacing between the
//   next transmission and the one after. this is why the bit counter seems to
//   be incremented too early in the diagram below
// - there is no barrel shifter so the bit extraction is quite inefficient. it
//   would be much better to store the current byte (or a mask) in the
//   state and shift by one each time. the utilities would then not be
//   required, so the isr would be simpler and need to save/restore fewer
//   registers too
//
//           __    __    __
//         _|  |__|  |__|  |_
// state:  s m  s  m  s  m  d
// bit:    0 1  1  2  2  3  3

volatile enum { XMIT_MARK, XMIT_SPACE, XMIT_DONE } xmit_state;
uint8_t xmit_buf[id_size + 1];
volatile uint8_t xmit_bit;
uint8_t xmit_num_bits;

#define xmit_freq_MHz 5
#define xmit_mark_len_1 500
#define xmit_mark_len_0 1500
#define xmit_space_len_1 500
#define xmit_space_len_0 500
#define xmit_warmup 500
#define xmit_gap 2000

#define xmit_load_timer(time_us)                                               \
  do {                                                                         \
    T2H = 0xff & ((0xffff - xmit_freq_MHz * time_us) >> 8);                    \
    T2L = 0xff & (0xffff - xmit_freq_MHz * time_us);                           \
  } while (0)

uint8_t xmit_get_bit() {
  uint8_t local_bit = xmit_bit;

  uint8_t bit = 7 - (local_bit & 0x7);
  uint8_t byte = local_bit >> 3;
  return (xmit_buf[byte] >> bit) & 0x01;
}

void xmit_load_mark() {
  if (xmit_get_bit()) {
    xmit_load_timer(xmit_mark_len_1);
  } else {
    xmit_load_timer(xmit_mark_len_0);
  }
}

void xmit_isr(void) __interrupt(TF2_VECTOR) {
  if (xmit_state == XMIT_MARK) {
    // end of mark, timer already reloaded with space time, so set up next mark
    // time
    io_ask = 0;

    if (xmit_bit < xmit_num_bits) {
      xmit_load_mark();
      xmit_state = XMIT_SPACE;
    } else {
      xmit_state = XMIT_DONE;
      AUXR &= ~T2R;
    }
  } else if (xmit_state == XMIT_SPACE) {
    // end of space, timer already reloaded with mark time, so set up next space
    // time
    io_ask = 1;

    if (xmit_get_bit()) {
      xmit_load_timer(xmit_space_len_1);
    } else {
      xmit_load_timer(xmit_space_len_0);
    }
    xmit_state = XMIT_MARK;

    xmit_bit++;
  }
}

void xmit_buffer() {
  io_tx = 0;
  io_ask = 0;

  // configure timer 2
  IE2 |= ET2;                // enable interrupt
  AUXR &= ~(T2_C_T | S1ST2); // timer mode, not used for uart
  AUXR |= T2x12;             // disable divide by 12
  AUXR2 &= ~T2CLKO;          // disable output

  xmit_load_timer(xmit_warmup);

  for (uint8_t i = 0; i < num_repeats; i++) {
    xmit_state = XMIT_SPACE;
    xmit_bit = 0;

    AUXR |= T2R; // start timer

    xmit_load_mark();

    while (xmit_state != XMIT_DONE) {
      PCON = IDL;
      __asm__("nop");
    }

    // timer will be disabled

    xmit_load_timer(xmit_gap);
  }

  IE2 &= ~ET2; // disable interrupt just in case

  io_tx = 1;
}

void xmit(uint8_t state) {
  for (uint8_t i = 0; i < id_size; i++) {
    xmit_buf[i] = GUID[(7 - id_size) + i];
  }
  xmit_buf[id_size] = state;
  xmit_num_bits = (id_size + 1) * 8;

  xmit_buffer();
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

  AUXR2 &= ~EX3; // disable so that this doesn't get triggered repeatedly due to
                 // noise
}

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
  // wait for battery state to settle
  for (uint16_t i = 0; i < 1000; i++)
    delay_500us();

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
