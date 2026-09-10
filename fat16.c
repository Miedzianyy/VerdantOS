#include <stdint.h>

void terminal_writestring(const char* data);

typedef struct {
    uint8_t  boot_jump[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;      
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  file_system_type[8];
} __attribute__((packed)) fat16_bpb_t;

typedef struct __attribute__((packed)) {
    char     name[64];          
    uint8_t  attr;              
    uint8_t  reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high; 
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t first_cluster_low;  
    uint32_t file_size;          
} fat16_dir_entry_t;

uint32_t g_fat_start_lba = 1;
uint32_t g_root_dir_lba = 65;
uint32_t g_root_sectors = 32;
uint32_t g_data_start_lba = 97;
uint32_t g_sectors_per_fat = 32;
uint8_t  g_sectors_per_cluster = 1;
uint32_t g_total_sectors = 2880; 

static uint32_t g_current_dir_cluster = 0;

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_SECTOR_COUNT 0x1F2
#define ATA_PRIMARY_LBA_LOW      0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HIGH     0x1F5
#define ATA_PRIMARY_DRIVE_HEAD   0x1F6
#define ATA_PRIMARY_COMMAND      0x1F7

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "d"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}

static inline void inw(uint16_t port, void* addr, int count) {
    __asm__ volatile ("cld; rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

int fat16_read_sector(uint32_t lba, char* buffer) {
    int timeout = 100000;
    while ((inb(ATA_PRIMARY_COMMAND) & 0x80) && --timeout);
    if (timeout == 0) return 0;

    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECTOR_COUNT, 1);
    outb(ATA_PRIMARY_LBA_LOW, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_COMMAND, 0x20);

    timeout = 100000;
    while (!(inb(ATA_PRIMARY_COMMAND) & 0x08) && --timeout);
    if (timeout == 0) return 0;

    inw(ATA_PRIMARY_DATA, buffer, 256);
    return 1;
}

int fat16_write_sector(uint32_t lba, const char* buffer) {
    int timeout = 100000;
    while ((inb(ATA_PRIMARY_COMMAND) & 0x80) && --timeout);
    if (timeout == 0) return 0;

    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECTOR_COUNT, 1);
    outb(ATA_PRIMARY_LBA_LOW, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_COMMAND, 0x30);

    timeout = 100000;
    while (!(inb(ATA_PRIMARY_COMMAND) & 0x08) && --timeout);
    if (timeout == 0) return 0;

    const unsigned short* ptr = (const unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        __asm__ volatile ("outw %0, %1" : : "a"(ptr[i]), "d"((uint16_t)0x1F0));
    }
    
    outb(ATA_PRIMARY_COMMAND, 0xE7);
    while (inb(ATA_PRIMARY_COMMAND) & 0x80);

    return 1;
}

void fat16_init_disk(void) {
    char boot_sector[512];
    g_current_dir_cluster = 0;
    if (!fat16_read_sector(0, boot_sector)) return;

    fat16_bpb_t* bpb = (fat16_bpb_t*)boot_sector;
    if (bpb->bytes_per_sector == 512 && bpb->sectors_per_fat > 0) {
        g_sectors_per_cluster = bpb->sectors_per_cluster ? bpb->sectors_per_cluster : 1;
        g_fat_start_lba = bpb->reserved_sectors;
        g_sectors_per_fat = bpb->sectors_per_fat;
        g_root_sectors = ((bpb->root_entry_count * 32) + 511) / 512;
        g_root_dir_lba = g_fat_start_lba + (bpb->fat_count * g_sectors_per_fat);
        g_data_start_lba = g_root_dir_lba + g_root_sectors;
        g_total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;
    }
}

void fat16_format_disk(void) {
    char sector_buf[512];
    for (int i = 0; i < 512; i++) sector_buf[i] = 0;
    for (uint32_t s = 0; s < 120; s++) fat16_write_sector(s, sector_buf);
    terminal_writestring("[FAT16] format: Wyczyszczono obszar dysku.\n");
}

void fat16_mkfs(void) {
    char sector_buf[512];
    for (int i = 0; i < 512; i++) sector_buf[i] = 0;

    fat16_bpb_t* bpb = (fat16_bpb_t*)sector_buf;
    bpb->boot_jump[0] = 0xEB; bpb->boot_jump[1] = 0x3C; bpb->boot_jump[2] = 0x90;
    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = 1;
    bpb->reserved_sectors = 1;
    bpb->fat_count = 2;
    bpb->root_entry_count = 512;
    bpb->sectors_per_fat = 32;
    bpb->media_type = 0xF8;
    bpb->total_sectors_16 = 2880;
    bpb->boot_signature = 0x29;

    fat16_write_sector(0, sector_buf);

    for (int i = 0; i < 512; i++) sector_buf[i] = 0;
    uint16_t* fat_table = (uint16_t*)sector_buf;
    fat_table[0] = 0xFFF8;
    fat_table[1] = 0xFFFF;

    fat16_write_sector(g_fat_start_lba, sector_buf);
    fat16_write_sector(g_fat_start_lba + bpb->sectors_per_fat, sector_buf);

    fat16_init_disk();
    terminal_writestring("[FAT16] mkfs: Utworzono strukturę systemu plików!\n");
}

uint16_t fat16_get_fat_entry(uint32_t cluster) {
    char sector_buf[512];
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = g_fat_start_lba + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    fat16_read_sector(fat_sector, sector_buf);
    return *(uint16_t*)&sector_buf[ent_offset];
}

void fat16_set_fat_entry(uint32_t cluster, uint16_t value) {
    char sector_buf[512];
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = g_fat_start_lba + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    for (int i = 0; i < 2; i++) {
        uint32_t target_sector = fat_sector + (i * g_sectors_per_fat);
        fat16_read_sector(target_sector, sector_buf);
        *(uint16_t*)&sector_buf[ent_offset] = value;
        fat16_write_sector(target_sector, sector_buf);
    }
}

uint16_t fat16_find_free_cluster(void) {
    uint32_t data_sectors = (g_total_sectors > g_data_start_lba) ? (g_total_sectors - g_data_start_lba) : 0;
    uint32_t max_cluster = 2 + (data_sectors / g_sectors_per_cluster);
    uint32_t table_limit = g_sectors_per_fat * 256;
    if (max_cluster > table_limit) max_cluster = table_limit;
    
    for (uint32_t cluster = 2; cluster < max_cluster; cluster++) {
        if (fat16_get_fat_entry(cluster) == 0x0000) {
            return (uint16_t)cluster;
        }
    }
    return 0;
}

void format_long_name(const char* input, char* output) {
    for (int i = 0; i < 64; i++) output[i] = '\0';
    int i = 0;
    while (input[i] != '\0' && i < 63) {
        char c = input[i];
        if (c >= 'a' && c <= 'z') c -= 32; 
        output[i] = c;
        i++;
    }
}

int fat16_write_file(const char* filename, const char* content, uint32_t size) {
    char sector_buf[512];
    char formatted_name[64];
    format_long_name(filename, formatted_name);

    int entries_per_sector = 512 / sizeof(fat16_dir_entry_t);

    if (g_current_dir_cluster == 0) {
        for (uint32_t s = 0; s < g_root_sectors; s++) {
            uint32_t sector_lba = g_root_dir_lba + s;
            if (!fat16_read_sector(sector_lba, sector_buf)) continue;
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

            for (int i = 0; i < entries_per_sector; i++) {
                if ((uint8_t)entries[i].name[0] == 0x00) break;
                if ((uint8_t)entries[i].name[0] == 0xE5) continue;

                int match = 1;
                for (int k = 0; k < 64; k++) {
                    if (entries[i].name[k] != formatted_name[k]) { match = 0; break; }
                }
                if (match) return -1; 
            }
        }
    } else {
        uint16_t curr_cluster = g_current_dir_cluster;
        while (curr_cluster >= 2 && curr_cluster < 0xFFF8) {
            for (uint8_t sec = 0; sec < g_sectors_per_cluster; sec++) {
                uint32_t sector_lba = g_data_start_lba + ((curr_cluster - 2) * g_sectors_per_cluster) + sec;
                if (!fat16_read_sector(sector_lba, sector_buf)) continue;
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

                for (int i = 0; i < entries_per_sector; i++) {
                    if ((uint8_t)entries[i].name[0] == 0x00) break;
                    if ((uint8_t)entries[i].name[0] == 0xE5) continue;

                    int match = 1;
                    for (int k = 0; k < 64; k++) {
                        if (entries[i].name[k] != formatted_name[k]) { match = 0; break; }
                    }
                    if (match) return -1;
                }
            }
            curr_cluster = fat16_get_fat_entry(curr_cluster);
        }
    }

    uint32_t clusters_needed = (size + 511) / 512;
    if (clusters_needed == 0) clusters_needed = 1;

    uint16_t first_cluster = 0, prev_cluster = 0;
    uint32_t remaining = size;
    const char* src = content;

    for (uint32_t c = 0; c < clusters_needed; c++) {
        uint16_t cluster = fat16_find_free_cluster();
        if (cluster == 0) return 0;

        fat16_set_fat_entry(cluster, 0xFFFF);

        if (prev_cluster != 0) {
            fat16_set_fat_entry(prev_cluster, cluster);
        } else {
            first_cluster = cluster;
        }

        char data_sector[512];
        for (int i = 0; i < 512; i++) data_sector[i] = 0;

        uint32_t chunk = remaining < 512 ? remaining : 512;
        for (uint32_t i = 0; i < chunk; i++) data_sector[i] = src[i];

        uint32_t data_lba = g_data_start_lba + ((cluster - 2) * g_sectors_per_cluster);
        fat16_write_sector(data_lba, data_sector);

        src += chunk;
        remaining -= chunk;
        prev_cluster = cluster;
    }

    if (g_current_dir_cluster == 0) {
        for (uint32_t s = 0; s < g_root_sectors; s++) {
            uint32_t sector_lba = g_root_dir_lba + s;
            if (!fat16_read_sector(sector_lba, sector_buf)) continue;
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

            int target_entry = -1;
            for (int i = 0; i < entries_per_sector; i++) {
                if ((uint8_t)entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                    target_entry = i;
                    break;
                }
            }

            if (target_entry != -1) {
                for (int k = 0; k < 64; k++) entries[target_entry].name[k] = formatted_name[k];
                entries[target_entry].attr = 0x20;
                entries[target_entry].file_size = size;
                entries[target_entry].first_cluster_low = first_cluster;

                fat16_write_sector(sector_lba, sector_buf);
                return 1;
            }
        }
    } else {
        uint16_t curr_cluster = g_current_dir_cluster;
        while (curr_cluster >= 2 && curr_cluster < 0xFFF8) {
            for (uint8_t sec = 0; sec < g_sectors_per_cluster; sec++) {
                uint32_t sector_lba = g_data_start_lba + ((curr_cluster - 2) * g_sectors_per_cluster) + sec;
                if (!fat16_read_sector(sector_lba, sector_buf)) continue;
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

                int target_entry = -1;
                for (int i = 0; i < entries_per_sector; i++) {
                    if ((uint8_t)entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                        target_entry = i;
                        break;
                    }
                }

                if (target_entry != -1) {
                    for (int k = 0; k < 64; k++) entries[target_entry].name[k] = formatted_name[k];
                    entries[target_entry].attr = 0x20;
                    entries[target_entry].file_size = size;
                    entries[target_entry].first_cluster_low = first_cluster;

                    fat16_write_sector(sector_lba, sector_buf);
                    return 1;
                }
            }
            curr_cluster = fat16_get_fat_entry(curr_cluster);
        }
    }
    return 0;
}

int fat16_make_directory(const char* dirname) {
    char sector_buf[512];
    char formatted_name[64];
    format_long_name(dirname, formatted_name);

    int entries_per_sector = 512 / sizeof(fat16_dir_entry_t);

    uint16_t cluster = fat16_find_free_cluster();
    if (cluster == 0) return 0;
    fat16_set_fat_entry(cluster, 0xFFFF);

    char dir_sector[512];
    for (int i = 0; i < 512; i++) dir_sector[i] = 0;
    uint32_t data_lba = g_data_start_lba + ((cluster - 2) * g_sectors_per_cluster);
    fat16_write_sector(data_lba, dir_sector);

    if (g_current_dir_cluster == 0) {
        for (uint32_t s = 0; s < g_root_sectors; s++) {
            uint32_t sector_lba = g_root_dir_lba + s;
            if (!fat16_read_sector(sector_lba, sector_buf)) continue;
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

            int target_entry = -1;
            for (int i = 0; i < entries_per_sector; i++) {
                if ((uint8_t)entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                    target_entry = i;
                    break;
                }
            }

            if (target_entry != -1) {
                for (int k = 0; k < 64; k++) entries[target_entry].name[k] = formatted_name[k];
                entries[target_entry].attr = 0x10;
                entries[target_entry].file_size = 0;
                entries[target_entry].first_cluster_low = cluster;

                fat16_write_sector(sector_lba, sector_buf);
                return 1;
            }
        }
    } else {
        uint16_t curr_cluster = g_current_dir_cluster;
        while (curr_cluster >= 2 && curr_cluster < 0xFFF8) {
            for (uint8_t sec = 0; sec < g_sectors_per_cluster; sec++) {
                uint32_t sector_lba = g_data_start_lba + ((curr_cluster - 2) * g_sectors_per_cluster) + sec;
                if (!fat16_read_sector(sector_lba, sector_buf)) continue;
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

                int target_entry = -1;
                for (int i = 0; i < entries_per_sector; i++) {
                    if ((uint8_t)entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                        target_entry = i;
                        break;
                    }
                }

                if (target_entry != -1) {
                    for (int k = 0; k < 64; k++) entries[target_entry].name[k] = formatted_name[k];
                    entries[target_entry].attr = 0x10;
                    entries[target_entry].file_size = 0;
                    entries[target_entry].first_cluster_low = cluster;

                    fat16_write_sector(sector_lba, sector_buf);
                    return 1;
                }
            }
            curr_cluster = fat16_get_fat_entry(curr_cluster);
        }
    }
    return 0;
}

int fat16_read_file(const char* filename, char* buffer) {
    char sector_buf[512];
    char formatted_name[64];
    format_long_name(filename, formatted_name);

    int entries_per_sector = 512 / sizeof(fat16_dir_entry_t);

    if (g_current_dir_cluster == 0) {
        for (uint32_t s = 0; s < g_root_sectors; s++) {
            uint32_t sector_lba = g_root_dir_lba + s;
            if (!fat16_read_sector(sector_lba, sector_buf)) continue;
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

            for (int i = 0; i < entries_per_sector; i++) {
                if ((uint8_t)entries[i].name[0] == 0x00) break;
                if ((uint8_t)entries[i].name[0] == 0xE5) continue;

                int match = 1;
                for (int k = 0; k < 64; k++) {
                    if (entries[i].name[k] != formatted_name[k]) { match = 0; break; }
                }

                if (match) {
                    uint16_t cluster = entries[i].first_cluster_low;
                    uint32_t file_size = entries[i].file_size;
                    
                    if (file_size == 0) file_size = 512;

                    uint32_t written = 0;

                    while (cluster >= 2 && cluster < 0xFFF8 && written < file_size) {
                        uint32_t data_lba = g_data_start_lba + ((cluster - 2) * g_sectors_per_cluster);
                        char data_sector[512];
                        if (!fat16_read_sector(data_lba, data_sector)) return 0;

                        uint32_t chunk = file_size - written;
                        if (chunk > 512) chunk = 512;

                        for (uint32_t j = 0; j < chunk; j++) {
                            if (data_sector[j] == '\0') break;
                            buffer[written + j] = data_sector[j];
                        }
                        written += chunk;

                        cluster = fat16_get_fat_entry(cluster);
                    }
                    buffer[written] = '\0';
                    return 1;
                }
            }
        }
    } else {
        uint16_t curr_cluster = g_current_dir_cluster;
        while (curr_cluster >= 2 && curr_cluster < 0xFFF8) {
            for (uint8_t sec = 0; sec < g_sectors_per_cluster; sec++) {
                uint32_t sector_lba = g_data_start_lba + ((curr_cluster - 2) * g_sectors_per_cluster) + sec;
                if (!fat16_read_sector(sector_lba, sector_buf)) continue;
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

                for (int i = 0; i < entries_per_sector; i++) {
                    if ((uint8_t)entries[i].name[0] == 0x00) break;
                    if ((uint8_t)entries[i].name[0] == 0xE5) continue;

                    int match = 1;
                    for (int k = 0; k < 64; k++) {
                        if (entries[i].name[k] != formatted_name[k]) { match = 0; break; }
                    }

                    if (match) {
                        uint16_t cluster = entries[i].first_cluster_low;
                        uint32_t file_size = entries[i].file_size;
                        
                        if (file_size == 0) file_size = 512;

                        uint32_t written = 0;

                        while (cluster >= 2 && cluster < 0xFFF8 && written < file_size) {
                            uint32_t data_lba = g_data_start_lba + ((cluster - 2) * g_sectors_per_cluster);
                            char data_sector[512];
                            if (!fat16_read_sector(data_lba, data_sector)) return 0;

                            uint32_t chunk = file_size - written;
                            if (chunk > 512) chunk = 512;

                            for (uint32_t j = 0; j < chunk; j++) {
                                if (data_sector[j] == '\0') break;
                                buffer[written + j] = data_sector[j];
                            }
                            written += chunk;

                            cluster = fat16_get_fat_entry(cluster);
                        }
                        buffer[written] = '\0';
                        return 1;
                    }
                }
            }
            curr_cluster = fat16_get_fat_entry(curr_cluster);
        }
    }
    return 0;
}

void fat16_list_directory(void) {
    char sector_buf[512];
    int entries_per_sector = 512 / sizeof(fat16_dir_entry_t);
    
    terminal_writestring("\n--- Zawartosc katalogu ---\n");
    if (g_current_dir_cluster == 0) {
        for (uint32_t s = 0; s < g_root_sectors; s++) {
            if (!fat16_read_sector(g_root_dir_lba + s, sector_buf)) break;
            
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;
            for (int i = 0; i < entries_per_sector; i++) {
                if ((uint8_t)entries[i].name[0] == 0x00) break;
                if ((uint8_t)entries[i].name[0] == 0xE5) continue;
                if (entries[i].attr & 0x08) continue;
                
                terminal_writestring("  ");
                terminal_writestring(entries[i].name);
                if (entries[i].attr & 0x10) terminal_writestring("   <DIR>");
                terminal_writestring("\n");
            }
        }
    } else {
        uint16_t curr_cluster = g_current_dir_cluster;
        while (curr_cluster >= 2 && curr_cluster < 0xFFF8) {
            for (uint8_t sec = 0; sec < g_sectors_per_cluster; sec++) {
                uint32_t sector_lba = g_data_start_lba + ((curr_cluster - 2) * g_sectors_per_cluster) + sec;
                if (!fat16_read_sector(sector_lba, sector_buf)) break;

                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;
                for (int i = 0; i < entries_per_sector; i++) {
                    if ((uint8_t)entries[i].name[0] == 0x00) break;
                    if ((uint8_t)entries[i].name[0] == 0xE5) continue;
                    if (entries[i].attr & 0x08) continue;

                    terminal_writestring("  ");
                    terminal_writestring(entries[i].name);
                    if (entries[i].attr & 0x10) terminal_writestring("   <DIR>");
                    terminal_writestring("\n");
                }
            }
            curr_cluster = fat16_get_fat_entry(curr_cluster);
        }
    }
    terminal_writestring("---------------------------\n");
}

int fat16_change_directory(const char* dirname) {
    if (dirname[0] == '.' && dirname[1] == '.') {
        g_current_dir_cluster = 0;
        return 1;
    }

    char sector_buf[512];
    char formatted_name[64];
    format_long_name(dirname, formatted_name);
    int entries_per_sector = 512 / sizeof(fat16_dir_entry_t);

    if (g_current_dir_cluster == 0) {
        for (uint32_t s = 0; s < g_root_sectors; s++) {
            if (!fat16_read_sector(g_root_dir_lba + s, sector_buf)) continue;
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

            for (int i = 0; i < entries_per_sector; i++) {
                if ((uint8_t)entries[i].name[0] == 0x00) break;
                int match = 1;
                for (int k = 0; k < 64; k++) {
                    if (entries[i].name[k] != formatted_name[k]) { match = 0; break; }
                }
                if (match) {
                    if (entries[i].attr & 0x10) {
                        g_current_dir_cluster = entries[i].first_cluster_low;
                        return 1;
                    }
                    return 0;
                }
            }
        }
    } else {
        uint16_t curr_cluster = g_current_dir_cluster;
        while (curr_cluster >= 2 && curr_cluster < 0xFFF8) {
            for (uint8_t sec = 0; sec < g_sectors_per_cluster; sec++) {
                uint32_t sector_lba = g_data_start_lba + ((curr_cluster - 2) * g_sectors_per_cluster) + sec;
                if (!fat16_read_sector(sector_lba, sector_buf)) continue;
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

                for (int i = 0; i < entries_per_sector; i++) {
                    if ((uint8_t)entries[i].name[0] == 0x00) break;
                    int match = 1;
                    for (int k = 0; k < 64; k++) {
                        if (entries[i].name[k] != formatted_name[k]) { match = 0; break; }
                    }
                    if (match) {
                        if (entries[i].attr & 0x10) {
                            g_current_dir_cluster = entries[i].first_cluster_low;
                            return 1;
                        }
                        return 0;
                    }
                }
            }
            curr_cluster = fat16_get_fat_entry(curr_cluster);
        }
    }
    return 0;
}

int fat16_is_valid_filename(const char* name) {
    if (!name || name[0] == '\0') return 0;
    int len = 0;
    while (name[len] != '\0') {
        if (len >= 63) return 0;
        len++;
    }
    return 1;
}

int fat16_delete_entry(const char* filename) {
    char sector_buf[512];
    char formatted_name[64];
    format_long_name(filename, formatted_name);
    int entries_per_sector = 512 / sizeof(fat16_dir_entry_t);

    if (g_current_dir_cluster == 0) {
        for (uint32_t s = 0; s < g_root_sectors; s++) {
            uint32_t sector_lba = g_root_dir_lba + s;
            if (!fat16_read_sector(sector_lba, sector_buf)) continue;
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

            for (int i = 0; i < entries_per_sector; i++) {
                if ((uint8_t)entries[i].name[0] == 0x00) break;
                if ((uint8_t)entries[i].name[0] == 0xE5) continue;

                int match = 1;
                for (int k = 0; k < 64; k++) {
                    if (entries[i].name[k] != formatted_name[k]) { match = 0; break; }
                }

                if (match) {
                    entries[i].name[0] = (char)0xE5;
                    
                    uint16_t cluster = entries[i].first_cluster_low;
                    while (cluster >= 2 && cluster < 0xFFF8) {
                        uint16_t next = fat16_get_fat_entry(cluster);
                        fat16_set_fat_entry(cluster, 0x0000);
                        cluster = next;
                    }

                    fat16_write_sector(sector_lba, sector_buf);
                    return 1;
                }
            }
        }
    } else {
        uint16_t curr_cluster = g_current_dir_cluster;
        while (curr_cluster >= 2 && curr_cluster < 0xFFF8) {
            for (uint8_t sec = 0; sec < g_sectors_per_cluster; sec++) {
                uint32_t sector_lba = g_data_start_lba + ((curr_cluster - 2) * g_sectors_per_cluster) + sec;
                if (!fat16_read_sector(sector_lba, sector_buf)) continue;
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

                for (int i = 0; i < entries_per_sector; i++) {
                    if ((uint8_t)entries[i].name[0] == 0x00) break;
                    if ((uint8_t)entries[i].name[0] == 0xE5) continue;

                    int match = 1;
                    for (int k = 0; k < 64; k++) {
                        if (entries[i].name[k] != formatted_name[k]) { match = 0; break; }
                    }

                    if (match) {
                        entries[i].name[0] = (char)0xE5;
                        uint16_t cluster = entries[i].first_cluster_low;
                        while (cluster >= 2 && cluster < 0xFFF8) {
                            uint16_t next = fat16_get_fat_entry(cluster);
                            fat16_set_fat_entry(cluster, 0x0000);
                            cluster = next;
                        }
                        fat16_write_sector(sector_lba, sector_buf);
                        return 1;
                    }
                }
            }
            curr_cluster = fat16_get_fat_entry(curr_cluster);
        }
    }
    return 0;
}