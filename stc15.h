#ifndef STC15_H
#define STC15_H

#include <8051.h>

__sfr __at(0xB1) P3M1;
__sfr __at(0xB2) P3M0;
__sfr __at(0x97) PCON2;

__sfr __at(0xAA) WKTCL;
__sfr __at(0xAB) WKTCH;

__sfr __at(0xAF) IE2;
#define ET2 0x4

__sfr __at(0x8E) AUXR;
#define S1ST2 0x01
#define T2x12 0x04
#define T2_C_T 0x08
#define T2R 0x10

__sfr __at(0x8F) AUXR2;
#define T2CLKO 0x04
#define EX2 0x10
#define EX3 0x20
#define EX4 0x40

__sfr __at(0xD6) T2H;
__sfr __at(0xD7) T2L;

// these are not SFRs, they're in the upper half of the memory (which doesn't
// exist?), at the same location as the SFRs but only indirectly accessible
//
// requires building with SDCC_NOGENRAMCLEAR=1
#define WIRC_L (*((__data uint8_t *)0xF9))
#define WIRC_H (*((__data uint8_t *)0xF8))

#define GUID ((__data uint8_t *)0x71)

#define IE2_VECTOR 10 // external interrupt 2
#define IE3_VECTOR 11 // external interrupt 3
#define TF2_VECTOR 12 // timer 2

#endif
