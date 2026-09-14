#include <stdint.h>
#include "php-parser.h"
#include "forth.h"
#include "vexe.h"
#include "rtl8139.h"
#include "zip.h"
#include "vget.h"
#include "ping.h"
#include "gui/gui.h"

void terminal_writestring(const char* data);
char keyboard_read_char(void);

// Zewnętrzne funkcje VGA z kernel.c
extern uint8_t terminal_color;
extern void terminal_set_color(uint8_t color);

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

static uint8_t parse_hex_byte(const char* str) {
    uint8_t val = 0;
    for (int i = 0; i < 2 && str[i] != '\0'; i++) {
        val *= 16;
        char c = str[i];
        if (c >= '0' && c <= '9') val += (c - '0');
        else if (c >= 'a' && c <= 'f') val += (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val += (c - 'A' + 10);
    }
    return val;
}

// Pomocnicza funkcja do konwersji bajtu na string Hex (np. dla MAC)
static void byte_to_hex(uint8_t b, char* out) {
    const char* hex = "0123456789ABCDEF";
    out[0] = hex[(b >> 4) & 0x0F];
    out[1] = hex[b & 0x0F];
    out[2] = '\0';
}

void shell_init(void) {
    fat16_init_disk();
    
    // Inicjalizacja karty RTL8139 przy starcie systemu (domyślny port QEMU: 0xC000)
    rtl8139_init(0xC000);

    if (fat16_change_directory("conf")) {
        char color_buf[16];
        if (fat16_read_file("CONF.CFG", color_buf)) {
            uint8_t saved_color = parse_hex_byte(color_buf);
            terminal_set_color(saved_color);
        }
        fat16_change_directory(".."); 
    }
}

static int strcmp_custom(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

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

static void parse_command(char* line) {
    if (line[0] == '\0') return;

    if (strcmp_custom(line, "help") == 0) {
        terminal_writestring("\nAvailable commands:\n");
        terminal_writestring("  help           - Shows this help menu\n");
        terminal_writestring("  infofetch      - Displays system information\n");
        terminal_writestring("  netstat        - Checks network card status and MAC\n");
        terminal_writestring("  ping           - Sends an ICMP echo request to 10.0.2.2\n");
        terminal_writestring("  gui            - Starts the VerdantOS GUI mode (1280x720)\n");
        terminal_writestring("  ls             - Lists files in the current directory\n");
        terminal_writestring("  cd <dir>       - Changes the current directory\n");
        terminal_writestring("  mkdir <dir>    - Creates a new directory (duplicate-protected)\n");
        terminal_writestring("  rm <file>      - Deletes a file from the disk\n");
        terminal_writestring("  rm -d <dir>    - Deletes a directory from the disk\n");
        terminal_writestring("  rawtxt <file>  - Opens the RawTXT text editor\n");
        terminal_writestring("  format         - Physically wipes the disk area\n");
        terminal_writestring("  mkfs           - Creates the FAT16 structure and FAT table\n");
        terminal_writestring("  write <file> <text> - Writes a file to the disk\n");
        terminal_writestring("  cat <file>     - Reads and displays file content\n");
        terminal_writestring("  echo <text>    - Prints the given text to the screen\n");
        terminal_writestring("  color <val>    - Sets and saves text color to /conf/conf.cfg\n");
        terminal_writestring("  php <file>     - Runs PHP file\n");
        terminal_writestring("  forth <file>   - Runs a Forth script source\n");
        terminal_writestring("  vexe <src> <name> - Compiles a .fs file into a .vexe app\n");
        terminal_writestring("  unzip <file>   - Unpacks a ZIP archive to the current directory\n");
        terminal_writestring("  vget <url>     - Downloads a file via HTTP\n");
        terminal_writestring("  <app>.vexe     - Directly executes a compiled VEXE application\n");
    }
    else if (strcmp_custom(line, "infofetch") == 0) {
        terminal_writestring("\nVerdantOS - Architektura x86 i386\n");
        terminal_writestring("System plikow: FAT16 z LFN (do 64 znakow) + Ochrona przed duplikatami\n");
    }
    else if (strcmp_custom(line, "netstat") == 0) {
        terminal_writestring("\n--- Network Status (RTL8139) ---\n");
        
        uint8_t mac[6];
        rtl8139_get_mac(mac);
        
        terminal_writestring("MAC Address: ");
        for (int i = 0; i < 6; i++) {
            char hex_buf[3];
            byte_to_hex(mac[i], hex_buf);
            terminal_writestring(hex_buf);
            if (i < 5) terminal_writestring(":");
        }
        terminal_writestring("\n");

        uint8_t linked = rtl8139_is_linked();
        terminal_writestring("Link Status: ");
        if (linked) {
            terminal_writestring("CONNECTED (Kabel podlaczony)\n");
        } else {
            terminal_writestring("DISCONNECTED (Brak polaczenia/kabla)\n");
        }
        terminal_writestring("--------------------------------\n");
    }
    else if (strcmp_custom(line, "ping") == 0) {
        ping_run();
    }
    else if (strcmp_custom(line, "gui") == 0) {
        terminal_writestring("\n[GUI] Initializing graphical mode (1280x720)...\n");
        gui_start();
    }
    else if (line[0] == 'u' && line[1] == 'n' && line[2] == 'z' && line[3] == 'i' && line[4] == 'p' && line[5] == ' ') {
        char* filename = &line[6];
        if (!fat16_is_valid_filename(filename)) {
            terminal_writestring("\n[ERR] Invalid ZIP file name!\n");
            return;
        }
        
        terminal_writestring("\n[ZIP] Extracting archive: ");
        terminal_writestring(filename);
        terminal_writestring("\n");
        
        if (zip_extract(filename)) {
            terminal_writestring("[OK] Archive extracted successfully!\n");
        } else {
            terminal_writestring("[ERR] Failed to extract archive (invalid format or not found).\n");
        }
    }
    else if (line[0] == 'v' && line[1] == 'g' && line[2] == 'e' && line[3] == 't' && line[4] == ' ') {
        char* url = &line[5];
        if (url[0] == '\0') {
            terminal_writestring("\n[ERR] Usage: vget <http://url>\n");
            return;
        }
        vget_download(url);
    }
    else if (strcmp_custom(line, "ls") == 0) {
        fat16_list_directory();
    }
    else if (line[0] == 'c' && line[1] == 'd' && line[2] == ' ') {
        char* path = &line[3];
        if (fat16_change_directory(path)) {
            terminal_writestring("\n[OK] Changed Directory.\n");
        } else {
            terminal_writestring("\n[ERR] Cannot find this directory.\n");
        }
    }
    else if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' && line[3] == 'o' && line[4] == ' ') {
        char* text = &line[5];
        terminal_writestring("\n");
        terminal_writestring(text);
        terminal_writestring("\n");
    }
    else if (line[0] == 'c' && line[1] == 'o' && line[2] == 'l' && line[3] == 'o' && line[4] == 'r' && line[5] == ' ') {
        char* color_val = &line[6];
        if (color_val[0] == '\0') {
            terminal_writestring("\n[ERR] Usage: color <value> (e.g. color 0A)\n");
            return;
        }

        uint8_t new_color = parse_hex_byte(color_val);
        terminal_set_color(new_color);

        fat16_change_directory(".."); 
        if (fat16_change_directory("conf")) {
            fat16_delete_entry("CONF.CFG", 0); 
            
            int len = 0;
            while (color_val[len] != '\0' && len < 15) len++;
            
            if (fat16_write_file("CONF.CFG", color_val, len)) {
                terminal_writestring("\n[OK] Color changed and saved to /conf/conf.cfg\n");
            } else {
                terminal_writestring("\n[ERR] Color changed, but failed to write conf.cfg\n");
            }
            fat16_change_directory(".."); 
        } else {
            terminal_writestring("\n[ERR] Color changed, but directory /conf not found! Run 'mkfs' first.\n");
        }
    }
    else if (line[0] == 'm' && line[1] == 'k' && line[2] == 'd' && line[3] == 'i' && line[4] == 'r' && line[5] == ' ') {
        char* dirname = &line[6];
        if (!fat16_is_valid_filename(dirname)) {
            terminal_writestring("\n[ERR] Invalid directory name; Avoid special characters.\n");
            return;
        }
        int res = fat16_make_directory(dirname);
        if (res == 1) {
            terminal_writestring("\n[OK] Created Directory.\n");
        } else if (res == -1) {
            terminal_writestring("\n[ERR] This directory already exists!\n");
        } else {
            terminal_writestring("\n[ERR] No free disk space!\n");
        }
    }
    else if (line[0] == 'r' && line[1] == 'm' && line[2] == ' ') {
        char* args = &line[3];
        int is_dir = 0;

        if (args[0] == '-' && args[1] == 'd' && args[2] == ' ') {
            is_dir = 1;
            args = &line[6]; 
        }

        terminal_writestring("\n[RM] Removing ");
        terminal_writestring(is_dir ? "directory: " : "file: ");
        terminal_writestring(args);
        terminal_writestring("\n");

        if (fat16_delete_entry(args, is_dir)) {
            terminal_writestring("[OK] Removed successfully.\n");
        } else {
            terminal_writestring("[ERR] Unable to remove (it doesn't exist or it has bad type).\n");
        }
    }
    else if (line[0] == 'r' && line[1] == 'a' && line[2] == 'w' && line[3] == 't' && line[4] == 'x' && line[5] == 't' && line[6] == ' ') {
        char* filename = &line[7];
        if (!fat16_is_valid_filename(filename)) {
            terminal_writestring("\n[ERR] Invalid File Name!\n");
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
            terminal_writestring("\n--- File ---\n");
            terminal_writestring(read_buf);
            terminal_writestring("\n-----------------------\n");
        } else {
            terminal_writestring("\n[ERR] Cannot find file or can't read it!\n");
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
            terminal_writestring("\n[ERR] Invalid File name!\n");
            return;
        }

        if (args[i] == ' ') {
            char* content = &args[i + 1];
            uint32_t size = 0;
            while (content[size] != '\0') size++;

            int res = fat16_write_file(filename, content, size);
            if (res == 1) {
                terminal_writestring("\n[OK] Saved file.\n");
            } else if (res == -1) {
                terminal_writestring("\n[ERR] This file already exists!\n");
            } else {
                terminal_writestring("\n[ERR] No free disk space!\n");
            }
        } else {
            terminal_writestring("\n[ERR] Type something as file content!\n");
        }
    } 
    else if (line[0] == 'p' && line[1] == 'h' && line[2] == 'p' && line[3] == ' ') {
        php_execute_file(&line[4]); 
    }
    else if (line[0] == 'f' && line[1] == 'o' && line[2] == 'r' && line[3] == 't' && line[4] == 'h' && line[5] == ' ') {
        char* filename = &line[6];
        if (!fat16_is_valid_filename(filename)) {
            terminal_writestring("\n[ERR] Invalid File Name!\n");
            return;
        }
        char read_buf[4096];
        if (fat16_read_file(filename, read_buf)) {
            terminal_writestring("\n[FORTH] Running script: ");
            terminal_writestring(filename);
            terminal_writestring("\n");
            forth_run_source(read_buf);
        } else {
            terminal_writestring("\n[ERR] Cannot find file or can't read it!\n");
        }
    }
    else if (line[0] == 'v' && line[1] == 'e' && line[2] == 'x' && line[3] == 'e' && line[4] == ' ') {
        char* args = &line[5];
        char source_file[64];
        char app_name[64];
        int i = 0;
        
        while (args[i] != ' ' && args[i] != '\0' && i < 63) {
            source_file[i] = args[i];
            i++;
        }
        source_file[i] = '\0';

        if (args[i] == ' ') {
            int j = 0;
            i++;
            while (args[i] != '\0' && j < 63) {
                app_name[j++] = args[i++];
            }
            app_name[j] = '\0';

            if (source_file[0] != '\0' && app_name[0] != '\0') {
                vexe_compile(source_file, app_name);
            } else {
                terminal_writestring("\n[ERR] Usage: vexe <source_file> <app_name>\n");
            }
        } else {
            terminal_writestring("\n[ERR] Usage: vexe <source_file> <app_name>\n");
        }
    }
    else if (has_extension(line, ".vexe")) {
        char read_buf[4096];
        if (fat16_read_file(line, read_buf)) {
            terminal_writestring("\n[VEXE] Executing application: ");
            terminal_writestring(line);
            terminal_writestring("\n");
            forth_run_source(read_buf);
        } else {
            terminal_writestring("\n[ERR] VEXE application not found or cannot be read!\n");
        }
    }
    else {
        terminal_writestring("\nUnknown Command. Type 'help' to see list of avaiable commands.\n");
    }
}

void shell_putchar(char c) {
    if (c == '\n' || c == '\r') {
        cmd_line[cmd_index] = '\0';
        terminal_writestring("\n");
        parse_command(cmd_line);
        cmd_index = 0;
        cmd_line[0] = '\0';
        terminal_writestring("A:/ $ ");
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
    terminal_writestring("Type 'help' to see list of avaiable commands!\n");
    terminal_writestring("A:/ $ ");
    while (1) {
        char c = keyboard_read_char();
        if (c != 0) {
            shell_putchar(c);
        }
    }
}