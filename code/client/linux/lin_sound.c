// Todo: have audio playing code run in a separate thread

// Note: Listing cards/devices in Bash:
// - playback devices:  aplay -l
// - recording devices: arecord -l
// - all cards:         cat /proc/asound/cards


internal void
lin_print_sound_card_list(void)
{
   int status;
   int card = -1;
   char* longname  = NULL;
   char* shortname = NULL;

   if ((status = snd_card_next(&card)) < 0) {
      printf("cannot determine card number: %s", snd_strerror(status));
      return;
   }
   if (card < 0) {
      printf("no sound cards found");
      return;
   }
   while (card >= 0) {
      printf("Card %d:", card);
      if ((status = snd_card_get_name(card, &shortname)) < 0) {
         printf("cannot determine card shortname: %s", snd_strerror(status));
         break;
      }
      if ((status = snd_card_get_longname(card, &longname)) < 0) {
         printf("cannot determine card longname: %s", snd_strerror(status));
         break;
      }
      printf("\tLONG NAME:  %s\n", longname);
      printf("\tSHORT NAME: %s\n", shortname);


      char cardname[128];
      //sprintf(cardname, "default");
      sprintf(cardname, "hw:%d", card);
      printf("cardname = %s\n", cardname);

      snd_ctl_t *ctl;
      int mode = SND_PCM_ASYNC; // SND_PCM_NONBLOCK or SND_PCM_ASYNC
      if (snd_ctl_open(&ctl, cardname, mode) >= 0)
      {
          printf("cannot snd_stl_open\n");
          int device = -1;
          if (snd_ctl_pcm_next_device(ctl, &device) < 0)
          {
              printf("no devices found\n");
              device = -1;
          }

          if (device == -1)
          {
              printf("no device\n");
          }
          while (device >= 0)
          {
              char devicename[256];
              sprintf(devicename, "%s,%d", cardname, device);
              printf("devicename = %s\n", devicename);

              snd_pcm_t *pcm_handle;
              snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK; // SND_PCM_STREAM_PLAYBACK or SND_PCM_STREAM_CAPTURE
              if (snd_pcm_open(&pcm_handle, devicename, stream, mode) >= 0)
              {
                  printf("snd_pcm_open success\n");
                  snd_pcm_close(pcm_handle);
              }
              else
              {
                  printf("snd_pcm_open failed\n");
              }

              if (snd_ctl_pcm_next_device(ctl, &device) < 0)
              {
                  printf("snd_ctl_pcm_next_device error\n");
                  break;
              }
          }

          snd_ctl_close(ctl);
      }
      else
      {
          printf("snd_ctl_open failed\n");
      }


      if ((status = snd_card_next(&card)) < 0) {
         printf("cannot determine card number: %s", snd_strerror(status));
         break;
      }
   } 
}


#define SND_CALL(snd_call) \
{ \
    int error = snd_call; \
    if (error < 0) \
    { \
        const char *error_str = snd_strerror(error); \
        printf(#snd_call" err: %s\n", error_str); \
        snd_pcm_close(pcm); \
        return false; \
    } \
}
                                
internal b32
lin_recover_sound_device(snd_pcm_t *pcm, s64 err)
{
    if (err == -EPIPE)
    {
        err = snd_pcm_prepare(pcm);
        if (err < 0)
        {
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
            return false;
        }
        return true;
    }
    else if (err == -ESTRPIPE)
    {
        while ((err = snd_pcm_resume(pcm)) == -EAGAIN)
        {
            struct timespec ts = {0, 100};
            nanosleep(&ts, 0);
        }
        if (err < 0)
        {
            err = snd_pcm_prepare(pcm);
            if (err < 0)
            {
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
                return false;
            }
            return true;
        }
        return true;
    }
    printf("Can't recover: %s\n", snd_strerror(err));
    return false;
}

internal b32
lin_play_sound_buffer(Lin_Sound_Device *device, Florilia_Sound_Buffer *sound_buffer)
{
    u32 channel_count = 2;

    u64 frames_to_write = sound_buffer->sample_count / channel_count;
    s16 *samples = sound_buffer->samples;

    while (frames_to_write > 0)
    {
        snd_pcm_sframes_t frames_written = snd_pcm_writei(device->pcm, samples, frames_to_write);
        if (frames_written < 0)
        {
            if (!lin_recover_sound_device(device->pcm, frames_written))
            {
                // Todo: don't exit
                printf("Exiting\n");
                exit(EXIT_SUCCESS);
            }
            continue;
        }

        samples += frames_written * channel_count;
        frames_to_write -= frames_written;
    }

    int state = snd_pcm_state(device->pcm);
    if (state != SND_PCM_STATE_RUNNING)
    {
        snd_pcm_t *pcm = device->pcm;
        SND_CALL(snd_pcm_start(pcm));
    }

    return true;
}

internal void
lin_get_sound_buffer(Lin_Sound_Device *device, Florilia_Sound_Buffer *sound_buffer)
{
    s64 frames_avail = snd_pcm_avail(device->pcm);
    if (frames_avail < 0)
    {
        if (!lin_recover_sound_device(device->pcm, frames_avail))
        {
            frames_avail = 0;
        }
        frames_avail = snd_pcm_avail(device->pcm);
        if (frames_avail < 0)
        {
            frames_avail = 0;
        }
    }

    u32 sample_count = frames_avail*2;

    sound_buffer->sample_count = sample_count;
    sound_buffer->samples_per_second = device->samples_per_second;
    sound_buffer->samples = device->samples;
    memset(sound_buffer->samples, 0, sound_buffer->sample_count * sizeof(s16));
}

internal b32
lin_open_sound_device(Lin_Sound_Device *sound_device)
{
    snd_pcm_t *pcm = 0;

    // open device
    const char *device_name = "default";
    int stream = SND_PCM_STREAM_PLAYBACK;
    int mode = SND_PCM_ASYNC;
    SND_CALL(snd_pcm_open(&pcm, device_name, stream, mode));


    // set hw params
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    SND_CALL(snd_pcm_hw_params_any(pcm, params)); // get full configuration space

    int access = SND_PCM_ACCESS_RW_INTERLEAVED;
    SND_CALL(snd_pcm_hw_params_set_access(pcm, params, access));

    int format = SND_PCM_FORMAT_S16_LE;
    SND_CALL(snd_pcm_hw_params_set_format(pcm, params, format));

    u32 channels = 2;
    SND_CALL(snd_pcm_hw_params_set_channels(pcm, params, channels));

    u32 sample_rate = 44100;
    SND_CALL(snd_pcm_hw_params_set_rate_near(pcm, params, &sample_rate, 0));

    u32 period_count = 3;
    u32 sample_count = period_count * (sample_rate / 30);
    u32 frame_count  = sample_count / channels;
    SND_CALL(snd_pcm_hw_params_set_buffer_size(pcm, params, frame_count));

    u32 frames_per_period = frame_count / period_count;
    SND_CALL(snd_pcm_hw_params_set_period_size(pcm, params, frames_per_period, 0));

    SND_CALL(snd_pcm_hw_params(pcm, params));


    s16 *samples = malloc(sample_count*sizeof(s16));
    if (!samples)
    {
        printf("out of memory\n");
        snd_pcm_close(pcm);
        return false;
    }

    sound_device->pcm = pcm;
    sound_device->samples_per_second = sample_rate;
    sound_device->sample_count = sample_count;
    sound_device->samples = samples;
    sound_device->frames_per_period = frames_per_period;
    sound_device->frames_avail_remained = frames_per_period * 2;
    return true;
}

