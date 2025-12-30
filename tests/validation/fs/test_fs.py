from pytest_embedded import Dut


def test_fs(dut: Dut):
    dut.expect_unity_test_output(timeout=300)
