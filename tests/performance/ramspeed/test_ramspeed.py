import json
import logging
import os

from collections import defaultdict


def test_ramspeed(dut, request):
    LOGGER = logging.getLogger(__name__)

    runs_results = []

    # Match "Runs: %d"
    res = dut.expect(r"Runs: (\d+)", timeout=60)
    runs = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Number of runs: {}".format(runs))
    assert runs > 0, "Invalid number of runs"

    # Match "Copies: %d"
    res = dut.expect(r"Copies: (\d+)", timeout=60)
    copies = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Number of copies in each test: {}".format(copies))
    assert copies > 0, "Invalid number of copies"

    # Match "Max test size: %lu"
    res = dut.expect(r"Max test size: (\d+)", timeout=60)
    max_test_size = int(res.group(0).decode("utf-8").split(" ")[3])
    LOGGER.info("Max test size: {}".format(max_test_size))
    assert max_test_size > 0, "Invalid max test size"

    for i in range(runs):
        # Match "Run %d"
        res = dut.expect(r"Run (\d+)", timeout=120)
        run = int(res.group(0).decode("utf-8").split(" ")[1])
        LOGGER.info("Run {}".format(run))
        assert run == i, "Invalid run number"

        for j in range(2):
            while True:
                # Match "Memcpy/Memtest %d Bytes test"
                res = dut.expect(r"(Memcpy|Memset) (\d+) Bytes test", timeout=60)
                current_test = res.group(0).decode("utf-8").split(" ")[0].lower()
                current_test_size = int(res.group(0).decode("utf-8").split(" ")[1])
                LOGGER.info("Current {} test size: {}".format(current_test, current_test_size))
                assert current_test_size > 0, "Invalid test size"

                for k in range(2):
                    # Match "System/Mock memcpy/memtest(): Rate = %d KB/s Time: %d ms" or "Error: %s"
                    res = dut.expect(
                        r"((System|Mock) (memcpy|memset)\(\): Rate = (\d+) KB/s Time: (\d+) ms|^Error)", timeout=90
                    )
                    implementation = res.group(0).decode("utf-8").split(" ")[0].lower()
                    assert implementation != "error:", "Error detected in test output"
                    test_type = res.group(0).decode("utf-8").split(" ")[1].lower()[:-3]
                    rate = int(res.group(0).decode("utf-8").split(" ")[4])
                    time = int(res.group(0).decode("utf-8").split(" ")[7])
                    assert rate > 0, "Invalid rate"
                    assert time > 0, "Invalid time"
                    assert test_type == current_test, "Missing test output"
                    LOGGER.info("{} {}: Rate = {} KB/s. Time = {} ms".format(implementation, test_type, rate, time))

                    runs_results.append(((current_test, str(current_test_size), implementation), (rate, time)))

                if current_test_size == max_test_size:
                    break

            LOGGER.info("=============================================================")

    # Calculate average rate and time for each test size
    sums = defaultdict(lambda: {"rate_sum": 0, "time_sum": 0})

    for (test, size, impl), (rate, time) in runs_results:
        sums[(test, size, impl)]["rate_sum"] += rate
        sums[(test, size, impl)]["time_sum"] += time

    avg_results = {}
    for (test, size, impl) in sums:
        rate_avg = round(sums[(test, size, impl)]["rate_sum"] / runs, 2)
        time_avg = round(sums[(test, size, impl)]["time_sum"] / runs, 2)
        LOGGER.info(
            "Test: {}-{}-{}: Average rate = {} KB/s. Average time = {} ms".format(test, size, impl, rate_avg, time_avg)
        )
        if test not in avg_results:
            avg_results[test] = {}
        if size not in avg_results[test]:
            avg_results[test][size] = {}
        avg_results[test][size][impl] = {"avg_rate": rate_avg, "avg_time": time_avg}

    # Create JSON with results and write it to file
    # Always create a JSON with this format (so it can be merged later on):
    # { TEST_NAME_STR: TEST_RESULTS_DICT }
    results = {"ramspeed": {"runs": runs, "copies": copies, "max_test_size": max_test_size, "results": avg_results}}

    current_folder = os.path.dirname(request.path)
    file_index = 0
    report_file = os.path.join(current_folder, "result_ramspeed" + str(file_index) + ".json")
    while os.path.exists(report_file):
        report_file = report_file.replace(str(file_index) + ".json", str(file_index + 1) + ".json")
        file_index += 1

    with open(report_file, "w") as f:
        try:
            f.write(json.dumps(results))
        except Exception as e:
            LOGGER.warning("Failed to write results to file: {}".format(e))
