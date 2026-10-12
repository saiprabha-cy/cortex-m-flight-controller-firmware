"""Two emulated machines on one CAN bus: flight controller (OBC) + power node."""
import time
import pytest
from fc_link import FcLink, flag_list
from renode_ctl import Renode
from conftest import BUILD, _require_env


@pytest.fixture
def two_nodes():
    _require_env()
    r = Renode(monitor_port=12800)
    r.cmd("$linkport=4800"); r.cmd(f"$obclog=@{BUILD / 'logs/uart1_obc.log'}"); r.cmd(f"$nodelog=@{BUILD / 'logs/uart1_node.log'}")
    r.include("renode/two_nodes.resc"); r.start()
    fc = FcLink("tcp://localhost:4800", timeout=8); fc.ensure_application()
    yield fc
    fc.close(); r.close()


def test_power_node_telemetry_and_node_loss_detection(two_nodes):
    fc = two_nodes; fc.pump(4.0)
    t = fc.last_telem
    assert 7.0 < t.node_vbat_v < 8.0 and 0.5 < t.node_current_a < 1.5, "power telemetry must arrive over CAN"
    assert fc.stats()["can_rx"] >= 6 and "NODE_LOST" not in flag_list(t.flags)
    fc.can_node_silence(True)                                    # node stops transmitting
    deadline = time.time() + 60
    while time.time() < deadline and "NODE_LOST" not in flag_list(fc.last_telem.flags): fc.pump(0.2)
    assert "NODE_LOST" in flag_list(fc.last_telem.flags)
    deadline = time.time() + 30
    while time.time() < deadline and fc.last_telem.mode != "DEGRADED": fc.pump(0.2)
    assert fc.last_telem.mode == "DEGRADED"
    assert "NODE_LOST" in [f["type"] for f in fc.faults()]
    fc.can_node_silence(False)                                   # node returns -> recovery after the quiet period
    deadline = time.time() + 240
    while time.time() < deadline and fc.last_telem.mode != "NOMINAL": fc.pump(0.3)
    assert fc.last_telem.mode == "NOMINAL"
