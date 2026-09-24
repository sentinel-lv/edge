/* EWMA baseline, tau ~= 10 min. Mirrors simulator/sim/node.py and the
 * Python model exactly: baseline += alpha * (x - baseline), alpha = DT/TAU.
 * Caller freezes updates while SUSPECT/CONFIRMED (safety rule, see detector). */
#pragma once

typedef struct { double value; int initialized; } cc_ewma_t;

void cc_ewma_init(cc_ewma_t *e, double x0);
void cc_ewma_update(cc_ewma_t *e, double x, double alpha);
double cc_ewma_alpha(double dt_s, double tau_s);
