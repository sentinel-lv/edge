/* Gateway fail-safe gate — the 7 rules from gateway/README §4.
 * Pure function (no I/O): easy to unit-test on host AND on target.
 * Rule 2 (LTE loss -> buffer) lives in uplink.py (StoreForward); everything
 * that can downgrade a trip lives here. This gate NEVER emits RESTORE
 * (rule 5: no auto-reclose in v1) and never trips on comms loss (rule 1).
 */
#pragma once
#include "arbiter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int lockout_engaged;   /* physical lockout switch OR boot lockout (rules 3,6) */
    int armed;             /* manual arm after boot (rule 3); 0 after watchdog reset */
    int comms_ok;          /* LoRa concentrator healthy (rule 1) */
    int auto_mode;         /* per-feeder opt-in (rule 7); default 0 = ALERT_ONLY */
    int has_last_isolate;
    long long last_isolate_ms;  /* rule 4: max 1 ISOLATE per 60 s */
} cc_safety_t;

typedef struct {
    cc_action_t action;    /* NONE | ALERT | ISOLATE — never a 4th value */
    char alarm[48];        /* "" | lockout | comms_loss_no_trip | rate_limited |
                              alert_only | not_armed */
} cc_gate_t;

void cc_safety_init(cc_safety_t *s);  /* boot state: LOCKOUT, disarmed (rule 3) */

/* Span stability (recovery-frontier guard): per-packet evaluation sees a
 * different span on every sample while a SUSPECT/RECOVERED wave sweeps past.
 * Only a physically stable fault repeats the same span. Feed every ISOLATE-
 * grade verdict here; actuate when it returns nonzero. ALERT/NONE pass
 * through immediately (return 0) and reset the streak. Mirrors the backend
 * ingest debounce so shadow and gateway agree.
 * DRIVE THIS PERIODICALLY (e.g. 4 Hz), not just per-packet: a minimum-quorum
 * fault produces few transitions then silence, and a pure event-driven caller
 * would never reach `need` consecutive evaluations. Same for the backend
 * confirmer. */
typedef struct {
    char up[8], down[8];
    int count;
} cc_stable_t;
void cc_stable_init(cc_stable_t *t);
int cc_stable_push(cc_stable_t *t, const cc_decision_t *d, int need);
void cc_safety_gate(const cc_safety_t *s, const cc_decision_t *d,
                    long long now_ms, cc_gate_t *out);

#ifdef __cplusplus
}
#endif
