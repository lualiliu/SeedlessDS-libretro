/* C port of spu/adpcm.S for non-AArch64 builds. */

#include <stdint.h>
#include "spu/adpcm_tables.h"
#include "spu/spu.h"

void spu_adpcm_decode_block(void *channel_ptr)
{
    spu_channel_t *c = (spu_channel_t *)channel_ptr;
    const spu_adpcm_tables_t *tables = &spu_adpcm_tables;
    uint32_t decoded = c->adpcm_decoded;
    const uint8_t *data = c->data;
    uint32_t index = c->adpcm_index;
    uint32_t word = *(const uint32_t *)(data + (decoded >> 1));
    int16_t *out = &c->adpcm_samples[decoded & 0x3fu];
    int32_t sample = (int16_t)c->adpcm_sample;

    c->adpcm_decoded = decoded + 8u;

    for (unsigned n = 0; n < 8; n++) {
        uint32_t nibble = (word >> (n * 4u)) & 0xfu;
        int32_t step = tables->step[index];
        int32_t diff = step >> 3;

        if (nibble & 1u)
            diff += step >> 2;
        if (nibble & 2u)
            diff += step >> 1;
        if (nibble & 4u)
            diff += step;

        if (nibble & 8u)
            sample += diff;
        else
            sample -= diff;

        if (sample > 32767)
            sample = 32767;
        if (sample < -32767)
            sample = -32767;

        out[n] = (int16_t)sample;

        int32_t next = (int32_t)index + tables->index_delta[nibble & 7u];
        if (next < 0)
            next = 0;
        if (next > 88)
            next = 88;
        index = (uint32_t)next;
    }

    c->adpcm_sample = (uint16_t)sample;
    c->adpcm_index = (uint8_t)index;
}
