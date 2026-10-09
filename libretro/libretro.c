#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <sys/stat.h>

#include <libretro.h>
#include "libretro_internal.h"

#include "seedlessds/nds.h"
#include "seedlessds/platform.h"

/* Internal core symbols (not in public headers). */
void recon_present_set_mode(unsigned mode);
#define RECON_PRESENT_PREDICATE 1u
#define RECON_PRESENT_NO_UPLOAD 4u

#define CORE_NAME "SeedlessDS"
#define CORE_VERSION "0.1.0"

static retro_log_printf_t log_cb;
static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static nds_t *g_nds;
static pthread_t g_emu_thread;
static int g_emu_running;
static int g_emu_started;
static char g_rom_path[4096];
static char g_system_dir[4096];
static char g_save_dir[4096];

static uint32_t g_frame[SEEDLESS_SCREEN_W * SEEDLESS_FB_H];
static uint32_t g_top[SEEDLESS_SCREEN_W * SEEDLESS_SCREEN_H];
static uint32_t g_bottom[SEEDLESS_SCREEN_W * SEEDLESS_SCREEN_H];
static int16_t g_audio_tmp[SEEDLESS_AUDIO_RATE]; /* stereo frames * 2 */

static void log_fallback(enum retro_log_level level, const char *fmt, ...)
{
    va_list ap;
    (void)level;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void mkdir_p(const char *path)
{
    char tmp[4096];
    size_t len = strlen(path);
    char *p;

    if (len == 0 || len >= sizeof tmp)
        return;
    memcpy(tmp, path, len + 1);
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static void install_system_file(const char *name)
{
    char src[4200], dst[4200];
    FILE *in, *out;
    char buf[4096];
    size_t n;

    snprintf(dst, sizeof dst, "%s/User/system/%s", g_system_dir, name);
    if ((out = fopen(dst, "rb"))) {
        fclose(out);
        return;
    }

    snprintf(src, sizeof src, "%s/%s", g_system_dir, name);
    in = fopen(src, "rb");
    if (!in) {
        snprintf(src, sizeof src, "%s/SeedlessDS/%s", g_system_dir, name);
        in = fopen(src, "rb");
    }
    if (!in)
        return;

    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return;
    }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
}

static void set_seedless_root(void)
{
    const char *sys = NULL;
    const char *save = NULL;
    char user_sys[4200];

    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sys) && sys && *sys)
        snprintf(g_system_dir, sizeof g_system_dir, "%s", sys);
    else
        snprintf(g_system_dir, sizeof g_system_dir, ".");

    if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save) && save && *save)
        snprintf(g_save_dir, sizeof g_save_dir, "%s", save);
    else
        snprintf(g_save_dir, sizeof g_save_dir, "%s", g_system_dir);

    /* Core reads {SEEDLESS_ROOT}/User/system/nds_bios_*.bin */
    setenv("SEEDLESS_ROOT", g_system_dir, 1);
    snprintf(user_sys, sizeof user_sys, "%s/User/system", g_system_dir);
    mkdir_p(user_sys);
    mkdir_p(g_save_dir);

    install_system_file("nds_bios_arm9.bin");
    install_system_file("nds_bios_arm9_replacement.bin");
    install_system_file("nds_bios_arm7.bin");
    install_system_file("nds_bios_arm7_replacement.bin");
    install_system_file("nds_firmware.bin");
    install_system_file("nds_firmware_modified.bin");
}

static void *emu_thread_main(void *arg)
{
    nds_rom_open_t open;
    (void)arg;

    memset(&open, 0, sizeof open);
    open.path = g_rom_path;
    open.state_slot = -1;
    open.config_bits = 0;
    /* Keep soft RGBA path (bit 23 clear = 32-bit screens). */

    g_emu_started = 1;
    if (!nds_load_rom(g_nds, &open)) {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, CORE_NAME ": nds_load_rom failed\n");
        g_emu_running = 0;
        return NULL;
    }
    /* nds_load_rom normally blocks in the scheduler until quit. */
    g_emu_running = 0;
    return NULL;
}

/* Frontend button flags expected by Seedless-core input_record remap. */
#define FE_UP (1u << 0)
#define FE_DOWN (1u << 1)
#define FE_LEFT (1u << 2)
#define FE_RIGHT (1u << 3)
#define FE_A (1u << 4)
#define FE_B (1u << 5)
#define FE_X (1u << 6)
#define FE_Y (1u << 7)
#define FE_L (1u << 8)
#define FE_R (1u << 9)
#define FE_START (1u << 10)
#define FE_SELECT (1u << 11)

static uint32_t poll_keys(void)
{
    uint32_t keys = 0;
#define P(id, mask)                                                            \
    do {                                                                       \
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id))                     \
            keys |= (mask);                                                    \
    } while (0)

    P(RETRO_DEVICE_ID_JOYPAD_UP, FE_UP);
    P(RETRO_DEVICE_ID_JOYPAD_DOWN, FE_DOWN);
    P(RETRO_DEVICE_ID_JOYPAD_LEFT, FE_LEFT);
    P(RETRO_DEVICE_ID_JOYPAD_RIGHT, FE_RIGHT);
    P(RETRO_DEVICE_ID_JOYPAD_A, FE_A);
    P(RETRO_DEVICE_ID_JOYPAD_B, FE_B);
    P(RETRO_DEVICE_ID_JOYPAD_X, FE_X);
    P(RETRO_DEVICE_ID_JOYPAD_Y, FE_Y);
    P(RETRO_DEVICE_ID_JOYPAD_L, FE_L);
    P(RETRO_DEVICE_ID_JOYPAD_R, FE_R);
    P(RETRO_DEVICE_ID_JOYPAD_START, FE_START);
    P(RETRO_DEVICE_ID_JOYPAD_SELECT, FE_SELECT);
#undef P

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3))
        nds_hinge(g_nds, 1);
    else
        nds_hinge(g_nds, 0);

    return keys;
}

static void poll_touch(nds_input_t *input)
{
    int16_t px = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
    int16_t py = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
    int16_t pressed =
        input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
    int16_t mx = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
    int16_t my = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
    int16_t mbtn =
        input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);

    input->touch_down = 0;
    input->touch_x = 0;
    input->touch_y = 0;

    if (pressed) {
        /* Pointer is [-0x7fff, 0x7fff] over the full framebuffer. */
        int x = (int)((px + 0x7fff) * SEEDLESS_SCREEN_W / 0xfffe);
        int y = (int)((py + 0x7fff) * SEEDLESS_FB_H / 0xfffe);
        if (y >= SEEDLESS_SCREEN_H) {
            input->touch_down = 1;
            input->touch_x = x;
            input->touch_y = y - SEEDLESS_SCREEN_H;
        }
    } else if (mbtn) {
        static int abs_x, abs_y;
        abs_x += mx;
        abs_y += my;
        if (abs_x < 0)
            abs_x = 0;
        if (abs_y < 0)
            abs_y = 0;
        if (abs_x >= SEEDLESS_SCREEN_W)
            abs_x = SEEDLESS_SCREEN_W - 1;
        if (abs_y >= SEEDLESS_FB_H)
            abs_y = SEEDLESS_FB_H - 1;
        if (abs_y >= SEEDLESS_SCREEN_H) {
            input->touch_down = 1;
            input->touch_x = abs_x;
            input->touch_y = abs_y - SEEDLESS_SCREEN_H;
        }
    }
}

static void present_video(void)
{
    unsigned i;

    if (!nds_screens_rgba(g_nds, g_top, g_bottom)) {
        const void *top = NULL, *bottom = NULL;
        nds_screen_buffers(g_nds, &top, &bottom);
        if (!top || !bottom)
            return;
        memcpy(g_top, top, sizeof g_top);
        memcpy(g_bottom, bottom, sizeof g_bottom);
    }

    for (i = 0; i < SEEDLESS_SCREEN_W * SEEDLESS_SCREEN_H; i++) {
        g_frame[i] = g_top[i];
        g_frame[i + SEEDLESS_SCREEN_W * SEEDLESS_SCREEN_H] = g_bottom[i];
    }

    video_cb(g_frame, SEEDLESS_SCREEN_W, SEEDLESS_FB_H,
             SEEDLESS_SCREEN_W * sizeof(uint32_t));
}

static void present_audio(void)
{
    size_t avail = seedless_audio_frames_available();
    size_t want = (SEEDLESS_AUDIO_RATE / 60);
    size_t n;

    if (avail > sizeof g_audio_tmp / (2 * sizeof(int16_t)))
        avail = sizeof g_audio_tmp / (2 * sizeof(int16_t));

    n = seedless_audio_read(g_audio_tmp, avail);
    if (n == 0) {
        /* Keep frontend clocked if the stub has not filled yet. */
        memset(g_audio_tmp, 0, want * 2 * sizeof(int16_t));
        n = want;
    }
    if (audio_batch_cb)
        audio_batch_cb(g_audio_tmp, n);
    else if (audio_cb) {
        size_t i;
        for (i = 0; i < n; i++)
            audio_cb(g_audio_tmp[i * 2], g_audio_tmp[i * 2 + 1]);
    }
}

/* ---- libretro API ---- */

void retro_set_environment(retro_environment_t cb)
{
    bool no_content = false;
    struct retro_log_callback logging;
    environ_cb = cb;

    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;
    else
        log_cb = log_fallback;

    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    audio_batch_cb = cb;
}
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_init(void)
{
    seedless_compat_init();
    seedless_audio_reset();
    set_seedless_root();
    g_nds = NULL;
    g_emu_running = 0;
    g_emu_started = 0;
}

void retro_deinit(void)
{
    retro_unload_game();
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    info->library_name = CORE_NAME;
    info->library_version = CORE_VERSION;
    info->valid_extensions = "nds|dsi|ids|srl";
    info->need_fullpath = true;
    info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    memset(info, 0, sizeof(*info));
    info->geometry.base_width = SEEDLESS_SCREEN_W;
    info->geometry.base_height = SEEDLESS_FB_H;
    info->geometry.max_width = SEEDLESS_SCREEN_W;
    info->geometry.max_height = SEEDLESS_FB_H;
    info->geometry.aspect_ratio =
        (float)SEEDLESS_SCREEN_W / (float)SEEDLESS_FB_H;
    info->timing.fps = 59.8261;
    info->timing.sample_rate = (double)SEEDLESS_AUDIO_RATE;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    (void)port;
    (void)device;
}

void retro_reset(void)
{
    if (g_nds)
        nds_reset(g_nds);
}

void retro_run(void)
{
    nds_input_t input;

    if (!g_nds || !g_emu_running)
        return;

    input_poll_cb();
    memset(&input, 0, sizeof input);
    input.keys = poll_keys();
    input.keys_mask = 0;
    poll_touch(&input);
    nds_input_set(g_nds, &input);

    /* Wait for the emu thread to flip a completed frame. */
    nds_wait_screen(g_nds);

    present_video();
    present_audio();
}

size_t retro_serialize_size(void)
{
    /* Slot-file based; expose a fixed upper bound for frontends. */
    return 8 * 1024 * 1024;
}

bool retro_serialize(void *data, size_t size)
{
    char path[4096];
    FILE *f;
    size_t n;

    if (!g_nds || size < 16)
        return false;

    snprintf(path, sizeof path, "%s/seedlessds_serialize.tmp", g_save_dir);
    nds_state_save(g_nds, 0);
    /* State write is async inside the core; wait briefly via frame sync. */
    nds_wait_screen(g_nds);

    snprintf(path, sizeof path, "%s/User/save0.dsv", g_system_dir);
    /* Fallback: try common DraStic-style paths under SEEDLESS_ROOT. */
    f = fopen(path, "rb");
    if (!f) {
        snprintf(path, sizeof path, "%s/save0", g_save_dir);
        f = fopen(path, "rb");
    }
    if (!f)
        return false;
    n = fread(data, 1, size, f);
    fclose(f);
    return n > 0;
}

bool retro_unserialize(const void *data, size_t size)
{
    char path[4096];
    FILE *f;

    if (!g_nds || !data || size == 0)
        return false;

    snprintf(path, sizeof path, "%s/User/save0.dsv", g_system_dir);
    f = fopen(path, "wb");
    if (!f) {
        snprintf(path, sizeof path, "%s/save0", g_save_dir);
        f = fopen(path, "wb");
    }
    if (!f)
        return false;
    fwrite(data, 1, size, f);
    fclose(f);
    nds_state_load(g_nds, 0);
    return true;
}

void retro_cheat_reset(void)
{
    /* Custom cheat list is managed by the core; disable all entries. */
    int i, n;
    if (!g_nds)
        return;
    n = nds_cheat_custom_count(g_nds);
    for (i = 0; i < n; i++)
        nds_cheat_set_custom_enabled(g_nds, i, 0);
    nds_cheat_update(g_nds, 0);
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void)index;
    (void)enabled;
    (void)code;
    /* Action Replay / CodeBreaker text parsing can be added later. */
}

bool retro_load_game(const struct retro_game_info *info)
{
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    struct retro_input_descriptor desc[] = {
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "Close lid"},
        {0, 0, 0, 0, NULL},
    };

    if (!info || !info->path)
        return false;
/*
#if !defined(__aarch64__) && !defined(__arm64__)
    if (log_cb)
        log_cb(RETRO_LOG_ERROR,
               CORE_NAME ": SeedlessDS-Core JIT requires AArch64\n");
    return false;
#endif
*/
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
    set_seedless_root();

    snprintf(g_rom_path, sizeof g_rom_path, "%s", info->path);

    g_nds = nds_create(NULL, 0, 0);
    if (!g_nds) {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, CORE_NAME ": nds_create failed\n");
        return false;
    }

    recon_present_set_mode(RECON_PRESENT_PREDICATE | RECON_PRESENT_NO_UPLOAD);
    seedless_audio_reset();

    g_emu_running = 1;
    g_emu_started = 0;
    if (pthread_create(&g_emu_thread, NULL, emu_thread_main, NULL) != 0) {
        nds_destroy(g_nds);
        g_nds = NULL;
        g_emu_running = 0;
        return false;
    }

    /* Wait until the worker has entered nds_load_rom. */
    while (g_emu_running && !g_emu_started)
        sched_yield();

    if (log_cb)
        log_cb(RETRO_LOG_INFO, CORE_NAME ": loaded %s (%s)\n", g_rom_path,
               nds_version_string());
    return g_emu_running != 0;
}

bool retro_load_game_special(unsigned game_type,
                             const struct retro_game_info *info,
                             size_t num_info)
{
    (void)game_type;
    (void)info;
    (void)num_info;
    return false;
}

void retro_unload_game(void)
{
    if (!g_nds)
        return;

    nds_quit(g_nds);
    /* Unblock a waiter stuck in nds_wait_screen. */
    nds_signal_screen(g_nds);
    if (g_emu_running || g_emu_started)
        pthread_join(g_emu_thread, NULL);
    nds_destroy(g_nds);
    g_nds = NULL;
    g_emu_running = 0;
    g_emu_started = 0;
    seedless_audio_reset();
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void *retro_get_memory_data(unsigned id)
{
    if (!g_nds)
        return NULL;
    if (id == RETRO_MEMORY_SYSTEM_RAM)
        return nds_main_ram(g_nds);
    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    if (id == RETRO_MEMORY_SYSTEM_RAM)
        return 4u * 1024u * 1024u;
    return 0;
}
