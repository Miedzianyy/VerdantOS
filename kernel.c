#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "shell.h"

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

uint16_t* terminal_buffer = (uint16_t*) VGA_ADDRESS;
size_t terminal_row = 0;
size_t terminal_column = 0;
uint8_t terminal_color = 0x0F; // Jasny biały na czarnym

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = (uint16_t) ' ' | (uint16_t) (terminal_color << 8);
        }
    }
}

void terminal_scroll(void) {
    // Przesuwamy wszystkie linie o jeden wiersz w górę
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[(y - 1) * VGA_WIDTH + x] = terminal_buffer[y * VGA_WIDTH + x];
        }
    }
    // Czścimy ostatnią (dolną) linię
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t) ' ' | (uint16_t) (terminal_color << 8);
    }
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
        return;
    }

    if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            const size_t index = terminal_row * VGA_WIDTH + terminal_column;
            terminal_buffer[index] = (uint16_t) ' ' | (uint16_t) (terminal_color << 8);
        }
        return;
    }

    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = (uint16_t) c | (uint16_t) (terminal_color << 8);
    
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    }
}

void terminal_writestring(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++) {
        terminal_putchar(data[i]);
    }
}

void kernel_main(void) {
    terminal_initialize();
    
    terminal_writestring("=== Welcome to VerdantOS! ===\n");
    terminal_writestring("OS Version: VerdantOS x86 Build 103-BETA\n");
    terminal_writestring("Filesystem: FAT16\n\n");

    shell_run();
}