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

import glob
import os


def find_files(paths, extensions, ignored_paths):
    extensions = set(extensions)
    ignored_absolute_paths = set()
    for path in ignored_paths:
        for expanded_path in glob.iglob(path):
            expanded_path = os.path.abspath(expanded_path)
            ignored_absolute_paths.add(expanded_path)

    def extension_matches(path):
        _base, ext = os.path.splitext(path)
        return ext in extensions

    def path_ignorable(path):
        path = os.path.abspath(path)
        if path in ignored_absolute_paths:
            return True
        last_path = None
        while path != last_path:
            # If we hit the root, this loop will stop since the resolution
            # of "/../" is still "/" when ran through the abspath function...
            last_path = path
            path = os.path.abspath(os.path.join(path, os.path.pardir))
            if path in ignored_absolute_paths:
                return True
        return False

    for path in paths:
        if os.path.isfile(path):
            if extension_matches(path):
                yield (path, path_ignorable(path))
        elif os.path.isdir(path):
            for root, dirnames, filenames in os.walk(path):
                for filename in filenames:
                    path = os.path.join(root, filename)
                    if extension_matches(path):
                        yield (path, path_ignorable(path))
        else:
            raise IOError("Invalid path: %s" % path)


def filtered_traverse(document, filter_func):
    for n in document.traverse(include_self=True):
        if filter_func(n):
            yield n


def contains_url(line):
    return "http://" in line or "https://" in line


def has_any_node_type(node, node_types):
    n = node
    while n is not None:
        if isinstance(n, node_types):
            return True
        n = n.parent
    return False
