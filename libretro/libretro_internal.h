#ifndef SEEDLESSDS_LIBRETRO_INTERNAL_H
#define SEEDLESSDS_LIBRETRO_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEEDLESS_AUDIO_RATE 44100
#define SEEDLESS_SCREEN_W 256
#define SEEDLESS_SCREEN_H 192
#define SEEDLESS_FB_H (SEEDLESS_SCREEN_H * 2)

/* OpenSL stub → libretro audio ring */
void seedless_audio_reset(void);
size_t seedless_audio_read(int16_t *out, size_t max_frames);
size_t seedless_audio_frames_available(void);

void seedless_compat_init(void);

#ifdef __cplusplus
}
#endif

#endif
