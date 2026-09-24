"""MQTT over LTE, store-and-forward buffer (gateway README Phase 4, rule 2).

Cloud is never in the trip path: when LTE drops the gateway keeps deciding
locally and buffers uplink for 1 h, replaying in order on reconnect.
TLS + client certificate and signed remote config live at the transport edge
(paho-mqtt + ssl), not in this module — this file owns the buffering contract.
"""
from collections import deque

ONE_HOUR_S = 3600

class StoreForward:
    """Bounded FIFO: keeps up to 1 h of telemetry at 2 Hz x 12 nodes (~86k)."""

    CAPACITY = ONE_HOUR_S * 2 * 12  # 86400 frames

    def __init__(self, capacity: int = CAPACITY):
        self.buf: deque = deque(maxlen=capacity)
        self.online = True
        self.dropped = 0

    def publish(self, frame: dict) -> str:
        """Returns 'sent' | 'buffered'. Oldest dropped past capacity (counted)."""
        if self.online:
            return "sent"
        if len(self.buf) >= self.buf.maxlen:
            self.dropped += 1
        self.buf.append(frame)
        return "buffered"

    def reconnect(self) -> list[dict]:
        """Replay buffered frames in original order, then resume live send."""
        out = list(self.buf)
        self.buf.clear()
        self.online = True
        return out

    def go_offline(self):
        self.online = False

    def __len__(self):
        return len(self.buf)
