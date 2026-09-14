#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

// Główna struktura BPB (BIOS Parameter Block) systemu plików FAT16
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

// Struktura wpisu w katalogu (Directory Entry)
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

// Deklaracje funkcji systemu plików FAT16
void fat16_init_disk(void);
void fat16_format_disk(void);
void fat16_mkfs(void);

int fat16_read_sector(uint32_t lba, char* buffer);
int fat16_write_sector(uint32_t lba, const char* buffer);

uint16_t fat16_get_fat_entry(uint32_t cluster);
void fat16_set_fat_entry(uint32_t cluster, uint16_t value);
uint16_t fat16_find_free_cluster(void);

void format_long_name(const char* input, char* output);

int fat16_write_file(const char* filename, const char* content, uint32_t size);
int fat16_read_file(const char* filename, char* buffer);
int fat16_make_directory(const char* dirname);
void fat16_list_directory(void);
int fat16_change_directory(const char* dirname);
int fat16_is_valid_filename(const char* name);
int fat16_delete_entry(const char* filename);

#endif