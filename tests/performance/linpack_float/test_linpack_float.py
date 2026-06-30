import logging


def test_linpack_float(dut, save_perf_result):
    LOGGER = logging.getLogger(__name__)

    # Match "Runs: %d"
    res = dut.expect(r"Runs: (\d+)", timeout=60)
    runs = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Number of runs: {}".format(runs))
    assert runs > 0, "Invalid number of runs"

    # Match "Type: %s"
    res = dut.expect(r"Type: (\w+)", timeout=60)
    data_type = res.group(0).decode("utf-8").split(" ")[1]
    LOGGER.info("Data type: {}".format(data_type))
    assert data_type == "float", "Invalid data type"

    # Match "Runs completed: %d"
    res = dut.expect(r"Runs completed: (\d+)", timeout=120)
    runs_completed = int(res.group(0).decode("utf-8").split(" ")[2])
    LOGGER.info("Runs completed: {}".format(runs_completed))
    assert runs_completed == runs, "Invalid number of runs completed"

    # Match "Average MFLOPS: %f"
    res = dut.expect(r"Average MFLOPS: (\d+\.\d+)", timeout=120)
    avg_score = float(res.group(0).decode("utf-8").split(" ")[2])
    LOGGER.info("Average MFLOPS: {}".format(avg_score))
    assert avg_score > 0, "Invalid average MFLOPS"

    # Match "Min MFLOPS: %f"
    res = dut.expect(r"Min MFLOPS: (\d+\.\d+)", timeout=120)
    min_score = float(res.group(0).decode("utf-8").split(" ")[2])
    LOGGER.info("Min MFLOPS: {}".format(min_score))
    assert min_score > 0 and min_score < 1000000000.0, "Invalid min MFLOPS"

    # Match "Max MFLOPS: %f"
    res = dut.expect(r"Max MFLOPS: (\d+\.\d+)", timeout=120)
    max_score = float(res.group(0).decode("utf-8").split(" ")[2])
    LOGGER.info("Max MFLOPS: {}".format(max_score))
    assert max_score > 0, "Invalid max MFLOPS"

    # Match "Median MFLOPS: %f"
    res = dut.expect(r"Median MFLOPS: (\d+\.\d+)", timeout=120)
    median_score = float(res.group(0).decode("utf-8").split(" ")[2])
    LOGGER.info("Median MFLOPS: {}".format(median_score))
    assert median_score > 0, "Invalid median MFLOPS"

    # Canonical performance result format (see .github/CI_README.md)
    results = {
        "test_name": "linpack_float",
        "runs": runs,
        "settings": "data_type=float",
        "metrics": [
            {"name": "avg_score", "value": avg_score, "unit": "MFLOPS"},
            {"name": "min_score", "value": min_score, "unit": "MFLOPS"},
            {"name": "max_score", "value": max_score, "unit": "MFLOPS"},
            {"name": "median_score", "value": median_score, "unit": "MFLOPS"},
        ],
    }

    save_perf_result(results)
