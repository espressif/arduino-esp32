import logging
from pytest_embedded_wokwi import Wokwi
from pytest_embedded import Dut
from time import sleep


def test_gpio(dut: Dut, wokwi: Wokwi):
    LOGGER = logging.getLogger(__name__)

    def test_read_basic():
        dut.expect_exact("BTN read as HIGH after pinMode INPUT_PULLUP")
        wokwi.client.set_control("btn1", "pressed", 1)
        wokwi.client.serial_write("OK\n")  # Sync ack R1
        dut.expect_exact("BTN read as LOW")

        wokwi.client.set_control("btn1", "pressed", 0)
        wokwi.client.serial_write("OK\n")  # Sync ack R2
        dut.expect_exact("BTN read as HIGH")
        LOGGER.info("GPIO read basic test passed.")

    def test_write_basic():
        dut.expect_exact("GPIO LED set to OUTPUT")
        assert wokwi.client.read_pin("led1", "A")["value"] == 0  # Anode pin
        wokwi.client.serial_write("OK\n")  # Sync ack W1

        dut.expect_exact("LED set to HIGH")
        assert wokwi.client.read_pin("led1", "A")["value"] == 1
        wokwi.client.serial_write("OK\n")  # Sync ack W2

        dut.expect_exact("LED set to LOW")
        assert wokwi.client.read_pin("led1", "A")["value"] == 0
        LOGGER.info("GPIO write basic test passed.")

    def test_interrupt_attach_detach():
        dut.expect_exact("Interrupt attached - FALLING edge")

        for i in range(1, 4):
            wokwi.client.set_control("btn1", "pressed", 1)
            wokwi.client.serial_write(f"OK:{i}\n")
            dut.expect_exact(f"{i} interrupt triggered successfully")
            wokwi.client.set_control("btn1", "pressed", 0)

        dut.expect_exact("Interrupt detached")
        wokwi.client.set_control("btn1", "pressed", 1)
        sleep(0.1)
        dut.expect_exact("No interrupt triggered after detach")
        wokwi.client.set_control("btn1", "pressed", 0)
        wokwi.client.serial_write("OK\n")
        LOGGER.info("GPIO interrupt attach/detach test passed.")

    def test_interrupt_falling():
        dut.expect_exact("Testing FALLING edge interrupt")
        for i in range(1, 4):
            wokwi.client.set_control("btn1", "pressed", 1)
            wokwi.client.serial_write(f"OK:{i}\n")
            dut.expect_exact(f"{i} FALLING edge interrupt worked")
            wokwi.client.set_control("btn1", "pressed", 0)

        dut.expect_exact("Testing FALLING edge END")
        wokwi.client.serial_write("OK\n")
        LOGGER.info("GPIO interrupt falling test passed.")

    def test_interrupt_rising():
        dut.expect_exact("Testing RISING edge interrupt")

        for i in range(1, 4):
            wokwi.client.set_control("btn1", "pressed", 1)
            wokwi.client.set_control("btn1", "pressed", 0)
            wokwi.client.serial_write(f"OK:{i}\n")
            dut.expect_exact(f"{i} RISING edge interrupt worked")

        dut.expect_exact("Testing RISING edge END")
        wokwi.client.serial_write("OK\n")
        LOGGER.info("GPIO interrupt rising test passed.")

    def test_interrupt_change():
        dut.expect_exact("Testing CHANGE edge interrupt")

        for i in range(1, 4):
            wokwi.client.set_control("btn1", "pressed", 1)
            wokwi.client.serial_write(f"OK:{i * 2 - 1}\n")
            dut.expect_exact(f"{i * 2 - 1} CHANGE edge interrupt worked")

            wokwi.client.set_control("btn1", "pressed", 0)
            wokwi.client.serial_write(f"OK:{i * 2}\n")
            dut.expect_exact(f"{i * 2} CHANGE edge interrupt worked")

        dut.expect_exact("Testing CHANGE edge END")
        wokwi.client.serial_write("OK\n")
        LOGGER.info("GPIO interrupt change test passed.")

    def test_interrupt_with_arg():
        dut.expect_exact("Testing interrupt with argument")

        for i in range(1, 4):
            wokwi.client.set_control("btn1", "pressed", 1)
            wokwi.client.serial_write(f"OK:{i}\n")
            dut.expect_exact(f"{i} interrupt with argument worked, received arg: {42 + i - 1}")
            wokwi.client.set_control("btn1", "pressed", 0)
        dut.expect_exact("Testing interrupt with argument END")
        wokwi.client.serial_write("OK\n")
        LOGGER.info("GPIO interrupt with argument test passed.")

    LOGGER.info("Waiting for GPIO test begin...")
    dut.expect_exact("GPIO test START")
    test_read_basic()
    test_write_basic()
    dut.expect_exact("GPIO interrupt START")
    test_interrupt_attach_detach()
    test_interrupt_falling()
    test_interrupt_rising()
    test_interrupt_change()
    test_interrupt_with_arg()
    LOGGER.info("GPIO test END")
