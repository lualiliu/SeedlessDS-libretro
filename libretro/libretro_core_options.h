#ifndef SEEDLESSDS_LIBRETRO_CORE_OPTIONS_H
#define SEEDLESSDS_LIBRETRO_CORE_OPTIONS_H

#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maps 1:1 onto SeedlessDS / DraStic config_bits (see core config_unpack_word). */

struct retro_core_option_v2_category option_cats_us[] = {
   {
      "video",
      "Video",
      "Frameskip, 3D quality and screen options."
   },
   {
      "audio",
      "Audio",
      "Sound enable, latency and volume."
   },
   {
      "system",
      "System",
      "Saves, cheats, RTC and cartridge options."
   },
   {
      "advanced",
      "Advanced",
      "Threading, Lua and controller profile."
   },
   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_us[] = {
   {
      "seedlessds_frameskip_type",
      "Frameskip Type",
      NULL,
      "None disables skipping. Manual skips N of every N+1 frames. Auto skips only when behind, up to N consecutive frames.",
      NULL,
      "video",
      {
         { "none", "None" },
         { "manual", "Manual" },
         { "auto", "Automatic" },
         { NULL, NULL },
      },
      "none"
   },
   {
      "seedlessds_frameskip_value",
      "Frameskip Value",
      NULL,
      "N for manual/automatic frameskip (0–9).",
      NULL,
      "video",
      {
         { "0", NULL }, { "1", NULL }, { "2", NULL }, { "3", NULL },
         { "4", NULL }, { "5", NULL }, { "6", NULL }, { "7", NULL },
         { "8", NULL }, { "9", NULL },
         { NULL, NULL },
      },
      "4"
   },
   {
      "seedlessds_frameskip_safe",
      "Safe Frameskip",
      NULL,
      "Force safer frameskip behaviour for games that glitch with aggressive skipping.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_hires_3d",
      "High-Resolution 3D",
      NULL,
      "Render 3D at 2× native resolution. Much heavier on CPU.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_threaded_3d",
      "Multi-threaded 3D",
      NULL,
      "Offload 3D rendering to another thread. Can cause glitches; disable when reporting bugs.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_disable_edge_marking",
      "Disable Edge Marking",
      NULL,
      "Disable DS 3D edge-marking.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_fix_main_screen",
      "Keep Main Screen on Top",
      NULL,
      "Always show the main (2D+3D) engine on the top screen. Useful for some 2D Sonic titles.",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_screen_16bit",
      "16-bit Colour",
      NULL,
      "Use 16-bit framebuffer path (lower quality, sometimes faster).",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_show_fps",
      "Show FPS (Core Overlay)",
      NULL,
      "Core-drawn FPS indicator (separate from RetroArch's FPS display).",
      NULL,
      "video",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_sound",
      "Sound",
      NULL,
      "Enable emulated DS sound.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "seedlessds_audio_latency",
      "Audio Latency",
      NULL,
      "OpenSL buffer size: Low ≈ 1 frame, Medium ≈ 2, High ≈ 4.",
      NULL,
      "audio",
      {
         { "low", "Low" },
         { "medium", "Medium" },
         { "high", "High" },
         { NULL, NULL },
      },
      "low"
   },
   {
      "seedlessds_audio_volume",
      "Audio Volume",
      NULL,
      "Core output gain level.",
      NULL,
      "audio",
      {
         { "0", "Quiet" },
         { "1", "Normal" },
         { "2", "Loud" },
         { "3", "Max" },
         { NULL, NULL },
      },
      "1"
   },
   {
      "seedlessds_mic_audio_path",
      "Host Mic / Audio Path",
      NULL,
      "Enables the Android host-mic audio path flag (bit26). Keep enabled for normal speaker output.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "seedlessds_cheats",
      "Enable Cheats",
      NULL,
      "Allow the core cheat engine.",
      NULL,
      "system",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "seedlessds_savestate_extra",
      "Store In-game Saves in Savestates",
      NULL,
      "Embed cartridge save data inside savestates. Can corrupt saves in some games (e.g. Pokémon) if not synced.",
      NULL,
      "system",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_raw_sav",
      "Raw .sav Backup Format (Restart)",
      NULL,
      "Write cartridge saves as raw .sav instead of .dsv.",
      NULL,
      "system",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_rtc_system_time",
      "RTC Uses Host Time",
      NULL,
      "Initialise DS RTC from the host clock.",
      NULL,
      "system",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "seedlessds_ignore_gamecard_limit",
      "Ignore Gamecard Size Limit (Restart)",
      NULL,
      "Try this if a patched ROM fails to load.",
      NULL,
      "system",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_auto_trim",
      "Auto-Trim ROM (Restart)",
      NULL,
      "Only load ROM data up to the length in the header.",
      NULL,
      "system",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_preload_roms",
      "Uncompress / Preload ROMs (Restart)",
      NULL,
      "Unpack compressed games to storage (uses less RAM, slower load).",
      NULL,
      "system",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "seedlessds_slot2",
      "Slot-2 Device (Restart)",
      NULL,
      "GBA slot / expansion pack behaviour (0=auto).",
      NULL,
      "system",
      {
         { "0", "Auto" },
         { "1", "GBA / Game Pak" },
         { "2", "None / ROM mirror" },
         { "3", "GPIO" },
         { "4", "Mapped both CPUs" },
         { "5", "Rumble / Motion" },
         { NULL, NULL },
      },
      "0"
   },
   {
      "seedlessds_thread_count",
      "Compose Thread Count",
      NULL,
      "Worker thread count field (0 = core default).",
      NULL,
      "advanced",
      {
         { "0", "Default" },
         { "1", NULL }, { "2", NULL }, { "3", NULL }, { "4", NULL },
         { "5", NULL }, { "6", NULL }, { "7", NULL }, { "8", NULL },
         { NULL, NULL },
      },
      "8"
   },
   {
      "seedlessds_button_profile",
      "Button Profile",
      NULL,
      "Core input remap profile index (0–7).",
      NULL,
      "advanced",
      {
         { "0", NULL }, { "1", NULL }, { "2", NULL }, { "3", NULL },
         { "4", NULL }, { "5", NULL }, { "6", NULL }, { "7", NULL },
         { NULL, NULL },
      },
      "0"
   },
   {
      "seedlessds_lua",
      "Enable Lua Scripts",
      NULL,
      "Allow Lua scripting support in the core.",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};

struct retro_core_options_v2 options_us = {
   option_cats_us,
   option_defs_us
};

static inline void libretro_set_core_options(retro_environment_t environ_cb,
                                             bool *categories_supported)
{
   unsigned version = 0;

   if (!environ_cb || !categories_supported)
      return;

   *categories_supported = false;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   if (version >= 2) {
      *categories_supported =
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_us);
      return;
   }

   if (version >= 1) {
      size_t i, num_options = 0;
      struct retro_core_option_definition *v1;

      while (option_defs_us[num_options].key)
         num_options++;

      v1 = (struct retro_core_option_definition *)calloc(
         num_options + 1, sizeof(*v1));
      if (!v1)
         return;

      for (i = 0; i < num_options; i++) {
         struct retro_core_option_value *src = option_defs_us[i].values;
         struct retro_core_option_value *dst = v1[i].values;

         v1[i].key = option_defs_us[i].key;
         v1[i].desc = option_defs_us[i].desc;
         v1[i].info = option_defs_us[i].info;
         v1[i].default_value = option_defs_us[i].default_value;
         while (src->value) {
            dst->value = src->value;
            dst->label = src->label;
            src++;
            dst++;
         }
      }

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, v1);
      free(v1);
      return;
   }

   /* Legacy SET_VARIABLES */
   {
      size_t i, j, num_options = 0, option_index = 0;
      struct retro_variable *variables;
      char **values_buf;

      while (option_defs_us[num_options].key)
         num_options++;

      variables = (struct retro_variable *)calloc(num_options + 1,
                                                  sizeof(*variables));
      values_buf = (char **)calloc(num_options, sizeof(char *));
      if (!variables || !values_buf)
         goto legacy_done;

      for (i = 0; i < num_options; i++) {
         const char *desc = option_defs_us[i].desc;
         const char *def = option_defs_us[i].default_value;
         struct retro_core_option_value *values = option_defs_us[i].values;
         size_t buf_len = 3;
         size_t default_index = 0;
         size_t num_values = 0;

         while (values[num_values].value) {
            if (def && strcmp(values[num_values].value, def) == 0)
               default_index = num_values;
            buf_len += strlen(values[num_values].value);
            num_values++;
         }

         if (!desc || num_values == 0)
            continue;

         buf_len += num_values - 1 + strlen(desc);
         values_buf[i] = (char *)calloc(buf_len, sizeof(char));
         if (!values_buf[i])
            goto legacy_done;

         strcpy(values_buf[i], desc);
         strcat(values_buf[i], "; ");
         strcat(values_buf[i], values[default_index].value);
         for (j = 0; j < num_values; j++) {
            if (j == default_index)
               continue;
            strcat(values_buf[i], "|");
            strcat(values_buf[i], values[j].value);
         }

         variables[option_index].key = option_defs_us[i].key;
         variables[option_index].value = values_buf[i];
         option_index++;
      }

      environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

   legacy_done:
      if (values_buf) {
         for (i = 0; i < num_options; i++)
            free(values_buf[i]);
         free(values_buf);
      }
      free(variables);
   }
}

#ifdef __cplusplus
}
#endif

#endif
