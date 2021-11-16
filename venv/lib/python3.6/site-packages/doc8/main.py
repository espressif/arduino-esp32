# Copyright (C) 2014 Ivan Melnikov <iv at altlinux dot org>
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.


"""Check documentation for simple style requirements.

What is checked:
    - invalid rst format - D000
    - lines should not be longer than 79 characters - D001
      - RST exception: line with no whitespace except in the beginning
      - RST exception: lines with http or https urls
      - RST exception: literal blocks
      - RST exception: rst target directives
    - no trailing whitespace - D002
    - no tabulation for indentation - D003
    - no carriage returns (use unix newlines) - D004
    - no newline at end of file - D005
"""

import argparse
import collections
import configparser
import logging
import os
import sys

try:
    import toml

    HAVE_TOML = True
except ImportError:
    HAVE_TOML = False

from stevedore import extension

from doc8 import checks
from doc8 import parser as file_parser
from doc8 import utils
from doc8 import version

FILE_PATTERNS = [".rst", ".txt"]
MAX_LINE_LENGTH = 79
CONFIG_FILENAMES = ["doc8.ini", "tox.ini", "pep8.ini", "setup.cfg"]
if HAVE_TOML:
    CONFIG_FILENAMES.extend(["pyproject.toml"])


def split_set_type(text, delimiter=","):
    return set([i.strip() for i in text.split(delimiter) if i.strip()])


def merge_sets(sets):
    m = set()
    for s in sets:
        m.update(s)
    return m


def parse_ignore_path_errors(entries):
    ignore_path_errors = collections.defaultdict(set)
    for path in entries:
        path, ignored_errors = path.split(";", 1)
        path = path.strip()
        ignored_errors = split_set_type(ignored_errors, delimiter=";")
        ignore_path_errors[path].update(ignored_errors)
    return dict(ignore_path_errors)


def from_ini(fp):
    parser = configparser.RawConfigParser()
    with open(fp, "r") as fh:
        parser.read_file(fh)

    cfg = {}
    try:
        cfg["max_line_length"] = parser.getint("doc8", "max-line-length")
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    try:
        cfg["ignore"] = split_set_type(parser.get("doc8", "ignore"))
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    try:
        cfg["ignore_path"] = split_set_type(parser.get("doc8", "ignore-path"))
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    try:
        ignore_path_errors = parser.get("doc8", "ignore-path-errors")
        ignore_path_errors = split_set_type(ignore_path_errors)
        ignore_path_errors = parse_ignore_path_errors(ignore_path_errors)
        cfg["ignore_path_errors"] = ignore_path_errors
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    try:
        cfg["allow_long_titles"] = parser.getboolean("doc8", "allow-long-titles")
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    try:
        cfg["sphinx"] = parser.getboolean("doc8", "sphinx")
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    try:
        cfg["verbose"] = parser.getboolean("doc8", "verbose")
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    try:
        cfg["file_encoding"] = parser.get("doc8", "file-encoding")
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    try:
        cfg["default_extension"] = parser.get("doc8", "default-extension")
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    try:
        extensions = parser.get("doc8", "extensions")
        extensions = extensions.split(",")
        extensions = [s.strip() for s in extensions if s.strip()]
        if extensions:
            cfg["extension"] = extensions
    except (configparser.NoSectionError, configparser.NoOptionError):
        pass
    return cfg


def from_toml(fp):
    cfg = toml.load(fp).get("tool", {}).get("doc8", {})
    return cfg


def extract_config(args):
    cfg = {}
    for cfg_file in args["config"] or CONFIG_FILENAMES:
        if not os.path.isfile(cfg_file):
            if args["config"]:
                print(
                    "Configuration file %s does not exist...ignoring" % (args["config"])
                )
            continue
        if cfg_file.endswith((".ini", ".cfg")):
            cfg = from_ini(cfg_file)
        elif cfg_file.endswith(".toml") and HAVE_TOML:
            cfg = from_toml(cfg_file)
        if cfg:
            break
    return cfg


def fetch_checks(cfg):
    base = [
        checks.CheckValidity(cfg),
        checks.CheckTrailingWhitespace(cfg),
        checks.CheckIndentationNoTab(cfg),
        checks.CheckCarriageReturn(cfg),
        checks.CheckMaxLineLength(cfg),
        checks.CheckNewlineEndOfFile(cfg),
    ]
    mgr = extension.ExtensionManager(
        namespace="doc8.extension.check", invoke_on_load=True, invoke_args=(cfg.copy(),)
    )
    addons = []
    for e in mgr:
        addons.append(e.obj)
    return base + addons


def setup_logging(verbose):
    if verbose:
        level = logging.DEBUG
    else:
        level = logging.ERROR
    logging.basicConfig(
        level=level, format="%(levelname)s: %(message)s", stream=sys.stdout
    )


def scan(cfg):
    if not cfg.get("quiet"):
        print("Scanning...")
    files = collections.deque()
    ignored_paths = cfg.get("ignore_path", [])
    files_ignored = 0
    file_iter = utils.find_files(
        cfg.get("paths", []), cfg.get("extension", []), ignored_paths
    )
    default_extension = cfg.get("default_extension")
    file_encoding = cfg.get("file_encoding")
    for filename, ignoreable in file_iter:
        if ignoreable:
            files_ignored += 1
            if cfg.get("verbose"):
                print("  Ignoring '%s'" % (filename))
        else:
            f = file_parser.parse(
                filename, default_extension=default_extension, encoding=file_encoding
            )
            files.append(f)
            if cfg.get("verbose"):
                print("  Selecting '%s'" % (filename))
    return (files, files_ignored)


def validate(cfg, files, result=None):
    if not cfg.get("quiet"):
        print("Validating...")
    error_counts = {}
    ignoreables = frozenset(cfg.get("ignore", []))
    ignore_targeted = cfg.get("ignore_path_errors", {})
    while files:
        f = files.popleft()
        if cfg.get("verbose"):
            print("Validating %s" % f)
        targeted_ignoreables = set(ignore_targeted.get(f.filename, set()))
        targeted_ignoreables.update(ignoreables)
        for c in fetch_checks(cfg):
            check_name = ".".join([c.__class__.__module__, c.__class__.__name__])
            error_counts.setdefault(check_name, 0)
            try:
                extension_matcher = c.EXT_MATCHER
            except AttributeError:
                pass
            else:
                if not extension_matcher.match(f.extension):
                    if cfg.get("verbose"):
                        print(
                            "  Skipping check '%s' since it does not"
                            " understand parsing a file with extension '%s'"
                            % (check_name, f.extension)
                        )
                    continue
            try:
                reports = set(c.REPORTS)
            except AttributeError:
                pass
            else:
                reports = reports - targeted_ignoreables
                if not reports:
                    if cfg.get("verbose"):
                        print(
                            "  Skipping check '%s', determined to only"
                            " check ignoreable codes" % check_name
                        )
                    continue
            if cfg.get("verbose"):
                print("  Running check '%s'" % check_name)
            if isinstance(c, checks.ContentCheck):
                for line_num, code, message in c.report_iter(f):
                    if code in targeted_ignoreables:
                        continue
                    if not isinstance(line_num, (float, int)):
                        line_num = "?"
                    if cfg.get("verbose"):
                        print(
                            "    - %s:%s: %s %s" % (f.filename, line_num, code, message)
                        )
                    elif not result.capture:
                        print("%s:%s: %s %s" % (f.filename, line_num, code, message))
                    result.error(check_name, f.filename, line_num, code, message)
                    error_counts[check_name] += 1
            elif isinstance(c, checks.LineCheck):
                for line_num, line in enumerate(f.lines_iter(), 1):
                    for code, message in c.report_iter(line):
                        if code in targeted_ignoreables:
                            continue
                        if cfg.get("verbose"):
                            print(
                                "    - %s:%s: %s %s"
                                % (f.filename, line_num, code, message)
                            )
                        elif not result.capture:
                            print(
                                "%s:%s: %s %s" % (f.filename, line_num, code, message)
                            )
                        result.error(check_name, f.filename, line_num, code, message)
                        error_counts[check_name] += 1
            else:
                raise TypeError("Unknown check type: %s, %s" % (type(c), c))
    return error_counts


def get_defaults():
    return {
        "paths": [os.getcwd()],
        "config": [],
        "allow_long_titles": False,
        "ignore": [],
        "sphinx": True,
        "ignore_path": [],
        "ignore_path_errors": [],
        "default_extension": "",
        "file_encoding": "",
        "max_line_length": MAX_LINE_LENGTH,
        "extension": list(FILE_PATTERNS),
        "quiet": False,
        "verbose": False,
        "version": False,
    }


class Result(object):
    def __init__(self):
        self.files_selected = 0
        self.files_ignored = 0
        self.error_counts = {}
        self.errors = []
        self.capture = False

    @property
    def total_errors(self):
        return len(self.errors)

    def error(self, check_name, filename, line_num, code, message):
        self.errors.append((check_name, filename, line_num, code, message))

    def finish(self, files_selected, files_ignored, error_counts):
        self.files_selected = files_selected
        self.files_ignored = files_ignored
        self.error_counts = error_counts

    def report(self):
        lines = []
        if self.capture:
            for error in self.errors:
                lines.append("%s:%s: %s %s" % error[1:])

        lines.extend(
            [
                "=" * 8,
                "Total files scanned = %s" % (self.files_selected),
                "Total files ignored = %s" % (self.files_ignored),
                "Total accumulated errors = %s" % (self.total_errors),
            ]
        )

        if self.error_counts:
            lines.append("Detailed error counts:")
            for check_name in sorted(self.error_counts.keys()):
                check_errors = self.error_counts[check_name]
                lines.append("    - %s = %s" % (check_name, check_errors))

        return "\n".join(lines)


def doc8(args=None, **kwargs):
    result = Result()
    if args is None:
        args = get_defaults()

        # Force reporting to suppress all output
        kwargs["quiet"] = True
        kwargs["verbose"] = False
        result.capture = True

    args["ignore"] = merge_sets(args["ignore"])
    cfg = extract_config(args)
    args["ignore"].update(cfg.pop("ignore", set()))
    if "sphinx" in cfg:
        args["sphinx"] = cfg.pop("sphinx")
    args["extension"].extend(cfg.pop("extension", []))
    args["ignore_path"].extend(cfg.pop("ignore_path", []))

    cfg.setdefault("ignore_path_errors", {})
    tmp_ignores = parse_ignore_path_errors(args.pop("ignore_path_errors", []))
    for path, ignores in tmp_ignores.items():
        if path in cfg["ignore_path_errors"]:
            cfg["ignore_path_errors"][path].update(ignores)
        else:
            cfg["ignore_path_errors"][path] = set(ignores)

    args.update(cfg)

    # Override args with any kwargs
    args.update(kwargs.items())
    setup_logging(args.get("verbose"))

    files, files_ignored = scan(args)
    files_selected = len(files)

    error_counts = validate(args, files, result=result)
    result.finish(files_selected, files_ignored, error_counts)
    return result


def main():
    defaults = get_defaults()
    parser = argparse.ArgumentParser(
        prog="doc8",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "paths",
        metavar="path",
        type=str,
        nargs="*",
        help=("path to scan for doc files (default: current directory)."),
        default=defaults["paths"],
    )
    parser.add_argument(
        "--config",
        metavar="path",
        action="append",
        help="user config file location"
        " (default: %s)." % ", ".join(CONFIG_FILENAMES),
        default=defaults["config"],
    )
    parser.add_argument(
        "--allow-long-titles",
        action="store_true",
        help="allow long section titles (default: false).",
        default=defaults["allow_long_titles"],
    )
    parser.add_argument(
        "--ignore",
        action="append",
        metavar="code",
        help="ignore the given error code(s).",
        type=split_set_type,
        default=defaults["ignore"],
    )
    parser.add_argument(
        "--no-sphinx",
        action="store_false",
        help="do not ignore sphinx specific false positives.",
        default=defaults["sphinx"],
        dest="sphinx",
    )
    parser.add_argument(
        "--ignore-path",
        action="append",
        default=defaults["ignore_path"],
        help="ignore the given directory or file (globs are supported).",
        metavar="path",
    )
    parser.add_argument(
        "--ignore-path-errors",
        action="append",
        default=defaults["ignore_path_errors"],
        help="ignore the given specific errors in the provided file.",
        metavar="path",
    )
    parser.add_argument(
        "--default-extension",
        action="store",
        help="default file extension to use when a file is"
        " found without a file extension.",
        default=defaults["default_extension"],
        dest="default_extension",
        metavar="extension",
    )
    parser.add_argument(
        "--file-encoding",
        action="store",
        help="set input files text encoding",
        default=defaults["file_encoding"],
        dest="file_encoding",
        metavar="encoding",
    )
    parser.add_argument(
        "--max-line-length",
        action="store",
        metavar="int",
        type=int,
        help="maximum allowed line"
        " length (default: %s)." % defaults["max_line_length"],
        default=defaults["max_line_length"],
    )
    parser.add_argument(
        "-e",
        "--extension",
        action="append",
        metavar="extension",
        help="check file extensions of the given type"
        " (default: %s)." % ", ".join(defaults["extension"]),
        default=defaults["extension"],
    )
    parser.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="only print violations",
        default=defaults["quiet"],
    )
    parser.add_argument(
        "-v",
        "--verbose",
        dest="verbose",
        action="store_true",
        help="run in verbose mode.",
        default=defaults["verbose"],
    )
    parser.add_argument(
        "--version",
        dest="version",
        action="store_true",
        help="show the version and exit.",
        default=defaults["version"],
    )
    args = vars(parser.parse_args())
    if args.get("version"):
        print(version.version_string)
        return 0

    result = doc8(args)

    if not args.get("quiet"):
        print(result.report())

    if result.total_errors:
        return 1
    else:
        return 0


if __name__ == "__main__":
    sys.exit(main())
