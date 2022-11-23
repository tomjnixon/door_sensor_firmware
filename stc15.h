#ifndef STC15_H
#define STC15_H

#include <8051.h>

__sfr __at(0xB1) P3M1;
__sfr __at(0xB2) P3M0;
__sfr __at(0x97) PCON2;

__sfr __at(0xAA) WKTCL;
__sfr __at(0xAB) WKTCH;

__sfr __at(0xAB) AUXR2;
#define EX2 0x08
#define EX3 0x10
#define EX4 0x20

// these are not SFRs, they're in the upper half of the memory (which doesn't
// exist?), at the same location as the SFRs but only indirectly accessible
//
// requires building with SDCC_NOGENRAMCLEAR=1
#define WIRC_L (*((__data uint8_t *)0xF9))
#define WIRC_H (*((__data uint8_t *)0xF8))

#define GUID ((__data uint8_t *)0x71)

#define IE2_VECTOR 10
#define IE3_VECTOR 11

#endif
