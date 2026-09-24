/* Relay actuation interlocks (gateway README §3-4). Pure logic: the coil
 * driver + flyback diode + opto-isolation are hardware; THIS file decides
 * what the coil may do. No auto-reclose in v1 (rule 5): ISOLATE latches the
 * relay open until a manual restore; software can never close it by itself.
 * Physical lockout (rule 6) forces the coil off regardless of latch state. */
#pragma once

typedef struct {
    int tripped_latched;  /* an ISOLATE was acted on and not yet manually restored */
    int relay_open;       /* 1 = coil driven = feeder isolated */
} cc_act_t;

void cc_act_init(cc_act_t *a);
/* gate_isolate: gated ISOLATE verdict for this evaluation (post safety+stable).
 * manual_restore: explicit crew/backoffice RESTORE command (validated upstream).
 * lockout: physical lockout switch engaged. */
void cc_act_apply(cc_act_t *a, int gate_isolate, int manual_restore, int lockout);
