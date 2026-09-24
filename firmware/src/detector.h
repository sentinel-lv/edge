/* Local collapse detector FSM. Mirrors simulator/sim/node.py sub-step
 * semantics: det_update() is called once per 50 ms detector window with the
 * window deviation_pct. Thresholds come from cc_config_t (PROTOCOL §5) so
 * they stay runtime-configurable: collapse->SUSPECT after sustain_cycles
 * consecutive windows; SUSPECT->RECOVERED above recovery threshold;
 * RECOVERED->NORMAL after 2 s clean; quorum latch via det_confirm()
 * (set by the mesh layer, never by this FSM alone). */
#pragma once
#include "arbiter.h"

typedef enum { DET_NORMAL = 0, DET_SUSPECT = 1, DET_RECOVERED = 2, DET_CONFIRMED = 3 } det_state_t;

#define DET_RECOVER_WINDOWS 40  /* 2 s at 50 ms */

typedef struct {
    det_state_t state;
    int sustain;
    int recover_ticks;
    long long suspect_ts;
} det_t;

void det_init(det_t *d);
void det_update(det_t *d, double deviation_pct, long long now_ms, const cc_config_t *c);
void det_confirm(det_t *d);  /* quorum met */
void det_reset(det_t *d);    /* manual reset / RESTORE command */
