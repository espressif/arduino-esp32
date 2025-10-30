import logging
from pytest_embedded_wokwi import Wokwi
from pytest_embedded import Dut


def test_gpio(dut: Dut, wokwi: Wokwi):
    LOGGER = logging.getLogger(__name__)

    LOGGER.info("Waiting for Button test begin...")
    dut.expect_exact("Button test")

    for i in range(3):
        LOGGER.info(f"Setting button pressed for {i + 1} seconds")
        wokwi.client.set_control("btn1", "pressed", 1)

        dut.expect_exact(f"Button pressed {i + 1} times")
        wokwi.client.set_control("btn1", "pressed", 0)
