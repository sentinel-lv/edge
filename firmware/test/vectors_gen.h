#pragma once
#include "../src/arbiter.h"

typedef struct {
    const char *name;
    long long now_ms;
    const cc_node_t *nodes;
    int n;
    cc_action_t exp_action;
    int exp_span;
    const char *exp_up, *exp_down;
    const char *exp_reason_sub;
} vec_case_t;

extern const vec_case_t VEC_CASES[7];
extern const int VEC_NCASES;
