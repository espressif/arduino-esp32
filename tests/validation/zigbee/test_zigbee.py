import logging
from conftest import expect_exact_and_capture


def test_zigbee(dut):
    LOGGER = logging.getLogger(__name__)

    coordinator = dut[0]
    end_device = dut[1]

    # Coordinator boots and runs Unity while the end device may still be flashing.
    LOGGER.info("Waiting for coordinator to be ready...")
    coordinator.expect_exact("[COORDINATOR] Ready", timeout=120)

    LOGGER.info("Waiting for coordinator network and deep endpoint tests...")
    coordinator_log = expect_exact_and_capture(coordinator, "[COORDINATOR] NETWORK_READY", timeout=180)
    coordinator_log = expect_exact_and_capture(
        coordinator, "[COORDINATOR] API_TESTS_DONE", timeout=300, captured=coordinator_log
    )
    coordinator.expect_unity_test_output(timeout=60, extra_before=coordinator_log)
    coordinator.expect_exact("[COORDINATOR] INTEROP_READY", timeout=60)
    coordinator.expect_exact("[COORDINATOR] DONE", timeout=30)

    LOGGER.info("Waiting for end device to be ready...")
    end_device.expect_exact("[ENDDEVICE] Ready", timeout=180)

    LOGGER.info("Starting end device Unity tests...")
    end_device.write("START\n")

    LOGGER.info("Waiting for end device to join network...")
    end_device_log = expect_exact_and_capture(end_device, "[ENDDEVICE] JOINED", timeout=60)

    LOGGER.info("Coordinator switch -> end device light interop...")
    coordinator.write("RUN_INTEROP\n")
    end_device.write("RUN_INTEROP\n")
    coordinator.expect_exact("[COORDINATOR] INTEROP_ARMED", timeout=30)

    for marker, timeout in (
        ("[ENDDEVICE] REMOTE_LIGHT_ON", 90),
        ("[ENDDEVICE] REMOTE_LIGHT_OFF", 90),
        ("[ENDDEVICE] SWITCH_INTEROP_DONE", 30),
    ):
        end_device_log = expect_exact_and_capture(end_device, marker, timeout=timeout, captured=end_device_log)
    coordinator.expect_exact("[COORDINATOR] SWITCH_INTEROP_DONE", timeout=90)

    LOGGER.info("Waiting for remaining end device Unity tests...")
    end_device.expect_unity_test_output(timeout=120, extra_before=end_device_log)
    end_device.expect_exact("[ENDDEVICE] DONE", timeout=30)

    LOGGER.info("Zigbee multi-DUT test passed!")
