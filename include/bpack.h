#include "../Arena/include/arena.h"

#define BPACK_MAGIC     0x4B435042
#define BPACK_VERSION   1
#define BPACK_ALIGNMENT 64

typedef struct{
    u32 magic;
    u32 version;
    u32 asset_count;
    u32 reserved;       // Padding
}BPackHeader;

typedef struct{
    u64 hash;
    u64 offset;
    u64 size_uncompressed;
    u64 size_compressed;
    u32 type;           // 0: raw, 1: texture, 2: audio
    u32 compression;    // 0: none, 1: LZ4
    char file_name[260];
}BPackEntry;