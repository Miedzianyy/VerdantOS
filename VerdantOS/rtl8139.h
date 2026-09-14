#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

void rtl8139_init(uint16_t pio_base);
void rtl8139_get_mac(uint8_t* mac_out);
uint8_t rtl8139_is_linked(void);

#endif