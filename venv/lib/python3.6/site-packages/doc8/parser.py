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

import errno
import os
import threading

from docutils import frontend
from docutils import parsers as docutils_parser
from docutils import utils
import restructuredtext_lint as rl


class ParsedFile(object):
    FALLBACK_ENCODING = "utf-8"

    def __init__(self, filename, encoding=None, default_extension=""):
        self._filename = filename
        self._content = None
        self._raw_content = None
        self._encoding = encoding
        self._doc = None
        self._errors = None
        self._lines = None
        self._has_read = False
        self._extension = os.path.splitext(filename)[1]
        self._read_lock = threading.Lock()
        if not self._extension:
            self._extension = default_extension

    @property
    def errors(self):
        if self._errors is not None:
            return self._errors
        self._errors = rl.lint(self.contents, filepath=self.filename)
        return self._errors

    @property
    def document(self):
        if self._doc is None:
            # Use the rst parsers document output to do as much of the
            # validation as we can without resorting to custom logic (this
            # parser is what sphinx and others use anyway so it's hopefully
            # mature).
            parser_cls = docutils_parser.get_parser_class("rst")
            parser = parser_cls()
            defaults = {
                "halt_level": 5,
                "report_level": 5,
                "quiet": True,
                "file_insertion_enabled": False,
                "traceback": True,
                # Development use only.
                "dump_settings": False,
                "dump_internals": False,
                "dump_transforms": False,
            }
            opt = frontend.OptionParser(components=[parser], defaults=defaults)
            doc = utils.new_document(
                source_path=self.filename, settings=opt.get_default_values()
            )
            parser.parse(self.contents, doc)
            self._doc = doc
        return self._doc

    def _read(self):
        if self._has_read:
            return
        with self._read_lock:
            if not self._has_read:
                with open(self.filename, "rb") as fh:
                    self._raw_content = fh.read()
                    self._lines = self._raw_content.splitlines(True)
                self._has_read = True

    def lines_iter(self, remove_trailing_newline=True):
        self._read()
        for line in self._lines:
            line = str(line, encoding=self.encoding)
            if remove_trailing_newline:
                # Cope with various OS new line conventions
                if line.endswith("\n"):
                    line = line[:-1]
                if line.endswith("\r"):
                    line = line[:-1]
            yield line

    @property
    def lines(self):
        self._read()
        return self._lines

    @property
    def extension(self):
        return self._extension

    @property
    def filename(self):
        return self._filename

    @property
    def encoding(self):
        if not self._encoding:
            self._encoding = self.FALLBACK_ENCODING
        return self._encoding

    @property
    def raw_contents(self):
        self._read()
        return self._raw_content

    @property
    def contents(self):
        if self._content is None:
            self._content = str(self.raw_contents, encoding=self.encoding)
        return self._content

    def __str__(self):
        return "%s (%s, %s chars, %s lines)" % (
            self.filename,
            self.encoding,
            len(self.contents),
            len(list(self.lines_iter())),
        )


def parse(filename, encoding=None, default_extension=""):
    if not os.path.isfile(filename):
        raise IOError(errno.ENOENT, "File not found", filename)
    return ParsedFile(filename, encoding=encoding, default_extension=default_extension)
