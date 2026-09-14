#include <stdint.h>
#include "vexe.h"

int fat16_read_file(const char* filename, char* buffer);
int fat16_write_file(const char* filename, const char* content, uint32_t size);
void terminal_writestring(const char* data);

#define MAX_APP_SIZE 4096
static char file_buffer[MAX_APP_SIZE];

// Pomocnicza funkcja do sprawdzania rozszerzenia pliku
static int has_extension(const char* filename, const char* ext) {
    int f_len = 0;
    int e_len = 0;
    while (filename[f_len] != '\0') f_len++;
    while (ext[e_len] != '\0') e_len++;
    
    if (f_len < e_len) return 0;
    
    for (int i = 0; i < e_len; i++) {
        char c1 = filename[f_len - e_len + i];
        char c2 = ext[i];
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return 0;
    }
    return 1;
}

void vexe_compile(const char* source_file, const char* app_name) {
    // ZASADA: .vexe nie może być źródłem dla innego .vexe!
    if (has_extension(source_file, ".vexe")) {
        terminal_writestring("\n[VEXE Error] Architecture rule violation: A .vexe file cannot be used as a source for another .vexe!\n");
        terminal_writestring("[VEXE Error] Use a standard source file (.fs) instead.\n");
        return;
    }

    // 1. Odczyt pliku źródłowego (.fs)
    for (int i = 0; i < MAX_APP_SIZE; i++) file_buffer[i] = 0;

    if (!fat16_read_file(source_file, file_buffer)) {
        terminal_writestring("\n[VEXE Error] Could not read source file: ");
        terminal_writestring(source_file);
        terminal_writestring("\n");
        return;
    }

    // Liczymy realną długość wczytanego tekstu (zamiast brać "1" z wyniku funkcji)
    uint32_t file_size = 0;
    while (file_buffer[file_size] != '\0' && file_size < MAX_APP_SIZE - 1) {
        file_size++;
    }

    if (file_size == 0) {
        terminal_writestring("\n[VEXE Error] Source file is empty!\n");
        return;
    }

    // 2. Sklejanie nazwy docelowej z rozszerzeniem ".vexe"
    char dest_filename[64];
    int i = 0;
    while (app_name[i] != '\0' && i < 48) {
        dest_filename[i] = app_name[i];
        i++;
    }
    
    const char* ext = ".vexe";
    int j = 0;
    while (ext[j] != '\0' && i < 63) {
        dest_filename[i++] = ext[j++];
    }
    dest_filename[i] = '\0';

    // 3. Zapis na dysk FAT16 z poprawnym rozmiarem całego pliku
    int res = fat16_write_file(dest_filename, file_buffer, file_size);
    if (res == 1) {
        terminal_writestring("\n[OK] Successfully compiled into VEXE: ");
        terminal_writestring(dest_filename);
        terminal_writestring("\n");
    } else if (res == -1) {
        terminal_writestring("\n[ERR] A VEXE file with this name already exists!\n");
    } else {
        terminal_writestring("\n[ERR] Not enough space on FAT16 disk!\n");
    }
}
//a ja leżałam na łóżku i słyszałam trzask, wyleciałam a chłop stoi mówie chlopie coś ty zrobil.