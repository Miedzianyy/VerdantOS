#include <stdint.h>

void terminal_writestring(const char* data);
int fat16_read_file(const char* filename, char* buffer);
int fat16_write_file(const char* filename, const char* content, uint32_t size);

// Struktura lokalnego nagłówka pliku w archiwum ZIP
typedef struct {
    uint32_t signature;       // 0x04034B50
    uint16_t version;
    uint16_t flags;
    uint16_t compression;     // 0 = STORE (brak kompresji), 8 = DEFLATE
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_length;
    uint16_t extra_field_length;
} __attribute__((packed)) zip_local_header_t;

// Bufor na cały plik ZIP w pamięci RAM kernela (np. do 64KB)
static char zip_buffer[65536];

int zip_extract(const char* archive_name) {
    // Odczytujemy plik ZIP z dysku FAT16 do bufora
    if (!fat16_read_file(archive_name, zip_buffer)) {
        return 0;
    }

    uint32_t offset = 0;
    int files_extracted = 0;

    // Przeszukujemy bufor w poszukiwaniu nagłówków plików w ZIP
    while (offset < sizeof(zip_buffer) - sizeof(zip_local_header_t)) {
        zip_local_header_t* header = (zip_local_header_t*)&zip_buffer[offset];

        // Sprawdzamy sygnaturę lokalnego nagłówka pliku ZIP: PK 03 04
        if (header->signature != 0x04034B50) {
            break; // Koniec archiwum lub brak kolejnych plików
        }

        // Odczytujemy nazwę pliku wewnątrz archiwum
        char entry_name[64];
        uint16_t name_len = header->filename_length;
        if (name_len > 63) name_len = 63;

        char* name_ptr = &zip_buffer[offset + sizeof(zip_local_header_t)];
        for (uint16_t i = 0; i < name_len; i++) {
            entry_name[i] = name_ptr[i];
        }
        entry_name[name_len] = '\0';

        // Wyznaczamy miejsce, gdzie zaczynają się dane pliku
        uint32_t data_offset = offset + sizeof(zip_local_header_t) + header->filename_length + header->extra_field_length;
        char* file_data = &zip_buffer[data_offset];

        // Obsługujemy pliki bez kompresji (Method 0 - STORE)
        if (header->compression == 0) {
            terminal_writestring("  Extracting: ");
            terminal_writestring(entry_name);
            terminal_writestring("\n");

            fat16_write_file(entry_name, file_data, header->uncompressed_size);
            files_extracted++;
        } else {
            terminal_writestring("  [WARN] Skipped (Compressed files require DEFLATE): ");
            terminal_writestring(entry_name);
            terminal_writestring("\n");
        }

        // Przesuwamy offset do kolejnego pliku w archiwum
        offset = data_offset + header->compressed_size;
    }

    return files_extracted > 0;
}