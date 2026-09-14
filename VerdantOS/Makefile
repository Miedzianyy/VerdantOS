CC = gcc
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra
AS = as
LD = ld
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

OBJS = boot.o kernel.o keyboard.o shell.o rawtxt.o fat16.o php-parser.o forth.o vexe.o rtl8139.o zip.o vget.o ping.o gui/gui.o gui/processes.o mouse.o

all: verdantos.bin

verdantos.bin: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o verdantos.bin

boot.o: boot.s
	$(AS) --32 boot.s -o boot.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

keyboard.o: keyboard.c
	$(CC) $(CFLAGS) -c keyboard.c -o keyboard.o

shell.o: shell.c
	$(CC) $(CFLAGS) -c shell.c -o shell.o

rawtxt.o: rawtxt.c
	$(CC) $(CFLAGS) -c rawtxt.c -o rawtxt.o

fat16.o: fat16.c
	$(CC) $(CFLAGS) -c fat16.c -o fat16.o

php-parser.o: php-parser.c
	$(CC) $(CFLAGS) -c php-parser.c -o php-parser.o

forth.o: forth.c
	$(CC) $(CFLAGS) -c forth.c -o forth.o

vexe.o: vexe.c vexe.h
	$(CC) $(CFLAGS) -c vexe.c -o vexe.o

rtl8139.o: rtl8139.c rtl8139.h
	$(CC) $(CFLAGS) -c rtl8139.c -o rtl8139.o

zip.o: zip.c zip.h
	$(CC) $(CFLAGS) -c zip.c -o zip.o

vget.o: vget.c vget.h
	$(CC) $(CFLAGS) -c vget.c -o vget.o

ping.o: ping.c ping.h
	$(CC) $(CFLAGS) -c ping.c -o ping.o

# Automatyczne generowanie icons.h z Pythona przed kompilacją GUI
gui/icons.h: convert.py assets/file.png assets/prog.png
	python3 convert.py

gui/gui.o: gui/gui.c gui/gui.h gui/processes.h gui/icons.h
	$(CC) $(CFLAGS) -c gui/gui.c -o gui/gui.o

gui/processes.o: gui/processes.c gui/processes.h gui/gui.h
	$(CC) $(CFLAGS) -c gui/processes.c -o gui/processes.o

mouse.o: mouse.c mouse.h
	$(CC) $(CFLAGS) -c mouse.c -o mouse.o

run: verdantos.bin
	if [ ! -f dysk.img ]; then dd if=/dev/zero of=dysk.img bs=512 count=2880; fi
	qemu-system-i386 -kernel verdantos.bin -hda dysk.img -net nic,model=rtl8139 -net user,hostfwd=tcp::8080-192.168.18.17:8080

clean:
	rm -f *.o gui/*.o gui/icons.h verdantos.bin dysk.img