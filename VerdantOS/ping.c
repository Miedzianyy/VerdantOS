#include <stdint.h>
#include <stddef.h>

void terminal_writestring(const char* data);

void rtl8139_get_mac(uint8_t* mac_out);
int rtl8139_send_packet(const void* packet_data, uint16_t len);
int rtl8139_receive_packet(uint8_t* buf, uint16_t max_len);

static uint8_t local_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint32_t local_ip  = (10 << 24) | (0 << 16) | (2 << 8) | 15;
static uint32_t target_ip = (10 << 24) | (0 << 16) | (2 << 8) | 2; // 10.0.2.2 (host gateway)
static uint8_t target_mac[6] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02}; // Prawidłowy MAC bramy QEMU

// Globalny bufor nadawczy o bezpiecznym, stałym adresie dla DMA
static uint8_t global_tx_buf[1500] __attribute__((aligned(4)));

static inline uint16_t htons(uint16_t val) { return (val << 8) | (val >> 8); }
static inline uint32_t htonl(uint32_t val) {
    return ((val & 0xFF) << 24) | (((val >> 8) & 0xFF) << 16) |
           (((val >> 16) & 0xFF) << 8) | ((val >> 24) & 0xFF);
}

static uint16_t calculate_checksum(void* vdata, size_t length) {
    char* data = (char*)vdata;
    uint32_t acc = 0;
    for (size_t i = 0; i < length; i += 2) {
        uint16_t word;
        if (i + 1 < length) {
            word = ((uint16_t)(unsigned char)data[i] << 8) | (unsigned char)data[i + 1];
        } else {
            word = ((uint16_t)(unsigned char)data[i] << 8);
        }
        acc += word;
    }
    while (acc >> 16) {
        acc = (acc & 0xFFFF) + (acc >> 16);
    }
    return ~acc;
}

struct ethernet_header {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
} __attribute__((packed));

struct ip_header {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed));

struct icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} __attribute__((packed));

void ping_run(void) {
    terminal_writestring("\n[PING] Sending ICMP Echo Request to 10.0.2.2...\n");

    struct ethernet_header* eth = (struct ethernet_header*)global_tx_buf;
    struct ip_header* ip = (struct ip_header*)(global_tx_buf + sizeof(struct ethernet_header));
    struct icmp_header* icmp = (struct icmp_header*)(global_tx_buf + sizeof(struct ethernet_header) + sizeof(struct ip_header));

    rtl8139_get_mac(local_mac);
    for (int i = 0; i < 6; i++) {
        eth->dest_mac[i] = target_mac[i];
        eth->src_mac[i] = local_mac[i];
    }
    eth->ethertype = htons(0x0800);

    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(sizeof(struct ip_header) + sizeof(struct icmp_header) + 8);
    ip->id = htons(0x1337);
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = 1; // ICMP
    ip->checksum = 0;
    ip->src_ip = htonl(local_ip);
    ip->dest_ip = htonl(target_ip);
    ip->checksum = calculate_checksum(ip, sizeof(struct ip_header));

    icmp->type = 8; // Echo Request
    icmp->code = 0;
    icmp->identifier = htons(0x0001);
    icmp->sequence = htons(0x0001);
    icmp->checksum = 0;

    char* payload = (char*)(global_tx_buf + sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct icmp_header));
    for (int i = 0; i < 8; i++) {
        payload[i] = 'A' + i;
    }

    icmp->checksum = calculate_checksum(icmp, sizeof(struct icmp_header) + 8);

    int total_len = sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct icmp_header) + 8;
    
    // Wysłanie pakietu z globalnego bufora
    rtl8139_send_packet(global_tx_buf, total_len);

    uint8_t rx_buf[1500];
    int attempts = 4000000;
    int packets_seen = 0;

    while (attempts--) {
        int len = rtl8139_receive_packet(rx_buf, sizeof(rx_buf));
        if (len > 0) {
            packets_seen++;
            struct ethernet_header* r_eth = (struct ethernet_header*)rx_buf;
            if (htons(r_eth->ethertype) == 0x0800) {
                struct ip_header* r_ip = (struct ip_header*)(rx_buf + sizeof(struct ethernet_header));
                if (r_ip->protocol == 1) {
                    int ip_header_len = (r_ip->ver_ihl & 0x0F) * 4;
                    struct icmp_header* r_icmp = (struct icmp_header*)(rx_buf + sizeof(struct ethernet_header) + ip_header_len);
                    
                    if (r_icmp->type == 0 && r_icmp->code == 0) {
                        terminal_writestring("[OK] Received PONG! Reply from gateway active.\n");
                        return;
                    }
                }
            }
        }
    }

    if (packets_seen == 0) {
        terminal_writestring("[ERR] Timeout: Zero packets received by driver.\n");
    } else {
        terminal_writestring("[ERR] Timeout: Packets seen, but no valid ICMP Echo Reply.\n");
    }
}