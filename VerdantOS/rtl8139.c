#include <stdint.h>

// Funkcje wejścia/wyjścia portów x86
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define RTL_PORT_IDR0       0x00
#define RTL_PORT_TSD0       0x10
#define RTL_PORT_TSAD0      0x20
#define RTL_PORT_RBSTART    0x30
#define RTL_PORT_COMMAND    0x37
#define RTL_PORT_CAPR       0x38
#define RTL_PORT_IMR        0x3C
#define RTL_PORT_ISR        0x3E
#define RTL_PORT_RCR        0x44
#define RTL_PORT_CONFIG1    0x52
#define RTL_PORT_MEDIA      0x58

static uint16_t rtl_io_base = 0xC000; // Standardowy adres I/O dla QEMU RTL8139

// Bufor odbiorczy (Ring Buffer) wymagany przez RTL8139 (8KB + 16 bajtów + margines)
static uint8_t rx_ring_buffer[8192 + 16 + 1500] __attribute__((aligned(4)));
static uint16_t rx_read_ptr = 0;

// Rejestry nadawcze TX (4 kanały rotacyjne)
static uint32_t tx_descriptors[4] = {0x20, 0x24, 0x28, 0x2C};
static uint32_t tx_status_registers[4] = {0x10, 0x14, 0x18, 0x1C};
static uint8_t current_tx_desc = 0;

void rtl8139_init(uint16_t pio_base) {
    rtl_io_base = pio_base;
    
    // 1. Zdejmij stan uśpienia karty (Power On - CONFIG1 = 0x00)
    outb(rtl_io_base + RTL_PORT_CONFIG1, 0x00);
    
    // 2. Wykonaj miękki reset karty (Reset bit = 0x10 w Command Register)
    outb(rtl_io_base + RTL_PORT_COMMAND, 0x10);
    while ((inb(rtl_io_base + RTL_PORT_COMMAND) & 0x10) != 0) {
        // Czekaj, aż bit resetu opadnie
    }
    
    // 3. Ustaw początek bufora odbiorczego (RBSTART)
    outl(rtl_io_base + RTL_PORT_RBSTART, (uint32_t)(uintptr_t)rx_ring_buffer);
    
    // 4. KLUCZOWE: Ustaw początkowy wskaźnik CAPR na 0xFFF0 (-16)
    outw(rtl_io_base + RTL_PORT_CAPR, 0xFFF0);
    
    // 5. Skonfiguruj RCR (Receive Configuration Register):
    //    AAP (Accept All Packets) | AB (Accept Broadcast) | AM (Accept Multicast) | APM (Accept Physical Match) | WRAP (Wrap buffer)
    outl(rtl_io_base + RTL_PORT_RCR, (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 7));
    
    // 6. Włącz przerwania dla TOK (Transmit OK) i ROK (Receive OK) w IMR
    outw(rtl_io_base + RTL_PORT_IMR, 0x0005);
    
    // 7. Włącz nadajnik (TE) oraz odbiornik (RE) w rejestrze Command (0x0C)
    outb(rtl_io_base + RTL_PORT_COMMAND, 0x0C);
    
    rx_read_ptr = 0;
}

void rtl8139_get_mac(uint8_t* mac_out) {
    // Adres MAC jest zapisany w rejestrach IDR0 do IDR5 (porty 0x00 - 0x05)
    for (int i = 0; i < 6; i++) {
        mac_out[i] = inb(rtl_io_base + RTL_PORT_IDR0 + i);
    }
}

uint8_t rtl8139_is_linked(void) {
    // Rejestr MediaStatus (0x58): bit 3 (LinkFail) -> 0 oznacza, że łącze jest aktywne
    uint8_t media = inb(rtl_io_base + RTL_PORT_MEDIA);
    return (media & 0x08) == 0;
}

// Funkcja wysyłająca surową ramkę Ethernet przez RTL8139
int rtl8139_send_packet(const void* packet_data, uint16_t len) {
    if (len > 1792) return 0; // Maksymalny rozmiar bufora TX dla RTL8139

    uint8_t desc = current_tx_desc;
    current_tx_desc = (current_tx_desc + 1) % 4;

    outl(rtl_io_base + tx_descriptors[desc], (uint32_t)(uintptr_t)packet_data);
    outl(rtl_io_base + tx_status_registers[desc], len);

    // Czekaj na zakończenie transmisji (TOK - Transmit OK)
    int timeout = 100000;
    while (timeout--) {
        uint32_t status = inl(rtl_io_base + tx_status_registers[desc]);
        if (status & (1 << 15)) { // TOK (Transmit OK)
            return 1;
        }
    }
    return 0;
}

// Funkcja odbierająca nadchodzące pakiety z bufora kołowego
int rtl8139_receive_packet(uint8_t* buf, uint16_t max_len) {
    uint16_t cmd = inb(rtl_io_base + RTL_PORT_COMMAND);
    // Sprawdź czy bufor nie jest pusty (bit BUFE - Buffer Empty)
    if (cmd & 0x01) {
        return 0; 
    }

    // Odczytaj nagłówek pakietu z bufora kołowego (względem aktualnego wskaźnika odczytu)
    uint16_t offset = rx_read_ptr % 8192;
    uint16_t* ring_ptr = (uint16_t*)(rx_ring_buffer + offset);
    uint16_t rx_status = ring_ptr[0];
    uint16_t rx_size = ring_ptr[1];

    if (!(rx_status & 0x0001)) { // ROK (Receive OK)
        return -1; // Błąd pakietu
    }

    uint16_t packet_len = rx_size - 4; // odliczamy CRC
    if (packet_len > max_len) {
        packet_len = max_len;
    }

    // Kopiuj dane pakietu z uwzględnieniem zawijania bufora kołowego (8KB)
    uint16_t data_offset = (offset + 4) % 8192;
    for (uint16_t i = 0; i < packet_len; i++) {
        buf[i] = rx_ring_buffer[(data_offset + i) % 8192];
    }

    // Aktualizuj wskaźnik odczytu (wyrównany do 4 bajtów: rozmiar pakietu + 4 bajty nagłówka)
    rx_read_ptr = (rx_read_ptr + rx_size + 4 + 3) & ~3;
    rx_read_ptr %= 8192;

    // Zapisz nowy wskaźnik CAPR (Command Allocation Pointer minus 16)
    outw(rtl_io_base + RTL_PORT_CAPR, rx_read_ptr - 16);

    return packet_len;
}