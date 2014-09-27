# Copyright (C) 2014 Erik Trædal

AVRDUDE = avrdude -c usbasp -p m8
CC = avr-gcc -Wall -Os -Iusbdrv -I. -mmcu=atmega8 -DF_CPU=12000000L 
OBJ = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o main.o

# symbolic targets:
all:	main.hex

.c.o:
	$(CC) -c $< -o $@
.S.o:
	$(CC) -x assembler-with-cpp -c $< -o $@
.c.s:
	$(CC) -S $< -o $@

flash:	all
	$(AVRDUDE) -u -U flash:w:main.hex

clean:
	rm -f main.hex main.lst main.obj main.cof main.list main.map main.eep.hex main.bin *.o usbdrv/*.o main.s usbdrv/oddebug.s usbdrv/usbdrv.s

main.bin:	$(OBJ)
	$(CC) -o main.bin $(OBJ)

main.hex:	main.bin
	rm -f main.hex main.eep.hex
	avr-objcopy -j .text -j .data -O ihex main.bin main.hex

disasm:	main.bin
	avr-objdump -d main.bin

cpp:
	$(CC) -E main.c
