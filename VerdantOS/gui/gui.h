#ifndef GUI_H
#define GUI_H

#include <stdint.h>

#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

void gui_init(void);
void gui_draw_pixel(int x, int y, uint32_t color);
void gui_draw_rect(int start_x, int start_y, int w, int h, uint32_t color);
void gui_draw_cursor(int x, int y);
void gui_draw_char(int x, int y, int c, uint32_t color);
void gui_draw_string(int x, int y, const char* str, uint32_t color);
void gui_draw_retro_icon(int x, int y);
void gui_start(void);

#endif