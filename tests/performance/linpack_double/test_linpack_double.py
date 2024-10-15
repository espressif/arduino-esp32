import json
import logging
import os


def test_linpack_double(dut, request):
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
    assert data_type == "double", "Invalid data type"

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

    # Create JSON with results and write it to file
    # Always create a JSON with this format (so it can be merged later on):
    # { TEST_NAME_STR: TEST_RESULTS_DICT }
    results = {"linpack_double": {"runs": runs, "avg_score": avg_score, "min_score": min_score, "max_score": max_score}}

    current_folder = os.path.dirname(request.path)
    file_index = 0
    report_file = os.path.join(current_folder, "result_linpack_double" + str(file_index) + ".json")
    while os.path.exists(report_file):
        report_file = report_file.replace(str(file_index) + ".json", str(file_index + 1) + ".json")
        file_index += 1

    with open(report_file, "w") as f:
        try:
            f.write(json.dumps(results))
        except Exception as e:
            LOGGER.warning("Failed to write results to file: {}".format(e))
