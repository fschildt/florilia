#include "../florilia_common.h"


typedef struct {
    u8 red_shift;
    u8 green_shift;
    u8 blue_shift;
    s32 width;
    s32 height;
    u32 *pixels;
} Florilia_Offscreen_Buffer;


typedef struct {
    u32 samples_per_second;
    u32 sample_count;
    s16 *samples;
} Florilia_Sound_Buffer;


typedef struct {
    s64 seconds;
    s32 nanoseconds;
} Florilia_Time;



enum {
    FLORILIA_KEY_LEFT  = -1,
    FLORILIA_KEY_RIGHT = -2,
    FLORILIA_KEY_UP    = -3,
    FLORILIA_KEY_DOWN  = -4
};

// key events
typedef struct {
    char c;
} Florilia_Key_Event;


// events
typedef enum {
    FLORILIA_EVENT_KEY,
} Florilia_Event_Type;

typedef struct {
    Florilia_Event_Type type;
    union {
        Florilia_Key_Event key;
    };
} Florilia_Event;



#define PLATFORM_GET_TIME(name) void name(Florilia_Time *florilia_time)
typedef PLATFORM_GET_TIME(Platform_Get_Time);

#define DEBUG_PLATFORM_CONNECT(name) s32 name(const char *address, u16 port)
typedef DEBUG_PLATFORM_CONNECT(DEBUG_Platform_Connect);

#define DEBUG_PLATFORM_DISCONNECT(name) void name(s32 netid)
typedef DEBUG_PLATFORM_DISCONNECT(DEBUG_Platform_Disconnect);

#define DEBUG_PLATFORM_SEND(name) s32 name(s32 netid, void *buffer, u32 size)
typedef DEBUG_PLATFORM_SEND(DEBUG_Platform_Send);

#define DEBUG_PLATFORM_RECV(name) s32 name(s32 netid, void *buffer, u32 size)
typedef DEBUG_PLATFORM_RECV(DEBUG_Platform_Recv);

// Todo: @TEMPORARY until we have a platform worker queue we push requests to
typedef struct {
    Platform_Get_Time         *get_time;
    DEBUG_Platform_Connect    *connect;
    DEBUG_Platform_Disconnect *disconnect;
    DEBUG_Platform_Send       *send;
    DEBUG_Platform_Recv       *recv;
} Florilia_Platform;



typedef struct {
    void *permanent_storage;
    u32   permanent_storage_size;

    Florilia_Platform platform;
} Florilia_Memory;



void florilia_init(Florilia_Memory *memory);
void florilia_update(Florilia_Memory *memory, Florilia_Offscreen_Buffer *offscreen_buffer, Florilia_Sound_Buffer *sound_buffer, Florilia_Event *events, u32 event_count);
