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

import abc
import collections
import re

from docutils import nodes as docutils_nodes

from doc8 import utils


class ContentCheck(metaclass=abc.ABCMeta):
    def __init__(self, cfg):
        self._cfg = cfg

    @abc.abstractmethod
    def report_iter(self, parsed_file):
        pass


class LineCheck(metaclass=abc.ABCMeta):
    def __init__(self, cfg):
        self._cfg = cfg

    @abc.abstractmethod
    def report_iter(self, line):
        pass


class CheckTrailingWhitespace(LineCheck):
    _TRAILING_WHITESPACE_REGEX = re.compile(r"\s$")
    REPORTS = frozenset(["D002"])

    def report_iter(self, line):
        if self._TRAILING_WHITESPACE_REGEX.search(line):
            yield ("D002", "Trailing whitespace")


class CheckIndentationNoTab(LineCheck):
    _STARTING_WHITESPACE_REGEX = re.compile(r"^(\s+)")
    REPORTS = frozenset(["D003"])

    def report_iter(self, line):
        match = self._STARTING_WHITESPACE_REGEX.search(line)
        if match:
            spaces = match.group(1)
            if "\t" in spaces:
                yield ("D003", "Tabulation used for indentation")


class CheckCarriageReturn(ContentCheck):
    REPORTS = frozenset(["D004"])

    def report_iter(self, parsed_file):
        for i, line in enumerate(parsed_file.lines):
            if b"\r" in line:
                yield (i + 1, "D004", "Found literal carriage return")


class CheckNewlineEndOfFile(ContentCheck):
    REPORTS = frozenset(["D005"])

    def __init__(self, cfg):
        super(CheckNewlineEndOfFile, self).__init__(cfg)

    def report_iter(self, parsed_file):
        if parsed_file.lines and not (
            parsed_file.lines[-1].endswith(b"\n")
            or parsed_file._lines[-1].endswith(b"\r")
        ):
            yield (len(parsed_file.lines), "D005", "No newline at end of file")


class CheckValidity(ContentCheck):
    REPORTS = frozenset(["D000"])
    EXT_MATCHER = re.compile(r"(.*)[.]rst", re.I)

    # From docutils docs:
    #
    # Report system messages at or higher than <level>: "info" or "1",
    # "warning"/"2" (default), "error"/"3", "severe"/"4", "none"/"5"
    #
    # See: http://docutils.sourceforge.net/docs/user/config.html#report-level
    WARN_LEVELS = frozenset([2, 3, 4])

    # Only used when running in sphinx mode.
    SPHINX_IGNORES_REGEX = [
        re.compile(r"^Unknown interpreted text"),
        re.compile(r"^Unknown directive type"),
        re.compile(r"^Undefined substitution"),
        re.compile(r"^Substitution definition contains illegal element"),
        re.compile(
            r'^Error in \"code-block\" directive\:\nunknown option: "caption".',
            re.MULTILINE,
        ),
        re.compile(
            r'^Error in "code-block" directive:\nunknown option: "emphasize-lines"'
        ),
        re.compile(r'^Error in "code-block" directive:\nunknown option: "linenos"'),
        re.compile(
            r'^Error in "code-block" directive:\nunknown option: "lineno-start"'
        ),
        re.compile(r'^Error in "code-block" directive:\nunknown option: "dedent"'),
        re.compile(r'^Error in "code-block" directive:\nunknown option: "force"'),
        re.compile(r'^Error in "math" directive:\nunknown option: "label"'),
        re.compile(r'^Error in "math" directive:\nunknown option: "nowrap"'),
        re.compile(
            r'^Error in \"code-block\" directive\:\nunknown option: "substitutions".',
            re.MULTILINE,
        ),
    ]

    def __init__(self, cfg):
        super(CheckValidity, self).__init__(cfg)
        self._sphinx_mode = cfg.get("sphinx")

    def report_iter(self, parsed_file):
        for error in parsed_file.errors:
            if error.level not in self.WARN_LEVELS:
                continue
            ignore = False
            if self._sphinx_mode:
                for m in self.SPHINX_IGNORES_REGEX:
                    if m.match(error.message):
                        ignore = True
                        break
            if not ignore:
                yield (error.line, "D000", error.message)


class CheckMaxLineLength(ContentCheck):
    REPORTS = frozenset(["D001"])

    def __init__(self, cfg):
        super(CheckMaxLineLength, self).__init__(cfg)
        self._max_line_length = self._cfg["max_line_length"]
        self._allow_long_titles = self._cfg["allow_long_titles"]

    def _extract_node_lines(self, doc):
        def extract_lines(node, start_line):
            lines = [start_line]
            if isinstance(node, (docutils_nodes.title)):
                start = start_line - len(node.rawsource.splitlines())
                if start >= 0:
                    lines.append(start)
            if isinstance(node, (docutils_nodes.literal_block)):
                end = start_line + len(node.rawsource.splitlines()) - 1
                lines.append(end)
            return lines

        def gather_lines(node):
            lines = []
            for n in node.traverse(include_self=True):
                lines.extend(extract_lines(n, find_line(n)))
            return lines

        def find_line(node):
            n = node
            while n is not None:
                if n.line is not None:
                    return n.line
                n = n.parent
            return None

        def filter_systems(node):
            if utils.has_any_node_type(node, (docutils_nodes.system_message,)):
                return False
            return True

        nodes_lines = []
        first_line = -1
        for n in utils.filtered_traverse(doc, filter_systems):
            line = find_line(n)
            if line is None:
                continue
            if first_line == -1:
                first_line = line
            contained_lines = set(gather_lines(n))
            nodes_lines.append((n, (min(contained_lines), max(contained_lines))))
        return (nodes_lines, first_line)

    def _extract_directives(self, lines):
        def starting_whitespace(line):
            m = re.match(r"^(\s+)(.*)$", line)
            if not m:
                return 0
            return len(m.group(1))

        def all_whitespace(line):
            return bool(re.match(r"^(\s*)$", line))

        def find_directive_end(start, lines):
            after_lines = collections.deque(lines[start + 1 :])
            k = 0
            while after_lines:
                line = after_lines.popleft()
                if all_whitespace(line) or starting_whitespace(line) >= 1:
                    k += 1
                else:
                    break
            return start + k

        # Find where directives start & end so that we can exclude content in
        # these directive regions (the rst parser may not handle this correctly
        # for unknown directives, so we have to do it manually).
        directives = []
        for i, line in enumerate(lines):
            if re.match(r"^\s*..\s(.*?)::\s*", line):
                directives.append((i, find_directive_end(i, lines)))
            elif re.match(r"^::\s*$", line):
                directives.append((i, find_directive_end(i, lines)))

        # Find definition terms in definition lists
        # This check may match the code, which is already appended
        lwhitespaces = r"^\s*"
        listspattern = r"^\s*(\* |- |#\. |\d+\. )"
        for i in range(0, len(lines) - 1):
            line = lines[i]
            next_line = lines[i + 1]
            # if line is a blank, line is not a definition term
            if all_whitespace(line):
                continue
            # if line is a list, line is checked as normal line
            if re.match(listspattern, line):
                continue
            if len(re.search(lwhitespaces, line).group()) < len(
                re.search(lwhitespaces, next_line).group()
            ):
                directives.append((i, i))

        return directives

    def _txt_checker(self, parsed_file):
        for i, line in enumerate(parsed_file.lines_iter()):
            if len(line) > self._max_line_length:
                if not utils.contains_url(line):
                    yield (i + 1, "D001", "Line too long")

    def _rst_checker(self, parsed_file):
        lines = list(parsed_file.lines_iter())
        doc = parsed_file.document
        nodes_lines, first_line = self._extract_node_lines(doc)
        directives = self._extract_directives(lines)

        def find_containing_nodes(num):
            if num < first_line and len(nodes_lines):
                return [nodes_lines[0][0]]
            contained_in = []
            for (n, (line_min, line_max)) in nodes_lines:
                if num >= line_min and num <= line_max:
                    contained_in.append((n, (line_min, line_max)))
            smallest_span = None
            best_nodes = []
            for (n, (line_min, line_max)) in contained_in:
                span = line_max - line_min
                if smallest_span is None:
                    smallest_span = span
                    best_nodes = [n]
                elif span < smallest_span:
                    smallest_span = span
                    best_nodes = [n]
                elif span == smallest_span:
                    best_nodes.append(n)
            return best_nodes

        def any_types(nodes, types):
            return any([isinstance(n, types) for n in nodes])

        skip_types = (docutils_nodes.target, docutils_nodes.literal_block)
        title_types = (
            docutils_nodes.title,
            docutils_nodes.subtitle,
            docutils_nodes.section,
        )
        for i, line in enumerate(lines):
            if len(line) > self._max_line_length:
                in_directive = False
                for (start, end) in directives:
                    if i >= start and i <= end:
                        in_directive = True
                        break
                if in_directive:
                    continue
                stripped = line.lstrip()
                if " " not in stripped:
                    # No room to split even if we could.
                    continue
                if utils.contains_url(stripped):
                    continue
                nodes = find_containing_nodes(i + 1)
                if any_types(nodes, skip_types):
                    continue
                if self._allow_long_titles and any_types(nodes, title_types):
                    continue
                yield (i + 1, "D001", "Line too long")

    def report_iter(self, parsed_file):
        if parsed_file.extension.lower() != ".rst":
            checker_func = self._txt_checker
        else:
            checker_func = self._rst_checker
        for issue in checker_func(parsed_file):
            yield issue
