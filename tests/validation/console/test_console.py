import logging
from pytest_embedded import Dut
from time import sleep


def test_console(dut: Dut):
    LOGGER = logging.getLogger(__name__)

    # Phase 1: Interactive serial test — verify commands can be sent and responses received
    LOGGER.info("Waiting for REPL to be ready...")
    dut.expect_exact("REPL_READY", timeout=30)
    LOGGER.info("REPL is ready.")

    sleep(0.5)

    # Send "help" and verify that registered commands appear in the output
    LOGGER.info("Sending 'help' command...")
    dut.write("help\n")
    dut.expect(r"ping", timeout=10)
    LOGGER.info("REPL responded to 'help' — 'ping' command found in output.")

    sleep(0.5)

    # Send "ping" and verify "pong" response
    LOGGER.info("Sending 'ping' command...")
    dut.write("ping\n")
    dut.expect_exact("pong", timeout=10)
    LOGGER.info("REPL responded to 'ping' with 'pong'.")

    sleep(0.5)

    # Trigger Unity tests by sending "run_tests"
    LOGGER.info("Sending 'run_tests' to start Unity tests...")
    dut.write("run_tests\n")

    # Phase 2: Unity tests
    LOGGER.info("Waiting for Unity test output...")
    dut.expect_unity_test_output(timeout=120)
    LOGGER.info("All tests passed.")
