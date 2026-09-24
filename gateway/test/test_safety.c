/* Host tests for the 7 fail-safe rules (gateway README §4, one test each). */
#include <stdio.h>
#include <string.h>
#include "../src/safety.h"

static int failures = 0;
#define CHECK(cond, name) do { \
    printf("[%s] %s\n", (cond) ? "PASS" : "FAIL", name); \
    if (!(cond)) failures++; \
} while (0)

static void isolate_decision(cc_decision_t *d) {
    cc_node_t n[3] = {
        {"N-006", ST_NORMAL, -2.0, 1000},
        {"N-007", ST_SUSPECT, -87.0, 1000},
        {"N-008", ST_SUSPECT, -88.0, 1100},
    };
    cc_config_t cfg;
    cc_config_default(&cfg);
    cc_decide(n, 3, 1200, &cfg, d);
}

static cc_safety_t armed_auto(void) {
    cc_safety_t s;
    cc_safety_init(&s);
    s.lockout_engaged = 0;  /* manual arm after boot */
    s.armed = 1;
    s.auto_mode = 1;
    return s;
}

int main(void) {
    cc_decision_t d;
    cc_gate_t g;
    isolate_decision(&d);
    if (d.action != ACT_ISOLATE) { printf("setup broken\n"); return 1; }

    /* Rule 3: fresh boot = LOCKOUT + disarmed -> no trip. */
    { cc_safety_t s; cc_safety_init(&s); cc_safety_gate(&s, &d, 5000, &g);
      CHECK(g.action == ACT_ALERT && strcmp(g.alarm, "lockout") == 0, "r3 boot enters LOCKOUT"); }
    /* Rule 6: physical lockout overrides an armed AUTO gateway. */
    { cc_safety_t s = armed_auto(); s.lockout_engaged = 1; cc_safety_gate(&s, &d, 5000, &g);
      CHECK(g.action == ACT_ALERT && strcmp(g.alarm, "lockout") == 0, "r6 lockout switch overrides software"); }
    /* Rule 3b: lockout cleared but not yet armed -> still no trip. */
    { cc_safety_t s; cc_safety_init(&s); s.lockout_engaged = 0; cc_safety_gate(&s, &d, 5000, &g);
      CHECK(g.action == ACT_ALERT && strcmp(g.alarm, "not_armed") == 0, "r3 requires manual arm"); }
    /* Rule 1: LoRa comms loss -> alarm, never a trip. */
    { cc_safety_t s = armed_auto(); s.comms_ok = 0; cc_safety_gate(&s, &d, 5000, &g);
      CHECK(g.action == ACT_ALERT && strcmp(g.alarm, "comms_loss_no_trip") == 0, "r1 comms loss never trips"); }
    /* Rule 7: default ALERT_ONLY. */
    { cc_safety_t s = armed_auto(); s.auto_mode = 0; cc_safety_gate(&s, &d, 5000, &g);
      CHECK(g.action == ACT_ALERT && strcmp(g.alarm, "alert_only") == 0, "r7 default ALERT_ONLY"); }
    /* Rule 4: second ISOLATE within 60 s is rate-limited. */
    { cc_safety_t s = armed_auto(); s.has_last_isolate = 1; s.last_isolate_ms = 1000;
      cc_safety_gate(&s, &d, 11000, &g);
      CHECK(g.action == ACT_ALERT && strcmp(g.alarm, "rate_limited") == 0, "r4 one ISOLATE per 60s"); }
    /* Rule 4b: ...but after 60 s it may trip again. */
    { cc_safety_t s = armed_auto(); s.has_last_isolate = 1; s.last_isolate_ms = 1000;
      cc_safety_gate(&s, &d, 61001, &g);
      CHECK(g.action == ACT_ISOLATE, "r4 window re-opens after 60s"); }
    /* Happy path: armed + AUTO + comms + outside window -> ISOLATE. */
    { cc_safety_t s = armed_auto(); cc_safety_gate(&s, &d, 70000, &g);
      CHECK(g.action == ACT_ISOLATE && g.alarm[0] == '\0', "armed AUTO gateway trips"); }
    /* Rule 5: gate output alphabet is NONE/ALERT/ISOLATE — no RESTORE exists. */
    { cc_safety_t s = armed_auto(); cc_safety_gate(&s, &d, 70000, &g);
      CHECK(g.action == ACT_NONE || g.action == ACT_ALERT || g.action == ACT_ISOLATE, "r5 no auto-reclose path"); }
    /* NONE decisions pass through untouched (no spurious alarms). */
    { cc_safety_t s = armed_auto(); cc_decision_t n;
      cc_node_t nn[2] = {{"N-001", ST_NORMAL, -1.0, 1000}, {"N-002", ST_NORMAL, -2.0, 1000}};
      cc_config_t cfg; cc_config_default(&cfg);
      cc_decide(nn, 2, 1500, &cfg, &n);
      cc_safety_gate(&s, &n, 1500, &g);
      CHECK(g.action == ACT_NONE && g.alarm[0] == '\0', "NONE passes through"); }

    /* Span stability: a walking span (recovery frontier) never reaches need=3. */
    { cc_stable_t t; cc_stable_init(&t);
      cc_config_t cfg; cc_config_default(&cfg);
      /* three snapshots, same shape, span walks downstream each sample */
      cc_node_t w0[3] = {{"N-006", ST_NORMAL, -2.0, 2000},
                         {"N-007", ST_SUSPECT, -87.0, 2000},
                         {"N-008", ST_SUSPECT, -86.0, 2040}};
      cc_node_t w1[3] = {{"N-007", ST_NORMAL, -3.0, 2040},
                         {"N-008", ST_SUSPECT, -87.0, 2040},
                         {"N-009", ST_SUSPECT, -86.0, 2080}};
      cc_node_t w2[3] = {{"N-008", ST_NORMAL, -2.0, 2080},
                         {"N-009", ST_SUSPECT, -87.0, 2080},
                         {"N-010", ST_SUSPECT, -86.0, 2120}};
      cc_decision_t d;
      int fired = 0;
      cc_decide(w0, 3, 2080, &cfg, &d); fired |= cc_stable_push(&t, &d, 3);
      cc_decide(w1, 3, 2120, &cfg, &d); fired |= cc_stable_push(&t, &d, 3);
      cc_decide(w2, 3, 2160, &cfg, &d); fired |= cc_stable_push(&t, &d, 3);
      CHECK(!fired, "stability suppresses sweeping verdicts"); }
    puts(failures ? "SAFETY: FAIL" : "SAFETY: all 7 rules green.");
    return failures != 0;
}

/* (appended) span-stability tests */
