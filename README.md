# SeedlessDS-libretro

基于 [SeedlessDS-Core](https://github.com/SapphireRhodonite/SeedlessDS-Core) 的 libretro NDS 模拟器核心。

SeedlessDS-Core 从 DraStic 重构而来，当前 CPU 后端为 **AArch64 JIT**，因此本核心仅支持 aarch64 主机或交叉编译目标（例如 Android arm64、Linux aarch64）。

## 功能

- 双屏竖直拼接输出（256×384，XRGB8888）
- 44100 Hz 立体声（经 OpenSL 桩送入 libretro）
- 标准 NDS 按键 + 触控（Pointer / Mouse 作用于下屏）
- L3：合盖
- 系统目录下的 BIOS / Firmware（与 SeedlessDS 相同文件名）

## 构建

```bash
git submodule update --init --recursive

# 本机即为 aarch64
make

# Debian/Ubuntu 交叉编译包
make CROSS=aarch64-linux-gnu-

# Bootlin 等自定义工具链（注意前缀以实际 bin 名为准）
make CROSS_PREFIX=/path/to/toolchain/bin/aarch64-buildroot-linux-gnu-
```

产物：`build/seedlessds_libretro.so`

依赖：CMake ≥ 3.18、pthread；目标缺少 zlib 时会自动拉取并静态编入 zlib 1.3.1。首次配置还会下载 UnRAR / LZMA SDK。

Android NDK 示例：

```bash
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-android -j
```

## 使用

1. 将 `seedlessds_libretro.so` 与 `seedlessds_libretro.info` 放到 RetroArch 的 cores / info 目录。
2. 将 BIOS/Firmware 放到 RetroArch **System Directory**（核心会复制到 `$system/User/system/`）：
   - `nds_bios_arm9.bin` / `nds_bios_arm7.bin`（或 `*_replacement.bin`）
   - 可选：`nds_firmware.bin` / `nds_firmware_modified.bin`
3. 加载 `.nds` / `.dsi` / `.ids` / `.srl` ROM。

## 架构说明

| 组件 | 说明 |
|------|------|
| `core/` | SeedlessDS-Core 子模块 |
| `libretro/libretro.c` | libretro API；模拟在后台线程运行，`retro_run` 做帧同步 |
| `libretro/opensl_stub.c` | 替换 OpenSL ES，音频写入环形缓冲 |
| `libretro/gl_stubs.c` | 软渲染路径下的 GLES 空实现 |
| `core/.../files_posix.c` | 非 JNI 文件 I/O（`SEEDLESS_ROOT`） |

## 归因

DraStic 归其原作者（含 Exophase）所有。本项目是重构库的 libretro 前端，不是官方 DraStic。游戏 ROM、原始 BIOS/Firmware 与 DraStic 二进制均不附带。依赖许可证见 `core/third_party/LICENSES.md`。
