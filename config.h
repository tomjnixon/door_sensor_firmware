#ifndef CONFIG_H
#define CONFIG_H

// retransmission delays, in multiples of 4 seconds
//
// reset_delay is the time between the first transmission when something
// changes, and the first
// retransmission
//
// the delay is doubled on each retransmission, until max_delay is reached
#define reset_delay 1
#define max_delay 450

// number of repeats during each transmission
#define num_repeats 5

// the number of bytes from the GUID to transmit
#define id_size 3

#endif
