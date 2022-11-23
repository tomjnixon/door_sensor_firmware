CC = sdcc
CFLAGS = -mmcs51 --iram-size 128 --xram-size 0 --code-size 4088

export SDCC_NOGENRAMCLEAR = 1

main.ihx: main.c stc15.h
	$(CC) $(CFLAGS) $< -o $@

.PHONY: flash
flash: main.ihx
	stcgal -P stc15 -t 5000 $<

.PHONY: clean
clean: main.ihx
	rm -f main.asm main.hex main.ihx main.lk main.lst main.map main.mem main.rel main.rst main.sym
