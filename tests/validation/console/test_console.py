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

    # Switch to the REPL task and verify it reads and dispatches commands.
    # cmd_test_repl prints "REPL_TASK_STARTED" before spawning the task so we
    # can wait for that marker before sending the first REPL command.
    LOGGER.info("Switching to REPL task...")
    dut.write("test_repl\n")
    dut.expect_exact("REPL_TASK_STARTED", timeout=10)
    LOGGER.info("REPL task started.")

    sleep(0.3)

    LOGGER.info("Sending 'ping' through REPL task...")
    dut.write("ping\n")
    dut.expect_exact("pong", timeout=10)
    LOGGER.info("REPL task dispatched 'ping' and returned 'pong'.")

    sleep(0.3)

    # Trigger Unity tests by sending "run_tests" through the REPL task
    LOGGER.info("Sending 'run_tests' through REPL task to start Unity tests...")
    dut.write("run_tests\n")

    # Phase 2: Unity tests
    LOGGER.info("Waiting for Unity test output...")
    dut.expect_unity_test_output(timeout=120)
    LOGGER.info("All tests passed.")
