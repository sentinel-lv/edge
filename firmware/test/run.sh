#!/bin/bash
# Parity gate: the Python decide() and the C decide() must agree on every
# vector in the protocol repo. Any disagreement is a broken build
# (PROTOCOL §7, CONTRIBUTING §4).
#
# Run from the repo root:  ./firmware/test/run.sh
set -eo pipefail

# Build into a temp dir created at run time. This used to be a hardcoded
# /tmp/opencode/, which does not exist on anyone else's machine — the script
# died at the first link step on a clean clone.
BUILD="$(mktemp -d "${TMPDIR:-/tmp}/cc-parity.XXXXXX")"
trap 'rm -rf "$BUILD"' EXIT

CFLAGS=(-Wall -Wextra -Werror -std=c99)

python3 firmware/test/gen_vectors.py

gcc "${CFLAGS[@]}" -o "$BUILD/cc_arbiter_test" \
  firmware/src/arbiter.c firmware/test/vectors_gen.c firmware/test/test_arbiter.c
"$BUILD/cc_arbiter_test" | tail -n 2

gcc "${CFLAGS[@]}" -o "$BUILD/cc_gw_hil" \
  gateway/src/arbiter.c firmware/test/vectors_gen.c gateway/test/test_hil.c
"$BUILD/cc_gw_hil" | tail -n 1

gcc "${CFLAGS[@]}" -o "$BUILD/cc_safety" \
  gateway/src/arbiter.c gateway/src/safety.c gateway/test/test_safety.c
"$BUILD/cc_safety" | tail -n 1

gcc "${CFLAGS[@]}" -o "$BUILD/cc_signal" \
  firmware/src/baseline.c firmware/src/detector.c firmware/src/arbiter.c \
  firmware/test/test_signal.c -lm
"$BUILD/cc_signal" | tail -n 1

gcc "${CFLAGS[@]}" -o "$BUILD/cc_actuate" \
  gateway/src/actuator.c gateway/src/beacon.c gateway/test/test_actuate.c
"$BUILD/cc_actuate" | tail -n 1

# Python-side tests that live in THIS repo. backend/ and simulator/ moved to
# the `platform` repo and hardware/ to its own in the org split, so running
# them from here silently collected nothing.
if command -v pytest >/dev/null 2>&1 || python3 -c "import pytest" >/dev/null 2>&1; then
  python3 -m pytest gateway/tests -q 2>&1 | tail -n 1
else
  echo "pytest not installed — skipped gateway/tests (C gate above still ran)"
fi

echo "parity gate: OK"
