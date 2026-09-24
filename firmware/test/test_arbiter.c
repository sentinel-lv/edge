/* Host tests: run arbiter.c against docs/vectors/ (via vectors_gen.c).
 * Exit 0 = all vectors agree with the Python decide(). */
#include <stdio.h>
#include <string.h>
#include "../src/arbiter.h"
#include "vectors_gen.h"

static const char *action_name(cc_action_t a) {
    return a == ACT_ISOLATE ? "ISOLATE" : a == ACT_ALERT ? "ALERT" : "NONE";
}

static int failures = 0;

static void check_case(const vec_case_t *vc) {
    cc_config_t cfg;
    cc_decision_t d;
    cc_config_default(&cfg);
    cc_decide(vc->nodes, vc->n, vc->now_ms, &cfg, &d);
    int ok = 1;
    if (d.action != vc->exp_action)
        ok = 0;
    if (vc->exp_span && !(d.has_span && strcmp(d.span_up, vc->exp_up) == 0 &&
                          strcmp(d.span_down, vc->exp_down) == 0))
        ok = 0;
    if (!vc->exp_span && d.has_span)
        ok = 0;
    if (vc->exp_reason_sub[0] && !strstr(d.reason, vc->exp_reason_sub))
        ok = 0;
    printf("[%s] %-22s -> %-7s span=%s%s%s reason=%s conf=%.2f%s\n",
           ok ? "PASS" : "FAIL", vc->name, action_name(d.action),
           d.has_span ? d.span_up : "-", d.has_span ? "<->" : "",
           d.has_span ? d.span_down : "", d.reason, d.confidence,
           d.has_latency ? "" : "");
    if (!ok) {
        printf("       expected action=%s", action_name(vc->exp_action));
        if (vc->exp_span)
            printf(" span=%s<->%s", vc->exp_up, vc->exp_down);
        if (vc->exp_reason_sub[0])
            printf(" reason~=%s", vc->exp_reason_sub);
        printf("\n");
        failures++;
    }
}

static void check_all_suspect_never_isolate(void) {
    /* Property test mirror of backend test_all_suspect_never_isolate. */
    cc_node_t nodes[12];
    cc_config_t cfg;
    cc_decision_t d;
    int i;
    cc_config_default(&cfg);
    for (i = 0; i < 12; i++) {
        snprintf(nodes[i].node_id, sizeof(nodes[i].node_id), "N-%03d", i + 1);
        nodes[i].state = ST_SUSPECT;
        nodes[i].deviation_pct = -95.0;
        nodes[i].ts = 1000;
    }
    cc_decide(nodes, 12, 1500, &cfg, &d);
    int ok = (d.action == ACT_NONE);
    printf("[%s] all_suspect_property -> %s (reason=%s)\n", ok ? "PASS" : "FAIL",
           action_name(d.action), d.reason);
    if (!ok)
        failures++;
}

int main(void) {
    int i;
    printf("vectors: %d\n", VEC_NCASES);
    for (i = 0; i < VEC_NCASES; i++)
        check_case(&VEC_CASES[i]);
    check_all_suspect_never_isolate();
    { /* all-stale unit check (not a vector file: PROTOCOL table is frozen) */
        cc_node_t nodes[12];
        cc_config_t cfg2;
        cc_decision_t d2;
        int i, ok;
        cc_config_default(&cfg2);
        for (i = 0; i < 12; i++) {
            snprintf(nodes[i].node_id, sizeof(nodes[i].node_id), "N-%03d", i + 1);
            nodes[i].state = ST_OFFLINE;
            nodes[i].deviation_pct = 0.0;
            nodes[i].ts = 1000;
        }
        cc_decide(nodes, 12, 32000, &cfg2, &d2);
        ok = (d2.action == ACT_ALERT && !d2.has_span && strstr(d2.reason, "stale") != 0);
        printf("[%s] all_stale -> %s (%s)\n", ok ? "PASS" : "FAIL",
               action_name(d2.action), d2.reason);
        if (!ok)
            failures++;
    }
    if (failures) {
        printf("RESULT: %d FAILURES — C/Python arbiters disagree, build broken.\n", failures);
        return 1;
    }
    printf("RESULT: all green — C agrees with Python on every vector.\n");
    return 0;
}
