# -*- coding: utf-8 -*-

#    Copyright (C) 2014 Yahoo! Inc. All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.

import tempfile

import testtools

from doc8 import checks
from doc8 import parser


class TestTrailingWhitespace(testtools.TestCase):
    def test_trailing(self):
        lines = ["a b  ", "ab"]
        check = checks.CheckTrailingWhitespace({})
        errors = []
        for line in lines:
            errors.extend(check.report_iter(line))
        self.assertEqual(1, len(errors))
        (code, msg) = errors[0]
        self.assertIn(code, check.REPORTS)


class TestTabIndentation(testtools.TestCase):
    def test_tabs(self):
        lines = ["    b", "\tabc", "efg", "\t\tc"]
        check = checks.CheckIndentationNoTab({})
        errors = []
        for line in lines:
            errors.extend(check.report_iter(line))
        self.assertEqual(2, len(errors))
        (code, msg) = errors[0]
        self.assertIn(code, check.REPORTS)


class TestCarriageReturn(testtools.TestCase):
    def test_cr(self):
        content = b"Windows line ending\r\nLegacy Mac line ending\r"
        content += (b"a" * 79) + b"\r\n" + b"\r"
        conf = {"max_line_length": 79}
        with tempfile.NamedTemporaryFile() as fh:
            fh.write(content)
            fh.flush()
            parsed_file = parser.ParsedFile(fh.name)
            check = checks.CheckCarriageReturn(conf)
            errors = list(check.report_iter(parsed_file))
            self.assertEqual(4, len(errors))
            (line, code, msg) = errors[0]
            self.assertIn(code, check.REPORTS)


class TestLineLength(testtools.TestCase):
    def test_over_length(self):
        content = b"""
===
aaa
===

----
test
----

"""
        content += b"\n\n"
        content += (b"a" * 60) + b" " + (b"b" * 60)
        content += b"\n"
        conf = {"max_line_length": 79, "allow_long_titles": True}
        for ext in [".rst", ".txt"]:
            with tempfile.NamedTemporaryFile(suffix=ext) as fh:
                fh.write(content)
                fh.flush()

                parsed_file = parser.ParsedFile(fh.name)
                check = checks.CheckMaxLineLength(conf)
                errors = list(check.report_iter(parsed_file))
                self.assertEqual(1, len(errors))
                (line, code, msg) = errors[0]
                self.assertIn(code, check.REPORTS)

    def test_correct_length(self):
        conf = {"max_line_length": 79, "allow_long_titles": True}
        with tempfile.NamedTemporaryFile(suffix=".rst") as fh:
            fh.write(
                b"known exploit in the wild, for example"
                b" \xe2\x80\x93 the time"
                b" between advance notification"
            )
            fh.flush()

            parsed_file = parser.ParsedFile(fh.name, encoding="utf-8")
            check = checks.CheckMaxLineLength(conf)
            errors = list(check.report_iter(parsed_file))
            self.assertEqual(0, len(errors))

    def test_ignore_code_block(self):
        conf = {"max_line_length": 79, "allow_long_titles": True}
        with tempfile.NamedTemporaryFile(suffix=".rst") as fh:
            fh.write(
                b"List which contains items with code-block\n"
                b"- this is a list item\n\n"
                b"  .. code-block:: ini\n\n"
                b"     this line exceeds 80 chars but should be ignored"
                b"this line exceeds 80 chars but should be ignored"
                b"this line exceeds 80 chars but should be ignored"
            )
            fh.flush()

            parsed_file = parser.ParsedFile(fh.name, encoding="utf-8")
            check = checks.CheckMaxLineLength(conf)
            errors = list(check.report_iter(parsed_file))
            self.assertEqual(0, len(errors))

    def test_unsplittable_length(self):
        content = b"""
===
aaa
===

----
test
----

"""
        content += b"\n\n"
        content += b"a" * 100
        content += b"\n"
        conf = {"max_line_length": 79, "allow_long_titles": True}
        # This number is different since rst parsing is aware that titles
        # are allowed to be over-length, while txt parsing is not aware of
        # this fact (since it has no concept of title sections).
        extensions = [(0, ".rst"), (1, ".txt")]
        for expected_errors, ext in extensions:
            with tempfile.NamedTemporaryFile(suffix=ext) as fh:
                fh.write(content)
                fh.flush()

                parsed_file = parser.ParsedFile(fh.name)
                check = checks.CheckMaxLineLength(conf)
                errors = list(check.report_iter(parsed_file))
                self.assertEqual(expected_errors, len(errors))

    def test_definition_term_length(self):
        conf = {"max_line_length": 79, "allow_long_titles": True}
        with tempfile.NamedTemporaryFile(suffix=".rst") as fh:
            fh.write(
                b"Definition List which contains long term.\n\n"
                b"looooooooooooooooooooooooooooooong definition term"
                b"this line exceeds 80 chars but should be ignored\n"
                b" this is a definition\n"
            )
            fh.flush()

            parsed_file = parser.ParsedFile(fh.name, encoding="utf-8")
            check = checks.CheckMaxLineLength(conf)
            errors = list(check.report_iter(parsed_file))
            self.assertEqual(0, len(errors))


class TestNewlineEndOfFile(testtools.TestCase):
    def test_newline(self):
        tests = [
            (1, b"testing"),
            (1, b"testing\ntesting"),
            (0, b"testing\n"),
            (0, b"testing\ntesting\n"),
            (0, b"testing\r\n"),
            (0, b"testing\r"),
        ]

        for expected_errors, line in tests:
            with tempfile.NamedTemporaryFile() as fh:
                fh.write(line)
                fh.flush()
                parsed_file = parser.ParsedFile(fh.name)
                check = checks.CheckNewlineEndOfFile({})
                errors = list(check.report_iter(parsed_file))
                self.assertEqual(expected_errors, len(errors))
