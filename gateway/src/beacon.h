/* Time-sync beacon helpers (firmware README §5, gateway README §3).
 * Slotted TDMA: transmit slot = node_index % nslots. node_id format N-%03d
 * (PROTOCOL §1); unparseable IDs fall in slot 0 (never crash on bad input). */
#pragma once
int cc_slot_of(const char *node_id, int nslots);
