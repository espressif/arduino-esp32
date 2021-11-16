# -*- coding: utf-8 -*-

from mock import patch, MagicMock
import os
from io import StringIO
import shutil
import sys
import testtools

from doc8.main import main, doc8


# Location to create test files
TMPFS_DIR_NAME = ".tmp"

# Expected output
OUTPUT_CMD_NO_QUIET = """\
Scanning...
Validating...
{path}/invalid.rst:1: D002 Trailing whitespace
{path}/invalid.rst:1: D005 No newline at end of file
========
Total files scanned = 1
Total files ignored = 0
Total accumulated errors = 2
Detailed error counts:
    - doc8.checks.CheckCarriageReturn = 0
    - doc8.checks.CheckIndentationNoTab = 0
    - doc8.checks.CheckMaxLineLength = 0
    - doc8.checks.CheckNewlineEndOfFile = 1
    - doc8.checks.CheckTrailingWhitespace = 1
    - doc8.checks.CheckValidity = 0
"""

OUTPUT_CMD_QUIET = """\
{path}/invalid.rst:1: D002 Trailing whitespace
{path}/invalid.rst:1: D005 No newline at end of file
"""

OUTPUT_CMD_VERBOSE = """\
Scanning...
  Selecting '{path}/invalid.rst'
Validating...
Validating {path}/invalid.rst (utf-8, 10 chars, 1 lines)
  Running check 'doc8.checks.CheckValidity'
  Running check 'doc8.checks.CheckTrailingWhitespace'
    - {path}/invalid.rst:1: D002 Trailing whitespace
  Running check 'doc8.checks.CheckIndentationNoTab'
  Running check 'doc8.checks.CheckCarriageReturn'
  Running check 'doc8.checks.CheckMaxLineLength'
  Running check 'doc8.checks.CheckNewlineEndOfFile'
    - {path}/invalid.rst:1: D005 No newline at end of file
========
Total files scanned = 1
Total files ignored = 0
Total accumulated errors = 2
Detailed error counts:
    - doc8.checks.CheckCarriageReturn = 0
    - doc8.checks.CheckIndentationNoTab = 0
    - doc8.checks.CheckMaxLineLength = 0
    - doc8.checks.CheckNewlineEndOfFile = 1
    - doc8.checks.CheckTrailingWhitespace = 1
    - doc8.checks.CheckValidity = 0
"""

OUTPUT_API_REPORT = """\
{path}/invalid.rst:1: D002 Trailing whitespace
{path}/invalid.rst:1: D005 No newline at end of file
========
Total files scanned = 1
Total files ignored = 0
Total accumulated errors = 2
Detailed error counts:
    - doc8.checks.CheckCarriageReturn = 0
    - doc8.checks.CheckIndentationNoTab = 0
    - doc8.checks.CheckMaxLineLength = 0
    - doc8.checks.CheckNewlineEndOfFile = 1
    - doc8.checks.CheckTrailingWhitespace = 1
    - doc8.checks.CheckValidity = 0"""


class Capture(object):
    """
    Context manager to capture output on stdout and stderr
    """

    def __enter__(self):
        self.old_out = sys.stdout
        self.old_err = sys.stderr
        self.out = StringIO()
        self.err = StringIO()

        sys.stdout = self.out
        sys.stderr = self.err

        return self.out, self.err

    def __exit__(self, *args):
        sys.stdout = self.out
        sys.stderr = self.err


class TmpFs(object):
    """
    Context manager to create and clean a temporary file area for testing
    """

    def __enter__(self):
        self.path = os.path.join(os.getcwd(), TMPFS_DIR_NAME)
        if os.path.exists(self.path):
            raise ValueError(
                "Tmp dir found at %s - remove before running tests" % self.path
            )
        os.mkdir(self.path)
        return self

    def __exit__(self, *args):
        shutil.rmtree(self.path)

    def create_file(self, filename, content):
        with open(os.path.join(self.path, filename), "w") as file:
            file.write(content)

    def mock(self):
        """
        Create a file which fails on a LineCheck and a ContentCheck
        """
        self.create_file("invalid.rst", "D002 D005 ")

    def expected(self, template):
        """
        Insert the path into a template to generate an expected test value
        """
        return template.format(path=self.path)


class FakeResult(object):
    """
    Minimum valid result returned from doc8
    """

    total_errors = 0

    def report(self):
        return ""


class TestCommandLine(testtools.TestCase):
    """
    Test command line invocation
    """

    def test_main__no_quiet_no_verbose__output_is_not_quiet(self):
        with TmpFs() as tmpfs, Capture() as (out, err), patch(
            "argparse._sys.argv", ["doc8", tmpfs.path]
        ):
            tmpfs.mock()
            state = main()
            self.assertEqual(out.getvalue(), tmpfs.expected(OUTPUT_CMD_NO_QUIET))
            self.assertEqual(err.getvalue(), "")
        self.assertEqual(state, 1)

    def test_main__quiet_no_verbose__output_is_quiet(self):
        with TmpFs() as tmpfs, Capture() as (out, err), patch(
            "argparse._sys.argv", ["doc8", "--quiet", tmpfs.path]
        ):
            tmpfs.mock()
            state = main()
            self.assertEqual(out.getvalue(), tmpfs.expected(OUTPUT_CMD_QUIET))
            self.assertEqual(err.getvalue(), "")
        self.assertEqual(state, 1)

    def test_main__no_quiet_verbose__output_is_verbose(self):
        with TmpFs() as tmpfs, Capture() as (out, err), patch(
            "argparse._sys.argv", ["doc8", "--verbose", tmpfs.path]
        ):
            tmpfs.mock()
            state = main()
            self.assertEqual(out.getvalue(), tmpfs.expected(OUTPUT_CMD_VERBOSE))
            self.assertEqual(err.getvalue(), "")
        self.assertEqual(state, 1)


class TestApi(testtools.TestCase):
    """
    Test direct code invocation
    """

    def test_doc8__defaults__no_output_and_report_as_expected(self):
        with TmpFs() as tmpfs, Capture() as (out, err):
            tmpfs.mock()
            result = doc8(paths=[tmpfs.path])

            self.assertEqual(result.report(), tmpfs.expected(OUTPUT_API_REPORT))
            self.assertEqual(
                result.errors,
                [
                    (
                        "doc8.checks.CheckTrailingWhitespace",
                        "{}/invalid.rst".format(tmpfs.path),
                        1,
                        "D002",
                        "Trailing whitespace",
                    ),
                    (
                        "doc8.checks.CheckNewlineEndOfFile",
                        "{}/invalid.rst".format(tmpfs.path),
                        1,
                        "D005",
                        "No newline at end of file",
                    ),
                ],
            )
            self.assertEqual(out.getvalue(), "")
            self.assertEqual(err.getvalue(), "")
        self.assertEqual(result.total_errors, 2)

    def test_doc8__verbose__verbose_overridden(self):
        with TmpFs() as tmpfs, Capture() as (out, err):
            tmpfs.mock()
            result = doc8(paths=[tmpfs.path], verbose=True)

            self.assertEqual(result.report(), tmpfs.expected(OUTPUT_API_REPORT))
            self.assertEqual(out.getvalue(), "")
            self.assertEqual(err.getvalue(), "")
        self.assertEqual(result.total_errors, 2)


class TestArguments(testtools.TestCase):
    """
    Test that arguments are parsed correctly
    """

    def get_args(self, **kwargs):
        # Defaults
        args = {
            "paths": [os.getcwd()],
            "config": [],
            "allow_long_titles": False,
            "ignore": set(),
            "sphinx": True,
            "ignore_path": [],
            "ignore_path_errors": {},
            "default_extension": "",
            "file_encoding": "",
            "max_line_length": 79,
            "extension": list([".rst", ".txt"]),
            "quiet": False,
            "verbose": False,
            "version": False,
        }
        args.update(kwargs)
        return args

    def test_get_args__override__value_overridden(self):
        # Just checking a dict is a dict, but confirms nothing silly has happened
        # so we can make assumptions about get_args in other tests
        self.assertEqual(
            self.get_args(paths=["/tmp/does/not/exist"]),
            {
                "paths": ["/tmp/does/not/exist"],
                "config": [],
                "allow_long_titles": False,
                "ignore": set(),
                "sphinx": True,
                "ignore_path": [],
                "ignore_path_errors": {},
                "default_extension": "",
                "file_encoding": "",
                "max_line_length": 79,
                "extension": [".rst", ".txt"],
                "quiet": False,
                "verbose": False,
                "version": False,
            },
        )

    def test_args__no_args__defaults(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch("argparse._sys.argv", ["doc8"]):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args())

    def test_args__paths__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "path1", "path2"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(paths=["path1", "path2"]))

    def test_args__config__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        mock_config = MagicMock(return_value={})
        with patch("doc8.main.scan", mock_scan), patch(
            "doc8.main.extract_config", mock_config
        ), patch(
            "argparse._sys.argv", ["doc8", "--config", "path1", "--config", "path2"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(config=["path1", "path2"]))

    def test_args__allow_long_titles__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "--allow-long-titles"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(allow_long_titles=True))

    def test_args__ignore__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv",
            ["doc8", "--ignore", "D002", "--ignore", "D002", "--ignore", "D005"],
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(ignore={"D002", "D005"}))

    def test_args__sphinx__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "--no-sphinx"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(sphinx=False))

    def test_args__ignore_path__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv",
            ["doc8", "--ignore-path", "path1", "--ignore-path", "path2"],
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(
                self.get_args(ignore_path=["path1", "path2"])
            )

    def test_args__ignore_path_errors__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv",
            [
                "doc8",
                "--ignore-path-errors",
                "path1;D002",
                "--ignore-path-errors",
                "path2;D005",
            ],
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(
                self.get_args(ignore_path_errors={"path1": {"D002"}, "path2": {"D005"}})
            )

    def test_args__default_extension__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "--default-extension", "rst"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(default_extension="rst"))

    def test_args__file_encoding__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "--file-encoding", "utf8"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(file_encoding="utf8"))

    def test_args__max_line_length__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "--max-line-length", "88"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(max_line_length=88))

    def test_args__extension__overrides_default(self):
        # ": [".rst", ".txt"],
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "--extension", "ext1", "--extension", "ext2"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(
                self.get_args(extension=[".rst", ".txt", "ext1", "ext2"])
            )

    def test_args__quiet__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "--quiet"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(quiet=True))

    def test_args__verbose__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "--verbose"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_called_once_with(self.get_args(verbose=True))

    def test_args__version__overrides_default(self):
        mock_scan = MagicMock(return_value=([], 0))
        with patch("doc8.main.scan", mock_scan), patch(
            "argparse._sys.argv", ["doc8", "--version"]
        ):
            state = main()
            self.assertEqual(state, 0)
            mock_scan.assert_not_called()
