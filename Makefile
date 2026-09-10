CC = gcc
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra
LD = ld
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

SRC = kernel.c keyboard.c shell.c rawtxt.c fat16.c
OBJ = $(SRC:.c=.o)
BIN = verdantos.bin
IMG = dysk.img

all: clean $(IMG) $(BIN) run

boot.o: boot.s
	as --32 boot.s -o boot.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): boot.o $(OBJ)
	$(LD) $(LDFLAGS) boot.o $(OBJ) -o $(BIN)

$(IMG):
	dd if=/dev/zero of=$(IMG) bs=1024 count=1440

run: $(BIN) $(IMG)
	qemu-system-i386 -kernel $(BIN) -drive file=$(IMG),format=raw,index=0,media=disk

clean:
	rm -f *.o $(BIN) $(IMG)