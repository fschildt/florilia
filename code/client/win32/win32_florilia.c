// Note: This is a minimal port to win32, the main target is linux.

#include "../florilia_platform.h"
#include <windows.h>


#include <stdio.h>

typedef struct {
    BITMAPINFO bitmap_info;
    s32 width;
    s32 height;
    u32 *pixels;
} Win32_Offscreen_Buffer;

typedef struct {
    HWND window;
    HDC dc;
    Win32_Offscreen_Buffer offscreen_buffer;
} Win32_Window;

typedef struct {
    const char *name;
    int width;
    int height;
} Win32_Window_Settings;

#include "win32_window.c"
#include "win32_net.c"

internal
PLATFORM_GET_TIME(win32_get_time)
{
    florilia_time->seconds = 0;
    florilia_time->nanoseconds = 0;
#if 0
    LARGE_INTEGER frequency;
    if (QueryPerformanceFrequency(&frequency) == FALSE)
    {
        printf("frequency.QuadPart = %lld\n", frequency.QuadPart);
    }

    LARGE_INTEGER perf_counter;
    if (QueryPerformanceCounter(&perf_counter) == FALSE)
    {
        printf("perf_counter.QuadPart = %lld\n", perf_counter.QuadPart);
    }
#endif
}


internal bool
win32_init_florilia_memory(Florilia_Memory *florilia_memory, u64 permanent_size)
{
    void *storage = VirtualAlloc(0, permanent_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!storage)
    {
        printf("VirtualAlloc failed\n");
        return false;
    }

    florilia_memory->permanent_storage      = storage;
    florilia_memory->permanent_storage_size = permanent_size;
    florilia_memory->platform.get_time      = win32_get_time;
    florilia_memory->platform.connect       = win32_connect;
    florilia_memory->platform.disconnect    = win32_disconnect;
    florilia_memory->platform.send          = win32_send;
    florilia_memory->platform.recv          = win32_recv;
    return true;
}

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
    Win32_Window window = {};

    if (!win32_init_windowing()) return 0;
    if (!win32_create_window(&window, "Florilia", 1024, 720)) return 0;
    if (!win32_init_networking()) return 0;

    Florilia_Memory florilia_memory = {};
    Florilia_Event events[64];

    if (!win32_init_florilia_memory(&florilia_memory, Megabytes(1))) return 0;
    florilia_init(&florilia_memory);

    for (;;)
    {
        u32 event_count = sizeof(events) / sizeof(events[0]);
        if (!win32_process_window_events(&window, events, &event_count))
        {
            break;
        }

        Florilia_Offscreen_Buffer offscreen_buffer = {};

        offscreen_buffer.red_shift   = 16;
        offscreen_buffer.green_shift = 8;
        offscreen_buffer.blue_shift  = 0;
        offscreen_buffer.width = window.offscreen_buffer.width;
        offscreen_buffer.height = window.offscreen_buffer.height;
        offscreen_buffer.pixels = window.offscreen_buffer.pixels;
        Florilia_Sound_Buffer sound_buffer = {};

        florilia_update(&florilia_memory, &offscreen_buffer, &sound_buffer, events, event_count);

        win32_show_offscreen_buffer(&window);
    }

    return 0;
}
