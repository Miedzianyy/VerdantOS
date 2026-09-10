#include <stdint.h>
#include "keyboard.h"

void terminal_writestring(const char* data);
void terminal_putchar(char c);
void shell_print_prompt(void);
int fat16_write_file(const char* filename, const char* content, uint32_t size);

#define MAX_TEXT_LENGTH 4096
static char file_buffer[MAX_TEXT_LENGTH];
static int text_index = 0;
static char current_filename[32];

void rawtxt_clear_screen(void) {
    for (int i = 0; i < 25; i++) {
        terminal_writestring("\n");
    }
}

void rawtxt_draw_header(void) {
    terminal_writestring("================================================================\n");
    terminal_writestring(" RawTXT Editor v0.2 | Plik: ");
    terminal_writestring(current_filename);
    terminal_writestring(" | [ESC] Zapisz i wyjdz\n");
    terminal_writestring("================================================================\n");
}

void rawtxt_start(const char* filename) {
    // Kopiujemy nazwę pliku
    int i = 0;
    while (filename[i] != '\0' && i < 31) {
        current_filename[i] = filename[i];
        i++;
    }
    current_filename[i] = '\0';

    // Resetujemy bufor tekstu dla nowego pliku
    text_index = 0;
    file_buffer[0] = '\0';

    rawtxt_clear_screen();
    rawtxt_draw_header();

    while (1) {
        char c = keyboard_read_char();
        if (c != 0) {
            if (c == 27) { // Klawisz ESC (ASCII 27)
                terminal_writestring("\n[RawTXT] Zapisywanie pliku na dysk...\n");
                
                // Faktyczny zapis przez nasz moduł FAT16!
                if (fat16_write_file(current_filename, file_buffer, text_index)) {
                    terminal_writestring("[RawTXT] Zapisano pomyslnie! Powrot do powloki.\n\n");
                } else {
                    terminal_writestring("[RawTXT] Blad: Brak miejsca na dysku!\n\n");
                }
                break;
            } else if (c == '\b') {
                if (text_index > 0) {
                    text_index--;
                    file_buffer[text_index] = '\0';
                    terminal_putchar('\b');
                }
            } else if (c == '\n') {
                if (text_index < MAX_TEXT_LENGTH - 1) {
                    file_buffer[text_index++] = '\n';
                    file_buffer[text_index] = '\0';
                    terminal_putchar('\n');
                }
            } else {
                if (text_index < MAX_TEXT_LENGTH - 1) {
                    file_buffer[text_index++] = c;
                    file_buffer[text_index] = '\0';
                    terminal_putchar(c);
                }
            }
        }
    }
}