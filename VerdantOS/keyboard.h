#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Pobieranie wciśniętego znaku ASCII z klawiatury (wersja uproszczona polling)
char keyboard_read_char(void);

#endif