#include "actuator.h"

void cc_act_init(cc_act_t *a) {
    a->tripped_latched = 0;
    a->relay_open = 0;
}

void cc_act_apply(cc_act_t *a, int gate_isolate, int manual_restore, int lockout) {
    if (lockout) {
        a->relay_open = 0;  /* rule 6: linemen trust the switch, not software */
        return;             /* latch preserved: re-trip stays visible after unlock */
    }
    if (manual_restore) {
        a->tripped_latched = 0;  /* crew-owned action only; never automatic */
        return;                  /* relay itself is reclosed at the switchgear */
    }
    if (gate_isolate) {
        a->tripped_latched = 1;
        a->relay_open = 1;
    }
    /* rule 5: no path here ever sets relay_open back to 0 on its own. */
}
