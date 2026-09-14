#ifndef PROCESSES_H
#define PROCESSES_H

#include <stdint.h>

#define MAX_PROCESSES 8

typedef enum {
    PROC_STATE_RUNNING,
    PROC_STATE_MINIMIZED,
    PROC_STATE_CLOSED
} ProcessState;

typedef struct {
    int id;
    char name[32];
    int x, y, w, h;
    int is_dragging;
    int drag_offset_x;
    int drag_offset_y;
    ProcessState state;
    // Pozycja ikony na pulpicie po zminimalizowaniu
    int icon_x, icon_y; 
} WindowProcess;

void init_processes(void);
WindowProcess* create_process(const char* name, int x, int y, int w, int h);
int update_processes(int mx, int my, uint8_t mb, int screen_w, int screen_h);
void draw_all_processes(void);

#endif