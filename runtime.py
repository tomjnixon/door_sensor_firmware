from scimath.units.time import us, s, day, hour, year
from scimath.units.electromagnetism import mA, amp, v

uA = amp / 1000000
mAh = mA * hour

# usage details
batt_capacity = 600 * mAh  # typical for AAA NiMH
state_chang_freq = 2 / day  # frequency of real state changes

wakeup_period = 8 * s

# parameters from config.h
reset_delay = 1 * wakeup_period
max_delay = 450 * wakeup_period

num_repeats = 5

id_size = 3

# parameters in main.c
xmit_mark_len_1 = 500 * us
xmit_mark_len_0 = 1500 * us
xmit_space_len_1 = 500 * us
xmit_space_len_0 = 500 * us
xmit_warmup = 500 * us
xmit_gap = 2000 * us
blink_time = 100 * 500 * us

# measured currents at 1.3v
current_sleep = 35 * uA
current_idle = 3.2 * mA
current_run = 11.9 * mA
current_led = 1.2 * mA

current_tx_off = 0.8 * mA
current_tx_on = 63.8 * mA

wakeup_time = 26 * us

# calc usage of one transmission
bits = (id_size + 1) * 8

bit_mark_time = (xmit_mark_len_1 + xmit_mark_len_0) / 2
bit_space_time = (xmit_space_len_1 + xmit_space_len_0) / 2

tx_on_time = num_repeats * bits * bit_mark_time
tx_off_time = (
    xmit_warmup
    + num_repeats * (bits - 1) * bit_space_time
    + (num_repeats - 1) * xmit_gap
)

tx_usage = (
    blink_time * (current_led + current_run)
    + tx_on_time * (current_idle + current_tx_on)
    + tx_off_time * (current_idle + current_tx_off)
)

# number of extra transmissions caused by a state change
def calc_extra_tx():
    n_tx = 1
    t = 0 * s
    gap = reset_delay

    while gap.value < max_delay.value:
        n_tx += 1
        t += gap
        gap *= 2

    return n_tx - t / max_delay


extra_tx = calc_extra_tx()

# total current from transmissions
current_tx_periodic = tx_usage * (1 / max_delay)
current_tx_changes = tx_usage * state_chang_freq * extra_tx
current_tx_total = current_tx_periodic + current_tx_changes

# and including sleep + wakeup
current_wakeup = current_run * wakeup_time / wakeup_period
total_current = current_sleep + current_wakeup + current_tx_total

runtime = batt_capacity / total_current

print(f"current:   {total_current / uA:.2f} uA")
print(f"runtime:   {runtime / year:.2f} years")
print()
print(f"fraction used by different activities")
print(f"sleep:     {100 * current_sleep / total_current:.1f}%")
print(f"wakeup:    {100 * current_wakeup / total_current:.1f}%")
print(f"tx:        {100 * current_tx_total / total_current:.1f}%")
print(f" periodic: {100 * current_tx_periodic / total_current:.1f}%")
print(f" changes:  {100 * current_tx_changes / total_current:.1f}%")
