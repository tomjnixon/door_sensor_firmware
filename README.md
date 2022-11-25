This repository contains alternative firmware for GS-WDS07 wireless door/window
sensors.

This was written mainly to add periodic re-transmission, so that state changes
will eventually get through even if one message is missed.

It also just seemed like a fun thing to do, and makes it possible to add extra
sensors and behaviours.

# The Hardware

I've only looked at 433MHz GS-WDS07 sensors [like
these](https://uk.banggood.com/5Pcs-GS-WDS07-Wireless-Door-Sensor-Magnetic-Strip-433MHz-for-Security-Alarm-Home-System-p-1596007.html),
but it wouldn't surprise me if other sensors use nearly the same hardware.

It has:

- An 8 pin [STC15W104](https://www.stcmicro.com/stc/stc15w104.html)
  microcontroller
  ([datasheet](https://www.stcmicro.com/datasheet/STC15F2K60S2-en.pdf)), a
  simple 8051 with no analogue peripherals but decent low-power support.

- a SYN115 ASK transmitter with a power switch

- a simple voltage detector for sensing low battery state

- a reed switch for detecting the open/close state

- space for an anti-tamper switch (with missing pull-up resistor R5)

- an LED

- a 3.3v boost converter to work from one AAA battery

# Building

Building the firmware requires SDCC and GNU Make.

Build with:

```sh
make main.ihx
```

Some basic settings can be changed in `config.h`.

# Flashing

I used [stcgal](https://github.com/grigorig/stcgal) with a USB-serial
converter, which works well. There's a windows flash utility too if that's more
your thing.

The released version is currently quite out of date and missing a few fixes, so install it with:

```sh
pip install git+https://github.com/grigorig/stcgal.git
```

There's a make target for flashing:

```sh
make flash
```

Connection details:

- The programming header is labeled `G T R V` for GND, Transmit, Receive and
  VIN

- 3.3V logic and power should be used

- Make sure to follow the diagram in the datasheet on page 852 (section
  16.2.2.2) when connecting the TX and RX lines.

  The MCU must be reset cleanly to start the bootloader. The resistor and diode
  are to make sure that the MCU is not powered through the serial
  data lines. The power switch is necessary to reset the MCU. I'd suggest
  adding a resistor from VIN to GND while flashing, as while the MCU is
  sleeping it uses very little current and may keep running on leakage.

- If you're feeling fancy you could build the auto-reset circuit from the
  [stcgal
  FAQ](https://github.com/grigorig/stcgal/blob/master/doc/FAQ.md#how-can-i-use-the-autoreset-feature)
  -- this would need to be enabled on the stcgal command line.

TODO: a better explanation, a diagram, and a printable programming jig

# Data Format

- ASK
- PWM encoding
    - zero mark: 1500us
    - one mark: 500us
    - space: 500us
    - gap between repeats: 2000us (including the previous space)
    - 5 repeats
- 4 bytes, MSB first
    - 3 byte ID; these are the trailing digits of the GUID (printed after `Target UID` in the stcgal output)
    - one byte data, starting from the MSB:
        - 5 zero bits (efficient!)
        - retransmit bit, set for periodic retransmissions, clear for initial
          transmission triggered by a state change
        - low battery bit (high when the battery is nearly empty)
        - open bit: 0 if the magnet is present, 1 if it's absent

The ID length and number of repeats can be changed in config.h; the others are trivial to change in the code (search xmit_mark_1 for example), 

To use this with [rtl_433](https://github.com/merbanan/rtl_433), use the
following generic decoder:

```
n=gswds07,m=OOK_PWM,s=500,l=1500,r=15000,g=1500,repeats>=3,bits=32,unique,get=@0:{24}:id,get=@31:{1}:open,get=@30:{1}:batt_low,get=@29:{1}:repeat
```

# Battery Life

TODO: calculate this

Using the wake-up timer to allow periodic retransmission does not seem to
measurably change the power usage (it's dominated by the boost converter), but
retransmission will. In the other hand, periodic retransmission may be more
efficient, as we can use fewer repeats in each message and get the same overall
reliability.

# Errata

- The wake-up timer on my test device seems quite inaccurate -- the built-in
  calibration says it's about 15% faster than nominal, but it's actually about
  15% slower. Perhaps the calibration is actually a scaled period rather than a
  frequency?

# License

Copyright (C) 2022 Thomas Nixon

GPL3; see LICENSE.txt
