#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

void init_ps2_mouse(void);
void poll_mouse(void);
int get_mouse_x(void);
int get_mouse_y(void);
uint8_t get_mouse_buttons(void);

#endif