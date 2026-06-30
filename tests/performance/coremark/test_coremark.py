import logging


def test_coremark(dut, save_perf_result):
    LOGGER = logging.getLogger(__name__)

    # Match "Runs: %d"
    res = dut.expect(r"Runs: (\d+)", timeout=60)
    runs = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Number of runs: {}".format(runs))
    assert runs > 0, "Invalid number of runs"

    # Match "Cores: %d"
    res = dut.expect(r"Cores: (\d+)", timeout=60)
    cores = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Number of cores: {}".format(cores))
    assert cores > 0, "Invalid number of cores"

    total_score = 0

    for i in range(runs):
        # Match "Run %d"
        res = dut.expect(r"Run (\d+)", timeout=120)
        run = int(res.group(0).decode("utf-8").split(" ")[1])
        LOGGER.info("Run {}".format(run))
        assert run == i, "Invalid run number"

        score = 0
        # Match "CoreMark 1.0 : %d"
        res = dut.expect(r"CoreMark 1.0 : (\d+)\.(\d+)", timeout=120)
        score = float(res.group(0).decode("utf-8").split(" ")[3])
        LOGGER.info("CoreMark score: {}".format(score))
        assert score > 0 and score < 10000, "Impossible CoreMark score"
        total_score += score

    avg_score = round(total_score / runs, 2)
    LOGGER.info("Average CoreMark score: {}".format(avg_score))
    assert avg_score > 0 and avg_score < 10000, "Impossible CoreMark score"

    # Canonical performance result format (see .github/CI_README.md)
    results = {
        "test_name": "coremark",
        "runs": runs,
        "settings": "cores={}".format(cores),
        "metrics": [{"name": "avg_score", "value": avg_score}],
    }

    save_perf_result(results)
