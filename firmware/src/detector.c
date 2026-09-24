#include "detector.h"

void det_init(det_t *d) {
    d->state = DET_NORMAL;
    d->sustain = 0;
    d->recover_ticks = 0;
    d->suspect_ts = 0;
}

void det_update(det_t *d, double dev, long long now_ms, const cc_config_t *c) {
    switch (d->state) {
    case DET_NORMAL:
        if (dev < c->collapse_threshold_pct) {
            if (++d->sustain >= c->sustain_cycles) {
                d->state = DET_SUSPECT;
                d->suspect_ts = now_ms;
                d->sustain = 0;
            }
        } else {
            d->sustain = 0;
        }
        break;
    case DET_SUSPECT:
        if (dev > c->recovery_threshold_pct) {
            d->state = DET_RECOVERED;
            d->recover_ticks = 0;
        }
        break;
    case DET_RECOVERED:
        d->recover_ticks++;
        if (d->recover_ticks >= DET_RECOVER_WINDOWS && dev > c->recovery_threshold_pct)
            d->state = DET_NORMAL;
        else if (dev < c->collapse_threshold_pct) {
            d->state = DET_SUSPECT; /* fell back */
            d->suspect_ts = now_ms;
        }
        break;
    case DET_CONFIRMED:
        break; /* latched: only det_reset() clears */
    }
}

void det_confirm(det_t *d) {
    if (d->state == DET_SUSPECT)
        d->state = DET_CONFIRMED;
}

void det_reset(det_t *d) {
    det_state_t keep_ts = d->state;
    (void)keep_ts;
    d->state = DET_NORMAL;
    d->sustain = 0;
    d->recover_ticks = 0;
}
