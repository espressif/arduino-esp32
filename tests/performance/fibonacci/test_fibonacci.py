import json
import logging
import os


def test_fibonacci(dut, request):
    LOGGER = logging.getLogger(__name__)

    # Fibonacci results starting from fib(35) to fib(45)
    fib_results = [
        9227465,
        14930352,
        24157817,
        39088169,
        63245986,
        102334155,
        165580141,
        267914296,
        433494437,
        701408733,
    ]

    # Match "Runs: %d"
    res = dut.expect(r"Runs: (\d+)", timeout=60)
    runs = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Number of runs: {}".format(runs))
    assert runs > 0, "Invalid number of runs"

    # Match "N: %d"
    res = dut.expect(r"N: (\d+)", timeout=300)
    fib_n = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Calculating Fibonacci({})".format(fib_n))
    assert fib_n > 30 and fib_n < 50, "Invalid Fibonacci number"

    list_time = []

    for i in range(runs):
        # Match "Run %d"
        res = dut.expect(r"Run (\d+)", timeout=120)
        run = int(res.group(0).decode("utf-8").split(" ")[1])
        LOGGER.info("Run {}".format(run))
        assert run == i, "Invalid run number"

        # Match "Fibonacci(N): %llu"
        res = dut.expect(r"Fibonacci\(N\): (\d+)", timeout=300)
        fib_result = int(res.group(0).decode("utf-8").split(" ")[1])
        LOGGER.info("Fibonacci({}) = {}".format(fib_n, fib_result))
        assert fib_result > 0, "Invalid Fibonacci result"

        # Check if the result is correct
        assert fib_result == fib_results[fib_n - 35]

        # Match "Time: %lu.%03lu s"
        res = dut.expect(r"Time: (\d+)\.(\d+) s", timeout=300)
        time = float(res.group(0).decode("utf-8").split(" ")[1])
        LOGGER.info("Time on run {}: {} s".format(i, time))
        assert time > 0 and time < 1000, "Invalid time"
        list_time.append(time)

    avg_time = round(sum(list_time) / len(list_time), 3)

    # Create JSON with results and write it to file
    # Always create a JSON with this format (so it can be merged later on):
    # { TEST_NAME_STR: TEST_RESULTS_DICT }
    results = {"fibonacci": {"runs": runs, "fib_n": fib_n, "avg_time": avg_time}}

    current_folder = os.path.dirname(request.path)
    file_index = 0
    report_file = os.path.join(current_folder, "result_fibonacci" + str(file_index) + ".json")
    while os.path.exists(report_file):
        report_file = report_file.replace(str(file_index) + ".json", str(file_index + 1) + ".json")
        file_index += 1

    with open(report_file, "w") as f:
        try:
            f.write(json.dumps(results))
        except Exception as e:
            LOGGER.warning("Failed to write results to file: {}".format(e))
