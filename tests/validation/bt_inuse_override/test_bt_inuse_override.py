def test_bt_inuse_override(dut):
    dut.expect_exact("[BT_INUSE_OVERRIDE] Ready", timeout=30)
    dut.expect_exact("[BT_INUSE_OVERRIDE] PASSED", timeout=30)
