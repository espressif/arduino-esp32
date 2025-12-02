from pytest_embedded_wokwi import Wokwi
from pytest_embedded import Dut


def test_fs(dut: Dut, wokwi: Wokwi):
    dut.expect_unity_test_output(timeout=300)
