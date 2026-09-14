#include "mouse.h"
#include "gui/gui.h" // dla SCREEN_WIDTH i SCREEN_HEIGHT

static int mouse_x = SCREEN_WIDTH / 2;
static int mouse_y = SCREEN_HEIGHT / 2;
static uint8_t mouse_b = 0;
static int mouse_cycle = 0;
static int8_t mouse_bytes[3];

static void mouse_write(uint8_t val) {
    __asm__ volatile("outb %%al, $0x64" : : "a"((uint8_t)0xD4));
    __asm__ volatile("outb %%al, $0x60" : : "a"(val));
}

void init_ps2_mouse(void) {
    uint8_t status;
    __asm__ volatile("outb %%al, $0x64" : : "a"((uint8_t)0xA8));
    
    __asm__ volatile("outb %%al, $0x64" : : "a"((uint8_t)0x20));
    __asm__ volatile("inb $0x60, %%al" : "=a"(status));
    status |= 2; 
    status &= ~0x20; 
    __asm__ volatile("outb %%al, $0x64" : : "a"((uint8_t)0x60));
    __asm__ volatile("outb %%al, $0x60" : : "a"(status));

    mouse_write(0xF6);
    mouse_write(0xF4);
}

void poll_mouse(void) {
    uint8_t status;
    __asm__ volatile("inb $0x64, %%al" : "=a"(status));
    if (status & 1) {
        uint8_t data;
        __asm__ volatile("inb $0x60, %%al" : "=a"(data));
        
        if (mouse_cycle == 0) {
            if (data & 0x08) {
                mouse_bytes[0] = data;
                mouse_cycle++;
            }
        } else if (mouse_cycle == 1) {
            mouse_bytes[1] = data;
            mouse_cycle++;
        } else if (mouse_cycle == 2) {
            mouse_bytes[2] = data;
            mouse_cycle = 0;

            int dx = mouse_bytes[1];
            int dy = mouse_bytes[2];
            
            if (mouse_bytes[0] & 0x10) dx |= 0xFFFFFF00;
            if (mouse_bytes[0] & 0x20) dy |= 0xFFFFFF00;

            mouse_x += dx;
            mouse_y -= dy;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= SCREEN_WIDTH) mouse_x = SCREEN_WIDTH - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= SCREEN_HEIGHT) mouse_y = SCREEN_HEIGHT - 1;

            mouse_b = mouse_bytes[0] & 0x07;
        }
    }
}

int get_mouse_x(void) { return mouse_x; }
int get_mouse_y(void) { return mouse_y; }
uint8_t get_mouse_buttons(void) { return mouse_b; }