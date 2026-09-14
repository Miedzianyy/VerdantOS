#include <stdint.h>
#include <stddef.h>

// Deklaracje funkcji systemowych VerdantOS (VFORTH LIB)
void terminal_writestring(const char* data);
char keyboard_read_char(void);
int fat16_change_directory(const char* dirname);
int fat16_make_directory(const char* dirname);
int fat16_write_file(const char* filename, const char* content, uint32_t size);
int fat16_read_file(const char* filename, char* buffer, uint32_t max_size);
void fat16_list_directory(void);

// Zewnętrzne zmienne VGA z kernela
extern uint8_t terminal_color;
extern uint16_t* terminal_buffer;

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define STACK_SIZE 256
#define DICTIONARY_SIZE 64
#define MAX_WORD_NAME 32
#define MAX_WORD_BODY 512

typedef struct {
    char name[MAX_WORD_NAME];
    char body[MAX_WORD_BODY];
    int is_active;
} ForthWord;

static int data_stack[STACK_SIZE];
static int stack_pointer = -1;

static ForthWord g_dictionary[DICTIONARY_SIZE];
static int g_dict_count = 0;

static int compiling_mode = 0;
static char compiling_name[MAX_WORD_NAME];
static char compiling_body[MAX_WORD_BODY];

// Bufor na ostatni tekst wpisany przez VFORTH:INPUT
static char user_input_buffer[128];

// Pomocnicza funkcja do parsowania hex (np. "0A" -> bajt)
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

// --- STACK OPERATIONS ---
void forth_push(int val) {
    if (stack_pointer < STACK_SIZE - 1) {
        data_stack[++stack_pointer] = val;
    } else {
        terminal_writestring("[VFORTH Error] Stack overflow!\n");
    }
}

int forth_pop(void) {
    if (stack_pointer >= 0) {
        return data_stack[stack_pointer--];
    } else {
        terminal_writestring("[VFORTH Error] Stack underflow!\n");
    }
    return 0;
}

// --- UTILS ---
void forth_init(void) {
    stack_pointer = -1;
    g_dict_count = 0;
    compiling_mode = 0;
    for (int i = 0; i < DICTIONARY_SIZE; i++) {
        g_dictionary[i].is_active = 0;
    }
    terminal_writestring("[VFORTH] VerdantOS Forth Environment & LIB Initialized.\n");
}

static int forth_strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return 0;
        i++;
    }
    return s1[i] == s2[i];
}

static int is_number(const char* str) {
    int i = 0;
    if (str[0] == '-' && str[1] != '\0') i = 1;
    while (str[i] != '\0') {
        if (str[i] < '0' || str[i] > '9') return 0;
        i++;
    }
    return 1;
}

static int custom_atoi(const char* str) {
    int res = 0;
    int sign = 1;
    int i = 0;
    if (str[0] == '-') {
        sign = -1;
        i = 1;
    }
    while (str[i] != '\0') {
        res = res * 10 + (str[i] - '0');
        i++;
    }
    return res * sign;
}

static void int_to_string(int n, char* buf) {
    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int i = 0, is_neg = 0;
    if (n < 0) {
        is_neg = 1;
        n = -n;
    }
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    if (is_neg) buf[i++] = '-';
    buf[i] = '\0';
    
    int start = 0, end = i - 1;
    while (start < end) {
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }
}

// Funkcja czyszcząca ekran pod tryb pełnoekranowy (Fullscreen TUI)
static void forth_clear_screen(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = (uint16_t) ' ' | (uint16_t) (terminal_color << 8);
        }
    }
}

// Funkcja odczytu linii z klawiatury do bufora dialogowego
static void forth_read_input(char* buf, int max_len) {
    int idx = 0;
    while (1) {
        char c = keyboard_read_char();
        if (c != 0) {
            if (c == '\n' || c == '\r') {
                terminal_writestring("\n");
                break;
            } else if (c == '\b' || c == 127) {
                if (idx > 0) {
                    idx--;
                    terminal_writestring("\b \b");
                }
            } else {
                if (idx < max_len - 1) {
                    buf[idx++] = c;
                    char s[2] = {c, 0};
                    terminal_writestring(s);
                }
            }
        }
    }
    buf[idx] = '\0';
}

void forth_run_source(const char* source);

int forth_find_word(const char* name) {
    for (int i = 0; i < DICTIONARY_SIZE; i++) {
        if (g_dictionary[i].is_active && forth_strcmp(g_dictionary[i].name, name)) {
            return i;
        }
    }
    return -1;
}

// --- CORE INTERPRETER (EVAL) ---
void forth_eval(const char* token) {
    if (!token || token[0] == '\0') return;

    if (compiling_mode) {
        if (forth_strcmp(token, ";")) {
            if (g_dict_count < DICTIONARY_SIZE) {
                int idx = g_dict_count++;
                int i = 0;
                while (compiling_name[i] != '\0') {
                    g_dictionary[idx].name[i] = compiling_name[i];
                    i++;
                }
                g_dictionary[idx].name[i] = '\0';

                i = 0;
                while (compiling_body[i] != '\0') {
                    g_dictionary[idx].body[i] = compiling_body[i];
                    i++;
                }
                g_dictionary[idx].body[i] = '\0';
                g_dictionary[idx].is_active = 1;

                terminal_writestring("[VFORTH] Word defined: ");
                terminal_writestring(g_dictionary[idx].name);
                terminal_writestring("\n");
            }
            compiling_mode = 0;
        } else {
            int len = 0;
            while (compiling_body[len] != '\0') len++;
            if (len > 0 && len < MAX_WORD_BODY - 1) {
                compiling_body[len++] = ' ';
                compiling_body[len] = '\0';
            }
            int i = 0;
            while (token[i] != '\0' && len < MAX_WORD_BODY - 1) {
                compiling_body[len++] = token[i++];
            }
            compiling_body[len] = '\0';
        }
        return;
    }

    if (forth_strcmp(token, ":")) {
        compiling_mode = 1;
        compiling_name[0] = '\0';
        compiling_body[0] = '\0';
        return;
    }

    if (is_number(token)) {
        forth_push(custom_atoi(token));
        return;
    }

    if (forth_strcmp(token, ".")) {
        int val = forth_pop();
        char buf[32];
        int_to_string(val, buf);
        terminal_writestring(buf);
        terminal_writestring(" ");
        return;
    }
    if (forth_strcmp(token, "+")) {
        int b = forth_pop();
        int a = forth_pop();
        forth_push(a + b);
        return;
    }
    if (forth_strcmp(token, "-")) {
        int b = forth_pop();
        int a = forth_pop();
        forth_push(a - b);
        return;
    }
    if (forth_strcmp(token, "*")) {
        int b = forth_pop();
        int a = forth_pop();
        forth_push(a * b);
        return;
    }
    if (forth_strcmp(token, "=")) {
        int b = forth_pop();
        int a = forth_pop();
        forth_push(a == b ? 1 : 0);
        return;
    }
    if (forth_strcmp(token, "CR")) {
        terminal_writestring("\n");
        return;
    }

    int word_idx = forth_find_word(token);
    if (word_idx != -1) {
        forth_run_source(g_dictionary[word_idx].body);
        return;
    }

    terminal_writestring("[VFORTH Warning] Unknown word: ");
    terminal_writestring(token);
    terminal_writestring("\n");
}

static void get_next_token(const char* source, int* idx, char* arg_buf, int max_len) {
    int a_idx = 0;
    while (source[*idx] == ' ' || source[*idx] == '\t' || source[*idx] == '\n' || source[*idx] == '\r') {
        if (source[*idx] == '\0') break;
        (*idx)++;
    }
    while (source[*idx] != '\0' && source[*idx] != ' ' && source[*idx] != '\t' && source[*idx] != '\n' && source[*idx] != '\r') {
        if (a_idx < max_len - 1) {
            arg_buf[a_idx++] = source[*idx];
        }
        (*idx)++;
    }
    arg_buf[a_idx] = '\0';
}

// --- STREAM PARSER ---
void forth_run_source(const char* source) {
    if (!source) return;

    char token[64];
    int t_idx = 0;
    int i = 0;

    while (source[i] != '\0') {
        char c = source[i];

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (t_idx > 0) {
                token[t_idx] = '\0';
                
                if (compiling_mode && compiling_name[0] == '\0') {
                    int k = 0;
                    while (token[k] != '\0' && k < MAX_WORD_NAME - 1) {
                        compiling_name[k] = token[k];
                        k++;
                    }
                    compiling_name[k] = '\0';
                } else {
                    if (forth_strcmp(token, "VFORTH:MKDIR")) {
                        char arg[64];
                        get_next_token(source, &i, arg, sizeof(arg));
                        if (arg[0] != '\0') {
                            fat16_make_directory(arg);
                            terminal_writestring("[VFORTH] Created directory: ");
                            terminal_writestring(arg);
                            terminal_writestring("\n");
                        }
                    } else if (forth_strcmp(token, "VFORTH:CD")) {
                        char arg[64];
                        get_next_token(source, &i, arg, sizeof(arg));
                        if (arg[0] != '\0') {
                            fat16_change_directory(arg);
                            terminal_writestring("[VFORTH] Changed directory to: ");
                            terminal_writestring(arg);
                            terminal_writestring("\n");
                        }
                    } else if (forth_strcmp(token, "VFORTH:LS")) {
                        fat16_list_directory();
                    } else if (forth_strcmp(token, "INCLUDE") || forth_strcmp(token, "include")) {
                        char filename[64];
                        get_next_token(source, &i, filename, sizeof(filename));
                        if (filename[0] != '\0') {
                            char file_buffer[512];
                            int bytes_read = fat16_read_file(filename, file_buffer, sizeof(file_buffer) - 1);
                            if (bytes_read > 0) {
                                file_buffer[bytes_read] = '\0';
                                forth_run_source(file_buffer);
                            }
                        }
                    } else if (forth_strcmp(token, "VFORTH:CLS")) {
                        forth_clear_screen();
                    } else if (forth_strcmp(token, "VFORTH:COLOR")) {
                        char arg[16];
                        get_next_token(source, &i, arg, sizeof(arg));
                        if (arg[0] != '\0') {
                            if (forth_strcmp(arg, "DEFAULT")) {
                                terminal_color = 0x0F;
                            } else {
                                terminal_color = parse_hex_byte(arg);
                            }
                        }
                    } else if (forth_strcmp(token, "VFORTH:INPUT")) {
                        forth_read_input(user_input_buffer, sizeof(user_input_buffer));
                    } else if (forth_strcmp(token, "VFORTH:INPUT_NUM")) {
                        char input_buf[32];
                        forth_read_input(input_buf, sizeof(input_buf));
                        int val = custom_atoi(input_buf);
                        forth_push(val);
                    } else if (forth_strcmp(token, "VFORTH:INPUT_EQUALS")) {
                        char arg[64];
                        get_next_token(source, &i, arg, sizeof(arg));
                        if (forth_strcmp(user_input_buffer, arg)) {
                            forth_push(1);
                        } else {
                            forth_push(0);
                        }
                    } else if (forth_strcmp(token, "VFORTH:PRINT_INPUT")) {
                        terminal_writestring(user_input_buffer);
                        terminal_writestring("\n");
                    } else if (forth_strcmp(token, "IF")) {
                        int condition = forth_pop();
                        if (!condition) {
                            int depth = 1;
                            while (source[i] != '\0' && depth > 0) {
                                char sub_token[32];
                                int st_idx = 0;
                                while (source[i] == ' ' || source[i] == '\t' || source[i] == '\n' || source[i] == '\r') i++;
                                while (source[i] != '\0' && source[i] != ' ' && source[i] != '\t' && source[i] != '\n' && source[i] != '\r') {
                                    if (st_idx < 31) sub_token[st_idx++] = source[i];
                                    i++;
                                }
                                sub_token[st_idx] = '\0';
                                if (forth_strcmp(sub_token, "IF")) depth++;
                                if (forth_strcmp(sub_token, "THEN") || forth_strcmp(sub_token, "ELSE")) {
                                    depth--;
                                    if (depth == 0) break;
                                }
                            }
                        }
                    } else if (forth_strcmp(token, "ELSE")) {
                        int depth = 1;
                        while (source[i] != '\0' && depth > 0) {
                            char sub_token[32];
                            int st_idx = 0;
                            while (source[i] == ' ' || source[i] == '\t' || source[i] == '\n' || source[i] == '\r') i++;
                            while (source[i] != '\0' && source[i] != ' ' && source[i] != '\t' && source[i] != '\n' && source[i] != '\r') {
                                if (st_idx < 31) sub_token[st_idx++] = source[i];
                                i++;
                            }
                            sub_token[st_idx] = '\0';
                            if (forth_strcmp(sub_token, "IF")) depth++;
                            if (forth_strcmp(sub_token, "THEN")) {
                                depth--;
                                if (depth == 0) break;
                            }
                        }
                    } else if (forth_strcmp(token, "THEN")) {
                        // Nic nie robi
                    } else if (forth_strcmp(token, "VFORTH:ECHO")) {
                        while (source[i] != '\0' && source[i] != '"') i++;
                        if (source[i] == '"') {
                            i++;
                            char echo_buf[128];
                            int e_idx = 0;
                            while (source[i] != '\0' && source[i] != '"' && (size_t)e_idx < sizeof(echo_buf) - 1) {
                                echo_buf[e_idx++] = source[i++];
                            }
                            echo_buf[e_idx] = '\0';
                            if (source[i] == '"') i++;
                            terminal_writestring(echo_buf);
                            terminal_writestring("\n");
                        }
                    } else {
                        forth_eval(token);
                    }
                }
                t_idx = 0;
            }
            if (c == ':' && !compiling_mode) {
                compiling_mode = 1;
                compiling_name[0] = '\0';
                compiling_body[0] = '\0';
            }
        } else {
            if (t_idx < (int)sizeof(token) - 1) {
                token[t_idx++] = c;
            }
        }
        i++;
    }
}