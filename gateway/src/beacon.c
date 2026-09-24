#include "beacon.h"

int cc_slot_of(const char *node_id, int nslots) {
    int idx = 0, i;
    if (nslots <= 0 || !node_id)
        return 0;
    /* expect "N-007": digits after "N-" */
    if (node_id[0] != 'N' || node_id[1] != '-')
        return 0;
    for (i = 2; node_id[i] >= '0' && node_id[i] <= '9'; i++)
        idx = idx * 10 + (node_id[i] - '0');
    if (i == 2)
        return 0; /* no digits */
    return idx % nslots;
}
