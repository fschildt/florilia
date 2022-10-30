// Note: embed assets into the binary

// Todo: @PERFORMANCE for the generated header
// guarantee that all values pushed to g_assets_memory are 4 or 8? byte aligned,
// because accessing non-aligned memory is slow
// You could also make a separate header for each asset, this might be better.

#include "../client/florilia.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// from https://de.wikipedia.org/wiki/RIFF_WAVE
typedef struct {
    // riff header
    char riff_name[4];
    u32  file_size_minus_8;
    char wave_name[4];

    // format header
    char format_name[4];
    u32 format_len;
    u16 format_tag;
    u16 channels;
    u32 samples_per_second;
    u32 bytes_per_second;
    u16 block_align;
    u16 bits_per_sample;

    // data header
    char data_name[4];
    u32 data_len;
} Wav_Header;

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

internal void*
read_entire_file(const char *pathname)
{
    FILE *f = fopen(pathname, "rb");
    if (!f)
    {
        printf("%s: fopen() failed", pathname);
        return 0;
    }

    u64 file_size;

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET); // same as rewind(f)

    void *buff = malloc(file_size);
    if (!buff)
    {
        printf("%s: malloc() failed", pathname);
        fclose(f);
        return 0;
    }

    u64 bytes_read = fread(buff, 1, file_size, f);
    if (bytes_read != file_size)
    {
        printf("%s: fread() failed", pathname);
        fclose(f);
        free(buff);
        return 0;
    }

    fclose(f);
    return buff;
}

internal void
write_entire_file(Memory_Arena *arena, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
    {
        printf("%s: fopen() failed\n", path);
        return;
    }

    size_t written = fwrite(arena->buff, 1, arena->used, f);
    if (written != arena->used)
    {
        printf("%s: fwrite failed\n", path);
    }

    fclose(f);
}

internal void
generate_string(Memory_Arena *arena, const char *str)
{
    void *mem = push_size(arena, strlen(str));
    memcpy(mem, str, strlen(str));
}

internal void
generate_buffer(Memory_Arena *arena, u8 *buffer, u32 size, b32 has_successor)
{
    char entry[16];
    for (u32 it = 0; it < size; it++)
    {
        b32 is_last = it == size-1;
        if (it % 4 == 0)
        {
            sprintf(entry, "\n\t");
            generate_string(arena, entry);
        }
        if (is_last)
        {
            if (has_successor)
            {
                sprintf(entry, "%3d,\n", buffer[it]);
            }
            else
            {
                sprintf(entry, "%3d\n", buffer[it]);
            }
        }
        else
        {
            sprintf(entry, "%3d,", buffer[it]);
        }
        generate_string(arena, entry);
    }
}

internal void
generate_font(u32 *bytes_generated, Memory_Arena *arena, Font *font, const char *filepath)
{
    void *font_buff = read_entire_file(filepath);
    if (!font_buff) return;

    int font_size = 24;
    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, font_buff, stbtt_GetFontOffsetForIndex(font_buff, 0)))
    {
        printf("stbtt_InitFont failed\n");
        free(font_buff);
        return;
    }

    f32 scale = stbtt_ScaleForPixelHeight(&font_info, font_size);

    // set font metadata (vmetrics)
    s32 baseline, ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);

    baseline = scale * -descent;
    ascent   = scale * ascent;
    descent  = scale * descent;
    line_gap = scale * line_gap;

    int x_advance;
    stbtt_GetCodepointHMetrics(&font_info, 'f', &x_advance, 0);
    font->baseline = baseline;
    font->y_advance = ascent - descent + scale*line_gap;
    font->x_advance = x_advance * scale;

    // set glyph metadata (hmetrics) AND generate glyph bitmaps
    u32 cnt_glyphs = '~' - ' ' + 1;
    for (u32 it = 0; it < cnt_glyphs; it++)
    {
        Glyph *glyph = font->glyphs + it;
        char c = it + ' ';


        s32 width, height, xoff, yoff;
        u8 *mono_bitmap_flipped = stbtt_GetCodepointBitmap(&font_info, 0, stbtt_ScaleForPixelHeight(&font_info, font_size), c, &width, &height, &xoff, &yoff);
        if (!mono_bitmap_flipped && c != ' ')
        {
            printf("stbtt_GetCodepointBitmap for '%c' failed\n", c);
        }
        yoff = -(yoff + height);


        u8 *mono_bitmap_correct = malloc(width*height);
        u8 *dest = mono_bitmap_correct;
        for (u32 y = 0; y < height; y++)
        {
            u8 *src = mono_bitmap_flipped + (height-y-1)*width;
            for (u32 x = 0; x < width; x++)
            {
                *dest++ = *src++;
            }
        }


        s32 x_advance, left_side_bearing;
        stbtt_GetCodepointHMetrics(&font_info, c, &x_advance, &left_side_bearing);
        x_advance *= scale;
        left_side_bearing *= scale;


        u64 bitmap_offset = *bytes_generated;
        u32 bitmap_size = width * height;
        glyph->xoff = left_side_bearing;
        glyph->xadvance = x_advance;
        glyph->yoff = yoff;
        glyph->bitmap.width  = width;
        glyph->bitmap.height = height;
        glyph->bitmap.alphas = (u8*)bitmap_offset;

        generate_buffer(arena, mono_bitmap_correct, bitmap_size, true);
        *bytes_generated += bitmap_size;

        free(mono_bitmap_correct);
        stbtt_FreeBitmap(mono_bitmap_flipped, 0);
    }

    free(font_buff);
}

internal void
generate_sound(u32 *bytes_generated, Memory_Arena *arena, Sound *sound, const char *filepath)
{
    void *wav_buff = read_entire_file(filepath);
    if (!wav_buff) return;


    // get wav info
    Wav_Header *wav = wav_buff;
    u32 samples_size = wav->data_len;
    u32 sample_count = samples_size / sizeof(s16);
    s16 *samples = wav_buff + 44;

    // set sound
    u64 samples_offset = *bytes_generated;
    sound->samples_per_second = wav->samples_per_second;
    sound->sample_count = sample_count;
    sound->samples = (s16*)samples_offset; // is initialized by florilia_init

    // generate samples
    u8 *buffer = (u8*)samples;
    u32 buffer_size = samples_size;
    generate_buffer(arena, buffer, buffer_size, true);


    free(wav_buff);
    *bytes_generated += buffer_size;
}

int main(int argc, char **argv)
{
    u32 arena_buff_size = Megabytes(512);
    void *arena_buff = malloc(arena_buff_size);
    if (!arena_buff)
    {
        printf("malloc failed\n");
        return EXIT_FAILURE;
    }

    Memory_Arena arena;
    initialize_arena(&arena, arena_buff, arena_buff_size);

    Assets assets = {};

    u32 bytes_generated = 0;

    generate_string(&arena, "static u8 g_assets_memory[] = {");
    generate_font(&bytes_generated, &arena, &assets.font, "./data/Inconsolata-Regular.ttf");
    generate_sound(&bytes_generated, &arena, &assets.hello, "./data/hello.wav");
    generate_buffer(&arena, (u8*)&assets, sizeof(assets), false);
    generate_string(&arena, "};");

    write_entire_file(&arena, "./code/client/generated/assets_memory.h");
    return 0;
}
