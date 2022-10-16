typedef struct {
    u32 width;
    u32 height;
    u32 *pixels;

    u32 gl_texture_id;
} Lin_Offscreen_Buffer;

typedef struct {
    Window xid;
    long event_mask;
    GLXContext glx_context;

    Lin_Offscreen_Buffer offscreen_buffer;
} Lin_Window;

typedef struct {
    snd_pcm_t *pcm;
    u32 frames_per_period;

    u32 samples_per_second;
    u32 sample_count;
    s16 *samples;

    u32 frames_avail_remained;
} Lin_Sound_Device;

typedef struct {
    struct timespec t0;
    struct timespec t1;
} Lin_Timer;
