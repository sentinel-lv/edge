#include "baseline.h"

void cc_ewma_init(cc_ewma_t *e, double x0) {
    e->value = x0;
    e->initialized = 1;
}

void cc_ewma_update(cc_ewma_t *e, double x, double alpha) {
    if (!e->initialized) {
        cc_ewma_init(e, x);
        return;
    }
    e->value += alpha * (x - e->value);
}

double cc_ewma_alpha(double dt_s, double tau_s) {
    return dt_s / tau_s;
}
