#include "florilia.h"
#include "generated/assets_memory.h"

#include <stdio.h> // @TEMPORARY, printf is expensive
#include <time.h>
#include <string.h>
#include <math.h>


Florilia_Platform *g_platform; // @TEMPORARY


typedef struct {
    Florilia_Time t0;
    Florilia_Time t1;
} Timer;

#define Timer_Start(name) \
    static Timer name; \
    g_platform->get_time(&name.t0);
    
#define Timer_End(name) \
    g_platform->get_time(&name.t1); \
    s64 sec_diff = name.t1.seconds - name.t0.seconds; \
    s64 nsec_total = sec_diff*1000*1000*1000 + (name.t1.nanoseconds - name.t0.nanoseconds); \
    s64 ms_part = nsec_total / (1000*1000); \
    s64 ns_part = nsec_total % (1000*1000); \
    printf("%-24s: %3ld,%06ld ms\n", #name, ms_part, ns_part);



#include <x86intrin.h>

#define Cycle_Counter_Start(name) \
    u64 cycle_counter_##name_start = __rdtsc();
#define Cycle_Counter_End(name) \
    u64 cycle_counter_##name_end = __rdtsc();\
    u64 cycles_counted_##name = cycle_counter_##name_end - cycle_counter_##name_start;\
    printf("cycles_counted_%s = %lu\n", #name, cycles_counted_##name);





internal void
reset_netman(Network_Manager *netman)
{
    netman->netid = -1;
    netman->bytes_filled = 0;
    netman->mem = netman->mem1;
}

internal void
netman_send(Network_Manager *netman, u8 *package, u32 package_size)
{
    s32 sent = g_platform->send(netman->netid, package, package_size);
    if (sent == -1)
    {
        reset_netman(netman);
    }
}

// Note: this should only be called once per frame (until voice-chatting)
internal void
netman_recv(Network_Manager *netman)
{
    if (netman->netid > 0)
    {
        u8 *dest = netman->mem + netman->bytes_filled;
        u32 size = sizeof(netman->mem1) - netman->bytes_filled;

        s32 recvd = g_platform->recv(netman->netid, dest, size);
        if (recvd > 0)
        {
            netman->bytes_filled += recvd;
        }
        if (recvd == -1)
        {
            reset_netman(netman);
        }
    }
}

internal void
netman_release_size(Network_Manager *netman, s32 size)
{
    u8 *src  = (netman->mem == netman->mem1) ? netman->mem1 : netman->mem2;
    u8 *dest = (netman->mem == netman->mem1) ? netman->mem2 : netman->mem1;

    s32 bytes_remaining = netman->bytes_filled - size;
    memcpy(dest, src+size, bytes_remaining);

    netman->mem = dest;
    netman->bytes_filled = bytes_remaining;
}


internal void
edit_text(char *str, u32 size, char c, s32 *cursor)
{
    u32 len = strlen(str);

    if (c >= 32 && c <= 126 && len < size-1)
    {
        // insert character
        memmove(str+*cursor+1, str+*cursor, len-*cursor+1);
        str[*cursor] = c;
        *cursor += 1;
    }
    else if (c == 8 && *cursor > 0)
    {
        // delete character left (8 is ascii backspace)
        memmove(str+*cursor-1, str+*cursor, len-*cursor+1);
        *cursor -= 1;
    }
    else if (c == 127 && len > 0)
    {
        // delete character right (127 is ascii delete)
        memmove(str+*cursor, str+*cursor+1, len-*cursor);
    }
}

internal void
move_cursor_left(const char *str, s32 *cursor)
{
    if (*cursor > 0)
    {
        *cursor -= 1;
    }
}

internal void
move_cursor_right(const char *str, s32 *cursor)
{
    s32 len = strlen(str);
    if (*cursor < len)
    {
        *cursor += 1;
    }
}

// Todo: have a fallback glyph and use that if the requested glyph is unavail
internal Glyph*
get_glyph(char c, Font *font)
{
    assert(c >= ' ' && c <= '~');
    s32 index = c - ' ';
    Glyph *glyph = &font->glyphs[index];
    return glyph;
}

internal s32
get_text_width(const char *str, Font *font)
{
    s32 width = 0;

    char c;
    while ((c = *str))
    {
        Glyph *glyph = get_glyph(c, font);
        width += glyph->xadvance;

        str++;
    }
    return width;
}


// rect:
// - the desired coordinates on the screen,
// - it can be too big, or in a place that's not drawn
//
// subrect:
// - coordinates within the desired rect that are on the screen
internal void
get_drawable_subrect(Florilia_Offscreen_Buffer *screen, Rect *rect, Rect *subrect)
{
    s32 sub_x0 = 0;
    s32 sub_x1 = rect->x1 - rect->x0;
    s32 sub_y0 = 0;
    s32 sub_y1 = rect->y1 - rect->y0;

    if (rect->y0 < 0)
    {
        sub_y0   = -rect->y0;
        rect->y0  = 0;
    }
    if (rect->y0 + (sub_y1 - sub_y0) > screen->height-1)
    {
        sub_y1 = sub_y0 + (screen->height - rect->y0 - 1);
    }
    if (rect->x0 < 0)
    {
        sub_x0   = -rect->x0;
        rect->x0  = 0;
    }
    if (rect->x0 + (sub_x1 - sub_x0) > screen->width - 1)
    {
        sub_x1 = sub_x0 + (screen->width - rect->x0 - 1);
    }

    subrect->x0 = sub_x0;
    subrect->x1 = sub_x1;
    subrect->y0 = sub_y0;
    subrect->y1 = sub_y1;
}

internal void
draw_mono_bitmap(Florilia_Offscreen_Buffer *screen, Bitmap *bitmap, s32 x_screen, s32 y_screen, V3 color)
{
    Rect rect = {x_screen, y_screen, x_screen + bitmap->width - 1, y_screen + bitmap->height - 1};
    Rect subrect;
    get_drawable_subrect(screen, &rect, &subrect);

    u8 rshift   = screen->red_shift;
    u8 gshift = screen->green_shift;
    u8 bshift  = screen->blue_shift;
    s32 width = subrect.x1 - subrect.x0 + 1;
    u32 *dest  = screen->pixels + rect.y0 * screen->width + rect.x0;
    u8 *alphas = bitmap->alphas + subrect.y0 * bitmap->width + subrect.x0;
    for (s32 y = subrect.y0; y <= subrect.y1; y++)
    {
        for (s32 x = subrect.x0; x <= subrect.x1; x++)
        {
            f32 alpha = *alphas++ / 255.0;

            u32 dest_color = *dest;
            f32 r0 = (dest_color >> rshift) & 0xff;
            f32 g0 = (dest_color >> gshift) & 0xff;
            f32 b0 = (dest_color >> bshift) & 0xff;

            // linear alpha blending
            f32 r1 = r0 + (color.r - r0)*alpha;
            f32 g1 = g0 + (color.g - g0)*alpha;
            f32 b1 = b0 + (color.b - b0)*alpha;

            u32 val = (u32)b1 << bshift | (u32)g1 << gshift | (u32)r1 << rshift;
            *dest++ = val;
        }
        dest   += screen->width - width;
        alphas += bitmap->width - width;
    }
}

internal void
draw_rect(Florilia_Offscreen_Buffer *screen, Rect rect, V3 color)
{
    Rect subrect;
    get_drawable_subrect(screen, &rect, &subrect);

    u8 rshift = screen->red_shift;
    u8 gshift = screen->green_shift;
    u8 bshift = screen->blue_shift;
    u32 *pixels = screen->pixels + rect.y0 * screen->width + rect.x0;
    for (s32 y = subrect.y0; y <= subrect.y1; y++)
    {
        for (s32 x = subrect.x0; x <= subrect.x1; x++)
        {
            u32 val = color.r << rshift | color.g << gshift | color.b << bshift;
            *pixels++ = val;
        }
        pixels += screen->width - (subrect.x1 - subrect.x0 + 1);
    }
}

internal void
draw_border(Florilia_Offscreen_Buffer *screen, Rect rect, s32 border_size, V3 color)
{
    assert(border_size > 0);
    s32 y0_top = rect.y1 - border_size + 1;
    s32 y1_bot = rect.y0 + border_size - 1;
    s32 x0_right = rect.x1 - border_size + 1;
    s32 x1_left  = rect.x0 + border_size - 1;


    // horizontal
    {
        Rect render_rect = {rect.x0, rect.y0, rect.x1, y1_bot};
        draw_rect(screen, render_rect, color);
    }

    {
        Rect render_rect = {rect.x0, y0_top, rect.x1, rect.y1};
        draw_rect(screen, render_rect, color);
    }

    // vertical
    {
        Rect render_rect = {rect.x0, rect.y0, x1_left, rect.y1};
        draw_rect(screen, render_rect, color);
    }

    {
        Rect render_rect = {x0_right, rect.y0, rect.x1, rect.y1};
        draw_rect(screen, render_rect, color);
    }
}

// Todo: don't draw more than what we have space for (use x1,y1)
internal void
draw_text(Florilia_Offscreen_Buffer *screen, Font *font, const char *str, s32 cursor, s32 x0, s32 y0)
{
    V3 color = {{0, 0, 0}};

    s32 y_baseline = y0 + font->baseline;
    while (*str)
    {
        Glyph *glyph = get_glyph(*str, font);
        if (*str != ' ')
        {
            Bitmap *mono_bitmap = &glyph->bitmap;

            s32 y = y_baseline + glyph->yoff;
            s32 x = x0 + glyph->xoff;
            draw_mono_bitmap(screen, mono_bitmap, x, y, color);
        }

        if (cursor == 0)
        {
            V3 color_cursor = {{0, 0, 0}};
            Rect rect = {x0, y0, x0+3, y0+font->y_advance};
            draw_rect(screen, rect, color_cursor);
        }

        x0 += glyph->xadvance;
        str++;
        cursor--;
    }

    if (cursor == 0)
    {
        V3 color_cursor = {{0, 0, 0}};
        Rect rect = {x0, y0, x0+3, y0+font->y_advance};
        draw_rect(screen, rect, color_cursor);
    }
}

internal u32
play_sound(Sound *sound, Florilia_Sound_Buffer *out)
{
    u32 samples_requested = out->sample_count;
    u32 samples_available = sound->sample_count - sound->samples_played;

    u32 samples_to_play;
    if (samples_available < samples_requested)
    {
        samples_to_play = samples_available;
    }
    else
    {
        samples_to_play = samples_requested;
    }


    s16 *samples_in  = sound->samples + sound->samples_played;
    s16 *samples_out = out->samples;
    for (u32 it = 0; it < samples_to_play; it++)
    {
        samples_out[it] += samples_in[it];
    }

    return samples_to_play;
}


#include "login.c"
#include "session.c"


void
florilia_update(Florilia_Memory *memory, Florilia_Offscreen_Buffer *screen, Florilia_Sound_Buffer *sound_buffer, Florilia_Event *events, u32 event_count)
{
#if 0
    Cycle_Counter_Start(florilia_update);
    Timer_Start(timer_floria_update);
#endif

    State *state = memory->permanent_storage;

    Network_Manager *netman = &state->network_manager;
    Login *login = &state->login;
    Session *session = &state->session;


    // process window events
    for (u32 it = 0; it < event_count; it++)
    {
        if (login->status == Login_Status_Success)
        {
            session_process_event(session, events+it, netman);
        }
        else
        {
            login_process_event(login, events+it, netman);
        }
    }


    // process network events
    netman_recv(netman);
    b32 processed;
    do
    {
        processed = false;
        if (login->status == Login_Status_Success)
        {
            processed = session_process_netman_buff(session, netman);
        }
        else if (login->status == Login_Status_Waiting)
        {
            processed = login_process_netman_buff(login, netman);
        }
    } while (processed);


    // Todo: handle lost connections properly
    // currently we need to send 2 messages to get a broken pipe signal
    // - use timeouts i guess
    // - maybe check getsockopt to see if socket is still connected?
    if (login->status == Login_Status_Success && netman->netid == -1)
    {
        reset_netman(netman);
        reset_session(session);
        strcpy(login->warning, "Lost Connection");
        login->status = Login_Status_Lost;
    }


    // optionally play sound
    Assets *assets = state->assets;
    Sound *sound = &assets->hello;
    if (sound->is_playing)
    {
        u32 samples_played = play_sound(sound, sound_buffer);
        if (sound->samples_played == sound->sample_count)
        {
            sound->samples_played = 0;
        }
        else
        {
            sound->samples_played += samples_played;
        }
    }
    

    // draw state
    if (state->login.status == Login_Status_Success)
    {
        draw_session(screen, &state->session, &assets->font);
    }
    else
    {
        draw_login(screen, &state->login, &assets->font);
    }


#if 0
    Timer_End(timer_floria_update);
    Cycle_Counter_End(florilia_update);
#endif
}

void
florilia_init(Florilia_Memory *memory)
{
    g_platform = &memory->platform;

    zero_size(memory->permanent_storage, memory->permanent_storage_size);
    State *state = memory->permanent_storage;

    Memory_Arena *arena = &state->arena;
    initialize_arena(arena,
                     memory->permanent_storage      + sizeof(State),
                     memory->permanent_storage_size - sizeof(State));



    // init assets
    Assets *assets = (Assets*)(g_assets_memory + sizeof(g_assets_memory) - sizeof(Assets));
    state->assets = assets;

    // - sound
    Sound *sound = &assets->hello;
    sound->is_playing = false;
    sound->samples = (void*)g_assets_memory + (u64)(sound->samples);

    // - font glyphs
    u32 glyph_count = '~' - ' ' + 1;
    Font *font = &assets->font;
    for (u32 it = 0; it < glyph_count; it++)
    {
        Glyph *glyph = font->glyphs + it;
        glyph->bitmap.alphas = (void*)g_assets_memory + (u64)(glyph->bitmap.alphas);
    }


    reset_netman(&state->network_manager);
    init_login(&state->login, arena);
    init_session(&state->session, arena);
}

