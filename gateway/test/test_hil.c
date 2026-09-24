/* Gateway HIL: same shared arbiter.c, same vectors. Builds through the
 * gateway/src symlink to prove it is one file, not a copy. */
#include <stdio.h>
#include <string.h>
#include "../src/arbiter.h"
#include "../../firmware/test/vectors_gen.h"

int main(void) {
    int i, failures = 0;
    cc_config_t cfg;
    cc_config_default(&cfg);
    for (i = 0; i < VEC_NCASES; i++) {
        const vec_case_t *vc = &VEC_CASES[i];
        cc_decision_t d;
        cc_decide(vc->nodes, vc->n, vc->now_ms, &cfg, &d);
        int ok = (d.action == vc->exp_action);
        printf("[%s] gateway-hil %-22s -> %d\n", ok ? "PASS" : "FAIL", vc->name, d.action);
        failures += !ok;
    }
    puts(failures ? "GATEWAY HIL: FAIL" : "GATEWAY HIL: all green (shared file).");
    return failures != 0;
}
