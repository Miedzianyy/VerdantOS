#include <stdint.h>
#include <stddef.h>
#include "vget.h"

void terminal_writestring(const char* data);
int fat16_write_file(const char* filename, const char* content, uint32_t size);

void rtl8139_get_mac(uint8_t* mac_out);
int rtl8139_send_packet(const void* packet_data, uint16_t len);
int rtl8139_receive_packet(uint8_t* buf, uint16_t max_len);

static uint8_t local_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint32_t local_ip  = (10 << 24) | (0 << 16) | (2 << 8) | 15;
static uint8_t target_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};

static inline uint16_t htons(uint16_t val) { return (val << 8) | (val >> 8); }
static inline uint32_t htonl(uint32_t val) {
    return ((val & 0xFF) << 24) | (((val >> 8) & 0xFF) << 16) |
           (((val >> 16) & 0xFF) << 8) | ((val >> 24) & 0xFF);
}

static uint32_t parse_ip_address(const char* ip_str) {
    uint32_t ip = 0;
    uint32_t current_octet = 0;
    int octet_count = 0;

    for (int i = 0; ; i++) {
        char c = ip_str[i];
        if (c >= '0' && c <= '9') {
            current_octet = current_octet * 10 + (c - '0');
        } else if (c == '.' || c == '\0' || c == ':' || c == '/') {
            if (current_octet > 255) return 0;
            ip = (ip << 8) | current_octet;
            current_octet = 0;
            octet_count++;
            if (c == '\0' || c == ':' || c == '/') break;
        } else {
            return 0;
        }
    }
    if (octet_count != 4) return 0;
    return ip;
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

struct tcp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset_reserved;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed));

static int resolve_target_arp(uint32_t target_ip_addr) {
    (void)target_ip_addr;
    return 1;
}

static void get_filename_from_url(const char* url_path, char* out_filename) {
    int last_slash = -1;
    int i = 0;
    while (url_path[i] != '\0') {
        if (url_path[i] == '/') last_slash = i;
        i++;
    }
    if (last_slash != -1 && url_path[last_slash + 1] != '\0') {
        int j = 0;
        while (url_path[last_slash + 1 + j] != '\0' && j < 63) {
            out_filename[j] = url_path[last_slash + 1 + j];
            j++;
        }
        out_filename[j] = '\0';
    } else {
        const char* def = "download.bin";
        int j = 0;
        while (def[j] != '\0') {
            out_filename[j] = def[j];
            j++;
        }
        out_filename[j] = '\0';
    }
}

int vget_download(const char* url) {
    if (url[0] != 'h' || url[1] != 't' || url[2] != 't' || url[3] != 'p' || url[4] != ':' || url[5] != '/' || url[6] != '/') {
        terminal_writestring("\n[ERR] vget supports only http:// URLs\n");
        return 0;
    }

    const char* ptr = &url[7]; 
    char host[64];
    uint16_t port = 80;        
    char path[128];

    int h = 0;
    while (*ptr != '\0' && *ptr != ':' && *ptr != '/' && h < 63) {
        host[h++] = *ptr++;
    }
    host[h] = '\0';

    if (*ptr == ':') {
        ptr++;
        port = 0;
        while (*ptr >= '0' && *ptr <= '9') {
            port = port * 10 + (*ptr - '0');
            ptr++;
        }
    }

    int p = 0;
    if (*ptr == '/') {
        while (*ptr != '\0' && p < 127) {
            path[p++] = *ptr++;
        }
    } else {
        path[p++] = '/';
    }
    path[p] = '\0';

    uint32_t server_ip = parse_ip_address(host);
    if (server_ip == 0) {
        terminal_writestring("\n[ERR] Invalid host IP\n");
        return 0;
    }

    char target_filename[64];
    get_filename_from_url(path, target_filename);
    resolve_target_arp(server_ip);

    // --- KROK 1: Wysłanie pakietu SYN ---
    uint8_t tcp_tx_buf[128];
    struct ethernet_header* eth = (struct ethernet_header*)tcp_tx_buf;
    struct ip_header* ip = (struct ip_header*)(tcp_tx_buf + sizeof(struct ethernet_header));
    struct tcp_header* tcp = (struct tcp_header*)(tcp_tx_buf + sizeof(struct ethernet_header) + sizeof(struct ip_header));

    rtl8139_get_mac(local_mac);
    for(int i=0; i<6; i++) {
        eth->dest_mac[i] = target_mac[i];
        eth->src_mac[i] = local_mac[i];
    }
    eth->ethertype = htons(0x0800);

    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(sizeof(struct ip_header) + sizeof(struct tcp_header));
    ip->id = htons(1234);
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = 6;
    ip->checksum = 0;
    ip->src_ip = htonl(local_ip);
    ip->dest_ip = htonl(server_ip);
    ip->checksum = calculate_checksum(ip, sizeof(struct ip_header));

    tcp->src_port = htons(12345);
    tcp->dest_port = htons(port);
    tcp->seq_num = htonl(1000);
    tcp->ack_num = 0;
    tcp->data_offset_reserved = (5 << 4);
    tcp->flags = 0x02; // SYN
    tcp->window = htons(8192);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;

    rtl8139_send_packet(tcp_tx_buf, sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct tcp_header));
    terminal_writestring("[VGET] Sent TCP SYN, waiting for SYN-ACK...\n");

    // --- KROK 2: Oczekiwanie na SYN-ACK ---
    uint8_t rx_buffer[1500];
    int attempts = 1000000;
    int got_syn_ack = 0;
    uint32_t remote_seq = 0;

    while (attempts--) {
        int len = rtl8139_receive_packet(rx_buffer, sizeof(rx_buffer));
        if (len > 0) {
            struct ethernet_header* r_eth = (struct ethernet_header*)rx_buffer;
            if (htons(r_eth->ethertype) == 0x0800) {
                struct ip_header* r_ip = (struct ip_header*)(rx_buffer + sizeof(struct ethernet_header));
                if (r_ip->protocol == 6) {
                    int ip_header_len = (r_ip->ver_ihl & 0x0F) * 4;
                    struct tcp_header* r_tcp = (struct tcp_header*)(rx_buffer + sizeof(struct ethernet_header) + ip_header_len);
                    
                    // Sprawdzamy czy to SYN-ACK (flagi SYN=0x02 | ACK=0x10 -> 0x12)
                    if ((r_tcp->flags & 0x12) == 0x12) {
                        remote_seq = htonl(r_tcp->seq_num);
                        got_syn_ack = 1;
                        break;
                    }
                }
            }
        }
    }

    if (!got_syn_ack) {
        terminal_writestring("[ERR] Timeout: Brak odpowiedzi SYN-ACK od serwera.\n");
        return 0;
    }

    terminal_writestring("[VGET] Got SYN-ACK! Sending HTTP GET...\n");

    // --- KROK 3: Wysłanie ACK + HTTP GET (PSH + ACK) ---
    uint8_t http_tx_buf[512];
    struct ethernet_header* h_eth = (struct ethernet_header*)http_tx_buf;
    struct ip_header* h_ip = (struct ip_header*)(http_tx_buf + sizeof(struct ethernet_header));
    struct tcp_header* h_tcp = (struct tcp_header*)(http_tx_buf + sizeof(struct ethernet_header) + sizeof(struct ip_header));
    char* h_payload = (char*)(http_tx_buf + sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct tcp_header));

    for(int i=0; i<6; i++) {
        h_eth->dest_mac[i] = target_mac[i];
        h_eth->src_mac[i] = local_mac[i];
    }
    h_eth->ethertype = htons(0x0800);

    int payload_len = 0;
    const char* prefix = "GET ";
    while(*prefix) h_payload[payload_len++] = *prefix++;
    const char* pp = path;
    while(*pp) h_payload[payload_len++] = *pp++;
    const char* suffix = " HTTP/1.0\r\nHost: 10.0.2.2\r\nConnection: close\r\n\r\n";
    while(*suffix) h_payload[payload_len++] = *suffix++;

    h_ip->ver_ihl = 0x45;
    h_ip->tos = 0;
    h_ip->total_length = htons(sizeof(struct ip_header) + sizeof(struct tcp_header) + payload_len);
    h_ip->id = htons(1235);
    h_ip->flags_fragment = 0;
    h_ip->ttl = 64;
    h_ip->protocol = 6;
    h_ip->checksum = 0;
    h_ip->src_ip = htonl(local_ip);
    h_ip->dest_ip = htonl(server_ip);
    h_ip->checksum = calculate_checksum(h_ip, sizeof(struct ip_header));

    h_tcp->src_port = htons(12345);
    h_tcp->dest_port = htons(port);
    h_tcp->seq_num = htonl(1001);
    h_tcp->ack_num = htonl(remote_seq + 1);
    h_tcp->data_offset_reserved = (5 << 4);
    h_tcp->flags = 0x18; // PSH, ACK
    h_tcp->window = htons(8192);
    h_tcp->checksum = 0;
    h_tcp->urgent_ptr = 0;

    rtl8139_send_packet(http_tx_buf, sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct tcp_header) + payload_len);

    // --- KROK 4: Odbiór danych pliku z serwera ---
    terminal_writestring("[VGET] Downloading file content...\n");

    attempts = 1500000;
    static char file_buffer[2048];
    int file_len = 0;
    int got_data = 0;

    while (attempts--) {
        int len = rtl8139_receive_packet(rx_buffer, sizeof(rx_buffer));
        if (len > 0) {
            struct ethernet_header* r_eth = (struct ethernet_header*)rx_buffer;
            if (htons(r_eth->ethertype) == 0x0800) {
                struct ip_header* r_ip = (struct ip_header*)(rx_buffer + sizeof(struct ethernet_header));
                if (r_ip->protocol == 6) {
                    int ip_header_len = (r_ip->ver_ihl & 0x0F) * 4;
                    struct tcp_header* r_tcp = (struct tcp_header*)(rx_buffer + sizeof(struct ethernet_header) + ip_header_len);
                    
                    int tcp_header_len = ((r_tcp->data_offset_reserved >> 4) & 0x0F) * 4;
                    int total_headers_len = sizeof(struct ethernet_header) + ip_header_len + tcp_header_len;
                    
                    if (len > total_headers_len) {
                        char* payload = (char*)(rx_buffer + total_headers_len);
                        int p_len = len - total_headers_len;
                        for (int i = 0; i < p_len && file_len < 2047; i++) {
                            file_buffer[file_len++] = payload[i];
                        }
                        got_data = 1;
                    }
                }
            }
        }
    }

    if (!got_data || file_len == 0) {
        terminal_writestring("[ERR] Timeout: Brak danych od serwera HTTP.\n");
        return 0;
    }

    file_buffer[file_len] = '\0';

    // Wyszukiwanie początku ZIPa (PK\x03\x04)
    char* file_data = file_buffer;
    uint32_t final_size = file_len;

    for (int i = 0; i < file_len - 4; i++) {
        if (file_buffer[i] == 'P' && file_buffer[i+1] == 'K' && file_buffer[i+2] == 0x03 && file_buffer[i+3] == 0x04) {
            file_data = &file_buffer[i];
            final_size = file_len - i;
            break;
        }
    }

    int res = fat16_write_file(target_filename, file_data, final_size);
    if (res == 1) {
        terminal_writestring("[OK] Plik ZIP pobrany i zapisany poprawnie!\n");
        return 1;
    }

    terminal_writestring("[ERR] Zapis na dysk FAT16 nie powiódł się.\n");
    return 0;
}