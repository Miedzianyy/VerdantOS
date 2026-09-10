#include "keyboard.h"
#include <stdint.h>

// Flaga określająca, czy Shift jest wciśnięty
static int shift_pressed = 0;

// Funkcja inline do odczytu z portu I/O
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Standardowa tablica dla małych liter i cyfr (Set 1)
static const char scancode_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// Tablica dla wciśniętego Shifta (wielkie litery i znaki specjalne)
static const char scancode_ascii_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

char keyboard_read_char(void) {
    char c = 0;
    while (1) {
        // Sprawdź czy bufor klawiatury ma dane (port statusu to 0x64, bit 0)
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            
            // Obsługa lewego Shifta (make: 0x2A, break: 0xAA)
            // Obsługa prawego Shifta (make: 0x36, break: 0xB6)
            if (scancode == 0x2A || scancode == 0x36) {
                shift_pressed = 1;
                continue;
            }
            if (scancode == 0xAA || scancode == 0xB6) {
                shift_pressed = 0;
                continue;
            }
            
            // Ignorujemy inne kody zwolnienia klawisza (release scancodes > 128)
            if (scancode & 0x80) {
                continue;
            }
            
            // Wybierz odpowiednią tablicę w zależności od stanu Shifta
            if (shift_pressed) {
                c = scancode_ascii_shift[scancode];
            } else {
                c = scancode_ascii[scancode];
            }

            if (c != 0) {
                return c;
            }
        }
    }
}