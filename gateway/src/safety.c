#include "safety.h"
#include <string.h>

void cc_safety_init(cc_safety_t *s) {
    /* Rule 3: watchdog reset -> come up in LOCKOUT, require manual arm. */
    s->lockout_engaged = 1;
    s->armed = 0;
    s->comms_ok = 1;
    s->auto_mode = 0; /* Rule 7: default ALERT_ONLY. */
    s->has_last_isolate = 0;
    s->last_isolate_ms = 0;
}

static void gate_out(cc_gate_t *o, cc_action_t a, const char *alarm) {
    o->action = a;
    strncpy(o->alarm, alarm, sizeof(o->alarm) - 1);
    o->alarm[sizeof(o->alarm) - 1] = '\0';
}

void cc_stable_init(cc_stable_t *t) {
    t->up[0] = t->down[0] = '\0';
    t->count = 0;
}

int cc_stable_push(cc_stable_t *t, const cc_decision_t *d, int need) {
    if (d->action != ACT_ISOLATE || !d->has_span) {
        t->count = 0;
        t->up[0] = t->down[0] = '\0';
        return 0;
    }
    if (strcmp(t->up, d->span_up) == 0 && strcmp(t->down, d->span_down) == 0)
        t->count++;
    else {
        strncpy(t->up, d->span_up, sizeof(t->up) - 1);
        strncpy(t->down, d->span_down, sizeof(t->down) - 1);
        t->up[sizeof(t->up) - 1] = t->down[sizeof(t->down) - 1] = '\0';
        t->count = 1;
    }
    return t->count >= need;
}

void cc_safety_gate(const cc_safety_t *s, const cc_decision_t *d,
                    long long now_ms, cc_gate_t *out) {
    if (d->action == ACT_NONE) {
        gate_out(out, ACT_NONE, "");
        return;
    }
    if (d->action == ACT_ALERT) {
        gate_out(out, ACT_ALERT, "");
        return;
    }
    /* From here: arbiter says ISOLATE. Every rule below can only downgrade. */
    /* Rules 3+6: lockout (boot or physical switch) overrides all software. */
    if (s->lockout_engaged) {
        gate_out(out, ACT_ALERT, "lockout");
        return;
    }
    if (!s->armed) {
        gate_out(out, ACT_ALERT, "not_armed");
        return;
    }
    /* Rule 1: loss of LoRa comms -> no trip, raise alarm. */
    if (!s->comms_ok) {
        gate_out(out, ACT_ALERT, "comms_loss_no_trip");
        return;
    }
    /* Rule 7: utility opts into AUTO per feeder; default ALERT_ONLY. */
    if (!s->auto_mode) {
        gate_out(out, ACT_ALERT, "alert_only");
        return;
    }
    /* Rule 4: max one ISOLATE per feeder per 60 s without manual reset. */
    if (s->has_last_isolate && now_ms - s->last_isolate_ms < 60000) {
        gate_out(out, ACT_ALERT, "rate_limited");
        return;
    }
    gate_out(out, ACT_ISOLATE, "");
    /* (stability applied by caller via cc_stable_push before actuation) */
    /* Rule 5 holds structurally: neither decide() nor this gate can produce
     * a RESTORE — cc_action_t here only ever carries NONE/ALERT/ISOLATE, and
     * actuator.c exposes no auto-reclose path (manual RESTORE command only). */
}
