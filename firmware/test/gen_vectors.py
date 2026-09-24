"""Generate C test cases from docs-vectors JSON (firmware README Phase 2).

CI gate: the C and Python arbiters must agree on every vector.
Usage: python3 firmware/test/gen_vectors.py   # writes firmware/test/vectors_gen.{h,c}
"""
import json, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
VDIR = ROOT / "docs" / "vectors"
OUT_C = pathlib.Path(__file__).resolve().parent / "vectors_gen.c"
OUT_H = pathlib.Path(__file__).resolve().parent / "vectors_gen.h"

STATES = {"NORMAL": "ST_NORMAL", "SUSPECT": "ST_SUSPECT", "CONFIRMED": "ST_CONFIRMED",
          "RECOVERED": "ST_RECOVERED", "OFFLINE": "ST_OFFLINE"}
ACTIONS = {"NONE": "ACT_NONE", "ALERT": "ACT_ALERT", "ISOLATE": "ACT_ISOLATE"}

def cstr(s):
    return '"' + s.replace('"', '\\"') + '"'

cases = []
for fp in sorted(VDIR.glob("*.json")):
    v = json.loads(fp.read_text())
    exp = v["expected"]
    span = exp.get("fault_span")
    cases.append({
        "name": fp.stem,
        "now": v["now_ms"],
        "nodes": [(n["node_id"], n["state"], n.get("deviation_pct", 0.0), n.get("ts", 0))
                  for n in v["nodes"]],
        "action": exp["action"],
        "span": tuple(span) if span else None,
        "reason": exp.get("reason_contains", ""),
    })

with open(OUT_H, "w") as f:
    f.write('#pragma once\n#include "../src/arbiter.h"\n\n')
    f.write("typedef struct {\n    const char *name;\n    long long now_ms;\n    const cc_node_t *nodes;\n    int n;\n    cc_action_t exp_action;\n    int exp_span;\n    const char *exp_up, *exp_down;\n    const char *exp_reason_sub;\n} vec_case_t;\n\n")
    f.write(f"extern const vec_case_t VEC_CASES[{len(cases)}];\nextern const int VEC_NCASES;\n")

with open(OUT_C, "w") as f:
    f.write('// AUTO-GENERATED from docs-vectors JSON - do not edit.\n#include "vectors_gen.h"\n\n')
    for idx, c in enumerate(cases):
        f.write(f"static const cc_node_t vec{idx}_nodes[] = {{\n")
        for nid, st, dev, ts in c["nodes"]:
            f.write(f"    {{\"{nid}\", {STATES[st]}, {dev}, {ts}LL}},\n")
        f.write("};\n")
    f.write(f"\nconst vec_case_t VEC_CASES[{len(cases)}] = {{\n")
    for idx, c in enumerate(cases):
        n = len(c["nodes"])
        if c["span"]:
            span = f"1, {cstr(c['span'][0])}, {cstr(c['span'][1])}"
        else:
            span = '0, "", ""'
        f.write(f"    {{{cstr(c['name'])}, {c['now']}LL, vec{idx}_nodes, {n}, "
                f"{ACTIONS[c['action']]}, {span}, {cstr(c['reason'])}}},\n")
    f.write("};\n")
    f.write(f"const int VEC_NCASES = {len(cases)};\n")
print(f"generated {len(cases)} cases -> {OUT_C.name}")
