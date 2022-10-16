// Note: enabling Posix stuff while setting -std=c99
// https://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_02
#ifndef _POXIS_C_SOURCE
#define _POSIX_C_SOURCE 200809L // enable POSIX.1-2017
#endif

#include "../florilia_platform.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <alloca.h>
#include <unistd.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>

#include <alsa/asoundlib.h>


internal
PLATFORM_GET_TIME(lin_get_time)
{
    struct timespec ts;
    s32 err = clock_gettime(CLOCK_REALTIME, &ts);
    assert(err == 0);

    florilia_time->seconds = ts.tv_sec;
    florilia_time->nanoseconds = ts.tv_nsec;
}


#include "lin_florilia.h"
#include "lin_window.c"
#include "lin_net.c"
#include "lin_sound.c"


internal void*
lin_allocate_memory(u64 size)
{
    // Note:
    // Using mmap with MAP_PRIVATE|MAP_ANONYMOUS and fd=-1 would be better,
    // but that requires -std=gnu99 (or maybe just _GNU_SOURCE)
    // and we don't want to force gnu on users.

    // Todo: @Performance keep fd for /dev/zero open for the whole runtime?
    int fd = open("/dev/zero", O_RDWR);
    if (fd == -1)
    {
        printf("open() failed: %s\n", strerror(errno));
        return 0;
    }

    void *mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);

    return mem;
}

internal void
lin_free_memory(void *mem, u64 size)
{
    int result = munmap(mem, size);
    if (result == -1)
    {
        printf("munmap error: %s\n", strerror(errno));
    }
}

internal b32
lin_init_florilia_memory(Florilia_Memory *florilia_memory, u64 permanent_size)
{
    void *storage = lin_allocate_memory(permanent_size);
    if (!storage)
    {
        printf("cannot allocate permanent memory\n");
        return false;
    }

    florilia_memory->permanent_storage      = storage;
    florilia_memory->permanent_storage_size = permanent_size;
    florilia_memory->platform.get_time      = lin_get_time;
    florilia_memory->platform.connect       = DEBUG_lin_connect;
    florilia_memory->platform.disconnect    = DEBUG_lin_disconnect;
    florilia_memory->platform.send          = DEBUG_lin_send;
    florilia_memory->platform.recv          = DEBUG_lin_recv;
    return true;
}

int main(int argc, char **argv)
{
    Lin_Window window = {};
    Lin_Sound_Device sound_device = {};
    Florilia_Memory florilia_memory = {};

    if (!lin_create_window(&window, "Florilia", 1024, 720) ||
        !lin_open_sound_device(&sound_device) ||
        !lin_init_florilia_memory(&florilia_memory, Megabytes(1)))
    {
        return 0;
    }

    florilia_init(&florilia_memory);

    Florilia_Event events[64];

    for (;;)
    {
        u32 event_count = sizeof(events) / sizeof(Florilia_Event);
        if (!lin_process_window_events(&window, events, &event_count))
        {
            break;
        }

        Lin_Offscreen_Buffer *lin_offscreen_buffer = &window.offscreen_buffer;

        Florilia_Offscreen_Buffer offscreen_buffer = {};
        offscreen_buffer.red_shift   = 0;
        offscreen_buffer.green_shift = 8;
        offscreen_buffer.blue_shift  = 16;
        offscreen_buffer.width  = lin_offscreen_buffer->width;
        offscreen_buffer.height = lin_offscreen_buffer->height;
        offscreen_buffer.pixels = lin_offscreen_buffer->pixels;

        Florilia_Sound_Buffer sound_buffer;
        lin_get_sound_buffer(&sound_device, &sound_buffer);


        florilia_update(&florilia_memory, &offscreen_buffer, &sound_buffer, events, event_count);


        lin_play_sound_buffer(&sound_device, &sound_buffer);
        lin_show_offscreen_buffer(&window);
    }

    // Note: not necessary; avoiding compiler warnings for unused functions
    lin_destroy_window(&window);
    lin_free_memory(florilia_memory.permanent_storage, florilia_memory.permanent_storage_size);

    return 0;
}
