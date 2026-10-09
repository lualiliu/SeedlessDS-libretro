# SeedlessDS libretro core
# AArch64 host or cross-toolchain required (SeedlessDS-Core JIT).

TARGET := seedlessds_libretro
BUILD  ?= build
CMAKE  ?= cmake

# Examples:
#   make                                    # native aarch64
#   make CROSS=aarch64-linux-gnu-           # Debian cross package
#   make CROSS_PREFIX=/path/to/aarch64-linux-gcc/bin/aarch64-linux-
CROSS ?=
CROSS_PREFIX ?=

ifneq ($(CROSS_PREFIX),)
  CMAKE_ARGS += -DCMAKE_C_COMPILER=$(CROSS_PREFIX)gcc
  CMAKE_ARGS += -DCMAKE_CXX_COMPILER=$(CROSS_PREFIX)g++
  CMAKE_ARGS += -DCMAKE_ASM_COMPILER=$(CROSS_PREFIX)gcc
  CMAKE_ARGS += -DCMAKE_SYSTEM_NAME=Linux
  CMAKE_ARGS += -DCMAKE_SYSTEM_PROCESSOR=aarch64
else ifneq ($(CROSS),)
  CMAKE_ARGS += -DCMAKE_C_COMPILER=$(CROSS)gcc
  CMAKE_ARGS += -DCMAKE_CXX_COMPILER=$(CROSS)g++
  CMAKE_ARGS += -DCMAKE_ASM_COMPILER=$(CROSS)gcc
  CMAKE_ARGS += -DCMAKE_SYSTEM_NAME=Linux
  CMAKE_ARGS += -DCMAKE_SYSTEM_PROCESSOR=aarch64
endif

.PHONY: all clean configure

all: configure
	$(CMAKE) --build $(BUILD) -j$(shell nproc 2>/dev/null || echo 2)

configure:
	@mkdir -p $(BUILD)
	$(CMAKE) -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release $(CMAKE_ARGS)

clean:
	rm -rf $(BUILD)

install: all
	$(CMAKE) --install $(BUILD)
