# SeedlessDS-libretro

基于 [SeedlessDS-Core](https://github.com/SapphireRhodonite/SeedlessDS-Core) 的 libretro NDS 模拟器核心。

A libretro Nintendo DS core built on [SeedlessDS-Core](https://github.com/SapphireRhodonite/SeedlessDS-Core).

SeedlessDS-Core 从 DraStic 重构而来，当前 CPU 后端为 **AArch64 JIT**，因此本核心仅支持 aarch64 主机或交叉编译目标（例如 Android arm64、Linux aarch64）。

SeedlessDS-Core is a DraStic reconstruction. The CPU backend is an **AArch64 JIT**, so this core only runs on aarch64 hosts or cross-compiled aarch64 targets (e.g. Android arm64, Linux aarch64).

---

## 功能 / Features

- 双屏竖直拼接输出（256×384，XRGB8888）  
  Dual screens stacked vertically (256×384, XRGB8888)
- 44100 Hz 立体声（OpenSL 桩 → 环形缓冲 → libretro）  
  44100 Hz stereo (OpenSL stub → ring buffer → libretro)
- 标准 NDS 按键 + 触控（Pointer / Mouse 作用于下屏）  
  Standard NDS buttons + touch (Pointer / Mouse on the lower screen)
- 虚拟触笔：右摇杆（左摇杆备用）移动指针，L3 按下触碰下屏；画面显示准星  
  Virtual stylus: right stick (left stick fallback) moves the cursor, L3 presses the lower screen; crosshair overlay
- 系统目录下的 BIOS / Firmware（与 SeedlessDS 相同文件名）  
  BIOS / Firmware under the system directory (same filenames as SeedlessDS)
- 核心选项映射 DraStic / Seedless `config_bits`（跳帧、高分 3D、音频、存档、Slot-2 等）  
  Core options map DraStic / Seedless `config_bits` (frameskip, hires 3D, audio, saves, Slot-2, etc.)

---

## 构建 / Build

```bash
git submodule update --init --recursive

# 本机即为 aarch64 / Native aarch64 host
make

# Debian/Ubuntu 交叉编译包 / Debian/Ubuntu cross packages
make CROSS=aarch64-linux-gnu-

# Bootlin 等自定义工具链（前缀以实际 bin 名为准）
# Custom toolchain (match the actual binary prefix)
make CROSS_PREFIX=/path/to/toolchain/bin/aarch64-buildroot-linux-gnu-
```

产物 / Output：`build/seedlessds_libretro.so`

依赖 / Dependencies：CMake ≥ 3.18、pthread。目标缺少 zlib 时会自动拉取并静态编入 zlib 1.3.1；首次配置还会下载 UnRAR / LZMA SDK。

Requires CMake ≥ 3.18 and pthread. If the target lacks zlib, zlib 1.3.1 is fetched and linked statically. The first configure also downloads UnRAR / LZMA SDK.

Android NDK 示例 / Android NDK example：

```bash
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-android -j
```

x86_64 可在定义 `SEEDLESS_ALLOW_NON_AARCH64` 时编译（JIT 桩），但**无法真正运行游戏**。

An x86_64 build is possible with `SEEDLESS_ALLOW_NON_AARCH64` (JIT stubs), but it **cannot actually run games**.

---

## 使用 / Usage

1. 将 `seedlessds_libretro.so` 与 `seedlessds_libretro.info` 放到 RetroArch 的 cores / info 目录。  
   Place `seedlessds_libretro.so` and `seedlessds_libretro.info` in RetroArch’s cores / info directories.

2. 将 BIOS / Firmware 放到 RetroArch **System Directory**（核心会复制到 `$system/User/system/`）：  
   Put BIOS / Firmware in the RetroArch **System Directory** (the core copies them to `$system/User/system/`):
   - `nds_bios_arm9.bin` / `nds_bios_arm7.bin`（或 `*_replacement.bin`）
   - 可选 / optional：`nds_firmware.bin` / `nds_firmware_modified.bin`

3. 加载 `.nds` / `.dsi` / `.ids` / `.srl` ROM。  
   Load a `.nds` / `.dsi` / `.ids` / `.srl` ROM.

环境变量 / Environment：核心通过 `SEEDLESS_ROOT`（默认等于 System Directory）定位 `User/system/` 下的 BIOS 与数据。

The core uses `SEEDLESS_ROOT` (defaults to the System Directory) to find BIOS and data under `User/system/`.

---

## 核心选项 / Core options

在 RetroArch「快速菜单 → 核心选项」中配置。选项按分类对应 Seedless / DraStic 的 `config_bits`；部分项需**重新加载 ROM** 才生效（标注 Restart）。

Configure under RetroArch Quick Menu → Core Options. Options map to Seedless / DraStic `config_bits` by category; some require a **ROM reload** (marked Restart).

| 分类 Category | 主要选项 Main options |
|---------------|------------------------|
| Video | Frameskip type/value、Safe frameskip、Hires 3D、Threaded 3D、Edge marking、Keep main on top、16-bit colour、Show FPS |
| Audio | Sound、Latency、Volume、Host mic / audio path |
| System | Cheats、Savestate extra、Raw `.sav`、RTC、Ignore gamecard limit、Auto-trim、Preload ROMs、Slot-2 |
| Advanced | Compose thread count、Button profile、Lua |

默认值 / Defaults：

- **Frameskip Type** = `None`
- **Compose Thread Count** = `8`
- **Sound** = enabled；**Audio Latency** = low

若菜单仍显示旧默认值，可在核心选项中「重置为默认」。

If the menu still shows old defaults, use “Reset to defaults” in core options.

---

## 架构说明 / Architecture

| 组件 Component | 说明 Description |
|----------------|------------------|
| `core/` | SeedlessDS-Core 子模块 / SeedlessDS-Core submodule |
| `libretro/libretro.c` | libretro API；模拟在后台线程运行，`retro_run` 做帧与音频同步 / libretro API; emu on a worker thread, `retro_run` syncs video/audio |
| `libretro/libretro_core_options.h` | 核心选项定义（v2） / Core option definitions (v2) |
| `libretro/opensl_stub.c` | 替换 OpenSL ES，音频写入环形缓冲 / OpenSL ES stub → ring buffer |
| `libretro/gl_stubs.c` | 软渲染路径下的 GLES 空实现 / GLES stubs for software path |
| `libretro/cpu_stub.c` | 非 aarch64 构建用的 JIT / LZMA 桩 / JIT/LZMA stubs for non-aarch64 builds |
| `core/.../files_posix.c` | 非 JNI 文件 I/O（`SEEDLESS_ROOT`） / Non-JNI file I/O (`SEEDLESS_ROOT`) |
| `core/.../mirror.c` | 内存映射；优先 memfd，兼容 ashmem / Memory maps; prefer memfd, ashmem fallback |

---

## 归因 / Attribution

DraStic 归其原作者（含 Exophase）所有。本项目是重构库的 libretro 前端，不是官方 DraStic。游戏 ROM、原始 BIOS / Firmware 与 DraStic 二进制均不附带。依赖许可证见 `core/third_party/LICENSES.md`。

DraStic is owned by its original authors (including Exophase). This project is a libretro frontend for the reconstructed library, not official DraStic. Game ROMs, original BIOS / Firmware, and DraStic binaries are not included. See `core/third_party/LICENSES.md` for dependency licenses.
