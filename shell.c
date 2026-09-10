#include <stdint.h>

void terminal_writestring(const char* data);
char keyboard_read_char(void);

void fat16_init_disk(void);
void fat16_format_disk(void);
void fat16_mkfs(void);
void fat16_list_directory(void);
int fat16_change_directory(const char* dirname);
int fat16_write_file(const char* filename, const char* content, uint32_t size);
int fat16_read_file(const char* filename, char* buffer);
int fat16_delete_entry(const char* filename, int expect_dir);
int fat16_make_directory(const char* dirname);
int fat16_is_valid_filename(const char* filename);

void rawtxt_start(const char* filename);

static char cmd_line[128];
static int cmd_index = 0;

void shell_init(void) {
    fat16_init_disk();
}

static int strcmp_custom(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void parse_command(char* line) {
    if (line[0] == '\0') return;

    if (strcmp_custom(line, "help") == 0) {
        terminal_writestring("\nDostepne polecenia:\n");
        terminal_writestring("  help           - Pokazuje te pomoc\n");
        terminal_writestring("  infofetch      - Wyświetla informacje o systemie\n");
        terminal_writestring("  ls             - Wyświetla listę plików w bieżącym katalogu\n");
        terminal_writestring("  cd <katalog>   - Zmienia bieżący katalog\n");
        terminal_writestring("  mkdir <katalog>- Tworzy nowy katalog (chroniony przed duplikatami)\n");
        terminal_writestring("  rm <plik>      - Usuwa plik z dysku\n");
        terminal_writestring("  rm -d <katalog>- Usuwa katalog z dysku\n");
        terminal_writestring("  rawtxt <plik>  - Otwiera edytor tekstowy RawTXT\n");
        terminal_writestring("  format         - Fizycznie czyści obszar dysku\n");
        terminal_writestring("  mkfs           - Tworzy strukturę FAT16 i tablicę FAT\n");
        terminal_writestring("  write <plik> <tekst> - Zapisuje plik na dysk\n");
        terminal_writestring("  cat <plik>     - Odczytuje i wyświetla zawartość pliku\n");
        terminal_writestring("  echo <tekst>   - Wyświetla podany tekst na ekranie\n");
    } 
    else if (strcmp_custom(line, "infofetch") == 0) {
        terminal_writestring("\nVerdantOS - Architektura x86 i386\n");
        terminal_writestring("System plików: FAT16 z LFN (do 64 znaków) + Ochrona przed duplikatami\n");
    }
    else if (strcmp_custom(line, "ls") == 0) {
        fat16_list_directory();
    }
    else if (line[0] == 'c' && line[1] == 'd' && line[2] == ' ') {
        char* path = &line[3];
        if (fat16_change_directory(path)) {
            terminal_writestring("\n[OK] Zmieniono katalog.\n");
        } else {
            terminal_writestring("\n[Błąd] Nie znaleziono katalogu lub to nie katalog!\n");
        }
    }
    // Obsługa komendy echo
    else if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' && line[3] == 'o' && line[4] == ' ') {
        char* text = &line[5];
        terminal_writestring("\n");
        terminal_writestring(text);
        terminal_writestring("\n");
    }
    else if (line[0] == 'm' && line[1] == 'k' && line[2] == 'd' && line[3] == 'i' && line[4] == 'r' && line[5] == ' ') {
        char* dirname = &line[6];
        if (!fat16_is_valid_filename(dirname)) {
            terminal_writestring("\n[Błąd] Nieprawidłowa nazwa katalogu! Unikaj znaków specjalnych.\n");
            return;
        }
        int res = fat16_make_directory(dirname);
        if (res == 1) {
            terminal_writestring("\n[Sukces] Utworzono katalog.\n");
        } else if (res == -1) {
            terminal_writestring("\n[Błąd] Katalog o takiej nazwie już istnieje!\n");
        } else {
            terminal_writestring("\n[Błąd] Brak miejsca na dysku!\n");
        }
    }
    else if (line[0] == 'r' && line[1] == 'm' && line[2] == ' ') {
        char* args = &line[3];
        int is_dir = 0;

        if (args[0] == '-' && args[1] == 'd' && args[2] == ' ') {
            is_dir = 1;
            args = &line[6]; 
        }

        terminal_writestring("\n[RM] Usuwanie ");
        terminal_writestring(is_dir ? "katalogu: " : "pliku: ");
        terminal_writestring(args);
        terminal_writestring("\n");

        if (fat16_delete_entry(args, is_dir)) {
            terminal_writestring("[OK] Usunięto pomyślnie.\n");
        } else {
            terminal_writestring("[Błąd] Nie udało się usunąć (nie istnieje lub zły typ).\n");
        }
    }
    else if (line[0] == 'r' && line[1] == 'a' && line[2] == 'w' && line[3] == 't' && line[4] == 'x' && line[5] == 't' && line[6] == ' ') {
        char* filename = &line[7];
        if (!fat16_is_valid_filename(filename)) {
            terminal_writestring("\n[Błąd] Nieprawidłowa nazwa pliku!\n");
            return;
        }
        rawtxt_start(filename);
    }
    else if (strcmp_custom(line, "format") == 0) {
        fat16_format_disk();
    } 
    else if (strcmp_custom(line, "mkfs") == 0) {
        fat16_mkfs();
    } 
    else if (line[0] == 'c' && line[1] == 'a' && line[2] == 't' && line[3] == ' ') {
        char* filename = &line[4];
        char read_buf[4096];
        
        if (fat16_read_file(filename, read_buf)) {
            terminal_writestring("\n--- Zawartosc pliku ---\n");
            terminal_writestring(read_buf);
            terminal_writestring("\n-----------------------\n");
        } else {
            terminal_writestring("\n[Błąd] Nie znaleziono pliku lub błąd odczytu!\n");
        }
    }
    else if (line[0] == 'w' && line[1] == 'r' && line[2] == 'i' && line[3] == 't' && line[4] == 'e' && line[5] == ' ') {
        char* args = &line[6];
        char filename[64];
        int i = 0;
        
        while (args[i] != ' ' && args[i] != '\0' && i < 63) {
            filename[i] = args[i];
            i++;
        }
        filename[i] = '\0';

        if (!fat16_is_valid_filename(filename)) {
            terminal_writestring("\n[Błąd] Nieprawidłowa nazwa pliku!\n");
            return;
        }

        if (args[i] == ' ') {
            char* content = &args[i + 1];
            uint32_t size = 0;
            while (content[size] != '\0') size++;

            int res = fat16_write_file(filename, content, size);
            if (res == 1) {
                terminal_writestring("\n[Sukces] Zapisano plik.\n");
            } else if (res == -1) {
                terminal_writestring("\n[Błąd] Plik o takiej nazwie już istnieje!\n");
            } else {
                terminal_writestring("\n[Błąd] Brak miejsca na dysku!\n");
            }
        } else {
            terminal_writestring("\n[Błąd] Podaj treść pliku!\n");
        }
    } 
    else {
        terminal_writestring("\nNieznane polecenie. Wpisz 'help'.\n");
    }
}

void shell_putchar(char c) {
    if (c == '\n' || c == '\r') {
        cmd_line[cmd_index] = '\0';
        terminal_writestring("\n");
        parse_command(cmd_line);
        cmd_index = 0;
        cmd_line[0] = '\0';
        terminal_writestring("A> ");
    } else if (c == '\b' || c == 127) {
        if (cmd_index > 0) {
            cmd_index--;
            cmd_line[cmd_index] = '\0';
            terminal_writestring("\b \b");
        }
    } else {
        if (cmd_index < 126) {
            cmd_line[cmd_index++] = c;
            cmd_line[cmd_index] = '\0';
            char s[2] = {c, 0};
            terminal_writestring(s);
        }
    }
}

void shell_run(void) {
    shell_init();
    terminal_writestring("Wpisz 'help', aby zobaczyć dostępne polecenia.\n");
    terminal_writestring("A> ");
    while (1) {
        char c = keyboard_read_char();
        if (c != 0) {
            shell_putchar(c);
        }
    }
}