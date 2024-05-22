import json
import logging
import os


def test_coremark(dut, request):
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

    # Create JSON with results and write it to file
    # Always create a JSON with this format (so it can be merged later on):
    # { TEST_NAME_STR: TEST_RESULTS_DICT }
    results = {"coremark": {"runs": runs, "cores": cores, "avg_score": avg_score}}

    current_folder = os.path.dirname(request.path)
    file_index = 0
    report_file = os.path.join(current_folder, "result_coremark" + str(file_index) + ".json")
    while os.path.exists(report_file):
        report_file = report_file.replace(str(file_index) + ".json", str(file_index + 1) + ".json")
        file_index += 1

    with open(report_file, "w") as f:
        try:
            f.write(json.dumps(results))
        except Exception as e:
            LOGGER.warning("Failed to write results to file: {}".format(e))
