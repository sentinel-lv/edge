/* Closed-Circuit arbiter — C port of backend/app/arbiter.py decide().
 * SHARED FILE with gateway/src/arbiter.c (symlink, not a copy).
 * PROTOCOL §5: pure snapshot logic, no I/O, no clock reads, no globals.
 * Vetoes 4 (rate limit) and 5 (ALERT_ONLY) live in the CALLER (safety.c /
 * backend events.py), which owns mode + last-isolate time.
 */
#ifndef CC_ARBITER_H
#define CC_ARBITER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ST_NORMAL = 0,
    ST_SUSPECT = 1,
    ST_CONFIRMED = 2,
    ST_RECOVERED = 3,
    ST_OFFLINE = 4
} cc_state_t;

typedef struct {
    char node_id[8];      /* "N-007" */
    cc_state_t state;
    double deviation_pct;
    long long ts;         /* ms: SUSPECT-assert time, else last sample */
} cc_node_t;

typedef struct {
    double collapse_threshold_pct;  /* default -60.0 */
    int sustain_cycles;             /* default 5 (enforced pre-SUSPECT, informational here) */
    int quorum_required;            /* default 2 */
    long long quorum_window_ms;     /* default 1500 */
    double recovery_threshold_pct;  /* default -20.0 */
    int global_collapse_veto;       /* default 1 */
} cc_config_t;

typedef enum { ACT_NONE = 0, ACT_ALERT = 1, ACT_ISOLATE = 2 } cc_action_t;

typedef struct {
    cc_action_t action;
    char reason[48];
    int has_span;
    char span_up[8];
    char span_down[8];
    int has_latency;
    long long latency_ms;
    double confidence;              /* 0.0 - 1.0 */
} cc_decision_t;

void cc_config_default(cc_config_t *c);

/* nodes ordered upstream -> downstream, n = count. Pure. */
void cc_decide(const cc_node_t *nodes, int n, long long now_ms,
               const cc_config_t *cfg, cc_decision_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CC_ARBITER_H */
