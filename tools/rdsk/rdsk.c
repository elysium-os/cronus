#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAJOR_REVISION 1
#define MINOR_REVISION 1

#define SECTION_ALIGNMENT 64
#define ALIGN_UP(VALUE, ALIGN) ((VALUE) % (ALIGN) > 0 ? (VALUE) + ((ALIGN) - (VALUE) % (ALIGN)) : (VALUE))

typedef struct {
    char signature[4];
    uint16_t revision;
    uint16_t header_size;
    uint64_t root_index;
    uint64_t nametable_offset;
    uint64_t nametable_size;
    uint16_t dirtable_entry_size;
    uint64_t dirtable_entry_count;
    uint64_t dirtable_offset;
    uint16_t filetable_entry_size;
    uint64_t filetable_entry_count;
    uint64_t filetable_offset;
} __attribute__((packed)) rdsk_header_t;

typedef struct {
    bool used;
    uint64_t nametable_offset;
    uint64_t data_offset;
    uint64_t size;
    uint64_t next_index;
    uint64_t parent_index;
} __attribute__((packed)) rdsk_file_t;

typedef struct {
    bool used;
    uint64_t nametable_offset;
    uint64_t filetable_index;
    uint64_t dirtable_index;
    uint64_t next_index;
    uint64_t parent_index;
} __attribute__((packed)) rdsk_dir_t;

FILE *g_file;
uint64_t g_data_offset;

rdsk_header_t *g_header;

rdsk_dir_t *g_dirtable;
size_t g_dirtable_size;

rdsk_file_t *g_filetable;
size_t g_filetable_size;

char *g_nametable;
size_t g_nametable_used;
size_t g_nametable_size;

/* UTIL - NAMETABLE */
static uint64_t nametable_write(const char *string) {
    if(g_nametable_used + strlen(string) + 1 > g_nametable_size) {
        char *new_table = malloc(g_nametable_size + strlen(string) + 1 + 32);
        memset(new_table, 0, g_nametable_size + strlen(string) + 1 + 32);
        memcpy(new_table, g_nametable, g_nametable_size);
        free(g_nametable);
        g_nametable = new_table;
        g_nametable_size += strlen(string) + 1 + 32;
    }
    uint64_t offset = g_nametable_used;
    strcpy(&g_nametable[offset], string);
    g_nametable_used += strlen(string) + 1;
    return offset;
}

/* UTIL - DIRTABLE */
static rdsk_dir_t *dirtable_get(uint64_t index) {
    return &g_dirtable[index - 1];
}

static uint64_t dirtable_create(char *name, uint64_t parent_index) {
    bool found = false;
    size_t i = 0;
    for(; i < g_dirtable_size; i++) {
        if(g_dirtable[i].used) continue;
        found = true;
        break;
    }
    if(!found) {
        rdsk_dir_t *new_table = malloc(sizeof(rdsk_dir_t) * (g_dirtable_size + 16));
        memset(new_table, 0, sizeof(rdsk_dir_t) * (g_dirtable_size + 16));
        memcpy(new_table, g_dirtable, sizeof(rdsk_dir_t) * g_dirtable_size);
        free(g_dirtable);
        g_dirtable = new_table;
        g_dirtable_size += 16;
        return dirtable_create(name, parent_index);
    }
    g_dirtable[i].used = true;
    g_dirtable[i].next_index = dirtable_get(parent_index)->dirtable_index;
    dirtable_get(parent_index)->dirtable_index = i + 1;
    g_dirtable[i].nametable_offset = nametable_write(name);
    g_dirtable[i].parent_index = parent_index;
    return i + 1;
}

/* UTIL - FILETABLE */
static uint64_t filetable_create(char *name, uint64_t parent_index, uint64_t off, uint64_t size) {
    bool found = false;
    size_t i = 0;
    for(; i < g_filetable_size; i++) {
        if(g_filetable[i].used) continue;
        found = true;
        break;
    }
    if(!found) {
        rdsk_file_t *new_table = malloc(sizeof(rdsk_file_t) * (g_filetable_size + 16));
        memset(new_table, 0, sizeof(rdsk_file_t) * (g_filetable_size + 16));
        memcpy(new_table, g_filetable, sizeof(rdsk_file_t) * g_filetable_size);
        free(g_filetable);
        g_filetable = new_table;
        g_filetable_size += 16;
        return filetable_create(name, parent_index, off, size);
    }
    g_filetable[i].used = true;
    g_filetable[i].next_index = dirtable_get(parent_index)->filetable_index;
    dirtable_get(parent_index)->filetable_index = i + 1;
    g_filetable[i].nametable_offset = nametable_write(name);
    g_filetable[i].parent_index = parent_index;
    g_filetable[i].size = size;
    g_filetable[i].data_offset = off;
    return i + 1;
}

/* CREATE */
static void create_recurse(const char *name, uint64_t cur_dir_index) {
    DIR *dir = opendir(name);
    if(errno != 0) {
        printf("Failed to pack dir %s (%s)\n", name, strerror(errno));
        return;
    }

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        char path[1024];
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);

        if(entry->d_type == DT_DIR) {
            printf("Packing dir %s\n", path);
            create_recurse(path, dirtable_create(entry->d_name, cur_dir_index));
        } else {
            printf("Packing file %s\n", path);
            FILE *file = fopen(path, "r");
            if(errno != 0) {
                printf("Failed to pack file %s (%s)\n", path, strerror(errno));
                continue;
            }
            struct stat s;
            stat(path, &s);
            if(errno != 0) {
                fclose(file);
                printf("Failed to pack file %s (%s)\n", path, strerror(errno));
                continue;
            }
            void *buf = malloc(s.st_size);
            fseek(file, 0, SEEK_SET);
            if(fread(buf, s.st_size, 1, file) != 1) {
                fclose(file);
                printf("Failed to read file %s (%s)\n", path, strerror(errno));
                continue;
            }
            fclose(file);

            uint64_t off = g_data_offset;
            fseek(g_file, g_data_offset, SEEK_SET);
            fwrite(buf, s.st_size, 1, g_file);
            free(buf);
            g_data_offset += s.st_size;
            filetable_create(entry->d_name, cur_dir_index, off, s.st_size);
        }
    }
    closedir(dir);
}

static void create(char *dir_path, char *out_path) {
    printf("Creating RDSK from \"%s\"\n", dir_path);
    if(dir_path == NULL || *dir_path == '\0') {
        errno = EINVAL;
        return;
    }

    g_file = fopen(out_path, "w+");
    if(errno != 0) return;
    g_data_offset = ALIGN_UP(sizeof(rdsk_header_t), SECTION_ALIGNMENT);
    fseek(g_file, g_data_offset, SEEK_SET);

    g_dirtable_size = 0;
    g_dirtable = NULL;
    g_filetable_size = 0;
    g_filetable = NULL;
    g_nametable_used = 0;
    g_nametable_size = 0;
    g_nametable = NULL;

    g_header = malloc(sizeof(rdsk_header_t));
    memset(g_header, 0, sizeof(rdsk_header_t));
    g_header->signature[0] = 'R';
    g_header->signature[1] = 'D';
    g_header->signature[2] = 'S';
    g_header->signature[3] = 'K';
    g_header->revision = (MAJOR_REVISION << 8) | MINOR_REVISION;
    g_header->header_size = sizeof(rdsk_header_t);
    g_header->root_index = dirtable_create("RDSK", 0);

    create_recurse(dir_path, g_header->root_index);

    g_header->nametable_offset = ALIGN_UP(g_data_offset, SECTION_ALIGNMENT);
    g_header->nametable_size = g_nametable_size;
    g_header->dirtable_entry_size = sizeof(rdsk_dir_t);
    g_header->dirtable_entry_count = g_dirtable_size;
    g_header->dirtable_offset = ALIGN_UP(g_header->nametable_offset + g_header->nametable_size, SECTION_ALIGNMENT);
    g_header->filetable_entry_size = sizeof(rdsk_dir_t);
    g_header->filetable_entry_count = g_filetable_size;
    g_header->filetable_offset = ALIGN_UP(g_header->dirtable_offset + g_header->dirtable_entry_count * g_header->dirtable_entry_size, SECTION_ALIGNMENT);

    fseek(g_file, 0, SEEK_SET);
    fwrite(g_header, sizeof(rdsk_header_t), 1, g_file);

    fseek(g_file, g_header->nametable_offset, SEEK_SET);
    fwrite(g_nametable, 1, g_header->nametable_size, g_file);

    fseek(g_file, g_header->dirtable_offset, SEEK_SET);
    fwrite(g_dirtable, sizeof(rdsk_dir_t), g_dirtable_size, g_file);

    fseek(g_file, g_header->filetable_offset, SEEK_SET);
    fwrite(g_filetable, sizeof(rdsk_file_t), g_filetable_size, g_file);

    fclose(g_file);

    free(g_header);
    free(g_nametable);
    free(g_dirtable);
    free(g_filetable);
}

int main(int argc, char **argv) {
    printf("RDSK Rev %u.%u\n", MAJOR_REVISION, MINOR_REVISION);
    enum {
        MODE_NONE,
        MODE_CREATE
    } mode = MODE_NONE;
    char *create_path = NULL;
    char *output_path = NULL;

    char c;
    while((c = getopt(argc, argv, "c:o:")) != -1) {
        switch(c) {
            case 'c':
                mode = MODE_CREATE;
                create_path = optarg;
                break;
            case 'o': output_path = optarg; break;
            default:  return EXIT_FAILURE;
        }
    }

    switch(mode) {
        case MODE_CREATE:
            if(!output_path) {
                printf("No output destination given\n");
                break;
            }
            create(create_path, output_path);
            break;
        default: printf("No mode was selected. Terminating\n"); break;
    }
    if(errno != 0) {
        printf("RDSK failed (%s)\n", strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
