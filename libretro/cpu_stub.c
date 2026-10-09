/* Placeholder CPU backend for non-AArch64 hosts.
 * Seedless-core currently only ships an AArch64 JIT; x86_64 cannot emulate. */

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
