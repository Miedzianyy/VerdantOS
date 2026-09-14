#include "php-parser.h"
#include "fat16.h"

extern void terminal_writestring(const char* data);

void php_execute_file(const char* filename) {
    char read_buf[4096];
    
    if (fat16_read_file((char*)filename, read_buf)) {
        terminal_writestring("\n--- PHP Execution Start ---\n");
        
        int i = 0;
        while (read_buf[i] != '\0') {
            if (read_buf[i] == 'e' && read_buf[i+1] == 'c' && read_buf[i+2] == 'h' && read_buf[i+3] == 'o') {
                i += 4;
                while (read_buf[i] == ' ' || read_buf[i] == '(') i++;
                if (read_buf[i] == '"' || read_buf[i] == '\'') {
                    char quote = read_buf[i];
                    i++;
                    while (read_buf[i] != quote && read_buf[i] != '\0') {
                        char s[2] = {read_buf[i], 0};
                        terminal_writestring(s);
                        i++;
                    }
                }
            }
            i++;
        }
        
        terminal_writestring("\n--- PHP Execution End ---\n");
    } else {
        terminal_writestring("\n[ERR] PHP script not found on disk!\n");
    }
}