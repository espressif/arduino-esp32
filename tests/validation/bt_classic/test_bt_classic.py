# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: CC0-1.0

import logging

from conftest import rand_str4


def test_bt_classic(dut, ci_job_id):
    logger = logging.getLogger(__name__)

    server = dut[0]
    client = dut[1]

    slave_name = "BT_SRV_" + ci_job_id if ci_job_id else "BT_SRV_" + rand_str4()
    logger.info("Slave name: %s", slave_name)

    logger.info("Waiting for devices to be ready...")
    server.expect_exact("[SERVER] Ready for name", timeout=120)
    client.expect_exact("[CLIENT] Ready for slave name", timeout=120)

    server.expect_exact("[SERVER] Send name:")
    client.expect_exact("[CLIENT] Send slave name:")

    logger.info("Sending slave name to both devices")
    server.write(f"{slave_name}\n")
    client.write(f"{slave_name}\n")

    server.expect_exact(f"[SERVER] Name: {slave_name}")
    client.expect_exact(f"[CLIENT] Slave name: {slave_name}")

    logger.info("Waiting for SPP server...")
    server.expect_exact("[SERVER] SPP listening", timeout=30)

    logger.info("Waiting for client connection...")
    client.expect_exact(f"[CLIENT] Connecting to {slave_name}", timeout=30)
    server.expect_exact("[SERVER] Client connected", timeout=90)
    client.expect_exact("[CLIENT] Connected", timeout=90)

    logger.info("Verifying ping/pong over SPP...")
    server.expect_exact("[SERVER] Received: BT_CLASSIC_PING", timeout=30)
    server.expect_exact("[SERVER] Sent response", timeout=10)
    client.expect_exact("[CLIENT] Received: BT_CLASSIC_PONG", timeout=30)

    server.expect_exact("[SERVER] Test complete", timeout=10)
    client.expect_exact("[CLIENT] Test complete", timeout=10)

    logger.info("BT Classic test passed")
