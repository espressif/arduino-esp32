def test_hello_world(dut):
    dut.expect_exact("Hello Arduino!")
