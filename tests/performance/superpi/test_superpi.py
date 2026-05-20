import logging


def test_superpi(dut, save_perf_result):
    LOGGER = logging.getLogger(__name__)

    # Match "Runs: %d"
    res = dut.expect(r"Runs: (\d+)", timeout=60)
    runs = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Number of runs: {}".format(runs))

    # Match "Digits: %d"
    res = dut.expect(r"Digits: (\d+)", timeout=60)
    digits = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Number of decimal digits: {}".format(digits))

    list_time = []

    for i in range(runs):
        # Match "Run %d"
        res = dut.expect(r"Run (\d+)", timeout=120)
        run = int(res.group(0).decode("utf-8").split(" ")[1])
        LOGGER.info("Run {}".format(run))
        assert run == i, "Invalid run number"

        # Match "Time: %lu.%03lu s"
        res = dut.expect(r"Time: (\d+)\.(\d+) s", timeout=300)
        time = float(res.group(0).decode("utf-8").split(" ")[1])
        LOGGER.info("Time on run {}: {} s".format(i, time))
        assert time > 0 and time < 1000, "Invalid time"
        list_time.append(time)

    avg_time = round(sum(list_time) / len(list_time), 3)

    # Canonical performance result format (see .github/CI_README.md)
    results = {
        "test_name": "superpi",
        "runs": runs,
        "settings": "digits={}".format(digits),
        "metrics": [{"name": "avg_time", "value": avg_time, "unit": "s"}],
    }

    save_perf_result(results)
