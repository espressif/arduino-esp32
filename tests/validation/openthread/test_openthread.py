import logging
import re


def test_openthread(dut):
    LOGGER = logging.getLogger(__name__)

    leader = dut[0]
    child = dut[1]

    LOGGER.info("Waiting for both devices to be ready...")
    leader.expect_exact("[LEADER] Ready", timeout=120)
    child.expect_exact("[CHILD] Ready", timeout=120)

    LOGGER.info("Starting leader Unity tests...")
    leader.write("GO\n")

    LOGGER.info("Waiting for leader to export dataset...")
    m_export = leader.expect(
        r"\[LEADER\] (DATASET=([0-9a-fA-F]+)|EXPORT_FAILED)",
        timeout=180,
    )
    export_tag = m_export.group(1)
    if isinstance(export_tag, bytes):
        export_tag = export_tag.decode()
    if export_tag == "EXPORT_FAILED":
        raise AssertionError("Leader Unity tests failed; no dataset exported")
    dataset_hex = m_export.group(2)
    if isinstance(dataset_hex, bytes):
        dataset_hex = dataset_hex.decode()
    dataset_hex = dataset_hex.strip()

    LOGGER.info("Waiting for leader network info...")
    m_net = leader.expect(
        r"\[LEADER\] NETINFO name=([^ ]+) channel=(\d+) panid=(0x[0-9a-fA-F]+) extpanid=([0-9a-fA-F]{16})",
        timeout=30,
    )
    leader.expect_exact("[LEADER] NETWORK_READY", timeout=30)

    net_name = m_net.group(1)
    net_channel = m_net.group(2)
    net_panid = m_net.group(3)
    net_extpanid = m_net.group(4)
    if isinstance(net_name, bytes):
        net_name = net_name.decode()
    if isinstance(net_channel, bytes):
        net_channel = net_channel.decode()
    if isinstance(net_panid, bytes):
        net_panid = net_panid.decode()
    if isinstance(net_extpanid, bytes):
        net_extpanid = net_extpanid.decode()

    assert len(dataset_hex) >= 100, f"Dataset hex too short: {len(dataset_hex)}"
    assert len(dataset_hex) % 2 == 0, f"Invalid dataset hex length: {len(dataset_hex)}"
    assert re.fullmatch(r"[0-9a-fA-F]{16}", net_extpanid), f"Invalid extpanid: {net_extpanid}"

    LOGGER.info(f"Leader dataset: {dataset_hex[:32]}... ({len(dataset_hex)} chars)")
    LOGGER.info(f"Leader netinfo: name={net_name} channel={net_channel} " f"panid={net_panid} extpanid={net_extpanid}")

    LOGGER.info("Sending dataset and network info to child...")
    child.write(f"DATASET {dataset_hex}\n")
    child.write(f"NETINFO name={net_name} channel={net_channel} " f"panid={net_panid} extpanid={net_extpanid}\n")

    LOGGER.info("Waiting for child to join network...")
    child.expect_exact("[CHILD] JOINED", timeout=60)

    LOGGER.info("Waiting for child completion...")
    child.expect_exact("[CHILD] DONE", timeout=120)

    LOGGER.info("OpenThread multi-DUT test passed!")
