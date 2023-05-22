#!/usr/bin/env python3
#
# ESP32 partition table generation tool
#
# Converts partition tables to/from CSV and binary formats.
#
# See https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/partition-tables.html
# for explanation of partition table structure and uses.
#
# SPDX-FileCopyrightText: 2016-2021 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import binascii
import hashlib
import os
import re
import struct
import sys

MAX_PARTITION_LENGTH = 0xC00  # 3K for partition data (96 entries) leaves 1K in a 4K sector for signature
MD5_PARTITION_BEGIN = b'\xEB\xEB' + b'\xFF' * 14  # The first 2 bytes are like magic numbers for MD5 sum
PARTITION_TABLE_SIZE = 0x1000  # Size of partition table

MIN_PARTITION_SUBTYPE_APP_OTA = 0x10
NUM_PARTITION_SUBTYPE_APP_OTA = 16

__version__ = '1.2'

APP_TYPE = 0x00
DATA_TYPE = 0x01

TYPES = {
    'app': APP_TYPE,
    'data': DATA_TYPE,
}

# Keep this map in sync with esp_partition_subtype_t enum in esp_partition.h
SUBTYPES = {
    APP_TYPE: {
        'factory': 0x00,
        'test': 0x20,
    },
    DATA_TYPE: {
        'ota': 0x00,
        'phy': 0x01,
        'nvs': 0x02,
        'coredump': 0x03,
        'nvs_keys': 0x04,
        'efuse': 0x05,
        'undefined': 0x06,
        'esphttpd': 0x80,
        'fat': 0x81,
        'spiffs': 0x82,
    },
}

ALIGNMENT = {
    APP_TYPE: 0x10000,
    DATA_TYPE: 0x4,
}

STRICT_DATA_ALIGNMENT = 0x1000


def get_ptype_as_int(ptype):
    """ Convert a string which might be numeric or the name of a partition type to an integer """
    return TYPES.get(ptype, int(ptype, 0))


def get_subtype_as_int(ptype, subtype):
    """ Convert a string which might be numeric or the name of a partition subtype to an integer """
    return SUBTYPES.get(get_ptype_as_int(ptype), {}).get(subtype, int(subtype, 0))


def get_alignment_for_type(ptype):
    return ALIGNMENT.get(ptype, ALIGNMENT[DATA_TYPE])


def status(msg):
    """ Print status message to stderr """
    if not quiet:
        critical(msg)


def critical(msg):
    """ Print critical message to stderr """
    sys.stderr.write(msg)
    sys.stderr.write('\n')


class PartitionDefinition:
    MAGIC_BYTES = b'\xAA\x50'

    FLAGS = {
        'encrypted': 0
    }

    def __init__(self):
        self.name = ''
        self.type = None
        self.subtype = None
        self.offset = None
        self.size = None
        self.encrypted = False

    @classmethod
    def from_csv(cls, line, line_no):
        """ Parse a line from the CSV """
        line_w_defaults = line + ',,,,'  # lazy way to support default fields
        fields = [f.strip() for f in line_w_defaults.split(',')]

        res = cls()
        res.line_no = line_no
        res.name = fields[0]
        res.type = get_ptype_as_int(fields[1])
        res.subtype = get_subtype_as_int(fields[2], fields[1])
        res.offset = parse_address(fields[3])
        res.size = parse_address(fields[4])
        if res.size is None:
            raise InputError("Size field can't be empty")

        flags = fields[5].split(':')
        for flag in flags:
            if flag in cls.FLAGS:
                setattr(res, flag, True)
            elif len(flag) > 0:
                raise InputError("CSV flag column contains unknown flag '%s'" % flag)

        return res

    def __eq__(self, other):
        return (
            self.name == other.name
            and self.type == other.type
            and self.subtype == other.subtype
            and self.offset == other.offset
            and self.size == other.size
        )

    def __repr__(self):
        return f"PartitionDefinition('{self.name}', {self.type}, {self.subtype or 0}, {hex(self.offset) if self.offset is not None else 'None'}, {hex(self.size) if self.size is not None else 'None'})"

    def __str__(self):
        return f"Part '{self.name}' {self.type}/{self.subtype} @ {hex(self.offset) if self.offset is not None else 'None'} size {hex(self.size) if self.size is not None else 'None'}"

    def __cmp__(self, other):
        return self.offset - other.offset

    def __lt__(self, other):
        return self.offset < other.offset

    def __gt__(self, other):
        return self.offset > other.offset

    def __le__(self, other):
        return self.offset <= other.offset

    def __ge__(self, other):
        return self.offset >= other.offset

    def verify(self):
        if self.type is None:
            raise ValidationError(self, 'Type field is not set')
        if self.subtype is None:
            raise ValidationError(self, 'Subtype field is not set')
        if self.offset is None:
            raise ValidationError(self, 'Offset field is not set')
        align = get_alignment_for_type(self.type)
        if self.offset % align:
            raise ValidationError(self, 'Offset {self.offset:#x} is not aligned to {align:#x}')
        if self.type != APP_TYPE and self.offset % STRICT_DATA_ALIGNMENT:
            critical(f'WARNING: Partition {self.name} not aligned to 0x{STRICT_DATA_ALIGNMENT:x}. '
                     'This is deprecated and will be considered an error in the future release.')
        if secure and self.size % align and self.type == APP_TYPE:
            raise ValidationError(self, 'Size {self.size:#x} is not aligned to {align:#x}')
        if self.size is None:
            raise ValidationError(self, 'Size field is not set')

        if self.name in TYPES and TYPES.get(self.name, '') != self.type:
            critical(f"WARNING: Partition has name '{self.name}' which is a partition type, but does not match "
                     f"this partition's type ({self.type:#x}). Mistake in partition table?")
        all_subtype_names = [name for subtype_names in SUBTYPES.values() for name in subtype_names]
        if self.name in all_subtype_names and SUBTYPES.get(self.type, {}).get(self.name, '') != self.subtype:
            critical(f"WARNING: Partition has name '{self.name}' which is a partition subtype, but this partition "
                     f"has non-matching type {self.type:#x} and subtype {self.subtype:#x}. Mistake in partition table?")

    STRUCT_FORMAT = '<2sBBLL16sL'

    @classmethod
    def from_binary(cls, b):
        if len(b) != 32:
            raise InputError('Partition definition length must be exactly 32 bytes. Got {len(b)} bytes.')
        res = cls()
        (
            magic, res.type, res.subtype, res.offset, res.size, res.name, flags
        ) = struct.unpack(cls.STRUCT_FORMAT, b)
        if magic != cls.MAGIC_BYTES:
            raise InputError('Invalid magic bytes ({magic!r}) for partition definition')
        for flag, bit in cls.FLAGS.items():
            if flags & (1 << bit):
                setattr(res, flag, True)
                flags &= ~(1 << bit)
        if flags != 0:
            critical(f'WARNING: Partition definition had unknown flag(s) {flags:#08x}. Newer binary format?')
        return res

    def get_flags_list(self):
        return [flag for flag in self.FLAGS.keys() if getattr(self, flag)]

    def to_binary(self):
        flags = sum(1 << self.FLAGS[flag] for flag in self.get_flags_list())
        return struct.pack(
            self.STRUCT_FORMAT,
            self.MAGIC_BYTES,
            self.type,
            self.subtype,
            self.offset,
            self.size,
            self.name.encode(),
            flags
        )

    def to_csv(self, simple_formatting=False):
        def addr_format(a, include_sizes):
            if not simple_formatting and include_sizes:
                for val, suffix in [(0x100000, 'M'), (0x400, 'K')]:
                    if a % val == 0:
                        return f'{a // val}{suffix}'
            return f'0x{a:x}'

        def lookup_keyword(t, keywords):
            for k, v in keywords.items():
                if simple_formatting is False and t == v:
                    return k
            return str(t)

        def generate_text_flags():
            """ colon-delimited list of flags """
            return ':'.join(self.get_flags_list())

        return ','.join(
            [
                self.name,
                lookup_keyword(self.type, TYPES),
                lookup_keyword(self.subtype, SUBTYPES.get(self.type, {})),
                addr_format(self.offset, False),
                addr_format(self.size, True),
                generate_text_flags(),
            ]
        )


class PartitionTable:
    def __init__(self):
        self.partitions = []

    @classmethod
    def from_file(cls, f):
        data = f.read()
        data_is_binary = data[0:2] == PartitionDefinition.MAGIC_BYTES
        if data_is_binary:
            status('Parsing binary partition table')
            if len(data) % 32 != 0:
                raise InputError('Invalid binary partition table size')
            res = cls()
            for i in range(0, len(data), 32):
                res.partitions.append(PartitionDefinition.from_binary(data[i:i + 32]))
            return res
        else:
            status('Parsing CSV partition table')
            lines = data.split('\n')
            res = cls()
            for line_no, line in enumerate(lines):
                line = line.strip()
                if len(line) > 0 and not line.startswith('#'):
                    res.partitions.append(PartitionDefinition.from_csv(line, line_no + 1))
            return res

    def to_file(self, f):
        f.write(self.to_binary())

    def add_partition(self, partition):
        self.partitions.append(partition)
        self.partitions.sort()

    def verify(self):
        for p in self.partitions:
            p.verify()
        for i in range(len(self.partitions) - 1):
            if self.partitions[i].offset + self.partitions[i].size > self.partitions[i + 1].offset:
                raise ValidationError(self.partitions[i + 1], 'Overlaps previous partition')
        if len(self.partitions) * 32 > PARTITION_TABLE_SIZE:
            raise ValidationError(None, 'Partition table too large')

    def to_binary(self):
        res = b''
        for partition in self.partitions:
            res += partition.to_binary()
        return res.ljust(PARTITION_TABLE_SIZE, b'\xFF')

    def to_csv(self, simple_formatting=False):
        res = []
        for partition in self.partitions:
            res.append(partition.to_csv(simple_formatting))
        return '\n'.join(res)


class PartitionGenerator:
    def __init__(self):
        self.partition_table = PartitionTable()

    def set_from_file(self, f):
        self.partition_table = PartitionTable.from_file(f)

    def add_partition(self, partition):
        self.partition_table.add_partition(partition)

    def generate_partition_table(self):
        self.partition_table.verify()
        return self.partition_table.to_binary()

    def get_partition_table(self):
        return self.partition_table


class InputError(Exception):
    pass


class ValidationError(Exception):
    def __init__(self, partition, message):
        self.partition = partition
        self.message = message
        super().__init__(message)


def parse_address(a):
    try:
        return int(a, 0)
    except ValueError:
        return None


def main(argv):
    global secure, quiet

    parser = argparse.ArgumentParser(
        description='ESP32 partition table generator'
    )
    parser.add_argument(
        '-i', '--input', metavar='input.csv', type=argparse.FileType('r'),
        help='input CSV file (default: stdin)'
    )
    parser.add_argument(
        '-o', '--output', metavar='output.bin', type=argparse.FileType('wb'),
        help='output binary file (default: stdout)'
    )
    parser.add_argument(
        '--simple-formatting', action='store_true',
        help='Use simplified formatting for CSV output'
    )
    parser.add_argument(
        '--secure', action='store_true',
        help='Enable secure boot options'
    )
    parser.add_argument(
        '--quiet', action='store_true',
        help='Suppress status messages'
    )
    parser.add_argument(
        '--version', action='version', version=f'%(prog)s {__version__}',
        help='Print version information'
    )

    args = parser.parse_args(argv)

    if args.input:
        generator = PartitionGenerator()
        generator.set_from_file(args.input)
    else:
        generator = PartitionGenerator()

    secure = args.secure
    quiet = args.quiet

    binary_data = generator.generate_partition_table()

    if args.output:
        args.output.write(binary_data)
    else:
        sys.stdout.buffer.write(binary_data)


if __name__ == '__main__':
    main(sys.argv[1:])
