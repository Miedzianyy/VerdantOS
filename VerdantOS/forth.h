#ifndef FORTH_H
#define FORTH_H

// Inicjalizacja rdzenia Fortha
void forth_init(void);

// Główna funkcja wykonująca kod źródłowy przekazany z pliku lub shella
void forth_run_source(const char* source_code);

#endif