/* Placeholder CPU / JIT symbols for non-AArch64 hosts.
 * Seedless-core currently only ships an AArch64 JIT; x86_64 cannot emulate. */

#include <stdint.h>
#include <stdio.h>
#include "seedlessds/cpu_backend.h"

static int stub_init(nds_t *m, arm_t *c)
{
    (void)m;
    (void)c;
    fprintf(stderr, "SeedlessDS-libretro: CPU JIT requires AArch64. "
                    "This host build cannot run games.\n");
    return -1;
}

static void stub_reset(arm_t *c) { (void)c; }
static uint32_t stub_run(arm_t *c, uint32_t cycles)
{
    (void)c;
    return cycles;
}
static void stub_invalidate(arm_t *c, uint32_t a, uint32_t n)
{
    (void)c;
    (void)a;
    (void)n;
}
static void stub_flush(arm_t *c) { (void)c; }
static void stub_irq(arm_t *c) { (void)c; }
static void stub_save(arm_t *c, state_stream_t *s)
{
    (void)c;
    (void)s;
}
static void stub_shutdown(arm_t *c) { (void)c; }

static const cpu_backend_t stub = {
    "stub-no-jit",
    stub_init,
    stub_reset,
    stub_run,
    stub_invalidate,
    stub_flush,
    stub_irq,
    stub_save,
    stub_shutdown,
};

const cpu_backend_t *nds_cpu_backend_interp(void)
{
    return &stub;
}

const cpu_backend_t *nds_cpu_backend_jit_arm64(void)
{
    return &stub;
}

const cpu_backend_t *nds_cpu_backend_default(void)
{
    return &stub;
}

/* Symbols normally provided by jit-arm64/cache.c + memhooks.S */

void jit_arena_protect_rwx(unsigned char *base)
{
    (void)base;
}

void jit_arena_regions_init(unsigned char *arena)
{
    (void)arena;
}

void *jit_cache_lookup_or_compile(void *ctx, uint32_t dir)
{
    (void)ctx;
    (void)dir;
    return NULL;
}

void jit_cache_refresh_block_info(unsigned char *ctx)
{
    (void)ctx;
}

int32_t jit_cache_flush(uint8_t *ctx, uint32_t arg)
{
    (void)ctx;
    (void)arg;
    return 0;
}

uint32_t jit_watch_write8(unsigned char *machine, uint32_t dir)
{
    (void)machine;
    (void)dir;
    return 1;
}

uint32_t jit_watch_write16(unsigned char *machine, uint32_t dir)
{
    (void)machine;
    (void)dir;
    return 1;
}

uint32_t jit_watch_write32(unsigned char *machine, uint32_t dir)
{
    (void)machine;
    (void)dir;
    return 1;
}

/* memhooks.S tail-call into JIT dispatcher; unused without AArch64 JIT. */
void script_lua_register_dual_slot_tail(void *a0, uint64_t a1)
{
    (void)a0;
    (void)a1;
}

/* LZMA 7zCrc.c (x86) expects this from CpuArch.c, which we do not build. */
int CPU_Is_InOrder(void)
{
    return 1;
}
