import json
import logging
import os

# Metrics reported by the sketch, in output order.
METRICS = (
    ("READ", "read_per_byte_us", "us"),
    ("READB", "read_bytes_one_shot_us", "us"),
    ("READBC", "read_bytes_chunked_us", "us"),
    ("READS", "read_string_us", "us"),
    ("READBU", "read_bytes_until_us", "us"),
)


def test_stream_string(dut, request):
    LOGGER = logging.getLogger(__name__)

    # Match "Runs: %d"
    res = dut.expect(r"Runs: (\d+)", timeout=60)
    runs = int(res.group(0).decode("utf-8").split(" ")[1])
    LOGGER.info("Number of runs: {}".format(runs))
    assert runs > 0, "Invalid number of runs"

    # Match "Read size: %d"
    res = dut.expect(r"Read size: (\d+)", timeout=60)
    read_size = int(res.group(0).decode("utf-8").split(" ")[2])
    LOGGER.info("Read size: {}".format(read_size))
    assert read_size > 0, "Invalid read size"

    # Match "Buffer size: %d"
    res = dut.expect(r"Buffer size: (\d+)", timeout=60)
    buffer_size = int(res.group(0).decode("utf-8").split(" ")[2])
    LOGGER.info("Buffer size: {}".format(buffer_size))
    assert buffer_size > 0, "Invalid buffer size"

    # Match "Chunk size: %d"
    res = dut.expect(r"Chunk size: (\d+)", timeout=60)
    chunk_size = int(res.group(0).decode("utf-8").split(" ")[2])
    LOGGER.info("Chunk size: {}".format(chunk_size))
    assert chunk_size > 0, "Invalid chunk size"

    # Match "String size: %d"
    res = dut.expect(r"String size: (\d+)", timeout=60)
    string_size = int(res.group(0).decode("utf-8").split(" ")[2])
    LOGGER.info("String size: {}".format(string_size))
    assert string_size > 0, "Invalid string size"

    samples = {name: [] for _, name, _ in METRICS}

    for i in range(runs):
        # Match "Run %d"
        res = dut.expect(r"Run (\d+)", timeout=120)
        run = int(res.group(0).decode("utf-8").split(" ")[1])
        LOGGER.info("Run {}".format(run))
        assert run == i, "Invalid run number"

        for tag, name, _ in METRICS:
            # Match e.g. "READ n=16384 ok=1 us=1234 avail=0"
            res = dut.expect(r"{} n=(\d+) ok=(\d+) us=(\d+) avail=(-?\d+)".format(tag), timeout=300)
            fields = res.group(0).decode("utf-8").split(" ")
            LOGGER.info("{}: {}".format(tag, " ".join(fields)))

            ok = int(fields[2].split("=")[1])
            elapsed = int(fields[3].split("=")[1])
            available = int(fields[4].split("=")[1])

            assert ok == 1, "{} reported a data mismatch".format(tag)
            assert elapsed > 0, "Invalid elapsed time for {}".format(tag)
            assert available == 0, "{} left {} bytes available".format(tag, available)
            samples[name].append(elapsed)

    metrics = []
    for _, name, unit in METRICS:
        values = samples[name]
        assert len(values) == runs, "Missing samples for {}".format(name)
        metrics.append(
            {
                "name": name,
                "value": round(sum(values) / len(values), 3),
                "unit": unit,
            }
        )

    # Canonical performance result format (see .github/CI_README.md)
    results = {
        "test_name": "stream_string",
        "runs": runs,
        "settings": "read_size={},buffer_size={},chunk_size={},string_size={}".format(
            read_size, buffer_size, chunk_size, string_size
        ),
        "metrics": metrics,
    }

    LOGGER.info("StreamString results: {}".format(json.dumps(results)))

    current_folder = os.path.dirname(request.path)
    os.makedirs(os.path.join(current_folder, dut.app.target), exist_ok=True)
    file_index = 0
    report_file = os.path.join(current_folder, dut.app.target, "result_stream_string" + str(file_index) + ".json")
    while os.path.exists(report_file):
        report_file = report_file.replace(str(file_index) + ".json", str(file_index + 1) + ".json")
        file_index += 1

    with open(report_file, "w+") as f:
        try:
            f.write(json.dumps(results))
        except Exception as e:
            LOGGER.warning("Failed to write results to file: {}".format(e))
