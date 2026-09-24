/* C port of backend/app/arbiter.py decide(). Behaviour must match exactly;
 * CI runs both against every file in docs-vectors. See arbiter.h. */
#include "arbiter.h"
#include <string.h>

void cc_config_default(cc_config_t *c) {
    c->collapse_threshold_pct = -60.0;
    c->sustain_cycles = 5;
    c->quorum_required = 2;
    c->quorum_window_ms = 1500;
    c->recovery_threshold_pct = -20.0;
    c->global_collapse_veto = 1;
}

static void set_dec(cc_decision_t *o, cc_action_t a, const char *r,
                    int hs, const char *up, const char *dn,
                    int hl, long long lat, double conf) {
    o->action = a;
    strncpy(o->reason, r, sizeof(o->reason) - 1);
    o->reason[sizeof(o->reason) - 1] = '\0';
    o->has_span = hs;
    if (hs && up && dn) {
        strncpy(o->span_up, up, sizeof(o->span_up) - 1);
        strncpy(o->span_down, dn, sizeof(o->span_down) - 1);
        o->span_up[sizeof(o->span_up) - 1] = '\0';
        o->span_down[sizeof(o->span_down) - 1] = '\0';
    } else {
        o->span_up[0] = o->span_down[0] = '\0';
    }
    o->has_latency = hl;
    o->latency_ms = lat;
    o->confidence = conf < 0.0 ? 0.0 : (conf > 1.0 ? 1.0 : conf);
}

static int is_healthy(const cc_node_t *n, const cc_config_t *c) {
    return (n->state == ST_NORMAL || n->state == ST_RECOVERED) &&
           n->deviation_pct > c->collapse_threshold_pct;
}

static int is_asserted(const cc_node_t *n, const cc_config_t *c, long long now_ms) {
    if (n->state != ST_SUSPECT && n->state != ST_CONFIRMED)
        return 0;
    if (n->deviation_pct > c->collapse_threshold_pct)
        return 0;
    if (n->state == ST_CONFIRMED)
        return 1; /* latched quorum: no expiry, cleared by reset/RESTORE */
    if (now_ms - n->ts > c->quorum_window_ms)
        return 0; /* SUSPECT vote expired (mesh collection window) */
    return 1;
}

void cc_decide(const cc_node_t *nodes, int n, long long now_ms,
               const cc_config_t *cfg_in, cc_decision_t *out) {
    cc_config_t dflt;
    const cc_config_t *c = cfg_in ? cfg_in : (cc_config_default(&dflt), &dflt);
    int i;

    if (n <= 0 || !nodes) {
        set_dec(out, ACT_NONE, "no_data", 0, 0, 0, 0, 0, 1.0);
        return;
    }

    /* Whole feeder silent = link/gateway outage: one alarm, never a trip. */
    {
        int i, active = 0;
        for (i = 0; i < n; i++)
            if (nodes[i].state != ST_OFFLINE)
                active++;
        if (active == 0) {
            set_dec(out, ACT_ALERT, "all_nodes_stale", 0, 0, 0, 0, 0, 0.6);
            return;
        }
    }

    /* Veto 1 — global collapse: every active node asserted => outage, never trip. */
    {
        int active = 0, all_down = 1;
        for (i = 0; i < n; i++) {
            if (nodes[i].state == ST_OFFLINE)
                continue;
            active++;
            if (!((nodes[i].state == ST_SUSPECT || nodes[i].state == ST_CONFIRMED) &&
                  nodes[i].deviation_pct <= c->collapse_threshold_pct)) {
                all_down = 0;
                break;
            }
        }
        if (c->global_collapse_veto && active >= 2 && all_down) {
            set_dec(out, ACT_NONE, "global_collapse_veto", 0, 0, 0, 0, 0, 0.95);
            return;
        }
    }

    /* NORMAL -> SUSPECT boundaries (OFFLINE never forms one: veto 3). */
    {
        int bcount = 0, bi = -1;
        for (i = 0; i < n - 1; i++) {
            if (nodes[i].state == ST_OFFLINE || nodes[i + 1].state == ST_OFFLINE)
                continue;
            if (is_healthy(&nodes[i], c) && is_asserted(&nodes[i + 1], c, now_ms)) {
                bcount++;
                bi = i;
            }
        }
        if (bcount == 0) {
            int noff = 0, nassert = 0, nactive_healthy = 0;
            for (i = 0; i < n; i++) {
                if (nodes[i].state == ST_OFFLINE)
                    noff++;
                else if (nodes[i].state == ST_SUSPECT || nodes[i].state == ST_CONFIRMED)
                    nassert++;
                else if (nodes[i].state == ST_NORMAL || nodes[i].state == ST_RECOVERED)
                    nactive_healthy++;
            }
            /* Veto 3 — comms loss is maintenance, never a vote. */
            if (noff > 0 && nassert == 0 && nactive_healthy > 0) {
                set_dec(out, ACT_ALERT, "node_offline", 0, 0, 0, 0, 0, 0.7);
                return;
            }
            if (nassert > 0) {
                set_dec(out, ACT_ALERT, "single_node_no_quorum", 0, 0, 0, 0, 0, 0.55);
                return;
            }
            set_dec(out, ACT_NONE, "no_fault", 0, 0, 0, 0, 0, 0.9);
            return;
        }
        /* Veto 2 is structural (boundary needs healthy upstream); ambiguity => ALERT. */
        if (bcount > 1) {
            set_dec(out, ACT_ALERT, "multiple_boundaries_ambiguous", 0, 0, 0, 0, 0, 0.5);
            return;
        }
        /* Quorum over asserted downstream neighbours (OFFLINE excluded: veto 3). */
        {
            long long first_ts = 0, newest_age = 0;
            int voters = 0, k, first = 1;
            long long max_ts = 0;
            for (k = bi + 1; k < n; k++) {
                if (is_asserted(&nodes[k], c, now_ms)) {
                    voters++;
                    if (first || nodes[k].ts < first_ts)
                        first_ts = nodes[k].ts;
                    if (nodes[k].ts > max_ts)
                        max_ts = nodes[k].ts;
                    first = 0;
                }
            }
            if (voters < c->quorum_required) {
                set_dec(out, ACT_ALERT, "quorum_not_met", 0, 0, 0, 0, 0, 0.55);
                return;
            }
            newest_age = now_ms - max_ts;
            {
                double conf = 0.95;
                if (voters <= c->quorum_required)
                    conf -= 0.15;
                if (newest_age > (long long)(c->quorum_window_ms * 0.8))
                    conf -= 0.15;
                set_dec(out, ACT_ISOLATE, "quorum_confirmed", 1,
                        nodes[bi].node_id, nodes[bi + 1].node_id,
                        1, now_ms - first_ts, conf);
                return;
            }
        }
    }
}
