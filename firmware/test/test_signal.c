/* Host tests for baseline EWMA + detector FSM (mirror simulator/sim/node.py). */
#include <stdio.h>
#include <math.h>
#include "../src/baseline.h"
#include "../src/detector.h"

static int failures = 0;
#define CHECK(cond, name) do { \
    printf("[%s] %s\n", (cond) ? "PASS" : "FAIL", name); \
    if (!(cond)) failures++; \
} while (0)

int main(void) {
    cc_config_t cfg;
    cc_config_default(&cfg);

    /* EWMA: alpha = DT/TAU, converges, first-sample init. */
    { cc_ewma_t e = {0, 0};
      double a = cc_ewma_alpha(0.5, 600.0);
      CHECK(fabs(a - 0.5 / 600.0) < 1e-12, "ewma alpha = DT/TAU");
      cc_ewma_update(&e, 4.9, a);
      CHECK(e.initialized && fabs(e.value - 4.9) < 1e-9, "ewma inits on first sample");
      cc_ewma_update(&e, 0.1, a);
      CHECK(e.value < 4.9 && e.value > 4.8, "ewma slow: single collapsed sample barely moves it"); }
    /* EWMA tracks slow drift (temperature/load) over many ticks. */
    { cc_ewma_t e; cc_ewma_init(&e, 4.9);
      double a = cc_ewma_alpha(0.5, 600.0);
      int k; for (k = 0; k < 7200; k++) cc_ewma_update(&e, 5.1, a); /* 1 h */
      CHECK(fabs(e.value - 5.1) < 0.01, "ewma follows slow drift within an hour"); }

    /* Detector: needs sustain_cycles consecutive collapsed windows. */
    { det_t d; det_init(&d);
      int k; for (k = 0; k < 4; k++) det_update(&d, -98.0, 1000 + k * 50, &cfg);
      CHECK(d.state == DET_NORMAL, "detector waits out 4/5 windows");
      det_update(&d, -98.0, 1200, &cfg);
      CHECK(d.state == DET_SUSPECT && d.suspect_ts == 1200, "SUSPECT on 5th window with ts"); }
    /* Single blip does not latch. */
    { det_t d; det_init(&d);
      det_update(&d, -98.0, 1000, &cfg);
      det_update(&d, -2.0, 1050, &cfg);
      CHECK(d.state == DET_NORMAL && d.sustain == 0, "transient resets sustain"); }
    /* SUSPECT -> RECOVERED above recovery threshold. */
    { det_t d; det_init(&d);
      int k; for (k = 0; k < 5; k++) det_update(&d, -98.0, 1000 + k * 50, &cfg);
      det_update(&d, -5.0, 1300, &cfg);
      CHECK(d.state == DET_RECOVERED, "recovers above -20%"); }
    /* RECOVERED -> NORMAL after 2 s clean; falls back on re-collapse. */
    { det_t d; det_init(&d);
      int k; for (k = 0; k < 5; k++) det_update(&d, -98.0, 1000 + k * 50, &cfg);
      det_update(&d, -5.0, 1300, &cfg);
      for (k = 0; k < 39; k++) det_update(&d, -5.0, 1350 + k * 50, &cfg);
      CHECK(d.state == DET_RECOVERED, "still RECOVERED before 2 s");
      det_update(&d, -5.0, 3350, &cfg);
      CHECK(d.state == DET_NORMAL, "NORMAL after 2 s clean"); }
    { det_t d; det_init(&d);
      int k; for (k = 0; k < 5; k++) det_update(&d, -98.0, 1000 + k * 50, &cfg);
      det_update(&d, -5.0, 1300, &cfg);
      det_update(&d, -97.0, 1350, &cfg);
      CHECK(d.state == DET_SUSPECT, "falls back to SUSPECT on re-collapse"); }
    /* CONFIRMED latches through clean field; only reset clears. */
    { det_t d; det_init(&d);
      int k; for (k = 0; k < 5; k++) det_update(&d, -98.0, 1000 + k * 50, &cfg);
      det_confirm(&d);
      for (k = 0; k < 100; k++) det_update(&d, -1.0, 2000 + k * 50, &cfg);
      CHECK(d.state == DET_CONFIRMED, "CONFIRMED latches (baseline frozen in fw)");
      det_reset(&d);
      CHECK(d.state == DET_NORMAL, "manual reset clears"); }
    /* Rain-grade dip (-25%) never asserts, however long. */
    { det_t d; det_init(&d);
      int k; for (k = 0; k < 200; k++) det_update(&d, -25.0, 1000 + k * 50, &cfg);
      CHECK(d.state == DET_NORMAL, "rain dip never reaches SUSPECT"); }

    puts(failures ? "SIGNAL: FAIL" : "SIGNAL: baseline + detector green.");
    return failures != 0;
}
