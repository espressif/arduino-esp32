import logging
import re

from conftest import rand_str4


def test_ble_dual_role(dut, ci_job_id):
    LOGGER = logging.getLogger(__name__)

    dut0 = dut[0]
    dut1 = dut[1]

    base = ci_job_id if ci_job_id else rand_str4()
    name0 = f"DR0_{base}"
    name1 = f"DR1_{base}"
    names_str = f"{name0},{name1}"
    LOGGER.info(f"Dual-role names: {names_str}")

    dut0.expect_exact("[DUT0] Device ready for names", timeout=120)
    dut1.expect_exact("[DUT1] Device ready for names", timeout=120)
    dut0.expect_exact("[DUT0] Send names:")
    dut1.expect_exact("[DUT1] Send names:")

    dut0.write(names_str)
    dut1.write(names_str)

    dut0.expect_exact(f"[DUT0] My name: {name0}, Peer: {name1}", timeout=10)
    dut1.expect_exact(f"[DUT1] My name: {name1}, Peer: {name0}", timeout=10)

    dut0.expect_exact("[DUT0] Init OK", timeout=30)
    dut1.expect_exact("[DUT1] Init OK", timeout=30)

    dut0.expect_exact("[DUT0] Server started", timeout=10)
    dut1.expect_exact("[DUT1] Server started", timeout=10)

    dut0.expect_exact("[DUT0] Advertising started", timeout=10)
    dut1.expect_exact("[DUT1] Advertising started", timeout=10)

    # Both scan for peer
    dut0.expect(r"\[DUT0\] Scanning for peer \(attempt \d+\)\.\.\.", timeout=10)
    dut1.expect(r"\[DUT1\] Scanning for peer \(attempt \d+\)\.\.\.", timeout=10)

    dut0.expect_exact("[DUT0] Found peer", timeout=60)
    dut1.expect_exact("[DUT1] Found peer", timeout=60)

    # DUT0 connects first and keeps its client link up.
    LOGGER.info("DUT0 connecting as client...")
    dut0.expect_exact("[DUT0] Connected as client to DUT1", timeout=30)
    dut0.expect_exact("[DUT0] Remote value: DUT1 data", timeout=10)
    dut0.expect_exact("[DUT0] Ready for reverse connect", timeout=10)

    # DUT1's server is now connected by DUT0. DUT1 opens one or two client
    # links back to DUT0's server, producing simultaneous client+server links.
    dut1.expect(r"\[DUT1\] Peer connected to our server handle=\d+", timeout=30)
    dut1.expect_exact("[DUT1] DUT0 client connected, opening reverse client", timeout=10)

    # Drain the optional reverse-client result lines, then the lookup summary.
    reverse_clients = 0
    for _ in range(3):
        m = dut1.expect(
            r"\[DUT1\] (Reverse client ([12]) (connected|failed: [^\r\n]*)|Lookup stack=[^\r\n]*)\r?\n",
            timeout=40,
        )
        line = m.group(0)
        if b"Lookup stack=" in line:
            m_lookup1 = re.search(
                r"Lookup stack=(\S+) conns=(\d+) clients=(\d+) hasServer=([01])\r?\n",
                line.decode(),
            )
            assert m_lookup1, line
            break
        if m.group(3) == b"connected":
            reverse_clients += 1
    else:
        raise AssertionError("DUT1 did not report its lookup summary")

    stack1, conns1, clients1, has_server1 = (
        m_lookup1.group(1),
        int(m_lookup1.group(2)),
        int(m_lookup1.group(3)),
        int(m_lookup1.group(4)),
    )
    assert clients1 == reverse_clients, f"DUT1 reported clients={clients1}, saw {reverse_clients} success lines"
    assert has_server1 == 1, "DUT1 getConnections() omitted its server connection"
    if stack1 == "Bluedroid":
        assert conns1 >= clients1 + 1, (
            f"DUT1 expected client+server descriptors ({clients1}+1), got {conns1}"
        )

    # DUT0 samples the same table while its client link is up.
    m0 = dut0.expect(
        r"\[DUT0\] (Reverse client connected to our server count=\d+ handle=\d+|Lookup stack=[^\r\n]*)\r?\n",
        timeout=30,
    )
    if b"Lookup stack=" not in m0.group(0):
        m0 = dut0.expect(r"\[DUT0\] Lookup stack=[^\r\n]*\r?\n", timeout=30)
    m_lookup0 = re.search(
        r"Lookup stack=(\S+) conns=(\d+) reverseConns=(\d+) hasClient=([01]) hasServer=([01])\r?\n",
        m0.group(0).decode(),
    )
    assert m_lookup0, m0.group(0).decode()
    stack0, conns0, reverse_conns, has_client0, has_server0 = (
        m_lookup0.group(1),
        int(m_lookup0.group(2)),
        int(m_lookup0.group(3)),
        int(m_lookup0.group(4)),
        int(m_lookup0.group(5)),
    )
    assert has_client0 == 1, "DUT0 getConnections() omitted its client connection"
    if reverse_conns > 0:
        assert has_server0 == 1, "DUT0 getConnections() omitted its server connection"
    if clients1 > 0:
        assert reverse_conns >= 1, "DUT0 saw no server connection for DUT1's reverse client"
    assert reverse_conns <= clients1 or clients1 == 0, (
        f"DUT0 saw {reverse_conns} server connections for {clients1} reverse clients"
    )
    if stack0 == "Bluedroid":
        assert conns0 >= 1 + reverse_conns, (
            f"DUT0 expected client+server descriptors (1+{reverse_conns}), got {conns0}"
        )
    assert stack0 == stack1, f"DUT stack mismatch: DUT0={stack0}, DUT1={stack1}"

    dut1.expect_exact("[DUT1] Dual role OK", timeout=20)
    dut0.expect_exact("[DUT0] Dual role OK", timeout=20)

    LOGGER.info("Dual-role test passed!")
