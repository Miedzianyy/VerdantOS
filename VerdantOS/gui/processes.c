#include "processes.h"
#include "gui.h"

static WindowProcess processes[MAX_PROCESSES];
static int process_count = 0;

static int connect4_board[6][7];
static int connect4_turn = 1; // 1 = Gracz (Czerwony), 2 = Komputer (Żółty)
static int connect4_winner = 0; // 0 = gra w toku, 1 = gracz, 2 = komputer, 3 = remis

void init_processes(void) {
    process_count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROC_STATE_CLOSED;
    }
    
    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 7; c++) {
            connect4_board[r][c] = 0;
        }
    }
    
    create_process("Program Manager", 50, 50, 560, 350);
}

WindowProcess* create_process(const char* name, int x, int y, int w, int h) {
    if (process_count >= MAX_PROCESSES) return 0;
    
    int id = process_count++;
    WindowProcess* p = &processes[id];
    p->id = id;
    
    int i = 0;
    while (name[i] != '\0' && i < 31) {
        p->name[i] = name[i];
        i++;
    }
    p->name[i] = '\0';

    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;
    p->is_dragging = 0;
    p->state = PROC_STATE_RUNNING;
    
    return p;
}

// Funkcja sprawdzająca czy ktoś wygrał w Connect 4
int check_connect4_win(void) {
    // Poziomo
    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 4; c++) {
            int p = connect4_board[r][c];
            if (p != 0 && p == connect4_board[r][c+1] && p == connect4_board[r][c+2] && p == connect4_board[r][c+3]) {
                return p;
            }
        }
    }
    // Pionowo
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 7; c++) {
            int p = connect4_board[r][c];
            if (p != 0 && p == connect4_board[r+1][c] && p == connect4_board[r+2][c] && p == connect4_board[r+3][c]) {
                return p;
            }
        }
    }
    // Skośnie (skos w dół)
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            int p = connect4_board[r][c];
            if (p != 0 && p == connect4_board[r+1][c+1] && p == connect4_board[r+2][c+2] && p == connect4_board[r+3][c+3]) {
                return p;
            }
        }
    }
    // Skośnie (skos w górę)
    for (int r = 3; r < 6; r++) {
        for (int c = 0; c < 4; c++) {
            int p = connect4_board[r][c];
            if (p != 0 && p == connect4_board[r-1][c+1] && p == connect4_board[r-2][c+2] && p == connect4_board[r-3][c+3]) {
                return p;
            }
        }
    }
    // Remis (pełna plansza)
    int full = 1;
    for (int c = 0; c < 7; c++) {
        if (connect4_board[0][c] == 0) full = 0;
    }
    if (full) return 3;

    return 0;
}

// Proste AI dla komputera
void connect4_ai_move(void) {
    if (connect4_winner != 0) return;

    int preferred_cols[7] = {3, 2, 4, 1, 5, 0, 6};
    
    for (int i = 0; i < 7; i++) {
        int c = preferred_cols[i];
        if (connect4_board[0][c] == 0) { 
            for (int r = 5; r >= 0; r--) {
                if (connect4_board[r][c] == 0) {
                    connect4_board[r][c] = 2; // Komputer to 2
                    connect4_winner = check_connect4_win();
                    connect4_turn = 1; 
                    return;
                }
            }
        }
    }
}

int update_processes(int mx, int my, uint8_t mb, int screen_w, int screen_h) {
    (void)screen_w;
    (void)screen_h;
    int changed = 0;

    static uint8_t old_mb = 0;
    uint8_t clicked_now = (mb & 1) && !(old_mb & 1);
    old_mb = mb;

    // 1. Obsługa kliknięcia w zminimalizowane okna na pasku zadań
    if (clicked_now) {
        int minimized_index = 0;
        for (int i = 0; i < process_count; i++) {
            WindowProcess* p = &processes[i];
            if (p->state == PROC_STATE_MINIMIZED) {
                int slot_x = 40 + (minimized_index * 140);
                int slot_y = SCREEN_HEIGHT - 110;

                if (mx >= slot_x && mx <= slot_x + 80 &&
                    my >= slot_y && my <= slot_y + 100) {
                    p->state = PROC_STATE_RUNNING;
                    return 1;
                }
                minimized_index++;
            }
        }
    }

    // 2. Obsługa aktywnych okien
    for (int i = process_count - 1; i >= 0; i--) {
        WindowProcess* p = &processes[i];
        if (p->state != PROC_STATE_RUNNING) continue;

        int is_prog_man = (i == 0);
        int is_connect4 = (p->name[0] == 'c' && p->name[1] == 'o');

        int title_h = 32;
        int title_bar_clicked = (mx >= p->x && mx <= p->x + p->w &&
                                 my >= p->y && my <= p->y + title_h);

        int btn_size = 12;
        int btn_y = p->y + 10;
        int close_btn_x = p->x + p->w - 24;
        int min_btn_x = p->x + p->w - 42;

        int clicked_close = (mx >= close_btn_x && mx <= close_btn_x + btn_size &&
                             my >= btn_y && my <= btn_y + btn_size);
        int clicked_min = (mx >= min_btn_x && mx <= min_btn_x + btn_size &&
                           my >= btn_y && my <= btn_y + btn_size);

        // Program Manager - kliknięcia ikon (zawsze otwiera nową instancję)
        if (is_prog_man && clicked_now && !title_bar_clicked) {
            int client_x = p->x + 16;
            int client_y = p->y + 48;
            
            // Ikona 3: connect4
            int icon3_x = client_x + 260;
            int icon3_y = client_y + 20;
            if (mx >= icon3_x && mx <= icon3_x + 80 && my >= icon3_y && my <= icon3_y + 90) {
                for (int r = 0; r < 6; r++) {
                    for (int c = 0; c < 7; c++) connect4_board[r][c] = 0;
                }
                connect4_turn = 1;
                connect4_winner = 0;
                create_process("connect4", 180, 80, 360, 360);
                return 1;
            }
            
            // Ikona 1: terminal
            int icon1_x = client_x + 20;
            int icon1_y = client_y + 20;
            if (mx >= icon1_x && mx <= icon1_x + 80 && my >= icon1_y && my <= icon1_y + 90) {
                create_process("terminal", 120, 90, 400, 250);
                return 1;
            }

            // Ikona 2: taskmgr
            int icon2_x = client_x + 140;
            int icon2_y = client_y + 20;
            if (mx >= icon2_x && mx <= icon2_x + 80 && my >= icon2_y && my <= icon2_y + 90) {
                create_process("taskmgr", 160, 120, 380, 240);
                return 1;
            }
        }

        // Connect 4 - kliknięcia gry
        if (is_connect4 && clicked_now && !title_bar_clicked) {
            int btn_reset_x = p->x + p->w - 95;
            int btn_reset_y = p->y + 8;
            if (mx >= btn_reset_x && mx <= btn_reset_x + 50 && my >= btn_reset_y && my <= btn_reset_y + 16) {
                for (int r = 0; r < 6; r++) {
                    for (int c = 0; c < 7; c++) connect4_board[r][c] = 0;
                }
                connect4_turn = 1;
                connect4_winner = 0;
                return 1;
            }

            if (connect4_turn == 1 && connect4_winner == 0) {
                int client_x = p->x + 20;
                int client_y = p->y + 56;
                int col_w = 45;
                int cols = 7;

                if (mx >= client_x && mx < client_x + (cols * col_w) &&
                    my >= client_y && my < client_y + (6 * 40)) {
                    
                    int clicked_col = (mx - client_x) / col_w;
                    
                    for (int r = 5; r >= 0; r--) {
                        if (connect4_board[r][clicked_col] == 0) {
                            connect4_board[r][clicked_col] = 1; // Gracz to 1
                            connect4_winner = check_connect4_win();
                            
                            if (connect4_winner == 0) {
                                connect4_turn = 2; 
                                connect4_ai_move(); 
                            }
                            break;
                        }
                    }
                    return 1;
                }
            }
        }

        // Przenoszenie okien (Dragging)
        if (mb & 1) {
            if (!p->is_dragging && title_bar_clicked) {
                if (clicked_close && clicked_now) {
                    if (!is_prog_man) {
                        p->state = PROC_STATE_CLOSED;
                        return 1;
                    }
                } else if (clicked_min && clicked_now) {
                    p->state = PROC_STATE_MINIMIZED;
                    return 1;
                } else if (!clicked_close && !clicked_min) {
                    p->is_dragging = 1;
                    p->drag_offset_x = mx - p->x;
                    p->drag_offset_y = my - p->y;
                }
            } else if (p->is_dragging) {
                int new_x = mx - p->drag_offset_x;
                int new_y = my - p->drag_offset_y;
                if (new_x != p->x || new_y != p->y) {
                    p->x = new_x;
                    p->y = new_y;
                    changed = 1;
                }
            }
        } else {
            if (p->is_dragging) {
                p->is_dragging = 0;
                changed = 1;
            }
        }
    }
    return changed;
}

void draw_all_processes(void) {
    int minimized_index = 0;

    for (int i = 0; i < process_count; i++) {
        WindowProcess* p = &processes[i];

        if (p->state == PROC_STATE_RUNNING) {
            int is_prog_man = (i == 0);
            int is_connect4 = (p->name[0] == 'c' && p->name[1] == 'o');
            int is_taskmgr = (p->name[0] == 't' && p->name[1] == 'a' && p->name[2] == 's' && p->name[3] == 'k');

            // Nowoczesne tło okna (czysta biel/szarość)
            gui_draw_rect(p->x, p->y, p->w, p->h, 0x00F8F9FA);
            
            // Delikatne, jednopikselowe obramowanie
            gui_draw_rect(p->x, p->y, p->w, 1, 0x00D0D7DE);                 
            gui_draw_rect(p->x, p->y + p->h - 1, p->w, 1, 0x00D0D7DE);     
            gui_draw_rect(p->x, p->y, 1, p->h, 0x00D0D7DE);                 
            gui_draw_rect(p->x + p->w - 1, p->y, 1, p->h, 0x00D0D7DE);     

            // Pasek tytułowy (Flat UI, ciemny grafit)
            gui_draw_rect(p->x + 1, p->y + 1, p->w - 2, 32, 0x0021262D);
            gui_draw_string(p->x + 12, p->y + 10, p->name, 0x00FFFFFF);

            // Nowoczesne przyciski okna
            if (!is_prog_man) {
                gui_draw_rect(p->x + p->w - 24, p->y + 10, 12, 12, 0x00DA3633); // Zamknij (Czerwony)
            } else {
                gui_draw_rect(p->x + p->w - 24, p->y + 10, 12, 12, 0x00484F58); // Nieaktywny
            }
            gui_draw_rect(p->x + p->w - 42, p->y + 10, 12, 12, 0x008C959F);     // Minimalizuj (Szary)

            // Program Manager
            if (is_prog_man) {
                int client_x = p->x + 16;
                int client_y = p->y + 48;
                
                gui_draw_rect(client_x, client_y, p->w - 32, p->h - 64, 0x00FFFFFF);

                int icon1_x = client_x + 20;
                int icon1_y = client_y + 20;
                gui_draw_retro_icon(icon1_x + 16, icon1_y);
                gui_draw_string(icon1_x, icon1_y + 68, "terminal", 0x0021262D);

                int icon2_x = client_x + 140;
                int icon2_y = client_y + 20;
                gui_draw_retro_icon(icon2_x + 8, icon2_y);
                gui_draw_string(icon2_x, icon2_y + 68, "taskmgr", 0x0021262D);

                int icon3_x = client_x + 260;
                int icon3_y = client_y + 20;
                gui_draw_retro_icon(icon3_x + 8, icon3_y);
                gui_draw_string(icon3_x, icon3_y + 68, "connect4", 0x0021262D);
            }

            // Task Manager - wyświetlanie listy aktywnych procesów
            if (is_taskmgr) {
                int client_x = p->x + 16;
                int client_y = p->y + 48;
                
                gui_draw_string(client_x, client_y, "PID   NAZWA PROC.       STAN", 0x0057606A);

                int row_y = client_y + 24;
                for (int k = 0; k < process_count; k++) {
                    WindowProcess* proc = &processes[k];
                    if (proc->state != PROC_STATE_CLOSED) {
                        // Wypisz ID / PID
                        char id_str[8];
                        id_str[0] = '0' + (k / 10);
                        id_str[1] = '0' + (k % 10);
                        id_str[2] = '\0';
                        gui_draw_string(client_x, row_y, id_str, 0x0021262D);

                        // Wypisz nazwę procesu
                        gui_draw_string(client_x + 50, row_y, proc->name, 0x0021262D);
                        
                        // Wypisz stan (RUNNING / MINIMIZED)
                        const char* status = (proc->state == PROC_STATE_RUNNING) ? "RUNNING" : "MINIMIZED";
                        uint32_t status_color = (proc->state == PROC_STATE_RUNNING) ? 0x001A7F37 : 0x009E6A03;
                        gui_draw_string(client_x + 220, row_y, status, status_color);
                        
                        row_y += 22;
                    }
                }
            }

            // Connect 4 window
            if (is_connect4) {
                int client_x = p->x + 20;
                int client_y = p->y + 56;
                
                // Przycisk Reset
                gui_draw_rect(p->x + p->w - 95, p->y + 8, 50, 16, 0x00484F58);
                gui_draw_string(p->x + p->w - 87, p->y + 12, "RESET", 0x00FFFFFF);

                // Status gry
                if (connect4_winner == 1) {
                    gui_draw_string(client_x, p->y + 36, "WYGRALES! (Czerwony)", 0x00DA3633);
                } else if (connect4_winner == 2) {
                    gui_draw_string(client_x, p->y + 36, "KOMPUTER WYGRAL!", 0x009E6A03);
                } else if (connect4_winner == 3) {
                    gui_draw_string(client_x, p->y + 36, "REMIS!", 0x0057606A);
                } else {
                    if (connect4_turn == 1) 
                        gui_draw_string(client_x, p->y + 36, "Twoja tura (Czerwony)", 0x0021262D);
                    else 
                        gui_draw_string(client_x, p->y + 36, "Ruch komputera...", 0x0057606A);
                }

                // Plansza gry
                gui_draw_rect(client_x, client_y, 7 * 45, 6 * 40, 0x001F2328);

                for (int r = 0; r < 6; r++) {
                    for (int c = 0; c < 7; c++) {
                        int cx = client_x + (c * 45) + 22;
                        int cy = client_y + (r * 40) + 20;
                        
                        uint32_t color = 0x00484F58; // Puste pole
                        if (connect4_board[r][c] == 1) color = 0x00DA3633; // Gracz = Czerwony
                        if (connect4_board[r][c] == 2) color = 0x00D29922; // Komputer = Żółty

                        gui_draw_rect(cx - 14, cy - 14, 28, 28, color);
                    }
                }
            }

        } else if (p->state == PROC_STATE_MINIMIZED) {
            int slot_x = 40 + (minimized_index * 140);
            int slot_y = SCREEN_HEIGHT - 110;

            gui_draw_retro_icon(slot_x + 24, slot_y);

            int is_prog_man = (i == 0);
            if (is_prog_man) {
                gui_draw_string(slot_x, slot_y + 68, "Program", 0x00FFFFFF);
                gui_draw_string(slot_x, slot_y + 84, "Manager", 0x00FFFFFF);
            } else {
                gui_draw_string(slot_x, slot_y + 68, p->name, 0x00FFFFFF);
            }

            minimized_index++;
        }
    }
}