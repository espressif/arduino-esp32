# Copyright 2021 Monty Taylor
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

"""pep-517 support

Add::

      [build-system]
      requires = ["pbr>=5.7.0", "setuptools>=36.6.0", "wheel"]
      build-backend = "pbr.build"

to pyproject.toml to use this
"""

from setuptools import build_meta

__all__ = [
    'get_requires_for_build_sdist',
    'get_requires_for_build_wheel',
    'prepare_metadata_for_build_wheel',
    'build_wheel',
    'build_sdist',
]


def get_requires_for_build_wheel(config_settings=None):
    return build_meta.get_requires_for_build_wheel(config_settings)


def get_requires_for_build_sdist(config_settings=None):
    return build_meta.get_requires_for_build_sdist(config_settings)


def prepare_metadata_for_build_wheel(metadata_directory, config_settings=None):
    return build_meta.prepare_metadata_for_build_wheel(
        metadata_directory, config_settings)


def build_wheel(
    wheel_directory,
    config_settings=None,
    metadata_directory=None,
):
    return build_meta.build_wheel(
        wheel_directory, config_settings, metadata_directory,
    )


def build_sdist(sdist_directory, config_settings=None):
    return build_meta.build_sdist(sdist_directory, config_settings)
