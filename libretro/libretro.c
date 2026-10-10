#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libretro.h>
#include "libretro_internal.h"
#include "libretro_core_options.h"

#include "seedlessds/nds.h"
#include "seedlessds/platform.h"

/* Internal core symbols (not in public headers). */
void recon_present_set_mode(unsigned mode);
extern unsigned recon_present_flips;
extern unsigned recon_present_seen;
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
/* Shared with emu thread — must be volatile or the wait loop is optimized
 * into an infinite sched_yield (black screen, menu unresponsive). */
static volatile int g_emu_running;
static volatile int g_emu_started;
static char g_rom_path[4096];
static char g_system_dir[4096];
static char g_save_dir[4096];
static uint64_t g_config_bits;
static bool g_libretro_supports_option_categories;
/* Virtual stylus for devices without a touchscreen: right stick moves,
 * L3 presses. Coordinates are on the lower DS screen (0..W-1, 0..H-1). */
static float g_stylus_x = (float)(SEEDLESS_SCREEN_W / 2);
static float g_stylus_y = (float)(SEEDLESS_SCREEN_H / 2);
static int g_stylus_touching;
#define STYLUS_DEADZONE 0x1800
#define STYLUS_SPEED_PX 4.5f

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

static const char *opt_value(const char *key, const char *fallback)
{
    struct retro_variable var;

    var.key = key;
    var.value = NULL;
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) &&
        var.value && *var.value)
        return var.value;
    return fallback;
}

static int opt_enabled(const char *key, int default_on)
{
    const char *v = opt_value(key, default_on ? "enabled" : "disabled");
    return strcmp(v, "enabled") == 0 || strcmp(v, "true") == 0 ||
           strcmp(v, "1") == 0;
}

static unsigned opt_unsigned(const char *key, unsigned fallback)
{
    const char *v = opt_value(key, NULL);
    char *end = NULL;
    unsigned long n;

    if (!v)
        return fallback;
    n = strtoul(v, &end, 10);
    if (end == v)
        return fallback;
    return (unsigned)n;
}

/* Pack DraStic/Seedless config_bits from libretro core options.
 * Layout matches core/src/core/config.c config_unpack_word(). */
static uint64_t build_config_bits(void)
{
    uint64_t bits = 0;
    const char *fs_type = opt_value("seedlessds_frameskip_type", "none");
    const char *latency = opt_value("seedlessds_audio_latency", "low");
    unsigned fs_val = opt_unsigned("seedlessds_frameskip_value", 4);
    unsigned volume = opt_unsigned("seedlessds_audio_volume", 1);
    unsigned threads = opt_unsigned("seedlessds_thread_count", 8);
    unsigned profile = opt_unsigned("seedlessds_button_profile", 0);
    unsigned slot2 = opt_unsigned("seedlessds_slot2", 0);
    unsigned fs_enc;

    if (fs_val > 9)
        fs_val = 9;
    if (volume > 3)
        volume = 3;
    if (threads > 15)
        threads = 15;
    if (profile > 7)
        profile = 7;
    if (slot2 > 15)
        slot2 = 15;

    /* bits 0-3: frameskip value */
    bits |= (uint64_t)fs_val & 0xfull;

    /* bits 5-6: frameskip type encoding
     *   1 → none (type 0), 2 → manual (type 1), 0/3 → auto (type 2) */
    if (strcmp(fs_type, "none") == 0)
        fs_enc = 1;
    else if (strcmp(fs_type, "manual") == 0)
        fs_enc = 2;
    else
        fs_enc = 0;
    bits |= (uint64_t)(fs_enc & 3u) << 5;

    /* bits 8-9: audio latency mode */
    if (strcmp(latency, "medium") == 0)
        bits |= 1ull << 8;
    else if (strcmp(latency, "high") == 0)
        bits |= 2ull << 8;

    /* bits 16-19: compose thread count */
    bits |= (uint64_t)(threads & 0xfu) << 16;

    if (opt_enabled("seedlessds_screen_16bit", 0))
        bits |= 1ull << 23; /* FRONTEND_CONFIG_SCREENSHOT_SWAP_RB */
    if (opt_enabled("seedlessds_ignore_gamecard_limit", 0))
        bits |= 1ull << 24;
    if (opt_enabled("seedlessds_savestate_extra", 0))
        bits |= 1ull << 25;
    if (opt_enabled("seedlessds_mic_audio_path", 1))
        bits |= 1ull << 26; /* audio_out_enabled */
    if (opt_enabled("seedlessds_cheats", 1))
        bits |= 1ull << 27;
    if (opt_enabled("seedlessds_threaded_3d", 0))
        bits |= 1ull << 28;
    if (opt_enabled("seedlessds_show_fps", 0))
        bits |= 1ull << 30;
    if (opt_enabled("seedlessds_sound", 1))
        bits |= 1ull << 31;

    bits |= (uint64_t)(profile & 7u) << 32;

    if (opt_enabled("seedlessds_fix_main_screen", 0))
        bits |= 1ull << 35;
    if (opt_enabled("seedlessds_auto_trim", 0))
        bits |= 1ull << 36;

    bits |= (uint64_t)(volume & 3u) << 37;

    if (opt_enabled("seedlessds_rtc_system_time", 1))
        bits |= 1ull << 39;
    if (opt_enabled("seedlessds_disable_edge_marking", 0))
        bits |= 1ull << 40;
    if (opt_enabled("seedlessds_hires_3d", 0))
        bits |= 1ull << 41;
    if (opt_enabled("seedlessds_lua", 0))
        bits |= 1ull << 42;

    bits |= (uint64_t)(slot2 & 0xfu) << 43;

    if (opt_enabled("seedlessds_frameskip_safe", 0))
        bits |= 1ull << 47;
    if (opt_enabled("seedlessds_preload_roms", 0))
        bits |= 1ull << 48;
    if (opt_enabled("seedlessds_raw_sav", 0))
        bits |= 1ull << 50;

    return bits;
}

static void apply_core_options(int runtime)
{
    g_config_bits = build_config_bits();
    if (runtime && g_nds)
        nds_apply_config(g_nds, g_config_bits);
}

static void check_variables(void)
{
    bool updated = false;

    if (environ_cb &&
        environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) &&
        updated)
        apply_core_options(1);
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
    open.config_bits = g_config_bits;

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

    return keys;
}

static void clamp_stylus(void)
{
    if (g_stylus_x < 0.0f)
        g_stylus_x = 0.0f;
    if (g_stylus_y < 0.0f)
        g_stylus_y = 0.0f;
    if (g_stylus_x > (float)(SEEDLESS_SCREEN_W - 1))
        g_stylus_x = (float)(SEEDLESS_SCREEN_W - 1);
    if (g_stylus_y > (float)(SEEDLESS_SCREEN_H - 1))
        g_stylus_y = (float)(SEEDLESS_SCREEN_H - 1);
}

static void move_stylus_from_stick(unsigned index)
{
    int16_t ax = input_state_cb(0, RETRO_DEVICE_ANALOG, index,
                                RETRO_DEVICE_ID_ANALOG_X);
    int16_t ay = input_state_cb(0, RETRO_DEVICE_ANALOG, index,
                                RETRO_DEVICE_ID_ANALOG_Y);
    float fx, fy, mag;

    if (ax > -STYLUS_DEADZONE && ax < STYLUS_DEADZONE)
        ax = 0;
    if (ay > -STYLUS_DEADZONE && ay < STYLUS_DEADZONE)
        ay = 0;
    if (ax == 0 && ay == 0)
        return;

    fx = (float)ax / 32767.0f;
    fy = (float)ay / 32767.0f;
    mag = fx * fx + fy * fy;
    if (mag > 1.0f) {
        mag = sqrtf(mag);
        fx /= mag;
        fy /= mag;
    }
    g_stylus_x += fx * STYLUS_SPEED_PX;
    g_stylus_y += fy * STYLUS_SPEED_PX;
    clamp_stylus();
}

static void put_fb_px(uint32_t *fb, int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= SEEDLESS_SCREEN_W || y >= SEEDLESS_FB_H)
        return;
    fb[y * SEEDLESS_SCREEN_W + x] = color;
}

static void draw_stylus_cursor(uint32_t *fb, int touching)
{
    int cx = (int)(g_stylus_x + 0.5f);
    int cy = (int)(g_stylus_y + 0.5f) + SEEDLESS_SCREEN_H;
    uint32_t ink = touching ? 0x00FF4040u : 0x00F0F0F0u;
    uint32_t edge = 0x00101010u;
    int i;

    for (i = -7; i <= 7; i++) {
        put_fb_px(fb, cx + i, cy - 1, edge);
        put_fb_px(fb, cx + i, cy + 1, edge);
        put_fb_px(fb, cx - 1, cy + i, edge);
        put_fb_px(fb, cx + 1, cy + i, edge);
        put_fb_px(fb, cx + i, cy, ink);
        put_fb_px(fb, cx, cy + i, ink);
    }
    put_fb_px(fb, cx, cy, touching ? 0x00FFFFFFu : 0x00FF2020u);
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
    int l3 = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0,
                            RETRO_DEVICE_ID_JOYPAD_L3) != 0;

    input->touch_down = 0;
    input->touch_x = 0;
    input->touch_y = 0;
    g_stylus_touching = 0;

    /* Right stick moves the virtual stylus; left stick as fallback. */
    move_stylus_from_stick(RETRO_DEVICE_INDEX_ANALOG_RIGHT);
    {
        int16_t rx = input_state_cb(0, RETRO_DEVICE_ANALOG,
                                    RETRO_DEVICE_INDEX_ANALOG_RIGHT,
                                    RETRO_DEVICE_ID_ANALOG_X);
        int16_t ry = input_state_cb(0, RETRO_DEVICE_ANALOG,
                                    RETRO_DEVICE_INDEX_ANALOG_RIGHT,
                                    RETRO_DEVICE_ID_ANALOG_Y);
        if ((rx > -STYLUS_DEADZONE && rx < STYLUS_DEADZONE) &&
            (ry > -STYLUS_DEADZONE && ry < STYLUS_DEADZONE))
            move_stylus_from_stick(RETRO_DEVICE_INDEX_ANALOG_LEFT);
    }

    if (pressed) {
        /* Pointer is [-0x7fff, 0x7fff] over the full framebuffer. */
        int x = (int)((px + 0x7fff) * SEEDLESS_SCREEN_W / 0xfffe);
        int y = (int)((py + 0x7fff) * SEEDLESS_FB_H / 0xfffe);
        if (y >= SEEDLESS_SCREEN_H) {
            input->touch_down = 1;
            input->touch_x = x;
            input->touch_y = y - SEEDLESS_SCREEN_H;
            g_stylus_x = (float)input->touch_x;
            g_stylus_y = (float)input->touch_y;
            g_stylus_touching = 1;
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
            g_stylus_x = (float)input->touch_x;
            g_stylus_y = (float)input->touch_y;
            g_stylus_touching = 1;
        }
    } else if (l3) {
        /* L3 = stylus press at the virtual pointer. */
        input->touch_down = 1;
        input->touch_x = (int)(g_stylus_x + 0.5f);
        input->touch_y = (int)(g_stylus_y + 0.5f);
        g_stylus_touching = 1;
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

    draw_stylus_cursor(g_frame, g_stylus_touching);

    video_cb(g_frame, SEEDLESS_SCREEN_W, SEEDLESS_FB_H,
             SEEDLESS_SCREEN_W * sizeof(uint32_t));
}

static void present_audio(void)
{
    size_t avail = seedless_audio_frames_available();
    size_t want = (SEEDLESS_AUDIO_RATE / 60);
    size_t n;
    size_t real_n;

    if (want > sizeof g_audio_tmp / (2 * sizeof(int16_t)))
        want = sizeof g_audio_tmp / (2 * sizeof(int16_t));
    if (avail > want)
        avail = want;

    n = seedless_audio_read(g_audio_tmp, avail);
    real_n = n;
    if (n < want) {
        /* Pad to one video-frame of audio so RetroArch stays clocked. */
        memset(g_audio_tmp + n * 2, 0, (want - n) * 2 * sizeof(int16_t));
        n = want;
    }
    if (real_n > 0)
        seedless_audio_mark_consumed(real_n);
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

    libretro_set_core_options(cb, &g_libretro_supports_option_categories);
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
    int ms;

    if (!g_nds || !g_emu_running)
        return;

    check_variables();

    input_poll_cb();
    memset(&input, 0, sizeof input);
    input.keys = poll_keys();
    input.keys_mask = 0;
    poll_touch(&input);
    nds_input_set(g_nds, &input);

    /* Bounded wait: never block RetroArch's menu / input forever if the
     * emu thread stalls before producing a flip. */
    for (ms = 0; ms < 32; ms++) {
        if (recon_present_seen != recon_present_flips) {
            nds_wait_screen(g_nds);
            break;
        }
        usleep(1000);
    }

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
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,
         "Virtual stylus touch"},
        {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
         RETRO_DEVICE_ID_ANALOG_X, "Virtual stylus X"},
        {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
         RETRO_DEVICE_ID_ANALOG_Y, "Virtual stylus Y"},
        {0, 0, 0, 0, NULL},
    };

    if (!info || !info->path)
        return false;

#if !defined(__aarch64__) && !defined(__arm64__)
    if (log_cb)
        log_cb(RETRO_LOG_ERROR,
               CORE_NAME ": SeedlessDS-Core JIT requires AArch64; "
               "this x86_64 build cannot run games\n");
    return false;
#endif

    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
    set_seedless_root();
    apply_core_options(0);

    snprintf(g_rom_path, sizeof g_rom_path, "%s", info->path);

    g_nds = nds_create(NULL, 0, 0);
    if (!g_nds) {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, CORE_NAME ": nds_create failed\n");
        return false;
    }

    recon_present_set_mode(RECON_PRESENT_PREDICATE | RECON_PRESENT_NO_UPLOAD);
    seedless_audio_reset();
    nds_apply_config(g_nds, g_config_bits);
    g_stylus_x = (float)(SEEDLESS_SCREEN_W / 2);
    g_stylus_y = (float)(SEEDLESS_SCREEN_H / 2);
    g_stylus_touching = 0;

    g_emu_running = 1;
    g_emu_started = 0;
    if (pthread_create(&g_emu_thread, NULL, emu_thread_main, NULL) != 0) {
        nds_destroy(g_nds);
        g_nds = NULL;
        g_emu_running = 0;
        return false;
    }

    /* Wait until the worker has entered nds_load_rom. */
    while (g_emu_running && !g_emu_started) {
        sched_yield();
    }

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
