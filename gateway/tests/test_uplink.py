"""Rule 2: LTE loss -> keep operating, buffer 1 h, replay in order (README §7.5)."""
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "src"))
from uplink import StoreForward

def _frame(i):
    return {"kind": "telemetry", "seq": i, "node_id": f"N-{i % 12 + 1:03d}"}

def test_online_sends_live():
    u = StoreForward()
    assert u.publish(_frame(0)) == "sent" and len(u) == 0

def test_offline_buffers_and_replays_in_order():
    u = StoreForward()
    u.go_offline()
    for i in range(100):
        assert u.publish(_frame(i)) == "buffered"
    replayed = u.reconnect()
    assert [f["seq"] for f in replayed] == list(range(100))
    assert len(u) == 0 and u.online
    assert u.publish(_frame(100)) == "sent"

def test_capacity_bounds_and_counts_drops():
    u = StoreForward(capacity=10)
    u.go_offline()
    for i in range(15):
        u.publish(_frame(i))
    assert len(u) == 10 and u.dropped == 5
    assert [f["seq"] for f in u.reconnect()] == list(range(5, 15))
