#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <direct.h> 

#define ARENA_IMPLEMENTATION
#include "bpack.h"
#include "lz4.h"

typedef struct {
    char path[MAX_PATH];
} FileInfo;

FileInfo *file_list = NULL;
int file_count = 0;
int file_capacity = 1000;

void add_file(const char* path) {
    if (file_count >= file_capacity) return;
    strncpy(file_list[file_count].path, path, MAX_PATH);
    file_count++;
}

void scan_directory(const char *dir_path) {
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_directory(full_path);
        } else {
            add_file(full_path);
        }
    } while (FindNextFileA(hFind, &find_data));
    FindClose(hFind);
}

u64 hash_string(const char *str) {
    u64 hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

void make_directories(const char *path) {
    char tmp[MAX_PATH];
    char *p = NULL;
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = 0;
            _mkdir(tmp);
            *p = '\\';
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("BPack v1.0 - Low Level Asset Tool\n");
        printf("Usage:\n  %s -pack <dir> <file.pak>\n  %s -unpack <file.pak> <out_dir>\n", argv[0], argv[0]);
        return 1;
    }

    char *mode = argv[1];
    arena *global_arena = arena_create(GiB(1));

    if (strcmp(mode, "-pack") == 0) {
        char *input_dir = argv[2];
        char *output_filename = argv[3];

        file_list = ARENA_PUSH_ARRAY(global_arena, FileInfo, file_capacity);
        scan_directory(input_dir);

        FILE *out = fopen(output_filename, "wb");
        if (!out) return 1;

        BPackHeader header = { BPACK_MAGIC, BPACK_VERSION, (u32)file_count, 0 };
        fwrite(&header, sizeof(BPackHeader), 1, out);

        BPackEntry *toc = ARENA_PUSH_ARRAY(global_arena, BPackEntry, file_count);
        long toc_pos = ftell(out);
        fseek(out, sizeof(BPackEntry) * file_count, SEEK_CUR);

        for (int i = 0; i < file_count; i++) {
            FILE *in = fopen(file_list[i].path, "rb");
            if (!in) continue;

            fseek(in, 0, SEEK_END);
            u64 size = ftell(in);
            fseek(in, 0, SEEK_SET);

            void *raw_data = arena_push(global_arena, size, false);
            fread(raw_data, size, 1, in);
            fclose(in);

            int max_comp_size = LZ4_compressBound((int)size);
            void *comp_data = arena_push(global_arena, max_comp_size, false);
            int comp_size = LZ4_compress_default(raw_data, comp_data, (int)size, max_comp_size);

            const char *rel_path = file_list[i].path + strlen(input_dir);
            if (*rel_path == '\\' || *rel_path == '/') rel_path++;

            long current_pos = ftell(out);
            long padding = (BPACK_ALIGNMENT - (current_pos % BPACK_ALIGNMENT)) % BPACK_ALIGNMENT;
            for (int p = 0; p < padding; p++) fputc(0, out);

            toc[i].hash = hash_string(rel_path);
            strncpy(toc[i].file_name, rel_path, 260);
            toc[i].offset = ftell(out);
            toc[i].size_uncompressed = size;

            if (comp_size > 0 && comp_size < (int)size) {
                toc[i].size_compressed = comp_size;
                toc[i].compression = 1; // LZ4
                fwrite(comp_data, comp_size, 1, out);
                printf("[LZ4] Packed: %s (%llu -> %d bytes)\n", rel_path, size, comp_size);
            } else {
                toc[i].size_compressed = size;
                toc[i].compression = 0; // RAW
                fwrite(raw_data, size, 1, out);
                printf("[RAW] Packed: %s (%llu bytes)\n", rel_path, size);
            }
            
        }

        fseek(out, toc_pos, SEEK_SET);
        fwrite(toc, sizeof(BPackEntry), file_count, out);
        fclose(out);
        printf("\nSuccess: %s created.\n", output_filename);

    } else if (strcmp(mode, "-unpack") == 0) {
        char *pak_name = argv[2];
        char *out_dir = argv[3];

        FILE *f = fopen(pak_name, "rb");
        if (!f) return 1;

        BPackHeader header;
        fread(&header, sizeof(BPackHeader), 1, f);
        if (header.magic != BPACK_MAGIC) { printf("Invalid Magic!\n"); return 1; }

        BPackEntry *toc = ARENA_PUSH_ARRAY(global_arena, BPackEntry, header.asset_count);
        fread(toc, sizeof(BPackEntry), header.asset_count, f);

        for (u32 i = 0; i < header.asset_count; i++) {
            char full_out_path[MAX_PATH];
            snprintf(full_out_path, MAX_PATH, "%s\\%s", out_dir, toc[i].file_name);
            make_directories(full_out_path);

            void *compressed_buffer = arena_push(global_arena, toc[i].size_compressed, false);
            fseek(f, toc[i].offset, SEEK_SET);
            fread(compressed_buffer, toc[i].size_compressed, 1, f);

            void *out_data = NULL;
            if (toc[i].compression == 1) {
                out_data = arena_push(global_arena, toc[i].size_uncompressed, false);
                LZ4_decompress_safe(compressed_buffer, out_data, (int)toc[i].size_compressed, (int)toc[i].size_uncompressed);
            } else {
                out_data = compressed_buffer;
            }

            FILE *out = fopen(full_out_path, "wb");
            if (out) {
                fwrite(out_data, toc[i].size_uncompressed, 1, out);
                fclose(out);
                printf("Extracted: %s\n", toc[i].file_name);
            }
        }
        fclose(f);
        printf("\nUnpack complete.\n");
    }

    arena_destroy(global_arena);
    return 0;
}