#!/bin/bash
# Parity gate: Python decide() + C decide() must agree on every docs/vectors/*.json.
# Any disagreement = broken build (PROTOCOL §7, CONTRIBUTING §4).
# Run from the repo root: ./firmware/test/run.sh
set -eo pipefail
python3 firmware/test/gen_vectors.py
gcc -Wall -Wextra -Werror -std=c99 \
  -o /tmp/opencode/cc_arbiter_test \
  firmware/src/arbiter.c firmware/test/vectors_gen.c firmware/test/test_arbiter.c
/tmp/opencode/cc_arbiter_test | tail -n 2
gcc -Wall -Wextra -Werror -std=c99 \
  -o /tmp/opencode/cc_gw_hil \
  gateway/src/arbiter.c firmware/test/vectors_gen.c gateway/test/test_hil.c
/tmp/opencode/cc_gw_hil | tail -n 1
gcc -Wall -Wextra -Werror -std=c99 \
  -o /tmp/opencode/cc_safety \
  gateway/src/arbiter.c gateway/src/safety.c gateway/test/test_safety.c
/tmp/opencode/cc_safety | tail -n 1
gcc -Wall -Wextra -Werror -std=c99 \
  -o /tmp/opencode/cc_signal \
  firmware/src/baseline.c firmware/src/detector.c firmware/src/arbiter.c firmware/test/test_signal.c -lm
/tmp/opencode/cc_signal | tail -n 1
gcc -Wall -Wextra -Werror -std=c99 \
  -o /tmp/opencode/cc_actuate \
  gateway/src/actuator.c gateway/src/beacon.c gateway/test/test_actuate.c
/tmp/opencode/cc_actuate | tail -n 1
if [ "${PARITY_SKIP_BOM:-0}" = "1" ]; then
  python3 -m pytest backend/tests simulator/tests gateway/tests hardware/tests -q \
    --deselect hardware/tests/test_bom.py::test_no_unpriced_rows 2>&1 | tail -n 1
else
  python3 -m pytest backend/tests simulator/tests gateway/tests hardware/tests -q 2>&1 | tail -n 1
fi
