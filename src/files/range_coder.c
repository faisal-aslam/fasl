// range_coder.c
#include "range_coder.h"
#include <stdlib.h>

#define PROB_BITS 12
#define PROB_INIT (1u << (PROB_BITS - 1))
#define MOVE_BITS 5
#define TOP_VALUE (1u << 24)

static uint16_t *alloc_probs(int order) {
    size_t cnt = order ? 256u * 256u : 256u;
    uint16_t *p = (uint16_t *)malloc(cnt * sizeof(uint16_t));
    if (!p) return NULL;
    for (size_t i = 0; i < cnt; i++) p[i] = (uint16_t)PROB_INIT;
    return p;
}

/* ---------------- encoder ---------------- */
typedef struct {
    uint64_t low;
    uint32_t range;
    uint8_t cache;
    uint64_t cache_size;
    uint8_t *out;
    size_t pos, cap;
    bool overflow;
} Enc;

static void enc_put(Enc *e, uint8_t b) {
    if (e->pos < e->cap) e->out[e->pos++] = b;
    else e->overflow = true;
}

static void enc_shift_low(Enc *e) {
    if ((uint32_t)e->low < 0xFF000000u || (e->low >> 32) != 0) {
        uint8_t carry = (uint8_t)(e->low >> 32);
        uint8_t temp = e->cache;
        do {
            enc_put(e, (uint8_t)(temp + carry));
            temp = 0xFF;
        } while (--e->cache_size != 0);
        e->cache = (uint8_t)((uint32_t)e->low >> 24);
    }
    e->cache_size++;
    e->low = (e->low & 0x00FFFFFFu) << 8;
}

static inline void enc_bit(Enc *e, uint16_t *p, int bit) {
    uint32_t bound = (e->range >> PROB_BITS) * (*p);
    if (!bit) {
        e->range = bound;
        *p = (uint16_t)(*p + (((1u << PROB_BITS) - *p) >> MOVE_BITS));
    } else {
        e->low += bound;
        e->range -= bound;
        *p = (uint16_t)(*p - (*p >> MOVE_BITS));
    }
    while (e->range < TOP_VALUE) {
        e->range <<= 8;
        enc_shift_low(e);
    }
}

size_t rc_encode_bytes(const uint8_t *in, size_t n, int order, uint8_t *out, size_t out_cap) {
    if (!in || !out || n == 0) return 0;
    uint16_t *probs = alloc_probs(order);
    if (!probs) return 0;

    Enc e = {.low = 0, .range = 0xFFFFFFFFu, .cache = 0, .cache_size = 1,
             .out = out, .pos = 0, .cap = out_cap, .overflow = false};

    uint32_t ctx = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t b = in[i];
        uint16_t *tree = probs + (order ? (size_t)ctx * 256u : 0);
        uint32_t node = 1;
        for (int k = 7; k >= 0; k--) {
            int bit = (b >> k) & 1;
            enc_bit(&e, &tree[node], bit);
            node = (node << 1) | (uint32_t)bit;
        }
        ctx = b;
    }
    for (int i = 0; i < 5; i++) enc_shift_low(&e);

    free(probs);
    return e.overflow ? 0 : e.pos;
}

/* ---------------- decoder ---------------- */
typedef struct {
    const uint8_t *in;
    size_t pos, len;
    uint32_t range, code;
    bool overrun;
} Dec;

static uint8_t dec_next(Dec *d) {
    if (d->pos < d->len) return d->in[d->pos++];
    d->overrun = true;
    return 0;
}

static inline int dec_bit(Dec *d, uint16_t *p) {
    uint32_t bound = (d->range >> PROB_BITS) * (*p);
    int bit;
    if (d->code < bound) {
        d->range = bound;
        *p = (uint16_t)(*p + (((1u << PROB_BITS) - *p) >> MOVE_BITS));
        bit = 0;
    } else {
        d->code -= bound;
        d->range -= bound;
        *p = (uint16_t)(*p - (*p >> MOVE_BITS));
        bit = 1;
    }
    while (d->range < TOP_VALUE) {
        d->range <<= 8;
        d->code = (d->code << 8) | dec_next(d);
    }
    return bit;
}

bool rc_decode_bytes(const uint8_t *in, size_t in_len, int order, uint8_t *out, size_t n) {
    if (!in || !out) return false;
    uint16_t *probs = alloc_probs(order);
    if (!probs) return false;

    Dec d = {.in = in, .pos = 0, .len = in_len, .range = 0xFFFFFFFFu, .code = 0, .overrun = false};
    if (dec_next(&d) != 0) { /* first byte is always 0 */
        free(probs);
        return false;
    }
    for (int i = 0; i < 4; i++) d.code = (d.code << 8) | dec_next(&d);

    uint32_t ctx = 0;
    for (size_t i = 0; i < n; i++) {
        uint16_t *tree = probs + (order ? (size_t)ctx * 256u : 0);
        uint32_t node = 1;
        for (int k = 0; k < 8; k++) node = (node << 1) | (uint32_t)dec_bit(&d, &tree[node]);
        out[i] = (uint8_t)(node & 0xFF);
        ctx = out[i];
        if (d.overrun) break;
    }
    free(probs);
    return !d.overrun;
}
