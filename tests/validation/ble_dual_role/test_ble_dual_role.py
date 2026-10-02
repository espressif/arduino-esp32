import logging

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

    # DUT0 connects first
    LOGGER.info("DUT0 connecting as client...")
    dut0.expect_exact("[DUT0] Connected as client to DUT1", timeout=30)
    dut0.expect_exact("[DUT0] Remote value: DUT1 data", timeout=10)
    dut0.expect_exact("[DUT0] Dual role OK", timeout=10)

    # DUT1 detects DUT0's disconnect and then connects
    LOGGER.info("DUT1 waiting for DUT0, then connecting as client...")
    dut1.expect_exact("[DUT1] Peer disconnected from our server", timeout=30)
    dut1.expect_exact("[DUT1] DUT0 finished, now connecting as client", timeout=10)
    dut1.expect_exact("[DUT1] Connected as client to DUT0", timeout=30)
    dut1.expect_exact("[DUT1] Remote value: DUT0 data", timeout=10)
    dut1.expect_exact("[DUT1] Dual role OK", timeout=10)

    LOGGER.info("Dual-role test passed!")
