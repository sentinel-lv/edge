/* Host tests: actuator latch (no auto-reclose) + beacon slot assignment. */
#include <stdio.h>
#include "../src/actuator.h"
#include "../src/beacon.h"

static int failures = 0;
#define CHECK(cond, name) do { \
    printf("[%s] %s\n", (cond) ? "PASS" : "FAIL", name); \
    if (!(cond)) failures++; \
} while (0)

int main(void) {
    /* Actuator: ISOLATE opens + latches; nothing auto-closes. */
    { cc_act_t a; cc_act_init(&a);
      cc_act_apply(&a, 0, 0, 0);
      CHECK(!a.relay_open && !a.tripped_latched, "idle stays closed-circuit"); }
    { cc_act_t a; cc_act_init(&a);
      cc_act_apply(&a, 1, 0, 0);
      CHECK(a.relay_open && a.tripped_latched, "gated ISOLATE trips + latches"); }
    { cc_act_t a; cc_act_init(&a);
      cc_act_apply(&a, 1, 0, 0);
      cc_act_apply(&a, 0, 0, 0);  /* verdict clears, no new command */
      cc_act_apply(&a, 0, 0, 0);
      CHECK(a.relay_open && a.tripped_latched, "no auto-reclose while latched"); }
    { cc_act_t a; cc_act_init(&a);
      cc_act_apply(&a, 1, 0, 0);
      cc_act_apply(&a, 0, 1, 0);  /* crew RESTORE */
      CHECK(!a.tripped_latched && a.relay_open, "manual restore clears latch (field reclose separate)"); }
    { cc_act_t a; cc_act_init(&a);
      cc_act_apply(&a, 1, 0, 1);  /* lockout engaged */
      CHECK(!a.relay_open, "lockout forces coil off"); }
    { cc_act_t a; cc_act_init(&a);
      cc_act_apply(&a, 1, 0, 0);
      cc_act_apply(&a, 0, 0, 1);  /* lockout during latched trip */
      CHECK(!a.relay_open && a.tripped_latched, "lockout opens coil, latch survives for visibility"); }
    /* Beacon slots: deterministic, bounded, garbage-safe. */
    CHECK(cc_slot_of("N-007", 8) == 7, "slot N-007 % 8 == 7");
    CHECK(cc_slot_of("N-012", 8) == 4, "slot N-012 % 8 == 4");
    { int k, seen[8] = {0};
      for (k = 1; k <= 12; k++) {
          char id[8]; snprintf(id, sizeof id, "N-%03d", k);
          int s = cc_slot_of(id, 8);
          if (s < 0 || s > 7) { CHECK(0, "slots in range"); break; }
          seen[s] = 1;
      }
      { int n = 0, j; for (j = 0; j < 8; j++) n += seen[j];
        CHECK(n >= 6, "12 nodes spread over slots"); } }
    CHECK(cc_slot_of("bogus", 8) == 0, "bad ID -> slot 0, no crash");
    CHECK(cc_slot_of("N-007", 0) == 0, "zero slots -> 0, no div-by-zero");
    CHECK(cc_slot_of(0, 8) == 0, "null ID -> 0, no crash");

    puts(failures ? "ACTUATE: FAIL" : "ACTUATE: latch + slots green.");
    return failures != 0;
}
