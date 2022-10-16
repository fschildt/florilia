#include "florilia_platform.h"
#include "../florilia_net.h"

#include <stdio.h> // @TEMPORARY, printf is expensive


internal void
zero_size(void *buff, u32 size)
{
    u8 *byte = buff;
    while (size--)
    {
        *byte++ = 0;
    }
}



typedef struct {
    u32 used;
    u32 size;
    void *buff;
} Memory_Arena;

internal void
initialize_arena(Memory_Arena *arena, void *memory, u32 size)
{
    arena->used = 0;
    arena->size = size;
    arena->buff = memory;
}

internal void*
push_size(Memory_Arena *arena, u32 size)
{
    void* result = arena->buff + arena->used;
    arena->used += size;

    assert(arena->used < arena->size);
    return result;
}

#define push_array(arena, type, count) (push_size(arena, sizeof(type)*count))
#define push_struct(arena, type)       (push_size(arena, sizeof(type)))



typedef struct {
    s32 width;
    s32 height;
    union {
        u32 *pixels;
        u8 *alphas;
    };
} Bitmap;

typedef struct {
    b32 is_playing;
    u32 samples_played;

    u32 samples_per_second;
    u32 sample_count;
    s16 *samples;
} Sound;

typedef struct {
    s32 xoff;
    s32 yoff;
    s32 xadvance;
    Bitmap bitmap;
} Glyph;

typedef struct {
    s32 baseline;
    s32 y_advance;
    Glyph glyphs[96];
} Font;

typedef struct {
    Sound hello;
    Font font;
} Assets;



typedef union {
    struct {
        s32 r;
        s32 g;
        s32 b;
    };

    struct {
        s32 x;
        s32 y;
        s32 z;
    };
} V3;

typedef struct {
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;
} Rect;



typedef struct {
    s32 size;
    void *mem;
} Network_Chunk;

typedef struct {
    s32 netid;
    s32 bytes_filled;
    void *mem;
    u8 mem1[1024];
    u8 mem2[1024];
} Network_Manager;



#include "login.h"
#include "session.h"


typedef struct {
    Memory_Arena arena;

    Assets *assets;

    Network_Manager network_manager;
    Login login;
    Session session;
} State;
