#!/usr/bin/env python
#
# ESP8266 & ESP32 family ROM Bootloader Utility
# Copyright (C) 2014-2016 Fredrik Ahlberg, Angus Gratton, Espressif Systems (Shanghai) PTE LTD, other contributors as noted.
# https://github.com/espressif/esptool
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301 USA.

from __future__ import division, print_function

import argparse
import base64
import binascii
import copy
import hashlib
import inspect
import io
import itertools
import os
import shlex
import string
import struct
import sys
import time
import zlib

try:
    import serial
except ImportError:
    print("Pyserial is not installed for %s. Check the README for installation instructions." % (sys.executable))
    raise

# check 'serial' is 'pyserial' and not 'serial' https://github.com/espressif/esptool/issues/269
try:
    if "serialization" in serial.__doc__ and "deserialization" in serial.__doc__:
        raise ImportError("""
esptool.py depends on pyserial, but there is a conflict with a currently installed package named 'serial'.

You may be able to work around this by 'pip uninstall serial; pip install pyserial' \
but this may break other installed Python software that depends on 'serial'.

There is no good fix for this right now, apart from configuring virtualenvs. \
See https://github.com/espressif/esptool/issues/269#issuecomment-385298196 for discussion of the underlying issue(s).""")
except TypeError:
    pass  # __doc__ returns None for pyserial

try:
    import serial.tools.list_ports as list_ports
except ImportError:
    print("The installed version (%s) of pyserial appears to be too old for esptool.py (Python interpreter %s). "
          "Check the README for installation instructions." % (sys.VERSION, sys.executable))
    raise
except Exception:
    if sys.platform == "darwin":
        # swallow the exception, this is a known issue in pyserial+macOS Big Sur preview ref https://github.com/espressif/esptool/issues/540
        list_ports = None
    else:
        raise


__version__ = "3.1-dev"

MAX_UINT32 = 0xffffffff
MAX_UINT24 = 0xffffff

DEFAULT_TIMEOUT = 3                   # timeout for most flash operations
START_FLASH_TIMEOUT = 20              # timeout for starting flash (may perform erase)
CHIP_ERASE_TIMEOUT = 120              # timeout for full chip erase
MAX_TIMEOUT = CHIP_ERASE_TIMEOUT * 2  # longest any command can run
SYNC_TIMEOUT = 0.1                    # timeout for syncing with bootloader
MD5_TIMEOUT_PER_MB = 8                # timeout (per megabyte) for calculating md5sum
ERASE_REGION_TIMEOUT_PER_MB = 30      # timeout (per megabyte) for erasing a region
ERASE_WRITE_TIMEOUT_PER_MB = 40       # timeout (per megabyte) for erasing and writing data
MEM_END_ROM_TIMEOUT = 0.05            # special short timeout for ESP_MEM_END, as it may never respond
DEFAULT_SERIAL_WRITE_TIMEOUT = 10     # timeout for serial port write
DEFAULT_CONNECT_ATTEMPTS = 7          # default number of times to try connection


def timeout_per_mb(seconds_per_mb, size_bytes):
    """ Scales timeouts which are size-specific """
    result = seconds_per_mb * (size_bytes / 1e6)
    if result < DEFAULT_TIMEOUT:
        return DEFAULT_TIMEOUT
    return result


def _chip_to_rom_loader(chip):
    return {
        'esp8266': ESP8266ROM,
        'esp32': ESP32ROM,
        'esp32s2': ESP32S2ROM,
        'esp32s3beta2': ESP32S3BETA2ROM,
        'esp32c3': ESP32C3ROM,
    }[chip]


def get_default_connected_device(serial_list, port, connect_attempts, initial_baud, chip='auto', trace=False,
                                 before='default_reset'):
    _esp = None
    for each_port in reversed(serial_list):
        print("Serial port %s" % each_port)
        try:
            if chip == 'auto':
                _esp = ESPLoader.detect_chip(each_port, initial_baud, before, trace,
                                             connect_attempts)
            else:
                chip_class = _chip_to_rom_loader(chip)
                _esp = chip_class(each_port, initial_baud, trace)
                _esp.connect(before, connect_attempts)
            break
        except (FatalError, OSError) as err:
            if port is not None:
                raise
            print("%s failed to connect: %s" % (each_port, err))
            _esp = None
    return _esp


DETECTED_FLASH_SIZES = {0x12: '256KB', 0x13: '512KB', 0x14: '1MB',
                        0x15: '2MB', 0x16: '4MB', 0x17: '8MB', 0x18: '16MB'}


def check_supported_function(func, check_func):
    """
    Decorator implementation that wraps a check around an ESPLoader
    bootloader function to check if it's supported.

    This is used to capture the multidimensional differences in
    functionality between the ESP8266 & ESP32/32S2/32S3/32C3 ROM loaders, and the
    software stub that runs on both. Not possible to do this cleanly
    via inheritance alone.
    """
    def inner(*args, **kwargs):
        obj = args[0]
        if check_func(obj):
            return func(*args, **kwargs)
        else:
            raise NotImplementedInROMError(obj, func)
    return inner


def stub_function_only(func):
    """ Attribute for a function only supported in the software stub loader """
    return check_supported_function(func, lambda o: o.IS_STUB)


def stub_and_esp32_function_only(func):
    """ Attribute for a function only supported by software stubs or ESP32/32S2/32S3/32C3 ROM """
    return check_supported_function(func, lambda o: o.IS_STUB or isinstance(o, ESP32ROM))


PYTHON2 = sys.version_info[0] < 3  # True if on pre-Python 3

# Function to return nth byte of a bitstring
# Different behaviour on Python 2 vs 3
if PYTHON2:
    def byte(bitstr, index):
        return ord(bitstr[index])
else:
    def byte(bitstr, index):
        return bitstr[index]

# Provide a 'basestring' class on Python 3
try:
    basestring
except NameError:
    basestring = str


def print_overwrite(message, last_line=False):
    """ Print a message, overwriting the currently printed line.

    If last_line is False, don't append a newline at the end (expecting another subsequent call will overwrite this one.)

    After a sequence of calls with last_line=False, call once with last_line=True.

    If output is not a TTY (for example redirected a pipe), no overwriting happens and this function is the same as print().
    """
    if sys.stdout.isatty():
        print("\r%s" % message, end='\n' if last_line else '')
    else:
        print(message)


def _mask_to_shift(mask):
    """ Return the index of the least significant bit in the mask """
    shift = 0
    while mask & 0x1 == 0:
        shift += 1
        mask >>= 1
    return shift


def esp8266_function_only(func):
    """ Attribute for a function only supported on ESP8266 """
    return check_supported_function(func, lambda o: o.CHIP_NAME == "ESP8266")


class ESPLoader(object):
    """ Base class providing access to ESP ROM & software stub bootloaders.
    Subclasses provide ESP8266 & ESP32 specific functionality.

    Don't instantiate this base class directly, either instantiate a subclass or
    call ESPLoader.detect_chip() which will interrogate the chip and return the
    appropriate subclass instance.

    """
    CHIP_NAME = "Espressif device"
    IS_STUB = False

    DEFAULT_PORT = "/dev/ttyUSB0"

    # Commands supported by ESP8266 ROM bootloader
    ESP_FLASH_BEGIN = 0x02
    ESP_FLASH_DATA  = 0x03
    ESP_FLASH_END   = 0x04
    ESP_MEM_BEGIN   = 0x05
    ESP_MEM_END     = 0x06
    ESP_MEM_DATA    = 0x07
    ESP_SYNC        = 0x08
    ESP_WRITE_REG   = 0x09
    ESP_READ_REG    = 0x0a

    # Some comands supported by ESP32 ROM bootloader (or -8266 w/ stub)
    ESP_SPI_SET_PARAMS = 0x0B
    ESP_SPI_ATTACH     = 0x0D
    ESP_READ_FLASH_SLOW  = 0x0e  # ROM only, much slower than the stub flash read
    ESP_CHANGE_BAUDRATE = 0x0F
    ESP_FLASH_DEFL_BEGIN = 0x10
    ESP_FLASH_DEFL_DATA  = 0x11
    ESP_FLASH_DEFL_END   = 0x12
    ESP_SPI_FLASH_MD5    = 0x13

    # Commands supported by ESP32-S2/S3/C3 ROM bootloader only
    ESP_GET_SECURITY_INFO = 0x14

    # Some commands supported by stub only
    ESP_ERASE_FLASH = 0xD0
    ESP_ERASE_REGION = 0xD1
    ESP_READ_FLASH = 0xD2
    ESP_RUN_USER_CODE = 0xD3

    # Flash encryption encrypted data command
    ESP_FLASH_ENCRYPT_DATA = 0xD4

    # Response code(s) sent by ROM
    ROM_INVALID_RECV_MSG = 0x05   # response if an invalid message is received

    # Maximum block sized for RAM and Flash writes, respectively.
    ESP_RAM_BLOCK   = 0x1800

    FLASH_WRITE_SIZE = 0x400

    # Default baudrate. The ROM auto-bauds, so we can use more or less whatever we want.
    ESP_ROM_BAUD    = 115200

    # First byte of the application image
    ESP_IMAGE_MAGIC = 0xe9

    # Initial state for the checksum routine
    ESP_CHECKSUM_MAGIC = 0xef

    # Flash sector size, minimum unit of erase.
    FLASH_SECTOR_SIZE = 0x1000

    UART_DATE_REG_ADDR = 0x60000078

    CHIP_DETECT_MAGIC_REG_ADDR = 0x40001000  # This ROM address has a different value on each chip model

    UART_CLKDIV_MASK = 0xFFFFF

    # Memory addresses
    IROM_MAP_START = 0x40200000
    IROM_MAP_END = 0x40300000

    # The number of bytes in the UART response that signify command status
    STATUS_BYTES_LENGTH = 2

    def __init__(self, port=DEFAULT_PORT, baud=ESP_ROM_BAUD, trace_enabled=False):
        """Base constructor for ESPLoader bootloader interaction

        Don't call this constructor, either instantiate ESP8266ROM
        or ESP32ROM, or use ESPLoader.detect_chip().

        This base class has all of the instance methods for bootloader
        functionality supported across various chips & stub
        loaders. Subclasses replace the functions they don't support
        with ones which throw NotImplementedInROMError().

        """
        self.secure_download_mode = False  # flag is set to True if esptool detects the ROM is in Secure Download Mode

        if isinstance(port, basestring):
            self._port = serial.serial_for_url(port)
        else:
            self._port = port
        self._slip_reader = slip_reader(self._port, self.trace)
        # setting baud rate in a separate step is a workaround for
        # CH341 driver on some Linux versions (this opens at 9600 then
        # sets), shouldn't matter for other platforms/drivers. See
        # https://github.com/espressif/esptool/issues/44#issuecomment-107094446
        self._set_port_baudrate(baud)
        self._trace_enabled = trace_enabled
        # set write timeout, to prevent esptool blocked at write forever.
        try:
            self._port.write_timeout = DEFAULT_SERIAL_WRITE_TIMEOUT
        except NotImplementedError:
            # no write timeout for RFC2217 ports
            # need to set the property back to None or it will continue to fail
            self._port.write_timeout = None

    @property
    def serial_port(self):
        return self._port.port

    def _set_port_baudrate(self, baud):
        try:
            self._port.baudrate = baud
        except IOError:
            raise FatalError("Failed to set baud rate %d. The driver may not support this rate." % baud)

    @staticmethod
    def detect_chip(port=DEFAULT_PORT, baud=ESP_ROM_BAUD, connect_mode='default_reset', trace_enabled=False,
                    connect_attempts=DEFAULT_CONNECT_ATTEMPTS):
        """ Use serial access to detect the chip type.

        We use the UART's datecode register for this, it's mapped at
        the same address on ESP8266 & ESP32 so we can use one
        memory read and compare to the datecode register for each chip
        type.

        This routine automatically performs ESPLoader.connect() (passing
        connect_mode parameter) as part of querying the chip.
        """
        detect_port = ESPLoader(port, baud, trace_enabled=trace_enabled)
        detect_port.connect(connect_mode, connect_attempts, detecting=True)
        try:
            print('Detecting chip type...', end='')
            sys.stdout.flush()
            chip_magic_value = detect_port.read_reg(ESPLoader.CHIP_DETECT_MAGIC_REG_ADDR)

            for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM, ESP32S3BETA2ROM, ESP32C3ROM]:
                if chip_magic_value == cls.CHIP_DETECT_MAGIC_VALUE:
                    # don't connect a second time
                    inst = cls(detect_port._port, baud, trace_enabled=trace_enabled)
                    inst._post_connect()
                    print(' %s' % inst.CHIP_NAME, end='')
                    return inst
        except UnsupportedCommandError:
            raise FatalError("Unsupported Command Error received. Probably this means Secure Download Mode is enabled, "
                             "autodetection will not work. Need to manually specify the chip.")
        finally:
            print('')  # end line
        raise FatalError("Unexpected CHIP magic value 0x%08x. Failed to autodetect chip type." % (chip_magic_value))

    """ Read a SLIP packet from the serial port """
    def read(self):
        return next(self._slip_reader)

    """ Write bytes to the serial port while performing SLIP escaping """
    def write(self, packet):
        buf = b'\xc0' \
              + (packet.replace(b'\xdb', b'\xdb\xdd').replace(b'\xc0', b'\xdb\xdc')) \
              + b'\xc0'
        self.trace("Write %d bytes: %s", len(buf), HexFormatter(buf))
        self._port.write(buf)

    def trace(self, message, *format_args):
        if self._trace_enabled:
            now = time.time()
            try:

                delta = now - self._last_trace
            except AttributeError:
                delta = 0.0
            self._last_trace = now
            prefix = "TRACE +%.3f " % delta
            print(prefix + (message % format_args))

    """ Calculate checksum of a blob, as it is defined by the ROM """
    @staticmethod
    def checksum(data, state=ESP_CHECKSUM_MAGIC):
        for b in data:
            if type(b) is int:  # python 2/3 compat
                state ^= b
            else:
                state ^= ord(b)

        return state

    """ Send a request and read the response """
    def command(self, op=None, data=b"", chk=0, wait_response=True, timeout=DEFAULT_TIMEOUT):
        saved_timeout = self._port.timeout
        new_timeout = min(timeout, MAX_TIMEOUT)
        if new_timeout != saved_timeout:
            self._port.timeout = new_timeout

        try:
            if op is not None:
                self.trace("command op=0x%02x data len=%s wait_response=%d timeout=%.3f data=%s",
                           op, len(data), 1 if wait_response else 0, timeout, HexFormatter(data))
                pkt = struct.pack(b'<BBHI', 0x00, op, len(data), chk) + data
                self.write(pkt)

            if not wait_response:
                return

            self._port.flush()

            # tries to get a response until that response has the
            # same operation as the request or a retries limit has
            # exceeded. This is needed for some esp8266s that
            # reply with more sync responses than expected.
            for retry in range(100):
                p = self.read()
                if len(p) < 8:
                    continue
                (resp, op_ret, len_ret, val) = struct.unpack('<BBHI', p[:8])
                if resp != 1:
                    continue
                data = p[8:]

                if op is None or op_ret == op:
                    return val, data
                if byte(data, 0) != 0 and byte(data, 1) == self.ROM_INVALID_RECV_MSG:
                    self.flush_input()  # Unsupported read_reg can result in more than one error response for some reason
                    raise UnsupportedCommandError(self, op)

        finally:
            if new_timeout != saved_timeout:
                self._port.timeout = saved_timeout

        raise FatalError("Response doesn't match request")

    def check_command(self, op_description, op=None, data=b'', chk=0, timeout=DEFAULT_TIMEOUT):
        """
        Execute a command with 'command', check the result code and throw an appropriate
        FatalError if it fails.

        Returns the "result" of a successful command.
        """
        val, data = self.command(op, data, chk, timeout=timeout)

        # things are a bit weird here, bear with us

        # the status bytes are the last 2/4 bytes in the data (depending on chip)
        if len(data) < self.STATUS_BYTES_LENGTH:
            raise FatalError("Failed to %s. Only got %d byte status response." % (op_description, len(data)))
        status_bytes = data[-self.STATUS_BYTES_LENGTH:]
        # we only care if the first one is non-zero. If it is, the second byte is a reason.
        if byte(status_bytes, 0) != 0:
            raise FatalError.WithResult('Failed to %s' % op_description, status_bytes)

        # if we had more data than just the status bytes, return it as the result
        # (this is used by the md5sum command, maybe other commands?)
        if len(data) > self.STATUS_BYTES_LENGTH:
            return data[:-self.STATUS_BYTES_LENGTH]
        else:  # otherwise, just return the 'val' field which comes from the reply header (this is used by read_reg)
            return val

    def flush_input(self):
        self._port.flushInput()
        self._slip_reader = slip_reader(self._port, self.trace)

    def sync(self):
        self.command(self.ESP_SYNC, b'\x07\x07\x12\x20' + 32 * b'\x55',
                     timeout=SYNC_TIMEOUT)
        for i in range(7):
            self.command()

    def _setDTR(self, state):
        self._port.setDTR(state)

    def _setRTS(self, state):
        self._port.setRTS(state)
        # Work-around for adapters on Windows using the usbser.sys driver:
        # generate a dummy change to DTR so that the set-control-line-state
        # request is sent with the updated RTS state and the same DTR state
        self._port.setDTR(self._port.dtr)

    def _connect_attempt(self, mode='default_reset', esp32r0_delay=False):
        """ A single connection attempt, with esp32r0 workaround options """
        # esp32r0_delay is a workaround for bugs with the most common auto reset
        # circuit and Windows, if the EN pin on the dev board does not have
        # enough capacitance.
        #
        # Newer dev boards shouldn't have this problem (higher value capacitor
        # on the EN pin), and ESP32 revision 1 can't use this workaround as it
        # relies on a silicon bug.
        #
        # Details: https://github.com/espressif/esptool/issues/136
        last_error = None

        # If we're doing no_sync, we're likely communicating as a pass through
        # with an intermediate device to the ESP32
        if mode == "no_reset_no_sync":
            return last_error

        # issue reset-to-bootloader:
        # RTS = either CH_PD/EN or nRESET (both active low = chip in reset
        # DTR = GPIO0 (active low = boot to flasher)
        #
        # DTR & RTS are active low signals,
        # ie True = pin @ 0V, False = pin @ VCC.
        if mode != 'no_reset':
            self._setDTR(False)  # IO0=HIGH
            self._setRTS(True)   # EN=LOW, chip in reset
            time.sleep(0.1)
            if esp32r0_delay:
                # Some chips are more likely to trigger the esp32r0
                # watchdog reset silicon bug if they're held with EN=LOW
                # for a longer period
                time.sleep(1.2)
            self._setDTR(True)   # IO0=LOW
            self._setRTS(False)  # EN=HIGH, chip out of reset
            if esp32r0_delay:
                # Sleep longer after reset.
                # This workaround only works on revision 0 ESP32 chips,
                # it exploits a silicon bug spurious watchdog reset.
                time.sleep(0.4)  # allow watchdog reset to occur
            time.sleep(0.05)
            self._setDTR(False)  # IO0=HIGH, done

        for _ in range(5):
            try:
                self.flush_input()
                self._port.flushOutput()
                self.sync()
                return None
            except FatalError as e:
                if esp32r0_delay:
                    print('_', end='')
                else:
                    print('.', end='')
                sys.stdout.flush()
                time.sleep(0.05)
                last_error = e
        return last_error

    def get_memory_region(self, name):
        """ Returns a tuple of (start, end) for the memory map entry with the given name, or None if it doesn't exist
        """
        try:
            return [(start, end) for (start, end, n) in self.MEMORY_MAP if n == name][0]
        except IndexError:
            return None

    def connect(self, mode='default_reset', attempts=DEFAULT_CONNECT_ATTEMPTS, detecting=False):
        """ Try connecting repeatedly until successful, or giving up """
        print('Connecting...', end='')
        sys.stdout.flush()
        last_error = None

        try:
            for _ in range(attempts) if attempts > 0 else itertools.count():
                last_error = self._connect_attempt(mode=mode, esp32r0_delay=False)
                if last_error is None:
                    break
                last_error = self._connect_attempt(mode=mode, esp32r0_delay=True)
                if last_error is None:
                    break
        finally:
            print('')  # end 'Connecting...' line

        if last_error is not None:
            raise FatalError('Failed to connect to %s: %s' % (self.CHIP_NAME, last_error))

        if not detecting:
            try:
                # check the date code registers match what we expect to see
                chip_magic_value = self.read_reg(ESPLoader.CHIP_DETECT_MAGIC_REG_ADDR)
                if chip_magic_value != self.CHIP_DETECT_MAGIC_VALUE:
                    actually = None
                    for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM, ESP32S3BETA2ROM, ESP32C3ROM]:
                        if chip_magic_value == cls.CHIP_DETECT_MAGIC_VALUE:
                            actually = cls
                            break
                    if actually is None:
                        print(("WARNING: This chip doesn't appear to be a %s (chip magic value 0x%08x). "
                               "Probably it is unsupported by this version of esptool.") % (self.CHIP_NAME, chip_magic_value))
                    else:
                        raise FatalError("This chip is %s not %s. Wrong --chip argument?" % (actually.CHIP_NAME, self.CHIP_NAME))
            except UnsupportedCommandError:
                self.secure_download_mode = True
            self._post_connect()

    def _post_connect(self):
        """
        Additional initialization hook, may be overridden by the chip-specific class.
        Gets called after connect, and after auto-detection.
        """
        pass

    def read_reg(self, addr, timeout=DEFAULT_TIMEOUT):
        """ Read memory address in target """
        # we don't call check_command here because read_reg() function is called
        # when detecting chip type, and the way we check for success (STATUS_BYTES_LENGTH) is different
        # for different chip types (!)
        val, data = self.command(self.ESP_READ_REG, struct.pack('<I', addr), timeout=timeout)
        if byte(data, 0) != 0:
            raise FatalError.WithResult("Failed to read register address %08x" % addr, data)
        return val

    """ Write to memory address in target """
    def write_reg(self, addr, value, mask=0xFFFFFFFF, delay_us=0, delay_after_us=0):
        command = struct.pack('<IIII', addr, value, mask, delay_us)
        if delay_after_us > 0:
            # add a dummy write to a date register as an excuse to have a delay
            command += struct.pack('<IIII', self.UART_DATE_REG_ADDR, 0, 0, delay_after_us)

        return self.check_command("write target memory", self.ESP_WRITE_REG, command)

    def update_reg(self, addr, mask, new_val):
        """ Update register at 'addr', replace the bits masked out by 'mask'
        with new_val. new_val is shifted left to match the LSB of 'mask'

        Returns just-written value of register.
        """
        shift = _mask_to_shift(mask)
        val = self.read_reg(addr)
        val &= ~mask
        val |= (new_val << shift) & mask
        self.write_reg(addr, val)

        return val

    """ Start downloading an application image to RAM """
    def mem_begin(self, size, blocks, blocksize, offset):
        if self.IS_STUB:  # check we're not going to overwrite a running stub with this data
            stub = self.STUB_CODE
            load_start = offset
            load_end = offset + size
            for (start, end) in [(stub["data_start"], stub["data_start"] + len(stub["data"])),
                                 (stub["text_start"], stub["text_start"] + len(stub["text"]))]:
                if load_start < end and load_end > start:
                    raise FatalError(("Software loader is resident at 0x%08x-0x%08x. "
                                      "Can't load binary at overlapping address range 0x%08x-0x%08x. "
                                      "Either change binary loading address, or use the --no-stub "
                                      "option to disable the software loader.") % (start, end, load_start, load_end))

        return self.check_command("enter RAM download mode", self.ESP_MEM_BEGIN,
                                  struct.pack('<IIII', size, blocks, blocksize, offset))

    """ Send a block of an image to RAM """
    def mem_block(self, data, seq):
        return self.check_command("write to target RAM", self.ESP_MEM_DATA,
                                  struct.pack('<IIII', len(data), seq, 0, 0) + data,
                                  self.checksum(data))

    """ Leave download mode and run the application """
    def mem_finish(self, entrypoint=0):
        # Sending ESP_MEM_END usually sends a correct response back, however sometimes
        # (with ROM loader) the executed code may reset the UART or change the baud rate
        # before the transmit FIFO is empty. So in these cases we set a short timeout and
        # ignore errors.
        timeout = DEFAULT_TIMEOUT if self.IS_STUB else MEM_END_ROM_TIMEOUT
        data = struct.pack('<II', int(entrypoint == 0), entrypoint)
        try:
            return self.check_command("leave RAM download mode", self.ESP_MEM_END,
                                      data=data, timeout=timeout)
        except FatalError:
            if self.IS_STUB:
                raise
            pass

    """ Start downloading to Flash (performs an erase)

    Returns number of blocks (of size self.FLASH_WRITE_SIZE) to write.
    """
    def flash_begin(self, size, offset, begin_rom_encrypted=False):
        num_blocks = (size + self.FLASH_WRITE_SIZE - 1) // self.FLASH_WRITE_SIZE
        erase_size = self.get_erase_size(offset, size)

        t = time.time()
        if self.IS_STUB:
            timeout = DEFAULT_TIMEOUT
        else:
            timeout = timeout_per_mb(ERASE_REGION_TIMEOUT_PER_MB, size)  # ROM performs the erase up front

        params = struct.pack('<IIII', erase_size, num_blocks, self.FLASH_WRITE_SIZE, offset)
        if isinstance(self, (ESP32S2ROM, ESP32S3BETA2ROM, ESP32C3ROM)) and not self.IS_STUB:
            params += struct.pack('<I', 1 if begin_rom_encrypted else 0)
        self.check_command("enter Flash download mode", self.ESP_FLASH_BEGIN,
                           params, timeout=timeout)
        if size != 0 and not self.IS_STUB:
            print("Took %.2fs to erase flash block" % (time.time() - t))
        return num_blocks

    """ Write block to flash """
    def flash_block(self, data, seq, timeout=DEFAULT_TIMEOUT):
        self.check_command("write to target Flash after seq %d" % seq,
                           self.ESP_FLASH_DATA,
                           struct.pack('<IIII', len(data), seq, 0, 0) + data,
                           self.checksum(data),
                           timeout=timeout)

    """ Encrypt before writing to flash """
    def flash_encrypt_block(self, data, seq, timeout=DEFAULT_TIMEOUT):
        if isinstance(self, (ESP32S2ROM, ESP32C3ROM)) and not self.IS_STUB:
            # ROM support performs the encrypted writes via the normal write command,
            # triggered by flash_begin(begin_rom_encrypted=True)
            return self.flash_block(data, seq, timeout)

        self.check_command("Write encrypted to target Flash after seq %d" % seq,
                           self.ESP_FLASH_ENCRYPT_DATA,
                           struct.pack('<IIII', len(data), seq, 0, 0) + data,
                           self.checksum(data),
                           timeout=timeout)

    """ Leave flash mode and run/reboot """
    def flash_finish(self, reboot=False):
        pkt = struct.pack('<I', int(not reboot))
        # stub sends a reply to this command
        self.check_command("leave Flash mode", self.ESP_FLASH_END, pkt)

    """ Run application code in flash """
    def run(self, reboot=False):
        # Fake flash begin immediately followed by flash end
        self.flash_begin(0, 0)
        self.flash_finish(reboot)

    """ Read SPI flash manufacturer and device id """
    def flash_id(self):
        SPIFLASH_RDID = 0x9F
        return self.run_spiflash_command(SPIFLASH_RDID, b"", 24)

    def get_security_info(self):
        # TODO: this only works on the ESP32S2 ROM code loader and needs to work in stub loader also
        res = self.check_command('get security info', self.ESP_GET_SECURITY_INFO, b'')
        res = struct.unpack("<IBBBBBBBB", res)
        flags, flash_crypt_cnt, key_purposes = res[0], res[1], res[2:]
        # TODO: pack this as some kind of better data type
        return (flags, flash_crypt_cnt, key_purposes)

    @classmethod
    def parse_flash_size_arg(cls, arg):
        try:
            return cls.FLASH_SIZES[arg]
        except KeyError:
            raise FatalError("Flash size '%s' is not supported by this chip type. Supported sizes: %s"
                             % (arg, ", ".join(cls.FLASH_SIZES.keys())))

    def run_stub(self, stub=None):
        if stub is None:
            if self.IS_STUB:
                raise FatalError("Not possible for a stub to load another stub (memory likely to overlap.)")
            stub = self.STUB_CODE

        # Upload
        print("Uploading stub...")
        for field in ['text', 'data']:
            if field in stub:
                offs = stub[field + "_start"]
                length = len(stub[field])
                blocks = (length + self.ESP_RAM_BLOCK - 1) // self.ESP_RAM_BLOCK
                self.mem_begin(length, blocks, self.ESP_RAM_BLOCK, offs)
                for seq in range(blocks):
                    from_offs = seq * self.ESP_RAM_BLOCK
                    to_offs = from_offs + self.ESP_RAM_BLOCK
                    self.mem_block(stub[field][from_offs:to_offs], seq)
        print("Running stub...")
        self.mem_finish(stub['entry'])

        p = self.read()
        if p != b'OHAI':
            raise FatalError("Failed to start stub. Unexpected response: %s" % p)
        print("Stub running...")
        return self.STUB_CLASS(self)

    @stub_and_esp32_function_only
    def flash_defl_begin(self, size, compsize, offset):
        """ Start downloading compressed data to Flash (performs an erase)

        Returns number of blocks (size self.FLASH_WRITE_SIZE) to write.
        """
        num_blocks = (compsize + self.FLASH_WRITE_SIZE - 1) // self.FLASH_WRITE_SIZE
        erase_blocks = (size + self.FLASH_WRITE_SIZE - 1) // self.FLASH_WRITE_SIZE

        t = time.time()
        if self.IS_STUB:
            write_size = size  # stub expects number of bytes here, manages erasing internally
            timeout = DEFAULT_TIMEOUT
        else:
            write_size = erase_blocks * self.FLASH_WRITE_SIZE  # ROM expects rounded up to erase block size
            timeout = timeout_per_mb(ERASE_REGION_TIMEOUT_PER_MB, write_size)  # ROM performs the erase up front
        print("Compressed %d bytes to %d..." % (size, compsize))
        params = struct.pack('<IIII', write_size, num_blocks, self.FLASH_WRITE_SIZE, offset)
        if isinstance(self, (ESP32S2ROM, ESP32S3BETA2ROM, ESP32C3ROM)) and not self.IS_STUB:
            params += struct.pack('<I', 0)  # extra param is to enter encrypted flash mode via ROM (not supported currently)
        self.check_command("enter compressed flash mode", self.ESP_FLASH_DEFL_BEGIN, params, timeout=timeout)
        if size != 0 and not self.IS_STUB:
            # (stub erases as it writes, but ROM loaders erase on begin)
            print("Took %.2fs to erase flash block" % (time.time() - t))
        return num_blocks

    """ Write block to flash, send compressed """
    @stub_and_esp32_function_only
    def flash_defl_block(self, data, seq, timeout=DEFAULT_TIMEOUT):
        self.check_command("write compressed data to flash after seq %d" % seq,
                           self.ESP_FLASH_DEFL_DATA, struct.pack('<IIII', len(data), seq, 0, 0) + data, self.checksum(data), timeout=timeout)

    """ Leave compressed flash mode and run/reboot """
    @stub_and_esp32_function_only
    def flash_defl_finish(self, reboot=False):
        if not reboot and not self.IS_STUB:
            # skip sending flash_finish to ROM loader, as this
            # exits the bootloader. Stub doesn't do this.
            return
        pkt = struct.pack('<I', int(not reboot))
        self.check_command("leave compressed flash mode", self.ESP_FLASH_DEFL_END, pkt)
        self.in_bootloader = False

    @stub_and_esp32_function_only
    def flash_md5sum(self, addr, size):
        # the MD5 command returns additional bytes in the standard
        # command reply slot
        timeout = timeout_per_mb(MD5_TIMEOUT_PER_MB, size)
        res = self.check_command('calculate md5sum', self.ESP_SPI_FLASH_MD5, struct.pack('<IIII', addr, size, 0, 0),
                                 timeout=timeout)

        if len(res) == 32:
            return res.decode("utf-8")  # already hex formatted
        elif len(res) == 16:
            return hexify(res).lower()
        else:
            raise FatalError("MD5Sum command returned unexpected result: %r" % res)

    @stub_and_esp32_function_only
    def change_baud(self, baud):
        print("Changing baud rate to %d" % baud)
        # stub takes the new baud rate and the old one
        second_arg = self._port.baudrate if self.IS_STUB else 0
        self.command(self.ESP_CHANGE_BAUDRATE, struct.pack('<II', baud, second_arg))
        print("Changed.")
        self._set_port_baudrate(baud)
        time.sleep(0.05)  # get rid of crap sent during baud rate change
        self.flush_input()

    @stub_function_only
    def erase_flash(self):
        # depending on flash chip model the erase may take this long (maybe longer!)
        self.check_command("erase flash", self.ESP_ERASE_FLASH,
                           timeout=CHIP_ERASE_TIMEOUT)

    @stub_function_only
    def erase_region(self, offset, size):
        if offset % self.FLASH_SECTOR_SIZE != 0:
            raise FatalError("Offset to erase from must be a multiple of 4096")
        if size % self.FLASH_SECTOR_SIZE != 0:
            raise FatalError("Size of data to erase must be a multiple of 4096")
        timeout = timeout_per_mb(ERASE_REGION_TIMEOUT_PER_MB, size)
        self.check_command("erase region", self.ESP_ERASE_REGION, struct.pack('<II', offset, size), timeout=timeout)

    def read_flash_slow(self, offset, length, progress_fn):
        raise NotImplementedInROMError(self, self.read_flash_slow)

    def read_flash(self, offset, length, progress_fn=None):
        if not self.IS_STUB:
            return self.read_flash_slow(offset, length, progress_fn)  # ROM-only routine

        # issue a standard bootloader command to trigger the read
        self.check_command("read flash", self.ESP_READ_FLASH,
                           struct.pack('<IIII',
                                       offset,
                                       length,
                                       self.FLASH_SECTOR_SIZE,
                                       64))
        # now we expect (length // block_size) SLIP frames with the data
        data = b''
        while len(data) < length:
            p = self.read()
            data += p
            if len(data) < length and len(p) < self.FLASH_SECTOR_SIZE:
                raise FatalError('Corrupt data, expected 0x%x bytes but received 0x%x bytes' % (self.FLASH_SECTOR_SIZE, len(p)))
            self.write(struct.pack('<I', len(data)))
            if progress_fn and (len(data) % 1024 == 0 or len(data) == length):
                progress_fn(len(data), length)
        if progress_fn:
            progress_fn(len(data), length)
        if len(data) > length:
            raise FatalError('Read more than expected')

        digest_frame = self.read()
        if len(digest_frame) != 16:
            raise FatalError('Expected digest, got: %s' % hexify(digest_frame))
        expected_digest = hexify(digest_frame).upper()
        digest = hashlib.md5(data).hexdigest().upper()
        if digest != expected_digest:
            raise FatalError('Digest mismatch: expected %s, got %s' % (expected_digest, digest))
        return data

    def flash_spi_attach(self, hspi_arg):
        """Send SPI attach command to enable the SPI flash pins

        ESP8266 ROM does this when you send flash_begin, ESP32 ROM
        has it as a SPI command.
        """
        # last 3 bytes in ESP_SPI_ATTACH argument are reserved values
        arg = struct.pack('<I', hspi_arg)
        if not self.IS_STUB:
            # ESP32 ROM loader takes additional 'is legacy' arg, which is not
            # currently supported in the stub loader or esptool.py (as it's not usually needed.)
            is_legacy = 0
            arg += struct.pack('BBBB', is_legacy, 0, 0, 0)
        self.check_command("configure SPI flash pins", ESP32ROM.ESP_SPI_ATTACH, arg)

    def flash_set_parameters(self, size):
        """Tell the ESP bootloader the parameters of the chip

        Corresponds to the "flashchip" data structure that the ROM
        has in RAM.

        'size' is in bytes.

        All other flash parameters are currently hardcoded (on ESP8266
        these are mostly ignored by ROM code, on ESP32 I'm not sure.)
        """
        fl_id = 0
        total_size = size
        block_size = 64 * 1024
        sector_size = 4 * 1024
        page_size = 256
        status_mask = 0xffff
        self.check_command("set SPI params", ESP32ROM.ESP_SPI_SET_PARAMS,
                           struct.pack('<IIIIII', fl_id, total_size, block_size, sector_size, page_size, status_mask))

    def run_spiflash_command(self, spiflash_command, data=b"", read_bits=0):
        """Run an arbitrary SPI flash command.

        This function uses the "USR_COMMAND" functionality in the ESP
        SPI hardware, rather than the precanned commands supported by
        hardware. So the value of spiflash_command is an actual command
        byte, sent over the wire.

        After writing command byte, writes 'data' to MOSI and then
        reads back 'read_bits' of reply on MISO. Result is a number.
        """

        # SPI_USR register flags
        SPI_USR_COMMAND = (1 << 31)
        SPI_USR_MISO    = (1 << 28)
        SPI_USR_MOSI    = (1 << 27)

        # SPI registers, base address differs ESP32* vs 8266
        base = self.SPI_REG_BASE
        SPI_CMD_REG       = base + 0x00
        SPI_USR_REG       = base + self.SPI_USR_OFFS
        SPI_USR1_REG      = base + self.SPI_USR1_OFFS
        SPI_USR2_REG      = base + self.SPI_USR2_OFFS
        SPI_W0_REG        = base + self.SPI_W0_OFFS

        # following two registers are ESP32 & 32S2/32C3 only
        if self.SPI_MOSI_DLEN_OFFS is not None:
            # ESP32/32S2/32C3 has a more sophisticated way to set up "user" commands
            def set_data_lengths(mosi_bits, miso_bits):
                SPI_MOSI_DLEN_REG = base + self.SPI_MOSI_DLEN_OFFS
                SPI_MISO_DLEN_REG = base + self.SPI_MISO_DLEN_OFFS
                if mosi_bits > 0:
                    self.write_reg(SPI_MOSI_DLEN_REG, mosi_bits - 1)
                if miso_bits > 0:
                    self.write_reg(SPI_MISO_DLEN_REG, miso_bits - 1)
        else:

            def set_data_lengths(mosi_bits, miso_bits):
                SPI_DATA_LEN_REG = SPI_USR1_REG
                SPI_MOSI_BITLEN_S = 17
                SPI_MISO_BITLEN_S = 8
                mosi_mask = 0 if (mosi_bits == 0) else (mosi_bits - 1)
                miso_mask = 0 if (miso_bits == 0) else (miso_bits - 1)
                self.write_reg(SPI_DATA_LEN_REG,
                               (miso_mask << SPI_MISO_BITLEN_S) | (
                                   mosi_mask << SPI_MOSI_BITLEN_S))

        # SPI peripheral "command" bitmasks for SPI_CMD_REG
        SPI_CMD_USR  = (1 << 18)

        # shift values
        SPI_USR2_COMMAND_LEN_SHIFT = 28

        if read_bits > 32:
            raise FatalError("Reading more than 32 bits back from a SPI flash operation is unsupported")
        if len(data) > 64:
            raise FatalError("Writing more than 64 bytes of data with one SPI command is unsupported")

        data_bits = len(data) * 8
        old_spi_usr = self.read_reg(SPI_USR_REG)
        old_spi_usr2 = self.read_reg(SPI_USR2_REG)
        flags = SPI_USR_COMMAND
        if read_bits > 0:
            flags |= SPI_USR_MISO
        if data_bits > 0:
            flags |= SPI_USR_MOSI
        set_data_lengths(data_bits, read_bits)
        self.write_reg(SPI_USR_REG, flags)
        self.write_reg(SPI_USR2_REG,
                       (7 << SPI_USR2_COMMAND_LEN_SHIFT) | spiflash_command)
        if data_bits == 0:
            self.write_reg(SPI_W0_REG, 0)  # clear data register before we read it
        else:
            data = pad_to(data, 4, b'\00')  # pad to 32-bit multiple
            words = struct.unpack("I" * (len(data) // 4), data)
            next_reg = SPI_W0_REG
            for word in words:
                self.write_reg(next_reg, word)
                next_reg += 4
        self.write_reg(SPI_CMD_REG, SPI_CMD_USR)

        def wait_done():
            for _ in range(10):
                if (self.read_reg(SPI_CMD_REG) & SPI_CMD_USR) == 0:
                    return
            raise FatalError("SPI command did not complete in time")
        wait_done()

        status = self.read_reg(SPI_W0_REG)
        # restore some SPI controller registers
        self.write_reg(SPI_USR_REG, old_spi_usr)
        self.write_reg(SPI_USR2_REG, old_spi_usr2)
        return status

    def read_status(self, num_bytes=2):
        """Read up to 24 bits (num_bytes) of SPI flash status register contents
        via RDSR, RDSR2, RDSR3 commands

        Not all SPI flash supports all three commands. The upper 1 or 2
        bytes may be 0xFF.
        """
        SPIFLASH_RDSR  = 0x05
        SPIFLASH_RDSR2 = 0x35
        SPIFLASH_RDSR3 = 0x15

        status = 0
        shift = 0
        for cmd in [SPIFLASH_RDSR, SPIFLASH_RDSR2, SPIFLASH_RDSR3][0:num_bytes]:
            status += self.run_spiflash_command(cmd, read_bits=8) << shift
            shift += 8
        return status

    def write_status(self, new_status, num_bytes=2, set_non_volatile=False):
        """Write up to 24 bits (num_bytes) of new status register

        num_bytes can be 1, 2 or 3.

        Not all flash supports the additional commands to write the
        second and third byte of the status register. When writing 2
        bytes, esptool also sends a 16-byte WRSR command (as some
        flash types use this instead of WRSR2.)

        If the set_non_volatile flag is set, non-volatile bits will
        be set as well as volatile ones (WREN used instead of WEVSR).

        """
        SPIFLASH_WRSR = 0x01
        SPIFLASH_WRSR2 = 0x31
        SPIFLASH_WRSR3 = 0x11
        SPIFLASH_WEVSR = 0x50
        SPIFLASH_WREN = 0x06
        SPIFLASH_WRDI = 0x04

        enable_cmd = SPIFLASH_WREN if set_non_volatile else SPIFLASH_WEVSR

        # try using a 16-bit WRSR (not supported by all chips)
        # this may be redundant, but shouldn't hurt
        if num_bytes == 2:
            self.run_spiflash_command(enable_cmd)
            self.run_spiflash_command(SPIFLASH_WRSR, struct.pack("<H", new_status))

        # also try using individual commands (also not supported by all chips for num_bytes 2 & 3)
        for cmd in [SPIFLASH_WRSR, SPIFLASH_WRSR2, SPIFLASH_WRSR3][0:num_bytes]:
            self.run_spiflash_command(enable_cmd)
            self.run_spiflash_command(cmd, struct.pack("B", new_status & 0xFF))
            new_status >>= 8

        self.run_spiflash_command(SPIFLASH_WRDI)

    def get_crystal_freq(self):
        # Figure out the crystal frequency from the UART clock divider
        # Returns a normalized value in integer MHz (40 or 26 are the only supported values)
        #
        # The logic here is:
        # - We know that our baud rate and the ESP UART baud rate are roughly the same, or we couldn't communicate
        # - We can read the UART clock divider register to know how the ESP derives this from the APB bus frequency
        # - Multiplying these two together gives us the bus frequency which is either the crystal frequency (ESP32)
        #   or double the crystal frequency (ESP8266). See the self.XTAL_CLK_DIVIDER parameter for this factor.
        uart_div = self.read_reg(self.UART_CLKDIV_REG) & self.UART_CLKDIV_MASK
        est_xtal = (self._port.baudrate * uart_div) / 1e6 / self.XTAL_CLK_DIVIDER
        norm_xtal = 40 if est_xtal > 33 else 26
        if abs(norm_xtal - est_xtal) > 1:
            print("WARNING: Detected crystal freq %.2fMHz is quite different to normalized freq %dMHz. Unsupported crystal in use?" % (est_xtal, norm_xtal))
        return norm_xtal

    def hard_reset(self):
        self._setRTS(True)  # EN->LOW
        time.sleep(0.1)
        self._setRTS(False)

    def soft_reset(self, stay_in_bootloader):
        if not self.IS_STUB:
            if stay_in_bootloader:
                return  # ROM bootloader is already in bootloader!
            else:
                # 'run user code' is as close to a soft reset as we can do
                self.flash_begin(0, 0)
                self.flash_finish(False)
        else:
            if stay_in_bootloader:
                # soft resetting from the stub loader
                # will re-load the ROM bootloader
                self.flash_begin(0, 0)
                self.flash_finish(True)
            elif self.CHIP_NAME != "ESP8266":
                raise FatalError("Soft resetting is currently only supported on ESP8266")
            else:
                # running user code from stub loader requires some hacks
                # in the stub loader
                self.command(self.ESP_RUN_USER_CODE, wait_response=False)


class ESP8266ROM(ESPLoader):
    """ Access class for ESP8266 ROM bootloader
    """
    CHIP_NAME = "ESP8266"
    IS_STUB = False

    CHIP_DETECT_MAGIC_VALUE = 0xfff0c101

    # OTP ROM addresses
    ESP_OTP_MAC0    = 0x3ff00050
    ESP_OTP_MAC1    = 0x3ff00054
    ESP_OTP_MAC3    = 0x3ff0005c

    SPI_REG_BASE    = 0x60000200
    SPI_USR_OFFS    = 0x1c
    SPI_USR1_OFFS   = 0x20
    SPI_USR2_OFFS   = 0x24
    SPI_MOSI_DLEN_OFFS = None
    SPI_MISO_DLEN_OFFS = None
    SPI_W0_OFFS     = 0x40

    UART_CLKDIV_REG = 0x60000014

    XTAL_CLK_DIVIDER = 2

    FLASH_SIZES = {
        '512KB': 0x00,
        '256KB': 0x10,
        '1MB': 0x20,
        '2MB': 0x30,
        '4MB': 0x40,
        '2MB-c1': 0x50,
        '4MB-c1': 0x60,
        '8MB': 0x80,
        '16MB': 0x90,
    }

    BOOTLOADER_FLASH_OFFSET = 0

    MEMORY_MAP = [[0x3FF00000, 0x3FF00010, "DPORT"],
                  [0x3FFE8000, 0x40000000, "DRAM"],
                  [0x40100000, 0x40108000, "IRAM"],
                  [0x40201010, 0x402E1010, "IROM"]]

    def get_efuses(self):
        # Return the 128 bits of ESP8266 efuse as a single Python integer
        result = self.read_reg(0x3ff0005c) << 96
        result |= self.read_reg(0x3ff00058) << 64
        result |= self.read_reg(0x3ff00054) << 32
        result |= self.read_reg(0x3ff00050)
        return result

    def _get_flash_size(self, efuses):
        # rX_Y = EFUSE_DATA_OUTX[Y]
        r0_4 = (efuses & (1 << 4)) != 0
        r3_25 = (efuses & (1 << 121)) != 0
        r3_26 = (efuses & (1 << 122)) != 0
        r3_27 = (efuses & (1 << 123)) != 0

        if r0_4 and not r3_25:
            if not r3_27 and not r3_26:
                return 1
            elif not r3_27 and r3_26:
                return 2
        if not r0_4 and r3_25:
            if not r3_27 and not r3_26:
                return 2
            elif not r3_27 and r3_26:
                return 4
        return -1

    def get_chip_description(self):
        efuses = self.get_efuses()
        is_8285 = (efuses & ((1 << 4) | 1 << 80)) != 0  # One or the other efuse bit is set for ESP8285
        if is_8285:
            flash_size = self._get_flash_size(efuses)
            max_temp = (efuses & (1 << 5)) != 0  # This efuse bit identifies the max flash temperature
            chip_name = {
                1: "ESP8285H08" if max_temp else "ESP8285N08",
                2: "ESP8285H16" if max_temp else "ESP8285N16"
            }.get(flash_size, "ESP8285")
            return chip_name
        return "ESP8266EX"

    def get_chip_features(self):
        features = ["WiFi"]
        if "ESP8285" in self.get_chip_description():
            features += ["Embedded Flash"]
        return features

    def flash_spi_attach(self, hspi_arg):
        if self.IS_STUB:
            super(ESP8266ROM, self).flash_spi_attach(hspi_arg)
        else:
            # ESP8266 ROM has no flash_spi_attach command in serial protocol,
            # but flash_begin will do it
            self.flash_begin(0, 0)

    def flash_set_parameters(self, size):
        # not implemented in ROM, but OK to silently skip for ROM
        if self.IS_STUB:
            super(ESP8266ROM, self).flash_set_parameters(size)

    def chip_id(self):
        """ Read Chip ID from efuse - the equivalent of the SDK system_get_chip_id() function """
        id0 = self.read_reg(self.ESP_OTP_MAC0)
        id1 = self.read_reg(self.ESP_OTP_MAC1)
        return (id0 >> 24) | ((id1 & MAX_UINT24) << 8)

    def read_mac(self):
        """ Read MAC from OTP ROM """
        mac0 = self.read_reg(self.ESP_OTP_MAC0)
        mac1 = self.read_reg(self.ESP_OTP_MAC1)
        mac3 = self.read_reg(self.ESP_OTP_MAC3)
        if (mac3 != 0):
            oui = ((mac3 >> 16) & 0xff, (mac3 >> 8) & 0xff, mac3 & 0xff)
        elif ((mac1 >> 16) & 0xff) == 0:
            oui = (0x18, 0xfe, 0x34)
        elif ((mac1 >> 16) & 0xff) == 1:
            oui = (0xac, 0xd0, 0x74)
        else:
            raise FatalError("Unknown OUI")
        return oui + ((mac1 >> 8) & 0xff, mac1 & 0xff, (mac0 >> 24) & 0xff)

    def get_erase_size(self, offset, size):
        """ Calculate an erase size given a specific size in bytes.

        Provides a workaround for the bootloader erase bug."""

        sectors_per_block = 16
        sector_size = self.FLASH_SECTOR_SIZE
        num_sectors = (size + sector_size - 1) // sector_size
        start_sector = offset // sector_size

        head_sectors = sectors_per_block - (start_sector % sectors_per_block)
        if num_sectors < head_sectors:
            head_sectors = num_sectors

        if num_sectors < 2 * head_sectors:
            return (num_sectors + 1) // 2 * sector_size
        else:
            return (num_sectors - head_sectors) * sector_size

    def override_vddsdio(self, new_voltage):
        raise NotImplementedInROMError("Overriding VDDSDIO setting only applies to ESP32")


class ESP8266StubLoader(ESP8266ROM):
    """ Access class for ESP8266 stub loader, runs on top of ROM.
    """
    FLASH_WRITE_SIZE = 0x4000  # matches MAX_WRITE_BLOCK in stub_loader.c
    IS_STUB = True

    def __init__(self, rom_loader):
        self.secure_download_mode = rom_loader.secure_download_mode
        self._port = rom_loader._port
        self._trace_enabled = rom_loader._trace_enabled
        self.flush_input()  # resets _slip_reader

    def get_erase_size(self, offset, size):
        return size  # stub doesn't have same size bug as ROM loader


ESP8266ROM.STUB_CLASS = ESP8266StubLoader


class ESP32ROM(ESPLoader):
    """Access class for ESP32 ROM bootloader

    """
    CHIP_NAME = "ESP32"
    IMAGE_CHIP_ID = 0
    IS_STUB = False

    CHIP_DETECT_MAGIC_VALUE = 0x00f01d83

    IROM_MAP_START = 0x400d0000
    IROM_MAP_END   = 0x40400000

    DROM_MAP_START = 0x3F400000
    DROM_MAP_END   = 0x3F800000

    # ESP32 uses a 4 byte status reply
    STATUS_BYTES_LENGTH = 4

    SPI_REG_BASE   = 0x3ff42000
    SPI_USR_OFFS    = 0x1c
    SPI_USR1_OFFS   = 0x20
    SPI_USR2_OFFS   = 0x24
    SPI_MOSI_DLEN_OFFS = 0x28
    SPI_MISO_DLEN_OFFS = 0x2c
    EFUSE_RD_REG_BASE = 0x3ff5a000

    EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT_REG = EFUSE_RD_REG_BASE + 0x18
    EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT = (1 << 7)  # EFUSE_RD_DISABLE_DL_ENCRYPT

    DR_REG_SYSCON_BASE = 0x3ff66000

    SPI_W0_OFFS = 0x80

    UART_CLKDIV_REG = 0x3ff40014

    XTAL_CLK_DIVIDER = 1

    FLASH_SIZES = {
        '1MB': 0x00,
        '2MB': 0x10,
        '4MB': 0x20,
        '8MB': 0x30,
        '16MB': 0x40
    }

    BOOTLOADER_FLASH_OFFSET = 0x1000

    OVERRIDE_VDDSDIO_CHOICES = ["1.8V", "1.9V", "OFF"]

    MEMORY_MAP = [[0x00000000, 0x00010000, "PADDING"],
                  [0x3F400000, 0x3F800000, "DROM"],
                  [0x3F800000, 0x3FC00000, "EXTRAM_DATA"],
                  [0x3FF80000, 0x3FF82000, "RTC_DRAM"],
                  [0x3FF90000, 0x40000000, "BYTE_ACCESSIBLE"],
                  [0x3FFAE000, 0x40000000, "DRAM"],
                  [0x3FFE0000, 0x3FFFFFFC, "DIRAM_DRAM"],
                  [0x40000000, 0x40070000, "IROM"],
                  [0x40070000, 0x40078000, "CACHE_PRO"],
                  [0x40078000, 0x40080000, "CACHE_APP"],
                  [0x40080000, 0x400A0000, "IRAM"],
                  [0x400A0000, 0x400BFFFC, "DIRAM_IRAM"],
                  [0x400C0000, 0x400C2000, "RTC_IRAM"],
                  [0x400D0000, 0x40400000, "IROM"],
                  [0x50000000, 0x50002000, "RTC_DATA"]]

    FLASH_ENCRYPTED_WRITE_ALIGN = 32

    """ Try to read the BLOCK1 (encryption key) and check if it is valid """

    def is_flash_encryption_key_valid(self):

        """ Bit 0 of efuse_rd_disable[3:0] is mapped to BLOCK1
        this bit is at position 16 in EFUSE_BLK0_RDATA0_REG """
        word0 = self.read_efuse(0)
        rd_disable = (word0 >> 16) & 0x1

        # reading of BLOCK1 is NOT ALLOWED so we assume valid key is programmed
        if rd_disable:
            return True
        else:
            # reading of BLOCK1 is ALLOWED so we will read and verify for non-zero.
            # When ESP32 has not generated AES/encryption key in BLOCK1, the contents will be readable and 0.
            # If the flash encryption is enabled it is expected to have a valid non-zero key. We break out on
            # first occurance of non-zero value
            key_word = [0] * 7
            for i in range(len(key_word)):
                key_word[i] = self.read_efuse(14 + i)
                # key is non-zero so break & return
                if key_word[i] != 0:
                    return True
            return False

    def get_flash_crypt_config(self):
        """ For flash encryption related commands we need to make sure
        user has programmed all the relevant efuse correctly so before
        writing encrypted write_flash_encrypt esptool will verify the values
        of flash_crypt_config to be non zero if they are not read
        protected. If the values are zero a warning will be printed

        bit 3 in efuse_rd_disable[3:0] is mapped to flash_crypt_config
        this bit is at position 19 in EFUSE_BLK0_RDATA0_REG """
        word0 = self.read_efuse(0)
        rd_disable = (word0 >> 19) & 0x1

        if rd_disable == 0:
            """ we can read the flash_crypt_config efuse value
            so go & read it (EFUSE_BLK0_RDATA5_REG[31:28]) """
            word5 = self.read_efuse(5)
            word5 = (word5 >> 28) & 0xF
            return word5
        else:
            # if read of the efuse is disabled we assume it is set correctly
            return 0xF

    def get_encrypted_download_disabled(self):
        if self.read_reg(self.EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT_REG) & self.EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT:
            return True
        else:
            return False

    def get_pkg_version(self):
        word3 = self.read_efuse(3)
        pkg_version = (word3 >> 9) & 0x07
        pkg_version += ((word3 >> 2) & 0x1) << 3
        return pkg_version

    def get_chip_revision(self):
        word3 = self.read_efuse(3)
        word5 = self.read_efuse(5)
        apb_ctl_date = self.read_reg(self.DR_REG_SYSCON_BASE + 0x7C)

        rev_bit0 = (word3 >> 15) & 0x1
        rev_bit1 = (word5 >> 20) & 0x1
        rev_bit2 = (apb_ctl_date >> 31) & 0x1
        if rev_bit0:
            if rev_bit1:
                if rev_bit2:
                    return 3
                else:
                    return 2
            else:
                return 1
        return 0

    def get_chip_description(self):
        pkg_version = self.get_pkg_version()
        chip_revision = self.get_chip_revision()
        rev3 = (chip_revision == 3)
        single_core = self.read_efuse(3) & (1 << 0)  # CHIP_VER DIS_APP_CPU

        chip_name = {
            0: "ESP32-S0WDQ6" if single_core else "ESP32-D0WDQ6",
            1: "ESP32-S0WD" if single_core else "ESP32-D0WD",
            2: "ESP32-D2WD",
            4: "ESP32-U4WDH",
            5: "ESP32-PICO-V3" if rev3 else "ESP32-PICO-D4",
            6: "ESP32-PICO-V3-02",
        }.get(pkg_version, "unknown ESP32")

        # ESP32-D0WD-V3, ESP32-D0WDQ6-V3
        if chip_name.startswith("ESP32-D0WD") and rev3:
            chip_name += "-V3"

        return "%s (revision %d)" % (chip_name, chip_revision)

    def get_chip_features(self):
        features = ["WiFi"]
        word3 = self.read_efuse(3)

        # names of variables in this section are lowercase
        #  versions of EFUSE names as documented in TRM and
        # ESP-IDF efuse_reg.h

        chip_ver_dis_bt = word3 & (1 << 1)
        if chip_ver_dis_bt == 0:
            features += ["BT"]

        chip_ver_dis_app_cpu = word3 & (1 << 0)
        if chip_ver_dis_app_cpu:
            features += ["Single Core"]
        else:
            features += ["Dual Core"]

        chip_cpu_freq_rated = word3 & (1 << 13)
        if chip_cpu_freq_rated:
            chip_cpu_freq_low = word3 & (1 << 12)
            if chip_cpu_freq_low:
                features += ["160MHz"]
            else:
                features += ["240MHz"]

        pkg_version = self.get_pkg_version()
        if pkg_version in [2, 4, 5, 6]:
            features += ["Embedded Flash"]

        if pkg_version == 6:
            features += ["Embedded PSRAM"]

        word4 = self.read_efuse(4)
        adc_vref = (word4 >> 8) & 0x1F
        if adc_vref:
            features += ["VRef calibration in efuse"]

        blk3_part_res = word3 >> 14 & 0x1
        if blk3_part_res:
            features += ["BLK3 partially reserved"]

        word6 = self.read_efuse(6)
        coding_scheme = word6 & 0x3
        features += ["Coding Scheme %s" % {
            0: "None",
            1: "3/4",
            2: "Repeat (UNSUPPORTED)",
            3: "Invalid"}[coding_scheme]]

        return features

    def read_efuse(self, n):
        """ Read the nth word of the ESP3x EFUSE region. """
        return self.read_reg(self.EFUSE_RD_REG_BASE + (4 * n))

    def chip_id(self):
        raise NotSupportedError(self, "chip_id")

    def read_mac(self):
        """ Read MAC from EFUSE region """
        words = [self.read_efuse(2), self.read_efuse(1)]
        bitstring = struct.pack(">II", *words)
        bitstring = bitstring[2:8]  # trim the 2 byte CRC
        try:
            return tuple(ord(b) for b in bitstring)
        except TypeError:  # Python 3, bitstring elements are already bytes
            return tuple(bitstring)

    def get_erase_size(self, offset, size):
        return size

    def override_vddsdio(self, new_voltage):
        new_voltage = new_voltage.upper()
        if new_voltage not in self.OVERRIDE_VDDSDIO_CHOICES:
            raise FatalError("The only accepted VDDSDIO overrides are '1.8V', '1.9V' and 'OFF'")
        RTC_CNTL_SDIO_CONF_REG = 0x3ff48074
        RTC_CNTL_XPD_SDIO_REG = (1 << 31)
        RTC_CNTL_DREFH_SDIO_M = (3 << 29)
        RTC_CNTL_DREFM_SDIO_M = (3 << 27)
        RTC_CNTL_DREFL_SDIO_M = (3 << 25)
        # RTC_CNTL_SDIO_TIEH = (1 << 23)  # not used here, setting TIEH=1 would set 3.3V output, not safe for esptool.py to do
        RTC_CNTL_SDIO_FORCE = (1 << 22)
        RTC_CNTL_SDIO_PD_EN = (1 << 21)

        reg_val = RTC_CNTL_SDIO_FORCE  # override efuse setting
        reg_val |= RTC_CNTL_SDIO_PD_EN
        if new_voltage != "OFF":
            reg_val |= RTC_CNTL_XPD_SDIO_REG  # enable internal LDO
        if new_voltage == "1.9V":
            reg_val |= (RTC_CNTL_DREFH_SDIO_M | RTC_CNTL_DREFM_SDIO_M | RTC_CNTL_DREFL_SDIO_M)  # boost voltage
        self.write_reg(RTC_CNTL_SDIO_CONF_REG, reg_val)
        print("VDDSDIO regulator set to %s" % new_voltage)

    def read_flash_slow(self, offset, length, progress_fn):
        BLOCK_LEN = 64  # ROM read limit per command (this limit is why it's so slow)

        data = b''
        while len(data) < length:
            block_len = min(BLOCK_LEN, length - len(data))
            r = self.check_command("read flash block", self.ESP_READ_FLASH_SLOW,
                                   struct.pack('<II', offset + len(data), block_len))
            if len(r) < block_len:
                raise FatalError("Expected %d byte block, got %d bytes. Serial errors?" % (block_len, len(r)))
            data += r[:block_len]  # command always returns 64 byte buffer, regardless of how many bytes were actually read from flash
            if progress_fn and (len(data) % 1024 == 0 or len(data) == length):
                progress_fn(len(data), length)
        return data


class ESP32S2ROM(ESP32ROM):
    CHIP_NAME = "ESP32-S2"
    IMAGE_CHIP_ID = 2

    IROM_MAP_START = 0x40080000
    IROM_MAP_END   = 0x40b80000
    DROM_MAP_START = 0x3F000000
    DROM_MAP_END   = 0x3F3F0000

    CHIP_DETECT_MAGIC_VALUE = 0x000007c6

    SPI_REG_BASE = 0x3f402000
    SPI_USR_OFFS    = 0x18
    SPI_USR1_OFFS   = 0x1c
    SPI_USR2_OFFS   = 0x20
    SPI_MOSI_DLEN_OFFS = 0x24
    SPI_MISO_DLEN_OFFS = 0x28
    SPI_W0_OFFS = 0x58

    MAC_EFUSE_REG = 0x3f41A044  # ESP32-S2 has special block for MAC efuses

    UART_CLKDIV_REG = 0x3f400014

    FLASH_ENCRYPTED_WRITE_ALIGN = 16

    # todo: use espefuse APIs to get this info
    EFUSE_BASE = 0x3f41A000
    EFUSE_RD_REG_BASE = EFUSE_BASE + 0x030  # BLOCK0 read base address

    EFUSE_PURPOSE_KEY0_REG = EFUSE_BASE + 0x34
    EFUSE_PURPOSE_KEY0_SHIFT = 24
    EFUSE_PURPOSE_KEY1_REG = EFUSE_BASE + 0x34
    EFUSE_PURPOSE_KEY1_SHIFT = 28
    EFUSE_PURPOSE_KEY2_REG = EFUSE_BASE + 0x38
    EFUSE_PURPOSE_KEY2_SHIFT = 0
    EFUSE_PURPOSE_KEY3_REG = EFUSE_BASE + 0x38
    EFUSE_PURPOSE_KEY3_SHIFT = 4
    EFUSE_PURPOSE_KEY4_REG = EFUSE_BASE + 0x38
    EFUSE_PURPOSE_KEY4_SHIFT = 8
    EFUSE_PURPOSE_KEY5_REG = EFUSE_BASE + 0x38
    EFUSE_PURPOSE_KEY5_SHIFT = 12

    EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT_REG = EFUSE_RD_REG_BASE
    EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT = 1 << 19

    PURPOSE_VAL_XTS_AES256_KEY_1 = 2
    PURPOSE_VAL_XTS_AES256_KEY_2 = 3
    PURPOSE_VAL_XTS_AES128_KEY = 4

    UARTDEV_BUF_NO = 0x3ffffd14  # Variable in ROM .bss which indicates the port in use
    UARTDEV_BUF_NO_USB = 2  # Value of the above variable indicating that USB is in use

    USB_RAM_BLOCK = 0x800  # Max block size USB CDC is used

    GPIO_STRAP_REG = 0x3f404038
    GPIO_STRAP_SPI_BOOT_MASK = 0x8   # Not download mode
    RTC_CNTL_OPTION1_REG = 0x3f408128
    RTC_CNTL_FORCE_DOWNLOAD_BOOT_MASK = 0x1  # Is download mode forced over USB?

    MEMORY_MAP = [[0x00000000, 0x00010000, "PADDING"],
                  [0x3F000000, 0x3FF80000, "DROM"],
                  [0x3F500000, 0x3FF80000, "EXTRAM_DATA"],
                  [0x3FF9E000, 0x3FFA0000, "RTC_DRAM"],
                  [0x3FF9E000, 0x40000000, "BYTE_ACCESSIBLE"],
                  [0x3FF9E000, 0x40072000, "MEM_INTERNAL"],
                  [0x3FFB0000, 0x40000000, "DRAM"],
                  [0x40000000, 0x4001A100, "IROM_MASK"],
                  [0x40020000, 0x40070000, "IRAM"],
                  [0x40070000, 0x40072000, "RTC_IRAM"],
                  [0x40080000, 0x40800000, "IROM"],
                  [0x50000000, 0x50002000, "RTC_DATA"]]

    def get_pkg_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 21) & 0x0F
        return pkg_version

    def get_chip_description(self):
        chip_name = {
            0: "ESP32-S2",
            1: "ESP32-S2FH16",
            2: "ESP32-S2FH32",
        }.get(self.get_pkg_version(), "unknown ESP32-S2")

        return "%s" % (chip_name)

    def get_chip_features(self):
        features = ["WiFi"]

        if self.secure_download_mode:
            features += ["Secure Download Mode Enabled"]

        pkg_version = self.get_pkg_version()

        if pkg_version in [1, 2]:
            if pkg_version == 1:
                features += ["Embedded 2MB Flash"]
            elif pkg_version == 2:
                features += ["Embedded 4MB Flash"]
            features += ["105C temp rating"]

        num_word = 4
        block2_addr = self.EFUSE_BASE + 0x05C
        word4 = self.read_reg(block2_addr + (4 * num_word))
        block2_version = (word4 >> 4) & 0x07

        if block2_version == 1:
            features += ["ADC and temperature sensor calibration in BLK2 of efuse"]
        return features

    def get_crystal_freq(self):
        # ESP32-S2 XTAL is fixed to 40MHz
        return 40

    def override_vddsdio(self, new_voltage):
        raise NotImplementedInROMError("VDD_SDIO overrides are not supported for ESP32-S2")

    def read_mac(self):
        mac0 = self.read_reg(self.MAC_EFUSE_REG)
        mac1 = self.read_reg(self.MAC_EFUSE_REG + 4)  # only bottom 16 bits are MAC
        bitstring = struct.pack(">II", mac1, mac0)[2:]
        try:
            return tuple(ord(b) for b in bitstring)
        except TypeError:  # Python 3, bitstring elements are already bytes
            return tuple(bitstring)

    def get_flash_crypt_config(self):
        return None  # doesn't exist on ESP32-S2

    def get_key_block_purpose(self, key_block):
        if key_block < 0 or key_block > 5:
            raise FatalError("Valid key block numbers must be in range 0-5")

        reg, shift = [(self.EFUSE_PURPOSE_KEY0_REG, self.EFUSE_PURPOSE_KEY0_SHIFT),
                      (self.EFUSE_PURPOSE_KEY1_REG, self.EFUSE_PURPOSE_KEY1_SHIFT),
                      (self.EFUSE_PURPOSE_KEY2_REG, self.EFUSE_PURPOSE_KEY2_SHIFT),
                      (self.EFUSE_PURPOSE_KEY3_REG, self.EFUSE_PURPOSE_KEY3_SHIFT),
                      (self.EFUSE_PURPOSE_KEY4_REG, self.EFUSE_PURPOSE_KEY4_SHIFT),
                      (self.EFUSE_PURPOSE_KEY5_REG, self.EFUSE_PURPOSE_KEY5_SHIFT)][key_block]
        return (self.read_reg(reg) >> shift) & 0xF

    def is_flash_encryption_key_valid(self):
        # Need to see either an AES-128 key or two AES-256 keys
        purposes = [self.get_key_block_purpose(b) for b in range(6)]

        if any(p == self.PURPOSE_VAL_XTS_AES128_KEY for p in purposes):
            return True

        return any(p == self.PURPOSE_VAL_XTS_AES256_KEY_1 for p in purposes) \
            and any(p == self.PURPOSE_VAL_XTS_AES256_KEY_2 for p in purposes)

    def uses_usb(self, _cache=[]):
        if self.secure_download_mode:
            return False  # can't detect native USB in secure download mode
        if not _cache:
            buf_no = self.read_reg(self.UARTDEV_BUF_NO) & 0xff
            _cache.append(buf_no == self.UARTDEV_BUF_NO_USB)
        return _cache[0]

    def _post_connect(self):
        if self.uses_usb():
            self.ESP_RAM_BLOCK = self.USB_RAM_BLOCK

    def _check_if_can_reset(self):
        """
        Check the strapping register to see if we can reset out of download mode.
        """
        if os.getenv("ESPTOOL_TESTING") is not None:
            print("ESPTOOL_TESTING is set, ignoring strapping mode check")
            # Esptool tests over USB CDC run with GPIO0 strapped low, don't complain in this case.
            return
        strap_reg = self.read_reg(self.GPIO_STRAP_REG)
        force_dl_reg = self.read_reg(self.RTC_CNTL_OPTION1_REG)
        if strap_reg & self.GPIO_STRAP_SPI_BOOT_MASK == 0 and force_dl_reg & self.RTC_CNTL_FORCE_DOWNLOAD_BOOT_MASK == 0:
            print("ERROR: {} chip was placed into download mode using GPIO0.\n"
                  "esptool.py can not exit the download mode over USB. "
                  "To run the app, reset the chip manually.\n"
                  "To suppress this error, set --after option to 'no_reset'.".format(self.get_chip_description()))
            raise SystemExit(1)

    def hard_reset(self):
        if self.uses_usb():
            self._check_if_can_reset()

        self._setRTS(True)  # EN->LOW
        if self.uses_usb():
            # Give the chip some time to come out of reset, to be able to handle further DTR/RTS transitions
            time.sleep(0.2)
            self._setRTS(False)
            time.sleep(0.2)
        else:
            self._setRTS(False)


class ESP32S3BETA2ROM(ESP32ROM):
    CHIP_NAME = "ESP32-S3(beta2)"
    IMAGE_CHIP_ID = 4

    IROM_MAP_START = 0x42000000
    IROM_MAP_END   = 0x44000000
    DROM_MAP_START = 0x3c000000
    DROM_MAP_END   = 0x3e000000

    UART_DATE_REG_ADDR = 0x60000080

    CHIP_DETECT_MAGIC_VALUE = 0xeb004136

    SPI_REG_BASE = 0x60002000
    SPI_USR_OFFS    = 0x18
    SPI_USR1_OFFS   = 0x1c
    SPI_USR2_OFFS   = 0x20
    SPI_MOSI_DLEN_OFFS = 0x24
    SPI_MISO_DLEN_OFFS = 0x28
    SPI_W0_OFFS = 0x58

    EFUSE_REG_BASE = 0x6001A030  # BLOCK0 read base address

    MAC_EFUSE_REG = 0x6001A000  # ESP32S3 has special block for MAC efuses

    UART_CLKDIV_REG = 0x60000014

    GPIO_STRAP_REG = 0x60004038

    MEMORY_MAP = [[0x00000000, 0x00010000, "PADDING"],
                  [0x3C000000, 0x3D000000, "DROM"],
                  [0x3D000000, 0x3E000000, "EXTRAM_DATA"],
                  [0x600FE000, 0x60100000, "RTC_DRAM"],
                  [0x3FC88000, 0x3FD00000, "BYTE_ACCESSIBLE"],
                  [0x3FC88000, 0x403E2000, "MEM_INTERNAL"],
                  [0x3FC88000, 0x3FD00000, "DRAM"],
                  [0x40000000, 0x4001A100, "IROM_MASK"],
                  [0x40370000, 0x403E0000, "IRAM"],
                  [0x600FE000, 0x60100000, "RTC_IRAM"],
                  [0x42000000, 0x42800000, "IROM"],
                  [0x50000000, 0x50002000, "RTC_DATA"]]

    def get_chip_description(self):
        return "ESP32-S3(beta2)"

    def get_chip_features(self):
        return ["WiFi", "BLE"]

    def get_crystal_freq(self):
        # ESP32S3 XTAL is fixed to 40MHz
        return 40

    def override_vddsdio(self, new_voltage):
        raise NotImplementedInROMError("VDD_SDIO overrides are not supported for ESP32-S3")

    def read_mac(self):
        mac0 = self.read_reg(self.MAC_EFUSE_REG)
        mac1 = self.read_reg(self.MAC_EFUSE_REG + 4)  # only bottom 16 bits are MAC
        bitstring = struct.pack(">II", mac1, mac0)[2:]
        try:
            return tuple(ord(b) for b in bitstring)
        except TypeError:  # Python 3, bitstring elements are already bytes
            return tuple(bitstring)


class ESP32C3ROM(ESP32ROM):
    CHIP_NAME = "ESP32-C3"
    IMAGE_CHIP_ID = 5

    IROM_MAP_START = 0x42000000
    IROM_MAP_END   = 0x42800000
    DROM_MAP_START = 0x3c000000
    DROM_MAP_END   = 0x3c800000

    SPI_REG_BASE = 0x60002000
    SPI_USR_OFFS    = 0x18
    SPI_USR1_OFFS   = 0x1C
    SPI_USR2_OFFS   = 0x20
    SPI_MOSI_DLEN_OFFS = 0x24
    SPI_MISO_DLEN_OFFS = 0x28
    SPI_W0_OFFS = 0x58

    BOOTLOADER_FLASH_OFFSET = 0x0

    CHIP_DETECT_MAGIC_VALUE = 0x6921506f

    UART_DATE_REG_ADDR = 0x60000000 + 0x7c

    EFUSE_BASE = 0x60008800
    MAC_EFUSE_REG  = EFUSE_BASE + 0x044

    EFUSE_RD_REG_BASE = EFUSE_BASE + 0x030  # BLOCK0 read base address

    EFUSE_PURPOSE_KEY0_REG = EFUSE_BASE + 0x34
    EFUSE_PURPOSE_KEY0_SHIFT = 24
    EFUSE_PURPOSE_KEY1_REG = EFUSE_BASE + 0x34
    EFUSE_PURPOSE_KEY1_SHIFT = 28
    EFUSE_PURPOSE_KEY2_REG = EFUSE_BASE + 0x38
    EFUSE_PURPOSE_KEY2_SHIFT = 0
    EFUSE_PURPOSE_KEY3_REG = EFUSE_BASE + 0x38
    EFUSE_PURPOSE_KEY3_SHIFT = 4
    EFUSE_PURPOSE_KEY4_REG = EFUSE_BASE + 0x38
    EFUSE_PURPOSE_KEY4_SHIFT = 8
    EFUSE_PURPOSE_KEY5_REG = EFUSE_BASE + 0x38
    EFUSE_PURPOSE_KEY5_SHIFT = 12

    EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT_REG = EFUSE_RD_REG_BASE
    EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT = 1 << 20

    PURPOSE_VAL_XTS_AES128_KEY = 4

    GPIO_STRAP_REG = 0x3f404038

    FLASH_ENCRYPTED_WRITE_ALIGN = 16

    MEMORY_MAP = [[0x00000000, 0x00010000, "PADDING"],
                  [0x3C000000, 0x3C800000, "DROM"],
                  [0x3FC80000, 0x3FCE0000, "DRAM"],
                  [0x3FC88000, 0x3FD00000, "BYTE_ACCESSIBLE"],
                  [0x3FF00000, 0x3FF20000, "DROM_MASK"],
                  [0x40000000, 0x40060000, "IROM_MASK"],
                  [0x42000000, 0x42800000, "IROM"],
                  [0x4037C000, 0x403E0000, "IRAM"],
                  [0x50000000, 0x50002000, "RTC_IRAM"],
                  [0x50000000, 0x50002000, "RTC_DRAM"],
                  [0x600FE000, 0x60100000, "MEM_INTERNAL2"]]

    def get_pkg_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 21) & 0x0F
        return pkg_version

    def get_chip_revision(self):
        # reads WAFER_VERSION field from EFUSE_RD_MAC_SPI_SYS_3_REG
        block1_addr = self.EFUSE_BASE + 0x044
        num_word = 3
        pos = 18
        return (self.read_reg(block1_addr + (4 * num_word)) & (0x7 << pos)) >> pos

    def get_chip_description(self):
        chip_name = {
            0: "ESP32-C3",
        }.get(self.get_pkg_version(), "unknown ESP32-C3")
        chip_revision = self.get_chip_revision()

        return "%s (revision %d)" % (chip_name, chip_revision)

    def get_chip_features(self):
        return ["Wi-Fi"]

    def get_crystal_freq(self):
        # ESP32C3 XTAL is fixed to 40MHz
        return 40

    def override_vddsdio(self, new_voltage):
        raise NotImplementedInROMError("VDD_SDIO overrides are not supported for ESP32-C3")

    def read_mac(self):
        mac0 = self.read_reg(self.MAC_EFUSE_REG)
        mac1 = self.read_reg(self.MAC_EFUSE_REG + 4)  # only bottom 16 bits are MAC
        bitstring = struct.pack(">II", mac1, mac0)[2:]
        try:
            return tuple(ord(b) for b in bitstring)
        except TypeError:  # Python 3, bitstring elements are already bytes
            return tuple(bitstring)

    def get_flash_crypt_config(self):
        return None  # doesn't exist on ESP32-C3

    def get_key_block_purpose(self, key_block):
        if key_block < 0 or key_block > 5:
            raise FatalError("Valid key block numbers must be in range 0-5")

        reg, shift = [(self.EFUSE_PURPOSE_KEY0_REG, self.EFUSE_PURPOSE_KEY0_SHIFT),
                      (self.EFUSE_PURPOSE_KEY1_REG, self.EFUSE_PURPOSE_KEY1_SHIFT),
                      (self.EFUSE_PURPOSE_KEY2_REG, self.EFUSE_PURPOSE_KEY2_SHIFT),
                      (self.EFUSE_PURPOSE_KEY3_REG, self.EFUSE_PURPOSE_KEY3_SHIFT),
                      (self.EFUSE_PURPOSE_KEY4_REG, self.EFUSE_PURPOSE_KEY4_SHIFT),
                      (self.EFUSE_PURPOSE_KEY5_REG, self.EFUSE_PURPOSE_KEY5_SHIFT)][key_block]
        return (self.read_reg(reg) >> shift) & 0xF

    def is_flash_encryption_key_valid(self):
        # Need to see an AES-128 key
        purposes = [self.get_key_block_purpose(b) for b in range(6)]

        return any(p == self.PURPOSE_VAL_XTS_AES128_KEY for p in purposes)


class ESP32StubLoader(ESP32ROM):
    """ Access class for ESP32 stub loader, runs on top of ROM.
    """
    FLASH_WRITE_SIZE = 0x4000  # matches MAX_WRITE_BLOCK in stub_loader.c
    STATUS_BYTES_LENGTH = 2  # same as ESP8266, different to ESP32 ROM
    IS_STUB = True

    def __init__(self, rom_loader):
        self.secure_download_mode = rom_loader.secure_download_mode
        self._port = rom_loader._port
        self._trace_enabled = rom_loader._trace_enabled
        self.flush_input()  # resets _slip_reader


ESP32ROM.STUB_CLASS = ESP32StubLoader


class ESP32S2StubLoader(ESP32S2ROM):
    """ Access class for ESP32-S2 stub loader, runs on top of ROM.

    (Basically the same as ESP32StubLoader, but different base class.
    Can possibly be made into a mixin.)
    """
    FLASH_WRITE_SIZE = 0x4000  # matches MAX_WRITE_BLOCK in stub_loader.c
    STATUS_BYTES_LENGTH = 2  # same as ESP8266, different to ESP32 ROM
    IS_STUB = True

    def __init__(self, rom_loader):
        self.secure_download_mode = rom_loader.secure_download_mode
        self._port = rom_loader._port
        self._trace_enabled = rom_loader._trace_enabled
        self.flush_input()  # resets _slip_reader

        if rom_loader.uses_usb():
            self.ESP_RAM_BLOCK = self.USB_RAM_BLOCK
            self.FLASH_WRITE_SIZE = self.USB_RAM_BLOCK


ESP32S2ROM.STUB_CLASS = ESP32S2StubLoader


class ESP32S3BETA2StubLoader(ESP32S3BETA2ROM):
    """ Access class for ESP32S3 stub loader, runs on top of ROM.

    (Basically the same as ESP32StubLoader, but different base class.
    Can possibly be made into a mixin.)
    """
    FLASH_WRITE_SIZE = 0x4000  # matches MAX_WRITE_BLOCK in stub_loader.c
    STATUS_BYTES_LENGTH = 2  # same as ESP8266, different to ESP32 ROM
    IS_STUB = True

    def __init__(self, rom_loader):
        self.secure_download_mode = rom_loader.secure_download_mode
        self._port = rom_loader._port
        self._trace_enabled = rom_loader._trace_enabled
        self.flush_input()  # resets _slip_reader


ESP32S3BETA2ROM.STUB_CLASS = ESP32S3BETA2StubLoader


class ESP32C3StubLoader(ESP32C3ROM):
    """ Access class for ESP32C3 stub loader, runs on top of ROM.

    (Basically the same as ESP32StubLoader, but different base class.
    Can possibly be made into a mixin.)
    """
    FLASH_WRITE_SIZE = 0x4000  # matches MAX_WRITE_BLOCK in stub_loader.c
    STATUS_BYTES_LENGTH = 2  # same as ESP8266, different to ESP32 ROM
    IS_STUB = True

    def __init__(self, rom_loader):
        self.secure_download_mode = rom_loader.secure_download_mode
        self._port = rom_loader._port
        self._trace_enabled = rom_loader._trace_enabled
        self.flush_input()  # resets _slip_reader


ESP32C3ROM.STUB_CLASS = ESP32C3StubLoader


class ESPBOOTLOADER(object):
    """ These are constants related to software ESP8266 bootloader, working with 'v2' image files """

    # First byte of the "v2" application image
    IMAGE_V2_MAGIC = 0xea

    # First 'segment' value in a "v2" application image, appears to be a constant version value?
    IMAGE_V2_SEGMENT = 4


def LoadFirmwareImage(chip, filename):
    """ Load a firmware image. Can be for any supported SoC.

        ESP8266 images will be examined to determine if they are original ROM firmware images (ESP8266ROMFirmwareImage)
        or "v2" OTA bootloader images.

        Returns a BaseFirmwareImage subclass, either ESP8266ROMFirmwareImage (v1) or ESP8266V2FirmwareImage (v2).
    """
    chip = chip.lower().replace("-", "")
    with open(filename, 'rb') as f:
        if chip == 'esp32':
            return ESP32FirmwareImage(f)
        elif chip == "esp32s2":
            return ESP32S2FirmwareImage(f)
        elif chip == "esp32s3beta2":
            return ESP32S3BETA2FirmwareImage(f)
        elif chip == 'esp32c3':
            return ESP32C3FirmwareImage(f)
        else:  # Otherwise, ESP8266 so look at magic to determine the image type
            magic = ord(f.read(1))
            f.seek(0)
            if magic == ESPLoader.ESP_IMAGE_MAGIC:
                return ESP8266ROMFirmwareImage(f)
            elif magic == ESPBOOTLOADER.IMAGE_V2_MAGIC:
                return ESP8266V2FirmwareImage(f)
            else:
                raise FatalError("Invalid image magic number: %d" % magic)


class ImageSegment(object):
    """ Wrapper class for a segment in an ESP image
    (very similar to a section in an ELFImage also) """
    def __init__(self, addr, data, file_offs=None):
        self.addr = addr
        self.data = data
        self.file_offs = file_offs
        self.include_in_checksum = True
        if self.addr != 0:
            self.pad_to_alignment(4)  # pad all "real" ImageSegments 4 byte aligned length

    def copy_with_new_addr(self, new_addr):
        """ Return a new ImageSegment with same data, but mapped at
        a new address. """
        return ImageSegment(new_addr, self.data, 0)

    def split_image(self, split_len):
        """ Return a new ImageSegment which splits "split_len" bytes
        from the beginning of the data. Remaining bytes are kept in
        this segment object (and the start address is adjusted to match.) """
        result = copy.copy(self)
        result.data = self.data[:split_len]
        self.data = self.data[split_len:]
        self.addr += split_len
        self.file_offs = None
        result.file_offs = None
        return result

    def __repr__(self):
        r = "len 0x%05x load 0x%08x" % (len(self.data), self.addr)
        if self.file_offs is not None:
            r += " file_offs 0x%08x" % (self.file_offs)
        return r

    def get_memory_type(self, image):
        """
        Return a list describing the memory type(s) that is covered by this
        segment's start address.
        """
        return [map_range[2] for map_range in image.ROM_LOADER.MEMORY_MAP if map_range[0] <= self.addr < map_range[1]]

    def pad_to_alignment(self, alignment):
        self.data = pad_to(self.data, alignment, b'\x00')


class ELFSection(ImageSegment):
    """ Wrapper class for a section in an ELF image, has a section
    name as well as the common properties of an ImageSegment. """
    def __init__(self, name, addr, data):
        super(ELFSection, self).__init__(addr, data)
        self.name = name.decode("utf-8")

    def __repr__(self):
        return "%s %s" % (self.name, super(ELFSection, self).__repr__())


class BaseFirmwareImage(object):
    SEG_HEADER_LEN = 8
    SHA256_DIGEST_LEN = 32

    """ Base class with common firmware image functions """
    def __init__(self):
        self.segments = []
        self.entrypoint = 0
        self.elf_sha256 = None
        self.elf_sha256_offset = 0

    def load_common_header(self, load_file, expected_magic):
        (magic, segments, self.flash_mode, self.flash_size_freq, self.entrypoint) = struct.unpack('<BBBBI', load_file.read(8))

        if magic != expected_magic:
            raise FatalError('Invalid firmware image magic=0x%x' % (magic))
        return segments

    def verify(self):
        if len(self.segments) > 16:
            raise FatalError('Invalid segment count %d (max 16). Usually this indicates a linker script problem.' % len(self.segments))

    def load_segment(self, f, is_irom_segment=False):
        """ Load the next segment from the image file """
        file_offs = f.tell()
        (offset, size) = struct.unpack('<II', f.read(8))
        self.warn_if_unusual_segment(offset, size, is_irom_segment)
        segment_data = f.read(size)
        if len(segment_data) < size:
            raise FatalError('End of file reading segment 0x%x, length %d (actual length %d)' % (offset, size, len(segment_data)))
        segment = ImageSegment(offset, segment_data, file_offs)
        self.segments.append(segment)
        return segment

    def warn_if_unusual_segment(self, offset, size, is_irom_segment):
        if not is_irom_segment:
            if offset > 0x40200000 or offset < 0x3ffe0000 or size > 65536:
                print('WARNING: Suspicious segment 0x%x, length %d' % (offset, size))

    def maybe_patch_segment_data(self, f, segment_data):
        """If SHA256 digest of the ELF file needs to be inserted into this segment, do so. Returns segment data."""
        segment_len = len(segment_data)
        file_pos = f.tell()  # file_pos is position in the .bin file
        if self.elf_sha256_offset >= file_pos and self.elf_sha256_offset < file_pos + segment_len:
            # SHA256 digest needs to be patched into this binary segment,
            # calculate offset of the digest inside the binary segment.
            patch_offset = self.elf_sha256_offset - file_pos
            # Sanity checks
            if patch_offset < self.SEG_HEADER_LEN or patch_offset + self.SHA256_DIGEST_LEN > segment_len:
                raise FatalError('Cannot place SHA256 digest on segment boundary'
                                 '(elf_sha256_offset=%d, file_pos=%d, segment_size=%d)' %
                                 (self.elf_sha256_offset, file_pos, segment_len))
            # offset relative to the data part
            patch_offset -= self.SEG_HEADER_LEN
            if segment_data[patch_offset:patch_offset + self.SHA256_DIGEST_LEN] != b'\x00' * self.SHA256_DIGEST_LEN:
                raise FatalError('Contents of segment at SHA256 digest offset 0x%x are not all zero. Refusing to overwrite.' %
                                 self.elf_sha256_offset)
            assert(len(self.elf_sha256) == self.SHA256_DIGEST_LEN)
            segment_data = segment_data[0:patch_offset] + self.elf_sha256 + \
                segment_data[patch_offset + self.SHA256_DIGEST_LEN:]
        return segment_data

    def save_segment(self, f, segment, checksum=None):
        """ Save the next segment to the image file, return next checksum value if provided """
        segment_data = self.maybe_patch_segment_data(f, segment.data)
        f.write(struct.pack('<II', segment.addr, len(segment_data)))
        f.write(segment_data)
        if checksum is not None:
            return ESPLoader.checksum(segment_data, checksum)

    def read_checksum(self, f):
        """ Return ESPLoader checksum from end of just-read image """
        # Skip the padding. The checksum is stored in the last byte so that the
        # file is a multiple of 16 bytes.
        align_file_position(f, 16)
        return ord(f.read(1))

    def calculate_checksum(self):
        """ Calculate checksum of loaded image, based on segments in
        segment array.
        """
        checksum = ESPLoader.ESP_CHECKSUM_MAGIC
        for seg in self.segments:
            if seg.include_in_checksum:
                checksum = ESPLoader.checksum(seg.data, checksum)
        return checksum

    def append_checksum(self, f, checksum):
        """ Append ESPLoader checksum to the just-written image """
        align_file_position(f, 16)
        f.write(struct.pack(b'B', checksum))

    def write_common_header(self, f, segments):
        f.write(struct.pack('<BBBBI', ESPLoader.ESP_IMAGE_MAGIC, len(segments),
                            self.flash_mode, self.flash_size_freq, self.entrypoint))

    def is_irom_addr(self, addr):
        """ Returns True if an address starts in the irom region.
        Valid for ESP8266 only.
        """
        return ESP8266ROM.IROM_MAP_START <= addr < ESP8266ROM.IROM_MAP_END

    def get_irom_segment(self):
        irom_segments = [s for s in self.segments if self.is_irom_addr(s.addr)]
        if len(irom_segments) > 0:
            if len(irom_segments) != 1:
                raise FatalError('Found %d segments that could be irom0. Bad ELF file?' % len(irom_segments))
            return irom_segments[0]
        return None

    def get_non_irom_segments(self):
        irom_segment = self.get_irom_segment()
        return [s for s in self.segments if s != irom_segment]

    def merge_adjacent_segments(self):
        if not self.segments:
            return  # nothing to merge

        segments = []
        # The easiest way to merge the sections is the browse them backward.
        for i in range(len(self.segments) - 1, 0, -1):
            # elem is the previous section, the one `next_elem` may need to be
            # merged in
            elem = self.segments[i - 1]
            next_elem = self.segments[i]
            if all((elem.get_memory_type(self) == next_elem.get_memory_type(self),
                    elem.include_in_checksum == next_elem.include_in_checksum,
                    next_elem.addr == elem.addr + len(elem.data))):
                # Merge any segment that ends where the next one starts, without spanning memory types
                #
                # (don't 'pad' any gaps here as they may be excluded from the image due to 'noinit'
                # or other reasons.)
                elem.data += next_elem.data
            else:
                # The section next_elem cannot be merged into the previous one,
                # which means it needs to be part of the final segments.
                # As we are browsing the list backward, the elements need to be
                # inserted at the beginning of the final list.
                segments.insert(0, next_elem)

        # The first segment will always be here as it cannot be merged into any
        # "previous" section.
        segments.insert(0, self.segments[0])

        # note: we could sort segments here as well, but the ordering of segments is sometimes
        # important for other reasons (like embedded ELF SHA-256), so we assume that the linker
        # script will have produced any adjacent sections in linear order in the ELF, anyhow.
        self.segments = segments


class ESP8266ROMFirmwareImage(BaseFirmwareImage):
    """ 'Version 1' firmware image, segments loaded directly by the ROM bootloader. """

    ROM_LOADER = ESP8266ROM

    def __init__(self, load_file=None):
        super(ESP8266ROMFirmwareImage, self).__init__()
        self.flash_mode = 0
        self.flash_size_freq = 0
        self.version = 1

        if load_file is not None:
            segments = self.load_common_header(load_file, ESPLoader.ESP_IMAGE_MAGIC)

            for _ in range(segments):
                self.load_segment(load_file)
            self.checksum = self.read_checksum(load_file)

            self.verify()

    def default_output_name(self, input_file):
        """ Derive a default output name from the ELF name. """
        return input_file + '-'

    def save(self, basename):
        """ Save a set of V1 images for flashing. Parameter is a base filename. """
        # IROM data goes in its own plain binary file
        irom_segment = self.get_irom_segment()
        if irom_segment is not None:
            with open("%s0x%05x.bin" % (basename, irom_segment.addr - ESP8266ROM.IROM_MAP_START), "wb") as f:
                f.write(irom_segment.data)

        # everything but IROM goes at 0x00000 in an image file
        normal_segments = self.get_non_irom_segments()
        with open("%s0x00000.bin" % basename, 'wb') as f:
            self.write_common_header(f, normal_segments)
            checksum = ESPLoader.ESP_CHECKSUM_MAGIC
            for segment in normal_segments:
                checksum = self.save_segment(f, segment, checksum)
            self.append_checksum(f, checksum)


ESP8266ROM.BOOTLOADER_IMAGE = ESP8266ROMFirmwareImage


class ESP8266V2FirmwareImage(BaseFirmwareImage):
    """ 'Version 2' firmware image, segments loaded by software bootloader stub
        (ie Espressif bootloader or rboot)
    """

    ROM_LOADER = ESP8266ROM

    def __init__(self, load_file=None):
        super(ESP8266V2FirmwareImage, self).__init__()
        self.version = 2
        if load_file is not None:
            segments = self.load_common_header(load_file, ESPBOOTLOADER.IMAGE_V2_MAGIC)
            if segments != ESPBOOTLOADER.IMAGE_V2_SEGMENT:
                # segment count is not really segment count here, but we expect to see '4'
                print('Warning: V2 header has unexpected "segment" count %d (usually 4)' % segments)

            # irom segment comes before the second header
            #
            # the file is saved in the image with a zero load address
            # in the header, so we need to calculate a load address
            irom_segment = self.load_segment(load_file, True)
            irom_segment.addr = 0  # for actual mapped addr, add ESP8266ROM.IROM_MAP_START + flashing_addr + 8
            irom_segment.include_in_checksum = False

            first_flash_mode = self.flash_mode
            first_flash_size_freq = self.flash_size_freq
            first_entrypoint = self.entrypoint
            # load the second header

            segments = self.load_common_header(load_file, ESPLoader.ESP_IMAGE_MAGIC)

            if first_flash_mode != self.flash_mode:
                print('WARNING: Flash mode value in first header (0x%02x) disagrees with second (0x%02x). Using second value.'
                      % (first_flash_mode, self.flash_mode))
            if first_flash_size_freq != self.flash_size_freq:
                print('WARNING: Flash size/freq value in first header (0x%02x) disagrees with second (0x%02x). Using second value.'
                      % (first_flash_size_freq, self.flash_size_freq))
            if first_entrypoint != self.entrypoint:
                print('WARNING: Entrypoint address in first header (0x%08x) disagrees with second header (0x%08x). Using second value.'
                      % (first_entrypoint, self.entrypoint))

            # load all the usual segments
            for _ in range(segments):
                self.load_segment(load_file)
            self.checksum = self.read_checksum(load_file)

            self.verify()

    def default_output_name(self, input_file):
        """ Derive a default output name from the ELF name. """
        irom_segment = self.get_irom_segment()
        if irom_segment is not None:
            irom_offs = irom_segment.addr - ESP8266ROM.IROM_MAP_START
        else:
            irom_offs = 0
        return "%s-0x%05x.bin" % (os.path.splitext(input_file)[0],
                                  irom_offs & ~(ESPLoader.FLASH_SECTOR_SIZE - 1))

    def save(self, filename):
        with open(filename, 'wb') as f:
            # Save first header for irom0 segment
            f.write(struct.pack(b'<BBBBI', ESPBOOTLOADER.IMAGE_V2_MAGIC, ESPBOOTLOADER.IMAGE_V2_SEGMENT,
                                self.flash_mode, self.flash_size_freq, self.entrypoint))

            irom_segment = self.get_irom_segment()
            if irom_segment is not None:
                # save irom0 segment, make sure it has load addr 0 in the file
                irom_segment = irom_segment.copy_with_new_addr(0)
                irom_segment.pad_to_alignment(16)  # irom_segment must end on a 16 byte boundary
                self.save_segment(f, irom_segment)

            # second header, matches V1 header and contains loadable segments
            normal_segments = self.get_non_irom_segments()
            self.write_common_header(f, normal_segments)
            checksum = ESPLoader.ESP_CHECKSUM_MAGIC
            for segment in normal_segments:
                checksum = self.save_segment(f, segment, checksum)
            self.append_checksum(f, checksum)

        # calculate a crc32 of entire file and append
        # (algorithm used by recent 8266 SDK bootloaders)
        with open(filename, 'rb') as f:
            crc = esp8266_crc32(f.read())
        with open(filename, 'ab') as f:
            f.write(struct.pack(b'<I', crc))


# Backwards compatibility for previous API, remove in esptool.py V3
ESPFirmwareImage = ESP8266ROMFirmwareImage
OTAFirmwareImage = ESP8266V2FirmwareImage


def esp8266_crc32(data):
    """
    CRC32 algorithm used by 8266 SDK bootloader (and gen_appbin.py).
    """
    crc = binascii.crc32(data, 0) & 0xFFFFFFFF
    if crc & 0x80000000:
        return crc ^ 0xFFFFFFFF
    else:
        return crc + 1


class ESP32FirmwareImage(BaseFirmwareImage):
    """ ESP32 firmware image is very similar to V1 ESP8266 image,
    except with an additional 16 byte reserved header at top of image,
    and because of new flash mapping capabilities the flash-mapped regions
    can be placed in the normal image (just @ 64kB padded offsets).
    """

    ROM_LOADER = ESP32ROM

    # ROM bootloader will read the wp_pin field if SPI flash
    # pins are remapped via flash. IDF actually enables QIO only
    # from software bootloader, so this can be ignored. But needs
    # to be set to this value so ROM bootloader will skip it.
    WP_PIN_DISABLED = 0xEE

    EXTENDED_HEADER_STRUCT_FMT = "<BBBBHB" + ("B" * 8) + "B"

    IROM_ALIGN = 65536

    def __init__(self, load_file=None):
        super(ESP32FirmwareImage, self).__init__()
        self.secure_pad = None
        self.flash_mode = 0
        self.flash_size_freq = 0
        self.version = 1
        self.wp_pin = self.WP_PIN_DISABLED
        # SPI pin drive levels
        self.clk_drv = 0
        self.q_drv = 0
        self.d_drv = 0
        self.cs_drv = 0
        self.hd_drv = 0
        self.wp_drv = 0
        self.min_rev = 0

        self.append_digest = True

        if load_file is not None:
            start = load_file.tell()

            segments = self.load_common_header(load_file, ESPLoader.ESP_IMAGE_MAGIC)
            self.load_extended_header(load_file)

            for _ in range(segments):
                self.load_segment(load_file)
            self.checksum = self.read_checksum(load_file)

            if self.append_digest:
                end = load_file.tell()
                self.stored_digest = load_file.read(32)
                load_file.seek(start)
                calc_digest = hashlib.sha256()
                calc_digest.update(load_file.read(end - start))
                self.calc_digest = calc_digest.digest()  # TODO: decide what to do here?

            self.verify()

    def is_flash_addr(self, addr):
        return (self.ROM_LOADER.IROM_MAP_START <= addr < self.ROM_LOADER.IROM_MAP_END) \
            or (self.ROM_LOADER.DROM_MAP_START <= addr < self.ROM_LOADER.DROM_MAP_END)

    def default_output_name(self, input_file):
        """ Derive a default output name from the ELF name. """
        return "%s.bin" % (os.path.splitext(input_file)[0])

    def warn_if_unusual_segment(self, offset, size, is_irom_segment):
        pass  # TODO: add warnings for ESP32 segment offset/size combinations that are wrong

    def save(self, filename):
        total_segments = 0
        with io.BytesIO() as f:  # write file to memory first
            self.write_common_header(f, self.segments)

            # first 4 bytes of header are read by ROM bootloader for SPI
            # config, but currently unused
            self.save_extended_header(f)

            checksum = ESPLoader.ESP_CHECKSUM_MAGIC

            # split segments into flash-mapped vs ram-loaded, and take copies so we can mutate them
            flash_segments = [copy.deepcopy(s) for s in sorted(self.segments, key=lambda s:s.addr) if self.is_flash_addr(s.addr)]
            ram_segments = [copy.deepcopy(s) for s in sorted(self.segments, key=lambda s:s.addr) if not self.is_flash_addr(s.addr)]

            # check for multiple ELF sections that are mapped in the same flash mapping region.
            # this is usually a sign of a broken linker script, but if you have a legitimate
            # use case then let us know
            if len(flash_segments) > 0:
                last_addr = flash_segments[0].addr
                for segment in flash_segments[1:]:
                    if segment.addr // self.IROM_ALIGN == last_addr // self.IROM_ALIGN:
                        raise FatalError(("Segment loaded at 0x%08x lands in same 64KB flash mapping as segment loaded at 0x%08x. "
                                          "Can't generate binary. Suggest changing linker script or ELF to merge sections.") %
                                         (segment.addr, last_addr))
                    last_addr = segment.addr

            def get_alignment_data_needed(segment):
                # Actual alignment (in data bytes) required for a segment header: positioned so that
                # after we write the next 8 byte header, file_offs % IROM_ALIGN == segment.addr % IROM_ALIGN
                #
                # (this is because the segment's vaddr may not be IROM_ALIGNed, more likely is aligned
                # IROM_ALIGN+0x18 to account for the binary file header
                align_past = (segment.addr % self.IROM_ALIGN) - self.SEG_HEADER_LEN
                pad_len = (self.IROM_ALIGN - (f.tell() % self.IROM_ALIGN)) + align_past
                if pad_len == 0 or pad_len == self.IROM_ALIGN:
                    return 0  # already aligned

                # subtract SEG_HEADER_LEN a second time, as the padding block has a header as well
                pad_len -= self.SEG_HEADER_LEN
                if pad_len < 0:
                    pad_len += self.IROM_ALIGN
                return pad_len

            # try to fit each flash segment on a 64kB aligned boundary
            # by padding with parts of the non-flash segments...
            while len(flash_segments) > 0:
                segment = flash_segments[0]
                pad_len = get_alignment_data_needed(segment)
                if pad_len > 0:  # need to pad
                    if len(ram_segments) > 0 and pad_len > self.SEG_HEADER_LEN:
                        pad_segment = ram_segments[0].split_image(pad_len)
                        if len(ram_segments[0].data) == 0:
                            ram_segments.pop(0)
                    else:
                        pad_segment = ImageSegment(0, b'\x00' * pad_len, f.tell())
                    checksum = self.save_segment(f, pad_segment, checksum)
                    total_segments += 1
                else:
                    # write the flash segment
                    assert (f.tell() + 8) % self.IROM_ALIGN == segment.addr % self.IROM_ALIGN
                    checksum = self.save_flash_segment(f, segment, checksum)
                    flash_segments.pop(0)
                    total_segments += 1

            # flash segments all written, so write any remaining RAM segments
            for segment in ram_segments:
                checksum = self.save_segment(f, segment, checksum)
                total_segments += 1

            if self.secure_pad:
                # pad the image so that after signing it will end on a a 64KB boundary.
                # This ensures all mapped flash content will be verified.
                if not self.append_digest:
                    raise FatalError("secure_pad only applies if a SHA-256 digest is also appended to the image")
                align_past = (f.tell() + self.SEG_HEADER_LEN) % self.IROM_ALIGN
                # 16 byte aligned checksum (force the alignment to simplify calculations)
                checksum_space = 16
                if self.secure_pad == '1':
                    # after checksum: SHA-256 digest + (to be added by signing process) version, signature + 12 trailing bytes due to alignment
                    space_after_checksum = 32 + 4 + 64 + 12
                elif self.secure_pad == '2':  # Secure Boot V2
                    # after checksum: SHA-256 digest + signature sector, but we place signature sector after the 64KB boundary
                    space_after_checksum = 32
                pad_len = (self.IROM_ALIGN - align_past - checksum_space - space_after_checksum) % self.IROM_ALIGN
                pad_segment = ImageSegment(0, b'\x00' * pad_len, f.tell())

                checksum = self.save_segment(f, pad_segment, checksum)
                total_segments += 1

            # done writing segments
            self.append_checksum(f, checksum)
            image_length = f.tell()

            if self.secure_pad:
                assert ((image_length + space_after_checksum) % self.IROM_ALIGN) == 0

            # kinda hacky: go back to the initial header and write the new segment count
            # that includes padding segments. This header is not checksummed
            f.seek(1)
            try:
                f.write(chr(total_segments))
            except TypeError:  # Python 3
                f.write(bytes([total_segments]))

            if self.append_digest:
                # calculate the SHA256 of the whole file and append it
                f.seek(0)
                digest = hashlib.sha256()
                digest.update(f.read(image_length))
                f.write(digest.digest())

            with open(filename, 'wb') as real_file:
                real_file.write(f.getvalue())

    def save_flash_segment(self, f, segment, checksum=None):
        """ Save the next segment to the image file, return next checksum value if provided """
        segment_end_pos = f.tell() + len(segment.data) + self.SEG_HEADER_LEN
        segment_len_remainder = segment_end_pos % self.IROM_ALIGN
        if segment_len_remainder < 0x24:
            # Work around a bug in ESP-IDF 2nd stage bootloader, that it didn't map the
            # last MMU page, if an IROM/DROM segment was < 0x24 bytes over the page boundary.
            segment.data += b'\x00' * (0x24 - segment_len_remainder)
        return self.save_segment(f, segment, checksum)

    def load_extended_header(self, load_file):
        def split_byte(n):
            return (n & 0x0F, (n >> 4) & 0x0F)

        fields = list(struct.unpack(self.EXTENDED_HEADER_STRUCT_FMT, load_file.read(16)))

        self.wp_pin = fields[0]

        # SPI pin drive stengths are two per byte
        self.clk_drv, self.q_drv = split_byte(fields[1])
        self.d_drv, self.cs_drv = split_byte(fields[2])
        self.hd_drv, self.wp_drv = split_byte(fields[3])

        chip_id = fields[4]
        if chip_id != self.ROM_LOADER.IMAGE_CHIP_ID:
            print(("Unexpected chip id in image. Expected %d but value was %d. "
                   "Is this image for a different chip model?") % (self.ROM_LOADER.IMAGE_CHIP_ID, chip_id))

        # reserved fields in the middle should all be zero
        if any(f for f in fields[6:-1] if f != 0):
            print("Warning: some reserved header fields have non-zero values. This image may be from a newer esptool.py?")

        append_digest = fields[-1]  # last byte is append_digest
        if append_digest in [0, 1]:
            self.append_digest = (append_digest == 1)
        else:
            raise RuntimeError("Invalid value for append_digest field (0x%02x). Should be 0 or 1.", append_digest)

    def save_extended_header(self, save_file):
        def join_byte(ln, hn):
            return (ln & 0x0F) + ((hn & 0x0F) << 4)

        append_digest = 1 if self.append_digest else 0

        fields = [self.wp_pin,
                  join_byte(self.clk_drv, self.q_drv),
                  join_byte(self.d_drv, self.cs_drv),
                  join_byte(self.hd_drv, self.wp_drv),
                  self.ROM_LOADER.IMAGE_CHIP_ID,
                  self.min_rev]
        fields += [0] * 8  # padding
        fields += [append_digest]

        packed = struct.pack(self.EXTENDED_HEADER_STRUCT_FMT, *fields)
        save_file.write(packed)


ESP32ROM.BOOTLOADER_IMAGE = ESP32FirmwareImage


class ESP32S2FirmwareImage(ESP32FirmwareImage):
    """ ESP32S2 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32S2ROM


ESP32S2ROM.BOOTLOADER_IMAGE = ESP32S2FirmwareImage


class ESP32S3BETA2FirmwareImage(ESP32FirmwareImage):
    """ ESP32S3 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32S3BETA2ROM


ESP32S3BETA2ROM.BOOTLOADER_IMAGE = ESP32S3BETA2FirmwareImage


class ESP32C3FirmwareImage(ESP32FirmwareImage):
    """ ESP32C3 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32C3ROM


ESP32C3ROM.BOOTLOADER_IMAGE = ESP32C3FirmwareImage


class ELFFile(object):
    SEC_TYPE_PROGBITS = 0x01
    SEC_TYPE_STRTAB = 0x03

    LEN_SEC_HEADER = 0x28

    SEG_TYPE_LOAD = 0x01
    LEN_SEG_HEADER = 0x20

    def __init__(self, name):
        # Load sections from the ELF file
        self.name = name
        with open(self.name, 'rb') as f:
            self._read_elf_file(f)

    def get_section(self, section_name):
        for s in self.sections:
            if s.name == section_name:
                return s
        raise ValueError("No section %s in ELF file" % section_name)

    def _read_elf_file(self, f):
        # read the ELF file header
        LEN_FILE_HEADER = 0x34
        try:
            (ident, _type, machine, _version,
             self.entrypoint, _phoff, shoff, _flags,
             _ehsize, _phentsize, _phnum, shentsize,
             shnum, shstrndx) = struct.unpack("<16sHHLLLLLHHHHHH", f.read(LEN_FILE_HEADER))
        except struct.error as e:
            raise FatalError("Failed to read a valid ELF header from %s: %s" % (self.name, e))

        if byte(ident, 0) != 0x7f or ident[1:4] != b'ELF':
            raise FatalError("%s has invalid ELF magic header" % self.name)
        if machine not in [0x5e, 0xf3]:
            raise FatalError("%s does not appear to be an Xtensa or an RISCV ELF file. e_machine=%04x" % (self.name, machine))
        if shentsize != self.LEN_SEC_HEADER:
            raise FatalError("%s has unexpected section header entry size 0x%x (not 0x%x)" % (self.name, shentsize, self.LEN_SEC_HEADER))
        if shnum == 0:
            raise FatalError("%s has 0 section headers" % (self.name))
        self._read_sections(f, shoff, shnum, shstrndx)
        self._read_segments(f, _phoff, _phnum, shstrndx)

    def _read_sections(self, f, section_header_offs, section_header_count, shstrndx):
        f.seek(section_header_offs)
        len_bytes = section_header_count * self.LEN_SEC_HEADER
        section_header = f.read(len_bytes)
        if len(section_header) == 0:
            raise FatalError("No section header found at offset %04x in ELF file." % section_header_offs)
        if len(section_header) != (len_bytes):
            raise FatalError("Only read 0x%x bytes from section header (expected 0x%x.) Truncated ELF file?" % (len(section_header), len_bytes))

        # walk through the section header and extract all sections
        section_header_offsets = range(0, len(section_header), self.LEN_SEC_HEADER)

        def read_section_header(offs):
            name_offs, sec_type, _flags, lma, sec_offs, size = struct.unpack_from("<LLLLLL", section_header[offs:])
            return (name_offs, sec_type, lma, size, sec_offs)
        all_sections = [read_section_header(offs) for offs in section_header_offsets]
        prog_sections = [s for s in all_sections if s[1] == ELFFile.SEC_TYPE_PROGBITS]

        # search for the string table section
        if not (shstrndx * self.LEN_SEC_HEADER) in section_header_offsets:
            raise FatalError("ELF file has no STRTAB section at shstrndx %d" % shstrndx)
        _, sec_type, _, sec_size, sec_offs = read_section_header(shstrndx * self.LEN_SEC_HEADER)
        if sec_type != ELFFile.SEC_TYPE_STRTAB:
            print('WARNING: ELF file has incorrect STRTAB section type 0x%02x' % sec_type)
        f.seek(sec_offs)
        string_table = f.read(sec_size)

        # build the real list of ELFSections by reading the actual section names from the
        # string table section, and actual data for each section from the ELF file itself
        def lookup_string(offs):
            raw = string_table[offs:]
            return raw[:raw.index(b'\x00')]

        def read_data(offs, size):
            f.seek(offs)
            return f.read(size)

        prog_sections = [ELFSection(lookup_string(n_offs), lma, read_data(offs, size)) for (n_offs, _type, lma, size, offs) in prog_sections
                         if lma != 0 and size > 0]
        self.sections = prog_sections

    def _read_segments(self, f, segment_header_offs, segment_header_count, shstrndx):
        f.seek(segment_header_offs)
        len_bytes = segment_header_count * self.LEN_SEG_HEADER
        segment_header = f.read(len_bytes)
        if len(segment_header) == 0:
            raise FatalError("No segment header found at offset %04x in ELF file." % segment_header_offs)
        if len(segment_header) != (len_bytes):
            raise FatalError("Only read 0x%x bytes from segment header (expected 0x%x.) Truncated ELF file?" % (len(segment_header), len_bytes))

        # walk through the segment header and extract all segments
        segment_header_offsets = range(0, len(segment_header), self.LEN_SEG_HEADER)

        def read_segment_header(offs):
            seg_type, seg_offs, _vaddr, lma, size, _memsize, _flags, _align = struct.unpack_from("<LLLLLLLL", segment_header[offs:])
            return (seg_type, lma, size, seg_offs)
        all_segments = [read_segment_header(offs) for offs in segment_header_offsets]
        prog_segments = [s for s in all_segments if s[0] == ELFFile.SEG_TYPE_LOAD]

        def read_data(offs, size):
            f.seek(offs)
            return f.read(size)

        prog_segments = [ELFSection(b'PHDR', lma, read_data(offs, size)) for (_type, lma, size, offs) in prog_segments
                         if lma != 0 and size > 0]
        self.segments = prog_segments

    def sha256(self):
        # return SHA256 hash of the input ELF file
        sha256 = hashlib.sha256()
        with open(self.name, 'rb') as f:
            sha256.update(f.read())
        return sha256.digest()


def slip_reader(port, trace_function):
    """Generator to read SLIP packets from a serial port.
    Yields one full SLIP packet at a time, raises exception on timeout or invalid data.

    Designed to avoid too many calls to serial.read(1), which can bog
    down on slow systems.
    """
    partial_packet = None
    in_escape = False
    while True:
        waiting = port.inWaiting()
        read_bytes = port.read(1 if waiting == 0 else waiting)
        if read_bytes == b'':
            waiting_for = "header" if partial_packet is None else "content"
            trace_function("Timed out waiting for packet %s", waiting_for)
            raise FatalError("Timed out waiting for packet %s" % waiting_for)
        trace_function("Read %d bytes: %s", len(read_bytes), HexFormatter(read_bytes))
        for b in read_bytes:
            if type(b) is int:
                b = bytes([b])  # python 2/3 compat

            if partial_packet is None:  # waiting for packet header
                if b == b'\xc0':
                    partial_packet = b""
                else:
                    trace_function("Read invalid data: %s", HexFormatter(read_bytes))
                    trace_function("Remaining data in serial buffer: %s", HexFormatter(port.read(port.inWaiting())))
                    raise FatalError('Invalid head of packet (0x%s)' % hexify(b))
            elif in_escape:  # part-way through escape sequence
                in_escape = False
                if b == b'\xdc':
                    partial_packet += b'\xc0'
                elif b == b'\xdd':
                    partial_packet += b'\xdb'
                else:
                    trace_function("Read invalid data: %s", HexFormatter(read_bytes))
                    trace_function("Remaining data in serial buffer: %s", HexFormatter(port.read(port.inWaiting())))
                    raise FatalError('Invalid SLIP escape (0xdb, 0x%s)' % (hexify(b)))
            elif b == b'\xdb':  # start of escape sequence
                in_escape = True
            elif b == b'\xc0':  # end of packet
                trace_function("Received full packet: %s", HexFormatter(partial_packet))
                yield partial_packet
                partial_packet = None
            else:  # normal byte in packet
                partial_packet += b


def arg_auto_int(x):
    return int(x, 0)


def div_roundup(a, b):
    """ Return a/b rounded up to nearest integer,
    equivalent result to int(math.ceil(float(int(a)) / float(int(b))), only
    without possible floating point accuracy errors.
    """
    return (int(a) + int(b) - 1) // int(b)


def align_file_position(f, size):
    """ Align the position in the file to the next block of specified size """
    align = (size - 1) - (f.tell() % size)
    f.seek(align, 1)


def flash_size_bytes(size):
    """ Given a flash size of the type passed in args.flash_size
    (ie 512KB or 1MB) then return the size in bytes.
    """
    if "MB" in size:
        return int(size[:size.index("MB")]) * 1024 * 1024
    elif "KB" in size:
        return int(size[:size.index("KB")]) * 1024
    else:
        raise FatalError("Unknown size %s" % size)


def hexify(s, uppercase=True):
    format_str = '%02X' if uppercase else '%02x'
    if not PYTHON2:
        return ''.join(format_str % c for c in s)
    else:
        return ''.join(format_str % ord(c) for c in s)


class HexFormatter(object):
    """
    Wrapper class which takes binary data in its constructor
    and returns a hex string as it's __str__ method.

    This is intended for "lazy formatting" of trace() output
    in hex format. Avoids overhead (significant on slow computers)
    of generating long hex strings even if tracing is disabled.

    Note that this doesn't save any overhead if passed as an
    argument to "%", only when passed to trace()

    If auto_split is set (default), any long line (> 16 bytes) will be
    printed as separately indented lines, with ASCII decoding at the end
    of each line.
    """
    def __init__(self, binary_string, auto_split=True):
        self._s = binary_string
        self._auto_split = auto_split

    def __str__(self):
        if self._auto_split and len(self._s) > 16:
            result = ""
            s = self._s
            while len(s) > 0:
                line = s[:16]
                ascii_line = "".join(c if (c == ' ' or (c in string.printable and c not in string.whitespace))
                                     else '.' for c in line.decode('ascii', 'replace'))
                s = s[16:]
                result += "\n    %-16s %-16s | %s" % (hexify(line[:8], False), hexify(line[8:], False), ascii_line)
            return result
        else:
            return hexify(self._s, False)


def pad_to(data, alignment, pad_character=b'\xFF'):
    """ Pad to the next alignment boundary """
    pad_mod = len(data) % alignment
    if pad_mod != 0:
        data += pad_character * (alignment - pad_mod)
    return data


class FatalError(RuntimeError):
    """
    Wrapper class for runtime errors that aren't caused by internal bugs, but by
    ESP8266 responses or input content.
    """
    def __init__(self, message):
        RuntimeError.__init__(self, message)

    @staticmethod
    def WithResult(message, result):
        """
        Return a fatal error object that appends the hex values of
        'result' as a string formatted argument.
        """
        message += " (result was %s)" % hexify(result)
        return FatalError(message)


class NotImplementedInROMError(FatalError):
    """
    Wrapper class for the error thrown when a particular ESP bootloader function
    is not implemented in the ROM bootloader.
    """
    def __init__(self, bootloader, func):
        FatalError.__init__(self, "%s ROM does not support function %s." % (bootloader.CHIP_NAME, func.__name__))


class NotSupportedError(FatalError):
    def __init__(self, esp, function_name):
        FatalError.__init__(self, "Function %s is not supported for %s." % (function_name, esp.CHIP_NAME))

# "Operation" commands, executable at command line. One function each
#
# Each function takes either two args (<ESPLoader instance>, <args>) or a single <args>
# argument.


class UnsupportedCommandError(RuntimeError):
    """
    Wrapper class for when ROM loader returns an invalid command response.

    Usually this indicates the loader is running in Secure Download Mode.
    """
    def __init__(self, esp, op):
        if esp.secure_download_mode:
            msg = "This command (0x%x) is not supported in Secure Download Mode" % op
        else:
            msg = "Invalid (unsupported) command 0x%x" % op
        RuntimeError.__init__(self, msg)


def load_ram(esp, args):
    image = LoadFirmwareImage(esp.CHIP_NAME, args.filename)

    print('RAM boot...')
    for seg in image.segments:
        size = len(seg.data)
        print('Downloading %d bytes at %08x...' % (size, seg.addr), end=' ')
        sys.stdout.flush()
        esp.mem_begin(size, div_roundup(size, esp.ESP_RAM_BLOCK), esp.ESP_RAM_BLOCK, seg.addr)

        seq = 0
        while len(seg.data) > 0:
            esp.mem_block(seg.data[0:esp.ESP_RAM_BLOCK], seq)
            seg.data = seg.data[esp.ESP_RAM_BLOCK:]
            seq += 1
        print('done!')

    print('All segments done, executing at %08x' % image.entrypoint)
    esp.mem_finish(image.entrypoint)


def read_mem(esp, args):
    print('0x%08x = 0x%08x' % (args.address, esp.read_reg(args.address)))


def write_mem(esp, args):
    esp.write_reg(args.address, args.value, args.mask, 0)
    print('Wrote %08x, mask %08x to %08x' % (args.value, args.mask, args.address))


def dump_mem(esp, args):
    with open(args.filename, 'wb') as f:
        for i in range(args.size // 4):
            d = esp.read_reg(args.address + (i * 4))
            f.write(struct.pack(b'<I', d))
            if f.tell() % 1024 == 0:
                print_overwrite('%d bytes read... (%d %%)' % (f.tell(),
                                                              f.tell() * 100 // args.size))
            sys.stdout.flush()
        print_overwrite("Read %d bytes" % f.tell(), last_line=True)
    print('Done!')


def detect_flash_size(esp, args):
    if args.flash_size == 'detect':
        if esp.secure_download_mode:
            raise FatalError("Detecting flash size is not supported in secure download mode. Need to manually specify flash size.")
        flash_id = esp.flash_id()
        size_id = flash_id >> 16
        args.flash_size = DETECTED_FLASH_SIZES.get(size_id)
        if args.flash_size is None:
            print('Warning: Could not auto-detect Flash size (FlashID=0x%x, SizeID=0x%x), defaulting to 4MB' % (flash_id, size_id))
            args.flash_size = '4MB'
        else:
            print('Auto-detected Flash size:', args.flash_size)


def _update_image_flash_params(esp, address, args, image):
    """ Modify the flash mode & size bytes if this looks like an executable bootloader image  """
    if len(image) < 8:
        return image  # not long enough to be a bootloader image

    # unpack the (potential) image header
    magic, _, flash_mode, flash_size_freq = struct.unpack("BBBB", image[:4])
    if address != esp.BOOTLOADER_FLASH_OFFSET:
        return image  # not flashing bootloader offset, so don't modify this

    if (args.flash_mode, args.flash_freq, args.flash_size) == ('keep',) * 3:
        return image  # all settings are 'keep', not modifying anything

    # easy check if this is an image: does it start with a magic byte?
    if magic != esp.ESP_IMAGE_MAGIC:
        print("Warning: Image file at 0x%x doesn't look like an image file, so not changing any flash settings." % address)
        return image

    # make sure this really is an image, and not just data that
    # starts with esp.ESP_IMAGE_MAGIC (mostly a problem for encrypted
    # images that happen to start with a magic byte
    try:
        test_image = esp.BOOTLOADER_IMAGE(io.BytesIO(image))
        test_image.verify()
    except Exception:
        print("Warning: Image file at 0x%x is not a valid %s image, so not changing any flash settings." % (address, esp.CHIP_NAME))
        return image

    if args.flash_mode != 'keep':
        flash_mode = {'qio': 0, 'qout': 1, 'dio': 2, 'dout': 3}[args.flash_mode]

    flash_freq = flash_size_freq & 0x0F
    if args.flash_freq != 'keep':
        flash_freq = {'40m': 0, '26m': 1, '20m': 2, '80m': 0xf}[args.flash_freq]

    flash_size = flash_size_freq & 0xF0
    if args.flash_size != 'keep':
        flash_size = esp.parse_flash_size_arg(args.flash_size)

    flash_params = struct.pack(b'BB', flash_mode, flash_size + flash_freq)
    if flash_params != image[2:4]:
        print('Flash params set to 0x%04x' % struct.unpack(">H", flash_params))
        image = image[0:2] + flash_params + image[4:]
    return image


def write_flash(esp, args):
    # set args.compress based on default behaviour:
    # -> if either --compress or --no-compress is set, honour that
    # -> otherwise, set --compress unless --no-stub is set
    if args.compress is None and not args.no_compress:
        args.compress = not args.no_stub

    # In case we have encrypted files to write, we first do few sanity checks before actual flash
    if args.encrypt or args.encrypt_files is not None:
        do_write = True

        if not esp.secure_download_mode:
            if esp.get_encrypted_download_disabled():
                raise FatalError("This chip has encrypt functionality in UART download mode disabled. "
                                 "This is the Flash Encryption configuration for Production mode instead of Development mode.")

            crypt_cfg_efuse = esp.get_flash_crypt_config()

            if crypt_cfg_efuse is not None and crypt_cfg_efuse != 0xF:
                print('Unexpected FLASH_CRYPT_CONFIG value: 0x%x' % (crypt_cfg_efuse))
                do_write = False

            enc_key_valid = esp.is_flash_encryption_key_valid()

            if not enc_key_valid:
                print('Flash encryption key is not programmed')
                do_write = False

        # Determine which files list contain the ones to encrypt
        files_to_encrypt = args.addr_filename if args.encrypt else args.encrypt_files

        for address, argfile in files_to_encrypt:
            if address % esp.FLASH_ENCRYPTED_WRITE_ALIGN:
                print("File %s address 0x%x is not %d byte aligned, can't flash encrypted" %
                      (argfile.name, address, esp.FLASH_ENCRYPTED_WRITE_ALIGN))
                do_write = False

        if not do_write and not args.ignore_flash_encryption_efuse_setting:
            raise FatalError("Can't perform encrypted flash write, consult Flash Encryption documentation for more information")

    # verify file sizes fit in flash
    if args.flash_size != 'keep':  # TODO: check this even with 'keep'
        flash_end = flash_size_bytes(args.flash_size)
        for address, argfile in args.addr_filename:
            argfile.seek(0, 2)  # seek to end
            if address + argfile.tell() > flash_end:
                raise FatalError(("File %s (length %d) at offset %d will not fit in %d bytes of flash. "
                                  "Use --flash-size argument, or change flashing address.")
                                 % (argfile.name, argfile.tell(), address, flash_end))
            argfile.seek(0)

    if args.erase_all:
        erase_flash(esp, args)

    """ Create a list describing all the files we have to flash. Each entry holds an "encrypt" flag
    marking whether the file needs encryption or not. This list needs to be sorted.

    First, append to each entry of our addr_filename list the flag args.encrypt
    For example, if addr_filename is [(0x1000, "partition.bin"), (0x8000, "bootloader")],
    all_files will be [(0x1000, "partition.bin", args.encrypt), (0x8000, "bootloader", args.encrypt)],
    where, of course, args.encrypt is either True or False
    """
    all_files = [(offs, filename, args.encrypt) for (offs, filename) in args.addr_filename]

    """Now do the same with encrypt_files list, if defined.
    In this case, the flag is True
    """
    if args.encrypt_files is not None:
        encrypted_files_flag = [(offs, filename, True) for (offs, filename) in args.encrypt_files]

        # Concatenate both lists and sort them.
        # As both list are already sorted, we could simply do a merge instead,
        # but for the sake of simplicity and because the lists are very small,
        # let's use sorted.
        all_files = sorted(all_files + encrypted_files_flag, key=lambda x: x[0])

    for address, argfile, encrypted in all_files:
        compress = args.compress

        # Check whether we can compress the current file before flashing
        if compress and encrypted:
            print('\nWARNING: - compress and encrypt options are mutually exclusive ')
            print('Will flash %s uncompressed' % argfile.name)
            compress = False

        if args.no_stub:
            print('Erasing flash...')
        image = pad_to(argfile.read(), esp.FLASH_ENCRYPTED_WRITE_ALIGN if encrypted else 4)
        if len(image) == 0:
            print('WARNING: File %s is empty' % argfile.name)
            continue
        image = _update_image_flash_params(esp, address, args, image)
        calcmd5 = hashlib.md5(image).hexdigest()
        uncsize = len(image)
        if compress:
            uncimage = image
            image = zlib.compress(uncimage, 9)
            # Decompress the compressed binary a block at a time, to dynamically calculate the
            # timeout based on the real write size
            decompress = zlib.decompressobj()
            blocks = esp.flash_defl_begin(uncsize, len(image), address)
        else:
            blocks = esp.flash_begin(uncsize, address, begin_rom_encrypted=encrypted)
        argfile.seek(0)  # in case we need it again
        seq = 0
        bytes_sent = 0  # bytes sent on wire
        bytes_written = 0  # bytes written to flash
        t = time.time()

        timeout = DEFAULT_TIMEOUT

        while len(image) > 0:
            print_overwrite('Writing at 0x%08x... (%d %%)' % (address + bytes_written, 100 * (seq + 1) // blocks))
            sys.stdout.flush()
            block = image[0:esp.FLASH_WRITE_SIZE]
            if compress:
                # feeding each compressed block into the decompressor lets us see block-by-block how much will be written
                block_uncompressed = len(decompress.decompress(block))
                bytes_written += block_uncompressed
                block_timeout = max(DEFAULT_TIMEOUT, timeout_per_mb(ERASE_WRITE_TIMEOUT_PER_MB, block_uncompressed))
                if not esp.IS_STUB:
                    timeout = block_timeout  # ROM code writes block to flash before ACKing
                esp.flash_defl_block(block, seq, timeout=timeout)
                if esp.IS_STUB:
                    timeout = block_timeout  # Stub ACKs when block is received, then writes to flash while receiving the block after it
            else:
                # Pad the last block
                block = block + b'\xff' * (esp.FLASH_WRITE_SIZE - len(block))
                if encrypted:
                    esp.flash_encrypt_block(block, seq)
                else:
                    esp.flash_block(block, seq)
                bytes_written += len(block)
            bytes_sent += len(block)
            image = image[esp.FLASH_WRITE_SIZE:]
            seq += 1

        if esp.IS_STUB:
            # Stub only writes each block to flash after 'ack'ing the receive, so do a final dummy operation which will
            # not be 'ack'ed until the last block has actually been written out to flash
            esp.read_reg(ESPLoader.CHIP_DETECT_MAGIC_REG_ADDR, timeout=timeout)

        t = time.time() - t
        speed_msg = ""
        if compress:
            if t > 0.0:
                speed_msg = " (effective %.1f kbit/s)" % (uncsize / t * 8 / 1000)
            print_overwrite('Wrote %d bytes (%d compressed) at 0x%08x in %.1f seconds%s...' % (uncsize,
                                                                                               bytes_sent,
                                                                                               address, t, speed_msg), last_line=True)
        else:
            if t > 0.0:
                speed_msg = " (%.1f kbit/s)" % (bytes_written / t * 8 / 1000)
            print_overwrite('Wrote %d bytes at 0x%08x in %.1f seconds%s...' % (bytes_written, address, t, speed_msg), last_line=True)

        if not encrypted and not esp.secure_download_mode:
            try:
                res = esp.flash_md5sum(address, uncsize)
                if res != calcmd5:
                    print('File  md5: %s' % calcmd5)
                    print('Flash md5: %s' % res)
                    print('MD5 of 0xFF is %s' % (hashlib.md5(b'\xFF' * uncsize).hexdigest()))
                    raise FatalError("MD5 of file does not match data in flash!")
                else:
                    print('Hash of data verified.')
            except NotImplementedInROMError:
                pass

    print('\nLeaving...')

    if esp.IS_STUB:
        # skip sending flash_finish to ROM loader here,
        # as it causes the loader to exit and run user code
        esp.flash_begin(0, 0)

        # Get the "encrypted" flag for the last file flashed
        # Note: all_files list contains triplets like:
        # (address: Integer, filename: String, encrypted: Boolean)
        last_file_encrypted = all_files[-1][2]

        # Check whether the last file flashed was compressed or not
        if args.compress and not last_file_encrypted:
            esp.flash_defl_finish(False)
        else:
            esp.flash_finish(False)

    if args.verify:
        print('Verifying just-written flash...')
        print('(This option is deprecated, flash contents are now always read back after flashing.)')
        # If some encrypted files have been flashed print a warning saying that we won't check them
        if args.encrypt or args.encrypt_files is not None:
            print('WARNING: - cannot verify encrypted files, they will be ignored')
        # Call verify_flash function only if there at least one non-encrypted file flashed
        if not args.encrypt:
            verify_flash(esp, args)


def image_info(args):
    image = LoadFirmwareImage(args.chip, args.filename)
    print('Image version: %d' % image.version)
    print('Entry point: %08x' % image.entrypoint if image.entrypoint != 0 else 'Entry point not set')
    print('%d segments' % len(image.segments))
    print()
    idx = 0
    for seg in image.segments:
        idx += 1
        segs = seg.get_memory_type(image)
        seg_name = ",".join(segs)
        print('Segment %d: %r [%s]' % (idx, seg, seg_name))
    calc_checksum = image.calculate_checksum()
    print('Checksum: %02x (%s)' % (image.checksum,
                                   'valid' if image.checksum == calc_checksum else 'invalid - calculated %02x' % calc_checksum))
    try:
        digest_msg = 'Not appended'
        if image.append_digest:
            is_valid = image.stored_digest == image.calc_digest
            digest_msg = "%s (%s)" % (hexify(image.calc_digest).lower(),
                                      "valid" if is_valid else "invalid")
            print('Validation Hash: %s' % digest_msg)
    except AttributeError:
        pass  # ESP8266 image has no append_digest field


def make_image(args):
    image = ESP8266ROMFirmwareImage()
    if len(args.segfile) == 0:
        raise FatalError('No segments specified')
    if len(args.segfile) != len(args.segaddr):
        raise FatalError('Number of specified files does not match number of specified addresses')
    for (seg, addr) in zip(args.segfile, args.segaddr):
        with open(seg, 'rb') as f:
            data = f.read()
            image.segments.append(ImageSegment(addr, data))
    image.entrypoint = args.entrypoint
    image.save(args.output)


def elf2image(args):
    e = ELFFile(args.input)
    if args.chip == 'auto':  # Default to ESP8266 for backwards compatibility
        print("Creating image for ESP8266...")
        args.chip = 'esp8266'

    if args.chip == 'esp32':
        image = ESP32FirmwareImage()
        if args.secure_pad:
            image.secure_pad = '1'
        elif args.secure_pad_v2:
            image.secure_pad = '2'
        image.min_rev = int(args.min_rev)
    elif args.chip == 'esp32s2':
        image = ESP32S2FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
        image.min_rev = 0
    elif args.chip == 'esp32s3beta2':
        image = ESP32S3BETA2FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
        image.min_rev = 0
    elif args.chip == 'esp32c3':
        image = ESP32C3FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
        image.min_rev = 0
    elif args.version == '1':  # ESP8266
        image = ESP8266ROMFirmwareImage()
    else:
        image = ESP8266V2FirmwareImage()
    image.entrypoint = e.entrypoint
    image.flash_mode = {'qio': 0, 'qout': 1, 'dio': 2, 'dout': 3}[args.flash_mode]

    # ELFSection is a subclass of ImageSegment, so can use interchangeably
    image.segments = e.segments if args.use_segments else e.sections

    image.flash_size_freq = image.ROM_LOADER.FLASH_SIZES[args.flash_size]
    image.flash_size_freq += {'40m': 0, '26m': 1, '20m': 2, '80m': 0xf}[args.flash_freq]

    if args.elf_sha256_offset:
        image.elf_sha256 = e.sha256()
        image.elf_sha256_offset = args.elf_sha256_offset

    before = len(image.segments)
    image.merge_adjacent_segments()
    if len(image.segments) != before:
        delta = before - len(image.segments)
        print("Merged %d ELF section%s" % (delta, "s" if delta > 1 else ""))

    image.verify()

    if args.output is None:
        args.output = image.default_output_name(args.input)
    image.save(args.output)


def read_mac(esp, args):
    mac = esp.read_mac()

    def print_mac(label, mac):
        print('%s: %s' % (label, ':'.join(map(lambda x: '%02x' % x, mac))))
    print_mac("MAC", mac)


def chip_id(esp, args):
    try:
        chipid = esp.chip_id()
        print('Chip ID: 0x%08x' % chipid)
    except NotSupportedError:
        print('Warning: %s has no Chip ID. Reading MAC instead.' % esp.CHIP_NAME)
        read_mac(esp, args)


def erase_flash(esp, args):
    print('Erasing flash (this may take a while)...')
    t = time.time()
    esp.erase_flash()
    print('Chip erase completed successfully in %.1fs' % (time.time() - t))


def erase_region(esp, args):
    print('Erasing region (may be slow depending on size)...')
    t = time.time()
    esp.erase_region(args.address, args.size)
    print('Erase completed successfully in %.1f seconds.' % (time.time() - t))


def run(esp, args):
    esp.run()


def flash_id(esp, args):
    flash_id = esp.flash_id()
    print('Manufacturer: %02x' % (flash_id & 0xff))
    flid_lowbyte = (flash_id >> 16) & 0xFF
    print('Device: %02x%02x' % ((flash_id >> 8) & 0xff, flid_lowbyte))
    print('Detected flash size: %s' % (DETECTED_FLASH_SIZES.get(flid_lowbyte, "Unknown")))


def read_flash(esp, args):
    if args.no_progress:
        flash_progress = None
    else:
        def flash_progress(progress, length):
            msg = '%d (%d %%)' % (progress, progress * 100.0 / length)
            padding = '\b' * len(msg)
            if progress == length:
                padding = '\n'
            sys.stdout.write(msg + padding)
            sys.stdout.flush()
    t = time.time()
    data = esp.read_flash(args.address, args.size, flash_progress)
    t = time.time() - t
    print_overwrite('Read %d bytes at 0x%x in %.1f seconds (%.1f kbit/s)...'
                    % (len(data), args.address, t, len(data) / t * 8 / 1000), last_line=True)
    with open(args.filename, 'wb') as f:
        f.write(data)


def verify_flash(esp, args):
    differences = False

    for address, argfile in args.addr_filename:
        image = pad_to(argfile.read(), 4)
        argfile.seek(0)  # rewind in case we need it again

        image = _update_image_flash_params(esp, address, args, image)

        image_size = len(image)
        print('Verifying 0x%x (%d) bytes @ 0x%08x in flash against %s...' % (image_size, image_size, address, argfile.name))
        # Try digest first, only read if there are differences.
        digest = esp.flash_md5sum(address, image_size)
        expected_digest = hashlib.md5(image).hexdigest()
        if digest == expected_digest:
            print('-- verify OK (digest matched)')
            continue
        else:
            differences = True
            if getattr(args, 'diff', 'no') != 'yes':
                print('-- verify FAILED (digest mismatch)')
                continue

        flash = esp.read_flash(address, image_size)
        assert flash != image
        diff = [i for i in range(image_size) if flash[i] != image[i]]
        print('-- verify FAILED: %d differences, first @ 0x%08x' % (len(diff), address + diff[0]))
        for d in diff:
            flash_byte = flash[d]
            image_byte = image[d]
            if PYTHON2:
                flash_byte = ord(flash_byte)
                image_byte = ord(image_byte)
            print('   %08x %02x %02x' % (address + d, flash_byte, image_byte))
    if differences:
        raise FatalError("Verify failed.")


def read_flash_status(esp, args):
    print('Status value: 0x%04x' % esp.read_status(args.bytes))


def write_flash_status(esp, args):
    fmt = "0x%%0%dx" % (args.bytes * 2)
    args.value = args.value & ((1 << (args.bytes * 8)) - 1)
    print(('Initial flash status: ' + fmt) % esp.read_status(args.bytes))
    print(('Setting flash status: ' + fmt) % args.value)
    esp.write_status(args.value, args.bytes, args.non_volatile)
    print(('After flash status:   ' + fmt) % esp.read_status(args.bytes))


def get_security_info(esp, args):
    (flags, flash_crypt_cnt, key_purposes) = esp.get_security_info()
    # TODO: better display
    print('Flags: 0x%08x (%s)' % (flags, bin(flags)))
    print('Flash_Crypt_Cnt: 0x%x' % flash_crypt_cnt)
    print('Key_Purposes: %s' % (key_purposes,))


def merge_bin(args):
    chip_class = _chip_to_rom_loader(args.chip)

    # sort the files by offset. The AddrFilenamePairAction has already checked for overlap
    input_files = sorted(args.addr_filename, key=lambda x: x[0])
    if not input_files:
        raise FatalError("No input files specified")
    first_addr = input_files[0][0]
    if first_addr < args.target_offset:
        raise FatalError("Output file target offset is 0x%x. Input file offset 0x%x is before this." % (args.target_offset, first_addr))

    if args.format != 'raw':
        raise FatalError("This version of esptool only supports the 'raw' output format")

    with open(args.output, 'wb') as of:
        def pad_to(flash_offs):
            # account for output file offset if there is any
            of.write(b'\xFF' * (flash_offs - args.target_offset - of.tell()))
        for addr, argfile in input_files:
            pad_to(addr)
            image = argfile.read()
            image = _update_image_flash_params(chip_class, addr, args, image)
            of.write(image)
        if args.fill_flash_size:
            pad_to(flash_size_bytes(args.fill_flash_size))
        print("Wrote 0x%x bytes to file %s, ready to flash to offset 0x%x" % (of.tell(), args.output, args.target_offset))


def version(args):
    print(__version__)

#
# End of operations functions
#


def main(argv=None, esp=None):
    """
    Main function for esptool

    argv - Optional override for default arguments parsing (that uses sys.argv), can be a list of custom arguments
    as strings. Arguments and their values need to be added as individual items to the list e.g. "-b 115200" thus
    becomes ['-b', '115200'].

    esp - Optional override of the connected device previously returned by get_default_connected_device()
    """

    external_esp = esp is not None

    parser = argparse.ArgumentParser(description='esptool.py v%s - ESP8266 ROM Bootloader Utility' % __version__, prog='esptool')

    parser.add_argument('--chip', '-c',
                        help='Target chip type',
                        type=lambda c: c.lower().replace('-', ''),  # support ESP32-S2, etc.
                        choices=['auto', 'esp8266', 'esp32', 'esp32s2', 'esp32s3beta2', 'esp32c3'],
                        default=os.environ.get('ESPTOOL_CHIP', 'auto'))

    parser.add_argument(
        '--port', '-p',
        help='Serial port device',
        default=os.environ.get('ESPTOOL_PORT', None))

    parser.add_argument(
        '--baud', '-b',
        help='Serial port baud rate used when flashing/reading',
        type=arg_auto_int,
        default=os.environ.get('ESPTOOL_BAUD', ESPLoader.ESP_ROM_BAUD))

    parser.add_argument(
        '--before',
        help='What to do before connecting to the chip',
        choices=['default_reset', 'no_reset', 'no_reset_no_sync'],
        default=os.environ.get('ESPTOOL_BEFORE', 'default_reset'))

    parser.add_argument(
        '--after', '-a',
        help='What to do after esptool.py is finished',
        choices=['hard_reset', 'soft_reset', 'no_reset'],
        default=os.environ.get('ESPTOOL_AFTER', 'hard_reset'))

    parser.add_argument(
        '--no-stub',
        help="Disable launching the flasher stub, only talk to ROM bootloader. Some features will not be available.",
        action='store_true')

    parser.add_argument(
        '--trace', '-t',
        help="Enable trace-level output of esptool.py interactions.",
        action='store_true')

    parser.add_argument(
        '--override-vddsdio',
        help="Override ESP32 VDDSDIO internal voltage regulator (use with care)",
        choices=ESP32ROM.OVERRIDE_VDDSDIO_CHOICES,
        nargs='?')

    parser.add_argument(
        '--connect-attempts',
        help=('Number of attempts to connect, negative or 0 for infinite. '
              'Default: %d.' % DEFAULT_CONNECT_ATTEMPTS),
        type=int,
        default=os.environ.get('ESPTOOL_CONNECT_ATTEMPTS', DEFAULT_CONNECT_ATTEMPTS))

    subparsers = parser.add_subparsers(
        dest='operation',
        help='Run esptool {command} -h for additional help')

    def add_spi_connection_arg(parent):
        parent.add_argument('--spi-connection', '-sc', help='ESP32-only argument. Override default SPI Flash connection. '
                            'Value can be SPI, HSPI or a comma-separated list of 5 I/O numbers to use for SPI flash (CLK,Q,D,HD,CS).',
                            action=SpiConnectionAction)

    parser_load_ram = subparsers.add_parser(
        'load_ram',
        help='Download an image to RAM and execute')
    parser_load_ram.add_argument('filename', help='Firmware image')

    parser_dump_mem = subparsers.add_parser(
        'dump_mem',
        help='Dump arbitrary memory to disk')
    parser_dump_mem.add_argument('address', help='Base address', type=arg_auto_int)
    parser_dump_mem.add_argument('size', help='Size of region to dump', type=arg_auto_int)
    parser_dump_mem.add_argument('filename', help='Name of binary dump')

    parser_read_mem = subparsers.add_parser(
        'read_mem',
        help='Read arbitrary memory location')
    parser_read_mem.add_argument('address', help='Address to read', type=arg_auto_int)

    parser_write_mem = subparsers.add_parser(
        'write_mem',
        help='Read-modify-write to arbitrary memory location')
    parser_write_mem.add_argument('address', help='Address to write', type=arg_auto_int)
    parser_write_mem.add_argument('value', help='Value', type=arg_auto_int)
    parser_write_mem.add_argument('mask', help='Mask of bits to write', type=arg_auto_int, nargs='?', default='0xFFFFFFFF')

    def add_spi_flash_subparsers(parent, allow_keep, auto_detect):
        """ Add common parser arguments for SPI flash properties """
        extra_keep_args = ['keep'] if allow_keep else []

        if auto_detect and allow_keep:
            extra_fs_message = ", detect, or keep"
        elif auto_detect:
            extra_fs_message = ", or detect"
        elif allow_keep:
            extra_fs_message = ", or keep"
        else:
            extra_fs_message = ""

        parent.add_argument('--flash_freq', '-ff', help='SPI Flash frequency',
                            choices=extra_keep_args + ['40m', '26m', '20m', '80m'],
                            default=os.environ.get('ESPTOOL_FF', 'keep' if allow_keep else '40m'))
        parent.add_argument('--flash_mode', '-fm', help='SPI Flash mode',
                            choices=extra_keep_args + ['qio', 'qout', 'dio', 'dout'],
                            default=os.environ.get('ESPTOOL_FM', 'keep' if allow_keep else 'qio'))
        parent.add_argument('--flash_size', '-fs', help='SPI Flash size in MegaBytes (1MB, 2MB, 4MB, 8MB, 16M)'
                            ' plus ESP8266-only (256KB, 512KB, 2MB-c1, 4MB-c1)' + extra_fs_message,
                            action=FlashSizeAction, auto_detect=auto_detect,
                            default=os.environ.get('ESPTOOL_FS', 'keep' if allow_keep else '1MB'))
        add_spi_connection_arg(parent)

    parser_write_flash = subparsers.add_parser(
        'write_flash',
        help='Write a binary blob to flash')

    parser_write_flash.add_argument('addr_filename', metavar='<address> <filename>', help='Address followed by binary filename, separated by space',
                                    action=AddrFilenamePairAction)
    parser_write_flash.add_argument('--erase-all', '-e',
                                    help='Erase all regions of flash (not just write areas) before programming',
                                    action="store_true")

    add_spi_flash_subparsers(parser_write_flash, allow_keep=True, auto_detect=True)
    parser_write_flash.add_argument('--no-progress', '-p', help='Suppress progress output', action="store_true")
    parser_write_flash.add_argument('--verify', help='Verify just-written data on flash '
                                    '(mostly superfluous, data is read back during flashing)', action='store_true')
    parser_write_flash.add_argument('--encrypt', help='Apply flash encryption when writing data (required correct efuse settings)',
                                    action='store_true')
    # In order to not break backward compatibility, our list of encrypted files to flash is a new parameter
    parser_write_flash.add_argument('--encrypt-files', metavar='<address> <filename>',
                                    help='Files to be encrypted on the flash. Address followed by binary filename, separated by space.',
                                    action=AddrFilenamePairAction)
    parser_write_flash.add_argument('--ignore-flash-encryption-efuse-setting', help='Ignore flash encryption efuse settings ',
                                    action='store_true')

    compress_args = parser_write_flash.add_mutually_exclusive_group(required=False)
    compress_args.add_argument('--compress', '-z', help='Compress data in transfer (default unless --no-stub is specified)',
                               action="store_true", default=None)
    compress_args.add_argument('--no-compress', '-u', help='Disable data compression during transfer (default if --no-stub is specified)',
                               action="store_true")

    subparsers.add_parser(
        'run',
        help='Run application code in flash')

    parser_image_info = subparsers.add_parser(
        'image_info',
        help='Dump headers from an application image')
    parser_image_info.add_argument('filename', help='Image file to parse')

    parser_make_image = subparsers.add_parser(
        'make_image',
        help='Create an application image from binary files')
    parser_make_image.add_argument('output', help='Output image file')
    parser_make_image.add_argument('--segfile', '-f', action='append', help='Segment input file')
    parser_make_image.add_argument('--segaddr', '-a', action='append', help='Segment base address', type=arg_auto_int)
    parser_make_image.add_argument('--entrypoint', '-e', help='Address of entry point', type=arg_auto_int, default=0)

    parser_elf2image = subparsers.add_parser(
        'elf2image',
        help='Create an application image from ELF file')
    parser_elf2image.add_argument('input', help='Input ELF file')
    parser_elf2image.add_argument('--output', '-o', help='Output filename prefix (for version 1 image), or filename (for version 2 single image)', type=str)
    parser_elf2image.add_argument('--version', '-e', help='Output image version', choices=['1', '2'], default='1')
    parser_elf2image.add_argument('--min-rev', '-r', help='Minimum chip revision', choices=['0', '1', '2', '3'], default='0')
    parser_elf2image.add_argument('--secure-pad', action='store_true',
                                  help='Pad image so once signed it will end on a 64KB boundary. For Secure Boot v1 images only.')
    parser_elf2image.add_argument('--secure-pad-v2', action='store_true',
                                  help='Pad image to 64KB, so once signed its signature sector will start at the next 64K block. '
                                  'For Secure Boot v2 images only.')
    parser_elf2image.add_argument('--elf-sha256-offset', help='If set, insert SHA256 hash (32 bytes) of the input ELF file at specified offset in the binary.',
                                  type=arg_auto_int, default=None)
    parser_elf2image.add_argument('--use_segments', help='If set, ELF segments will be used instead of ELF sections to genereate the image.',
                                  action='store_true')

    add_spi_flash_subparsers(parser_elf2image, allow_keep=False, auto_detect=False)

    subparsers.add_parser(
        'read_mac',
        help='Read MAC address from OTP ROM')

    subparsers.add_parser(
        'chip_id',
        help='Read Chip ID from OTP ROM')

    parser_flash_id = subparsers.add_parser(
        'flash_id',
        help='Read SPI flash manufacturer and device ID')
    add_spi_connection_arg(parser_flash_id)

    parser_read_status = subparsers.add_parser(
        'read_flash_status',
        help='Read SPI flash status register')

    add_spi_connection_arg(parser_read_status)
    parser_read_status.add_argument('--bytes', help='Number of bytes to read (1-3)', type=int, choices=[1, 2, 3], default=2)

    parser_write_status = subparsers.add_parser(
        'write_flash_status',
        help='Write SPI flash status register')

    add_spi_connection_arg(parser_write_status)
    parser_write_status.add_argument('--non-volatile', help='Write non-volatile bits (use with caution)', action='store_true')
    parser_write_status.add_argument('--bytes', help='Number of status bytes to write (1-3)', type=int, choices=[1, 2, 3], default=2)
    parser_write_status.add_argument('value', help='New value', type=arg_auto_int)

    parser_read_flash = subparsers.add_parser(
        'read_flash',
        help='Read SPI flash content')
    add_spi_connection_arg(parser_read_flash)
    parser_read_flash.add_argument('address', help='Start address', type=arg_auto_int)
    parser_read_flash.add_argument('size', help='Size of region to dump', type=arg_auto_int)
    parser_read_flash.add_argument('filename', help='Name of binary dump')
    parser_read_flash.add_argument('--no-progress', '-p', help='Suppress progress output', action="store_true")

    parser_verify_flash = subparsers.add_parser(
        'verify_flash',
        help='Verify a binary blob against flash')
    parser_verify_flash.add_argument('addr_filename', help='Address and binary file to verify there, separated by space',
                                     action=AddrFilenamePairAction)
    parser_verify_flash.add_argument('--diff', '-d', help='Show differences',
                                     choices=['no', 'yes'], default='no')
    add_spi_flash_subparsers(parser_verify_flash, allow_keep=True, auto_detect=True)

    parser_erase_flash = subparsers.add_parser(
        'erase_flash',
        help='Perform Chip Erase on SPI flash')
    add_spi_connection_arg(parser_erase_flash)

    parser_erase_region = subparsers.add_parser(
        'erase_region',
        help='Erase a region of the flash')
    add_spi_connection_arg(parser_erase_region)
    parser_erase_region.add_argument('address', help='Start address (must be multiple of 4096)', type=arg_auto_int)
    parser_erase_region.add_argument('size', help='Size of region to erase (must be multiple of 4096)', type=arg_auto_int)

    parser_merge_bin = subparsers.add_parser(
        'merge_bin',
        help='Merge multiple raw binary files into a single file for later flashing')

    parser_merge_bin.add_argument('--output', '-o', help='Output filename', type=str, required=True)
    parser_merge_bin.add_argument('--format', '-f', help='Format of the output file', choices='raw', default='raw')  # for future expansion
    add_spi_flash_subparsers(parser_merge_bin, allow_keep=True, auto_detect=False)

    parser_merge_bin.add_argument('--target-offset', '-t', help='Target offset where the output file will be flashed',
                                  type=arg_auto_int, default=0)
    parser_merge_bin.add_argument('--fill-flash-size', help='If set, the final binary file will be padded with FF '
                                  'bytes up to this flash size.', action=FlashSizeAction)
    parser_merge_bin.add_argument('addr_filename', metavar='<address> <filename>',
                                  help='Address followed by binary filename, separated by space',
                                  action=AddrFilenamePairAction)

    subparsers.add_parser(
        'version', help='Print esptool version')

    subparsers.add_parser('get_security_info', help='Get some security-related data')

    # internal sanity check - every operation matches a module function of the same name
    for operation in subparsers.choices.keys():
        assert operation in globals(), "%s should be a module function" % operation

    argv = expand_file_arguments(argv or sys.argv[1:])

    args = parser.parse_args(argv)
    print('esptool.py v%s' % __version__)

    # operation function can take 1 arg (args), 2 args (esp, arg)
    # or be a member function of the ESPLoader class.

    if args.operation is None:
        parser.print_help()
        sys.exit(1)

    # Forbid the usage of both --encrypt, which means encrypt all the given files,
    # and --encrypt-files, which represents the list of files to encrypt.
    # The reason is that allowing both at the same time increases the chances of
    # having contradictory lists (e.g. one file not available in one of list).
    if args.operation == "write_flash" and args.encrypt and args.encrypt_files is not None:
        raise FatalError("Options --encrypt and --encrypt-files must not be specified at the same time.")

    operation_func = globals()[args.operation]

    if PYTHON2:
        # This function is depreciated in Python3
        operation_args = inspect.getargspec(operation_func).args
    else:
        operation_args = inspect.getfullargspec(operation_func).args

    if operation_args[0] == 'esp':  # operation function takes an ESPLoader connection object
        if args.before != "no_reset_no_sync":
            initial_baud = min(ESPLoader.ESP_ROM_BAUD, args.baud)  # don't sync faster than the default baud rate
        else:
            initial_baud = args.baud

        if args.port is None:
            ser_list = get_port_list()
            print("Found %d serial ports" % len(ser_list))
        else:
            ser_list = [args.port]
        esp = esp or get_default_connected_device(ser_list, port=args.port, connect_attempts=args.connect_attempts,
                                                  initial_baud=initial_baud, chip=args.chip, trace=args.trace,
                                                  before=args.before)
        if esp is None:
            raise FatalError("Could not connect to an Espressif device on any of the %d available serial ports." % len(ser_list))

        if esp.secure_download_mode:
            print("Chip is %s in Secure Download Mode" % esp.CHIP_NAME)
        else:
            print("Chip is %s" % (esp.get_chip_description()))
            print("Features: %s" % ", ".join(esp.get_chip_features()))
            print("Crystal is %dMHz" % esp.get_crystal_freq())
            read_mac(esp, args)

        if not args.no_stub:
            if esp.secure_download_mode:
                print("WARNING: Stub loader is not supported in Secure Download Mode, setting --no-stub")
                args.no_stub = True
            else:
                esp = esp.run_stub()

        if args.override_vddsdio:
            esp.override_vddsdio(args.override_vddsdio)

        if args.baud > initial_baud:
            try:
                esp.change_baud(args.baud)
            except NotImplementedInROMError:
                print("WARNING: ROM doesn't support changing baud rate. Keeping initial baud rate %d" % initial_baud)

        # override common SPI flash parameter stuff if configured to do so
        if hasattr(args, "spi_connection") and args.spi_connection is not None:
            if esp.CHIP_NAME != "ESP32":
                raise FatalError("Chip %s does not support --spi-connection option." % esp.CHIP_NAME)
            print("Configuring SPI flash mode...")
            esp.flash_spi_attach(args.spi_connection)
        elif args.no_stub:
            print("Enabling default SPI flash mode...")
            # ROM loader doesn't enable flash unless we explicitly do it
            esp.flash_spi_attach(0)

        if hasattr(args, "flash_size"):
            print("Configuring flash size...")
            detect_flash_size(esp, args)
            if args.flash_size != 'keep':  # TODO: should set this even with 'keep'
                esp.flash_set_parameters(flash_size_bytes(args.flash_size))

        try:
            operation_func(esp, args)
        finally:
            try:  # Clean up AddrFilenamePairAction files
                for address, argfile in args.addr_filename:
                    argfile.close()
            except AttributeError:
                pass

        # Handle post-operation behaviour (reset or other)
        if operation_func == load_ram:
            # the ESP is now running the loaded image, so let it run
            print('Exiting immediately.')
        elif args.after == 'hard_reset':
            print('Hard resetting via RTS pin...')
            esp.hard_reset()
        elif args.after == 'soft_reset':
            print('Soft resetting...')
            # flash_finish will trigger a soft reset
            esp.soft_reset(False)
        else:
            print('Staying in bootloader.')
            if esp.IS_STUB:
                esp.soft_reset(True)  # exit stub back to ROM loader

        if not external_esp:
            esp._port.close()

    else:
        operation_func(args)


def get_port_list():
    if list_ports is None:
        raise FatalError("Listing all serial ports is currently not available. Please try to specify the port when "
                         "running esptool.py or update the pyserial package to the latest version")
    return sorted(ports.device for ports in list_ports.comports())


def expand_file_arguments(argv):
    """ Any argument starting with "@" gets replaced with all values read from a text file.
    Text file arguments can be split by newline or by space.
    Values are added "as-is", as if they were specified in this order on the command line.
    """
    new_args = []
    expanded = False
    for arg in argv:
        if arg.startswith("@"):
            expanded = True
            with open(arg[1:], "r") as f:
                for line in f.readlines():
                    new_args += shlex.split(line)
        else:
            new_args.append(arg)
    if expanded:
        print("esptool.py %s" % (" ".join(new_args[1:])))
        return new_args
    return argv


class FlashSizeAction(argparse.Action):
    """ Custom flash size parser class to support backwards compatibility with megabit size arguments.

    (At next major relase, remove deprecated sizes and this can become a 'normal' choices= argument again.)
    """
    def __init__(self, option_strings, dest, nargs=1, auto_detect=False, **kwargs):
        super(FlashSizeAction, self).__init__(option_strings, dest, nargs, **kwargs)
        self._auto_detect = auto_detect

    def __call__(self, parser, namespace, values, option_string=None):
        try:
            value = {
                '2m': '256KB',
                '4m': '512KB',
                '8m': '1MB',
                '16m': '2MB',
                '32m': '4MB',
                '16m-c1': '2MB-c1',
                '32m-c1': '4MB-c1',
            }[values[0]]
            print("WARNING: Flash size arguments in megabits like '%s' are deprecated." % (values[0]))
            print("Please use the equivalent size '%s'." % (value))
            print("Megabit arguments may be removed in a future release.")
        except KeyError:
            value = values[0]

        known_sizes = dict(ESP8266ROM.FLASH_SIZES)
        known_sizes.update(ESP32ROM.FLASH_SIZES)
        if self._auto_detect:
            known_sizes['detect'] = 'detect'
            known_sizes['keep'] = 'keep'
        if value not in known_sizes:
            raise argparse.ArgumentError(self, '%s is not a known flash size. Known sizes: %s' % (value, ", ".join(known_sizes.keys())))
        setattr(namespace, self.dest, value)


class SpiConnectionAction(argparse.Action):
    """ Custom action to parse 'spi connection' override. Values are SPI, HSPI, or a sequence of 5 pin numbers separated by commas.
    """
    def __call__(self, parser, namespace, value, option_string=None):
        if value.upper() == "SPI":
            value = 0
        elif value.upper() == "HSPI":
            value = 1
        elif "," in value:
            values = value.split(",")
            if len(values) != 5:
                raise argparse.ArgumentError(self, '%s is not a valid list of comma-separate pin numbers. Must be 5 numbers - CLK,Q,D,HD,CS.' % value)
            try:
                values = tuple(int(v, 0) for v in values)
            except ValueError:
                raise argparse.ArgumentError(self, '%s is not a valid argument. All pins must be numeric values' % values)
            if any([v for v in values if v > 33 or v < 0]):
                raise argparse.ArgumentError(self, 'Pin numbers must be in the range 0-33.')
            # encode the pin numbers as a 32-bit integer with packed 6-bit values, the same way ESP32 ROM takes them
            # TODO: make this less ESP32 ROM specific somehow...
            clk, q, d, hd, cs = values
            value = (hd << 24) | (cs << 18) | (d << 12) | (q << 6) | clk
        else:
            raise argparse.ArgumentError(self, '%s is not a valid spi-connection value. '
                                         'Values are SPI, HSPI, or a sequence of 5 pin numbers CLK,Q,D,HD,CS).' % value)
        setattr(namespace, self.dest, value)


class AddrFilenamePairAction(argparse.Action):
    """ Custom parser class for the address/filename pairs passed as arguments """
    def __init__(self, option_strings, dest, nargs='+', **kwargs):
        super(AddrFilenamePairAction, self).__init__(option_strings, dest, nargs, **kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        # validate pair arguments
        pairs = []
        for i in range(0, len(values), 2):
            try:
                address = int(values[i], 0)
            except ValueError:
                raise argparse.ArgumentError(self, 'Address "%s" must be a number' % values[i])
            try:
                argfile = open(values[i + 1], 'rb')
            except IOError as e:
                raise argparse.ArgumentError(self, e)
            except IndexError:
                raise argparse.ArgumentError(self, 'Must be pairs of an address and the binary filename to write there')
            pairs.append((address, argfile))

        # Sort the addresses and check for overlapping
        end = 0
        for address, argfile in sorted(pairs, key=lambda x: x[0]):
            argfile.seek(0, 2)  # seek to end
            size = argfile.tell()
            argfile.seek(0)
            sector_start = address & ~(ESPLoader.FLASH_SECTOR_SIZE - 1)
            sector_end = ((address + size + ESPLoader.FLASH_SECTOR_SIZE - 1) & ~(ESPLoader.FLASH_SECTOR_SIZE - 1)) - 1
            if sector_start < end:
                message = 'Detected overlap at address: 0x%x for file: %s' % (address, argfile.name)
                raise argparse.ArgumentError(self, message)
            end = sector_end
        setattr(namespace, self.dest, pairs)


# Binary stub code (see flasher_stub dir for source & details)
ESP8266ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
eNq9PGtj1Da2f8V2QpIZJkWyPR6bR5lMkikUKJAuge6md+MnvdwCYcg22W7Y3359XpLsmSTQ14fAyJalo3OOzlv6z+ZpfX66edsrNo/O8/ToXKujc6Wm7T/66Lxp4G9+Co/sX9b+pfj2fvsgla5tI1X0Jz3T2G1P\
p/LrwQ5/kMVmKPg3oyl1eHReQlt5Hswd5u0/0f7ReQ0/4G0OgNXt98kCXj1vWzF8C+Mm8EPLk3YENQYovn3RDqo8mP4n+GbWzjNGsBT11cUuQAg/ud/sBfx7OzEPgl38V75sJ6kLmgS+m7TwhPf99qGAQD9aoGpc\
2e2wC8Jz6XG0CUuhhadJF9vyxx8O2n8shD/AMHNAUafTD51O8EnUQlMhrLdb8FUJj0yH8ICnAVII8ltGWAAJ6FWOLcMBqcsBwg71/acPdh4SG+Ulv81j09hql6tg4BbLuu3T5NxAcHDoY+G57tr1CnwA9lc9S8su\
D9IMvY52kt5LXgjhzzTa+TedEeNV4DLIncWr1Bmk6Lzp7JR00NtdjTtAxA3AgmnASGa0rOQt0dyHESrZjql5TDTTRQ8VqV2J2eTP2n9qp6FDadx1wCyUM38RO40KGjk27jgfNB2pUbqQwVC1I0JU0SW9eYk9ZXUr\
GUB1IaR9ygDoLmNlLj2wkctySqHnlJBoGrltvEIqTvfm+N/oMf53/sDw1Lf8q4gf8q+y/Ip/VWmGv9relQwNa6txSdPHGzI3f+sTTAB9BtKPtid+ojVs4yBfC3CT0urCvBVUZZi3Mq4Kc5A/YQ6iLcwZbzVL2tLg\
C6YIWWwVEWOqpH2gEhfbAFL4KJhAb2DPMaFaaYYANqAqd4ftiCB584j35ZhIrrR/wZPrX3nCHCXTcGQkUvsDMBT6hHILDD7V7tMDGr5aWhH0QukaEMaUNsMDlSJv3euKSITf4+Gi/nD8PPzc5x9kI7ZSPqtZD9AP\
LT8Khkzx8kKGslpBnQwZQBOm1gzkKX2mk4/yDSOw4jc9WZ/ZpzoMdoDHAhTtAEe5jk/Gs+e7YR5sILe1ckGX0QS6e6ydUnej0ddRiP+B/kL9qFXgNaBi9da2mXsLegR50OUoXQbB811mkMhlJFDKOaurFPR/xp81\
NFsU2EnbScBy0KCJS6/F14QFczMhztYRD4tYbefKtDNXXRMAGdOzHPN20u3+rkpXcjjoAj6Z0J5EpCb8u1UX0XdAXXhagv3AMrRJFl4TyvNGHs4AqcCn9PwRPJ+TGlYK0EijX2DzgJfcDpvzsPC7qCwjZX2o0glD\
hRMtLGBZ44yAdG3sl/Awm9hhCzvs/FeGsYiFiu6caZLxhFsNoZ+e7kpn5MkWnHc0cuOMvM0TJiSepb/ujv9Y3qzZLyuGKUeYQ6JgpZF3RzDbqTxTE9JrYIaUDiLtV2FnthvuKwfW9h0sdDynzS6IAnhL+L+Bj/9J\
/Ibckck8G6TlDR0SoYP01v9aljRgMcG4WRnwZ6G7Fb3RDny46H94COtZw22D+qZBdTyiLmV5C6b9iadVHprDh4BwsrFrQGDT8N6PsA/sMgTyzYq5MhDbtbJvzFzN5Gsy4p2hCOZyxTgocFbAfCj2//wHAZrFXE07\
jXa5g2GV/LfXC4hVzy8cahQN2f4q+YeL04beEJ1mQknHI5IJld6wXRX6JSoAXo5BxsDi6tCqaANmCJYlKFya4QRl+o9b2/unLIiS284i1UwmQ7hUiBO06NbZVXNM7B4sk9coiu+B8munTUOYbZajhv4AI2siTdPM\
fLQUYJgqugkqec37jkxQxB9YyISsFoCmIm4hDIeOSyJQtCSvYedEsqn+jmsF9ZAeEKFLQ2hUWEZg8zAZiHCYWqf3YJCCZXdDo5NCmQlngbkbTVsfcleYbx2MQFId4wP8j3UfSEj4CNdO+uvZrjcNPXo9sYyD2k2B\
dvM2YFhZnGZ0ZtF831HadfyFWCkTNB7T0OmSUhcdCW99TWPU4YwHA/yp5K4VlmloQWaBtEniiEbIrNTH1SF/NU9Jpi2Ey0VpKS9etcEYLJKEobXyyxgszorJDAZLdUF+OVKpdLfX1v0iH1t12pUB6a0tAPoV2Nze\
Gti8hT/eQPouOrbcWCVqCvRridc16sbk9IB6FwYDy7kEQVTmVos2Da2e9Y52bM7OmltBr6sl2TBncci2skb985yMWEv+B4zXkmWrYfhtmqVRa8AI06/hX+TCVYwSfpcTq6FsRiyf07SmK8w6mUAsxLirK4gHuoM2\
k8NfNdtmSKTbA7TYHkUYdHi0J2ajIoOCNlVEUOgkAI58SGtErcgET9EIb6w0hgfo6cRWWxoAk2XHbpVMY1P3dP/oiD0ahaGiMbX3f4Z/D8kZKcOOydaQMavRGtJjMa/QDECxJoYb2kTAEfFGaPsosfQTkrrGUXUs\
ER4N2CB2DQodfuMYM1psiKZEbSbj+ew5AH1B0FUo5cU3YJbq+gaFPLQ+ATwGh7amnQMYCkgwVAqF/5jNQo2ydJtpoAnH3SkCssxrjvUVTb8D82fJ7I7PDmDI8Qw1FA4fgSuQjl8zKRIasKmXjXkUxV7khczpKrCs\
moKxZQZAUA/k7UG+frk7EvDKWmd5yIyHRs1HVqHgQzT1B7KJYRlFCLJe5XeomQobtpBu0uCtpTOEFzkqccZCi5XNzOHuHCQM9QjBIU3HgsbQTz78ggvw1wt/ePKILbJ8780LioKk8UF+A2e4ydSKhb/AAI7P2lGQ\
W+eP6B3EMWA75oBxjTTQ4623AGq+hYBsHOSDu9+D0vgE+NsmAQGefKvCHVsEd2mFoUOQTTVp4y16iXhAka8fuSJOJARFGECpwf8pCJAK/kEuamEgewH61wgX22MsfNTCRwzjRjmBmG98AWrleBfGfItxgOJHwNDC\
X59zDIV1fhr660iYG/5djhFPyLWpcG+mYhqcZ0RS1IFsfechT7ym3gD6LmibICDx8T6sxJ2dPoZQgYpaSA7aOTHY97KdcALx1gNWr4pDJeEPJCFSdYuMDbR7o78Hrf11AJFS4hDQWBWq5V+s2NJRa48d+DHxzSGL\
D/JfPe8FgHtIPqzobGH41Btz4BBN+x1Wyo0QC+SUbFvdoP1xADrwZWLDmqrB2Lhit7kknDWIg6jVzuCTzO2uUivEeNVSxqeQXFOPdg0O0S9CfcMPiuRuwHTpxIGBK4AizBIC/vGa0AWNnoh1cOKMWCFTlCsi68iG\
CbLNDdph4EdDMAmJAG+L7cDabSTppyIUNeGyLD2wMTyEHaw+gKb5wFPUqJ3WB8AVnr8zAPODHKQ62iEagVPXVIck0DTptvWjzbtbczFKya2i9UOY4ioUfO9z9qPKik9i4KKFGbmrd5AzX8ZIjhjJBSP6t2KEV0Iq\
Hxsco5wyDpCnAvbpUo89Z0UGkuh9LUzCBkXXivw8zuDPa3ZnYd2yjTvUxyTFZ61V1kgrBuqTxVGDPqliQqahL6Y80DdGmxiCeMaER0ykO6sWx5S3hiMTn0RiLEsE0pYfictx6u4SgZw+cR4AZehatXNWIOCrUDI8\
TR58GwhgggjNIqNJnlr9QIExn6xfY+UD3TC4AlEK/FF5jTqGZEn8mkNOEzagKisbENARruB+L5LPaAVFhPipP4/odyM24hD2q9m9JwCK3y4AhAXekvxC/lanooENoTtsgUCYPN7dXUYvG5MGO/5Ho6XH5Pe2ZAWV\
XLyHn/tCshe0Yci9O2Q6pfRAl26ckE0WjApfhSD/fzsz567evJ4Qo5LDVVk3BXPpFgSOzPT/0OwVZEtL9QwEp3rNNrnGgV+zB4at5glBBPbtsxteq1DRtySdioat7EjDW60/aVTqGyD3mxfH7zHGBKyeQQy2ZpMf\
0bDBJgSqktguAqy/f3ic14r8MyN755yDcgUvZpc2jP3vWXESvAZxPxKDFMz3agbPQhGFY1rVAgwpuzBMkHUWtvAjWhWtEEz8CYdTMhNESme74FsXAYZKDg8kWaJOhD3AX0Grn/qcUR9ioixeANCvgFI/MoLK87m7\
VX4NwAlBtwPFyTZhQhcwLjBfo32IVlYYs6w40t/JOM2MHArYvYKAfpocYybpPQee8sBuUdA1upr3rQe27liB5lcqULaCk7/1xKhfmfkK/8brVbLiXtilNssKmBKdsNpzokAkOiaoHH3wXJtWHB+ZOZyeYlbjkJEV\
P9OnTEsgUE1BA0WCxTBD1NG0DzAkifJ6uK4gk6yCkq1RI+PvOzIeM1X+U+RFVnqUU3IsVqVP+hxIhgfyutli825Glu11Q5T0GqLUbNtRLHNhZEeGsgPt/Imo7zN8tflFyhtNN/8OkQEdtyXmakdX68M9P+3p9haX\
HVUOhh2h0+doTwXeZOv33yePQZcRxC/hFTLOBCXYnOUzsv5jJ3yAMJd6zxVqC8qWPVq59yHVrpDKuNhjLs6BvRPuoP813MK1CTXZW0AXO8R8iN7tj4zkCn1feAHLey4fB0ML/REgcwthLxgJPIghcmBAHIgiD9Va\
vh3kXz1h7mmFmBFoLHwjxomC8KAwriYnNQyEnwnHOvS/mt4CMCBpXYuFFQ7g3YBFaOUkRsFLVbs0CIbJ9N63VJ6UYQHAtzvDAYcleJ4BudEQccnBkAX5UY81Rx5LKLpJeQs3yfCmz/uqnFGSulLTb9gdhenSVxBW\
SFlgF+VwHYkyQvBOwcyKZnkWUFxwd8nR6ZcQke4aRf0cFIrEEc75TsgLhiHaDinZWRpc/lydc5q5IPnbZI5WTG32mlES5OnMT088jLDntwi2ppkG3smHncN/2ogBzJZOJndOzhnZ6gzV4Rk0T070zFcL/B4DKa1D\
VdQULsPNnHLxDhRu5BpQFp8Q5BmHorHECJRLHjk5LCMQZv4t+DqYvbL5tfbzTRIkmKhNPNpSmUc7CCvWctpJOW/OQrW7vdAEWY5jTAWOBfMMZgRfhzBR+YkiVBzF9QY7ZEhiFUTJfk9sAkpIGp5dxXYfly342QXr\
Q3ymGLSYq21SepGrlcCcETDqowmzYZDLn7BBRoJgMvM5/VGkrfNKUwMSytRD+npnAMoFygA/O/lV/boIBP7hmU+pzaaEbhn6Mi9huBnwHXBThq723iwfLfyvSIhj+JxjqAWXQrRwbW9sOWn2zIYdU6eEAvznspPu\
sj7UDdhnA5JGeSIyPdngIKqgv8buGCYB+VkO16Zgxih2Zsg5CzGo6GleHnKUGoAswKzJGK2Q1rhbt4kWHXZtQGBJBDXao6gKxrXLCzLVHc19DK/BfWqRvaWGG2hTGZgyEVpF6xxUCed+kpsl2nyvqXIIVMxSoOsz\
TCEtxSPJL25o1zGJHDuoDsSD1MW96/WtkAvVH+LCiZ1XrAjUJfUm6Hfd7dhDWBjFINzkYfsgrHkmLBFKoMH/1KF7/pKIfowIDl2iRx2ip0T0TMOYsOdKB9YU0/e8yjzmbaqlWkLFSFko+9JYFht7R5sKxXweHHdJ\
+4TQkwHHGhJ0bTpj8QXgYlKS42gT4v6xD0q8XPhbFHx4TVBJGdFiJsUWsdHvWLGREu3BcczI3jcerBvbcxw4SmugYYQ8nEBSokx2XL7RaNy2yLWxeBHHQb5BljtFVRdfyqdrAFhCk61gzstiOgt/4zqf/oxjVy1q\
NvRDwl9GbncpcUajjBr6PkPTrGhMAgMg2MAMZs8ZTDuEHDYgYhsuYSWaDm8ZGxAokx0tNEdRgc5gpqRkaJW3Y+N8oCvc94AP6Ju++8s1HGygd93UD0B3MFMgfdwiEP4L31Sv3VKHf9tCKyxjmTg+YIdBvuekIEcB\
GlDZTenfhH/voam3bN1+3Ye5NQR9zllQ8YTWBF14G4sbs/4Hrh+scWecN8UWEaZ1mwZMIs1/xTnZpxUUShTpHADTBSgPI7BUIkoEbK96h82S4lksz9cEN49F/n/Fss3oJqlzcjqjsoDcORC3wKgxSKSO+ilWqJ/a\
VT8ssEQDwdxrtqJKh6sU0C4rncyxPH6rAipWaZ+5VT2fF63VEMrEYE7CQaDLo7V/kNqxAU0W5ZM/TeXs4D6+hsaBlDMqUTgYz/AGWopkltUOkRW1TWFJqYu1Dj1JZ7A5iUNN2PhtIhQGU6415iA8RmyRotBIbjki\
vRvF94wesln6hg1lp7qlb4ZzzQr7J1XUEx0Y9SzcqGdDoU6OsOrixZyC6l0rENgoo6i2NjwiyeU17zJWYUJaamkyeB1qodOglq1CIpnSSCPgXFOzUxI1dLF+mQGgMnUbjU5mjDSPphGbIJi7QUMgZIMz4thE5aiT\
S2hxBqi94FoprLGYAGT5bAkzewj+unf5Jqq4ZkSFHXb+dRk7xNAzFzs4/NTT04eCndDBDuY9Rf8kS3Jn0Mqd/L2gpm8jveXsN9viTRXuO5os7iBm6iBGcTwXQ+aIzHDe5cwWATCUvvkCjkEhQwNcadxQssMUikhA\
gg4rDCHug2lWxAuWJr8JuAB6wxcyt+bHm4INfk5aYsHt+OwuJ0cS9zTGl4XMNEf4VfLjkgx1QmZXZrxaA2zj+qRHTiHMAvu6kcrE/caNUR73YpRJ1wyy6emuQzdnBYKuHEiEji/Hdbo791aLVkynNH+yGwdZruIv\
8eG+hA0QyGSbeXw5+9Jlhz/Ik5vLkbO/xI3LOWlsCX/cJfxvd+XYHPtrXTl9UxaDWmUYpRg1yjcgYtlQRndjuMYiZQuFxztO3cN21FhOU3++wUUyDFbeZDbIdn2qrrpMdmSfJTuyCftXLGpc8ZHbzwrKc1oJMqTs\
zrsO9ri6ERFY5FsfWFPm5Nie/EsqH/2b2gYIUyNJSJ2r28EiH9yj/U9GBxfJ7VDEEi2lHDGHp0sw1WtEg0/VEk0++j5DYxgDeHehKAOSiU3+6WiBL8qN+c/X1WHIwZ/cmD+EqUGLj4V/83iIrMLxkvyEMLPmEAFI\
06rjfRHn+YC1nSreH1KtiaTAbdmBSTwVzBCsT+W5xqZR3+GdAB+Ed0CLghcOmRotssr0UzQK5D7VWGDUpF/fvHBLTySuekHMmBq/ZCEnpubsdNU+HwmELYBKYsIOkUQKU3EIsQ7FcxKDnydBR2xiJazby8+rn+mJ\
1OzPrZ+5gNWY2hlKPHcKsCAh5pYctDy0DtWDOQfuuWyh0T6fvQZI6McTPGaXxybpvO8cPIEdCf5165a3/rVHCQ9t4gfABo8vK9v5AgpkTAEi3R+f+pOiDXSPTruocmtz3n1JXGqExQCQGc2zYnVFWydONZEixYns\
3bOdN5zEQEW6JTk3qWkd0AMqyuGamgpD5jK0j9JjtJYVtmwvNRE86YUBuw0OH8SP4GhDicZ4dAn7pMw+aZ99QF/+AFOKjYscnfHZpexcmEmKGCjXSBGqkusUJdEocTbWDMBqEPevMVLhD5gyIR8SDn+CH5gnBOY2\
SefCH7NKA1piL3AV8Aef8qu4mh0zLmOubJd9DLWWqta/sNyHStJUqqXHF/bYA554ip9IIoz/xp+4a3mOgGCU7Ua/vss1uEWoUpkN1tGGtv4bngESINOIucWcKwylT0Tv5A/9+JwOnJo+8RXvxle8S654N+m+A9hq\
bqdFcBtW8U0GqJ2uQfgDWLhglGfquON1ha76gk/tYMOMTzqG30CGtYHykybHAxSz1khYYqr9jxh3PiBE0XGJbdqG5XhBOUBtcsvTM6I9MN8O5NVzYh45ywJRisk2V6liHZBUvifLlxZgQhrEKSZ9cyZnQyStQsth\
VXkoMpk5qeaUWs31Y5jii5Y8UydQy/6BkvQhqnUMnuYH/nBrrYC8cIVFBHj+8QX/gI5Y24n2w2AtRcO7uHWQDz/YWxrK/Hvvllfk6z8cLTxIx45PxnJ6EMCeUdQmxbphPJ42vUFbWTuHFPE4ayP0KnGrbZrT3gtE\
+5CNJ7ZCUaakW/dooMIcMrql5VQALAL9P0kGsrFemYCZnCKbDEDUJRPnhGHbYQGDo9YM5//gQgY5dWXz6PIwgzHK5CXaG9s2p69ASKDMw3I+SE7qG4y7zknxreWHafKcH6YCCpbLPRVonJMXaK71vi+Vo1dB2TZ4\
S4rf7+lx3YWUAVTcExcLriFM36iRB04QeDeMFWOBtjBhgJwOcwR8KCUda+dwrD0BK+cLIwh0N8X5p5/evvrxwZP03ta25VnUQunymmq3GiOkTVBKICxhBwg/DldgWQaHwDd2RJsctcCDH9Pth3w8Bs/4Z5mVbShL\
4bx+xZfR0J0DKUlFex5PUSlhCokIPKuf3bOz6hLPQOEkpqM9FwbwdDKdDQe4y0RCesLueLgFe60zqULI5rk3QJShj3c8+HjHg493PPj3SQJr7V7g0r/uw9aBQsPKk2P3tpVjn7NMnUt5SII51ykwf4ZHp3hAyWdz\
V8t2cK8lKNliQOGBZbElHRwCnKNzBVWXKnZhMpf5dG4SaB3iBd4PhLQ3hgrwrcSOuEyVPvY5dtzwRTcWeGaBMvXp2gSoRDE7buV1OEbGyyULBm+hi9FwCb0wt4NU98CWUuuHu1JPncltRnmno1x/0Uw6jx92rnjA\
2y0OT5cQZqqbqHBDBUtLSpeuUZnaG26MfovdNTtcUYbsTJT6k5QI4c1NwYi+0ngqerJvl2XSVRNCHyY8yrFzFVQiRXR11LnXqX+hhYvJQ7wLI33M+lBO8IQ7/4GBIR8Bj4p09CzibVRCvZ6YK7gzwnSUc/apmX4v\
96yYORJDLXPNE541hVOfmL7rAggyslKfR7P0sYfPs8fpaLg2GAm7pv5qbnwsN83ISxSWIAFK/ZIxCZGRJgUnMts5Ov0AG/S5lbIYxOBrWiSjoMVZiXlPlBRkgLNWsJKq3Fl9zAeph7TaesxhfTQ5t0ag7NFVD0lN\
o/naWBsB/Rs8EaWegoemvubLWKRjR2vCqU6Sx/Hg6Ai/fHCXVWYDuYYSan1aaQzhxfQDIBDAT5+zM9fYy648QaZ7kZNSz+8R9hweP5XDWPEMQxnq8c7DG2abQt/xYIwp5/jbYLjmjXYG+1IdF9Rys8N7Grejs1It\
6mXsrZGiu0r0qDs9oiNw5nQvl0prud6kYnXZFH1MSodySeq6V14UcrRVbntLLhun4g7xinty0mvlaUecWEVCigW59Taou44lHFjzRzxUucFFxAz4dxlVxZDqx52HP0Ir8i8D7ZWrEk+6t5GBhYwldUiLEm9umcbO\
M/TvMrnjySHB0m6FrYk71exPe7+T7NWArXXF6JBNO15xVwip/wwtC76vQlwEzFc0fHNRRUcnFljowEfRlbYbojNoGT6BYbXSngMLbtJquXvFjmBlgrKHYJ9WvgjJ0I5BN/2w5l26/agOHdkcYe99Wfe2jWx0hRHy\
ZElor2rrm+KJus8UCV2utFcInfKNUkXxGxj9Z5eJfnIbp27j3G1cdBkv7V2Ll/Xb7rVlaXlnhY7ItDVESENUvD1KMS8AW8CKwJLIny5LtjQw9giSZ8SphDxC5bZjeajqEHrPuTVFHVSyd9+SG7qa93LRz3KyDjZt\
3sC1Nclrt97zAR004exSMVl10Vksihv5Z7hjYZ7fc+mUU3wC+1ZsAsmk+A3biRHfNyN3YhA0oMNCL5eI3D02KPMGrq1JnsilD7dYy+WrbjCrUpk1ekUTpSUHidA3mb1lX8FcyNLLUEKtiVxOZgycfA9viNyXS/EA\
tHqbt8+Yry1p2JlGBCC0vIJKbTPIWbmCTGVuZ0MXMscbPDCVMRYAKBWOSQ2IvwJvYBVW82wkzEUJL5v6tXsYj6bjLHKSSc2e/OxTNNucA0jlKi+14m4husUhl0+CowXuSRic7Lt95+g2ZxQ0Kr7vpOQrNMgEaOJg\
1yZcar7JgrBr7iudyGmCje8GWHXayGUMHYDTJYD3RSr650tikDSd1MuiXTVxNqPBQNqPOsi1Lzlay3hTQN7dU+D0pi/vWgfZXLKXOL6UJN70bxGwEJIJZyJV0a37hvgEK1VKJwbg5r7N+Q0FxU50g0Sj5EzdeIMP\
ZmAat3DuTaoKvJQCp4Cvy2BgszvkHhuDCC9lHdvzDZrmw/ssrEiXgfHwayiwF51PrBFmPruQS+yazjnXNaxpCp8Bo+GB+wjOhRTjwbM9Sz4d2yNlgooJHkhUzDW6CMxZDKXBUC/2Dvn8Yz1hPQmdHABSqhbBJ5PE\
iWeEqww1+tt7eGivLuRe7SK2AP7QhR9iH5ctgYIhFMxrgZ0xnM0h16c1bgeZpcRi0isB6YOLc4Kr1g5/6skRKU66LX0Tuti1rzdHHt6Y/M+Pp/kC7k3WahKn8WQSx+2b+t3p4t/uw7R9WOWnOV+w3LkPFnff2LG6\
pXhXIpRyYJ8L2zLe2nihbVk4DTDyTANvAMGLnKc7XKqAfWrz+G8U8sDHuTIN27sg8YodqtppYO+i19v+SimS05sZol51I8NVTgPDy+rS4YhxljpwShetiek9Pm6uphdk3re/7vB9znK1cDO+dIpVv26Te9N5psyv\
93zdkZr+n0Hof80clFK0yCtXYJf5OxEIi8+E68t/2SxS27jvwIXFTgLx0j26/RxD1Gv3vcTeiU5bN4t/3cvDF7092ZvbvVAJTbVOELBj1HSstP4Vzp2AhF5xcbTu9de992GvHfXaca+d9Nppr11227oHj+7099xG\
p6d7W7U+vvr+4z/0T1/TDr+Qh67jqet4rN9OrmlPrmmnV7ZPr2i9u6LVucl6Zbu8sr24au9c+/el+zb5IhydfsG6+5A310iBHuS6B0n/GnPdGW/Nbdx0G51h77iNXbfRsSw6BPnYkzQ9OPNeu+y162jFLtF/4S7+\
s6XA75USv1eK/F4p83ul0HXtL/zTygYmzQ6c4M6jSiRxNWJzzTffeicxPLPTVum4S1e6yVara+RGk1DFafrp/wG5amLt\
""")))
ESP32ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
eNqNWntz3LYR/yoUrXfkDMDjkaDHre8uyunhtJGcRJYyN01JkIwzcTWyfKnOqtzPXuyLAHmXpH9QAkE8dhe7v33g/rO3bFbLvRdRtbdYKeMetVi16avFStvgBRrdS5kuVk3lXmoY5r9kU2huuXbpnnaxsiqCHlg1\
cd/aote97/6kUbRcrAq3VZO418w9Y7+bUjBrTLOMdv+z3gqOFFjbkWMMUV9Cn3JLNsqzo6q4BRJcb+6GwhoprAOU6t6CBQ3TtetVAdcmYtZbE7LqKIf59YAoR4yjAEYatX11Sl9xZPn/jBzuDo9WUXcS0eBM8DFC\
UQPissJeRUsqS9LwGzOnSFUVCLgYUFgk76jhe1DUV5/WWXErPrneBLiJVRTR0WxiR6kJ0dsIsW6cO5ei9KQ0dSA4OySrGDDUp2rznvxo3zbKz7aKNRoWkAcHptFAvZGaJGb+sq9Aa4ET4zlpSvpajkXAZkbnAKPg\
v04vRRFzVuTKxEDTiKzK2tEliRMXtaLp8cSN1XrH9Y+Ck1PcBrZwhaCzf+4j96VOel/e2N4pX8Ooq9+I7MkI+ouZic+/Oov7wi1UIDhlJtwygWxx2zR8n0ykdUrdOKdIu6VElystQo3QrB3L1Yg3FhmP+8hQBO0O\
DAo+YhPqcZUcBDbMkixYk3sjC4AE4xkuFB2cdudqC6Lacl83ySbXPMNRWVQhZiVnQ5MKNkDVLz01spmIFNs5WMCcB6ee84axsoQ2ayPuH5qKRdCpgqm4HqgkrAmwoNQTLQBftFug0XOYFNpRqFO3oVyLxbJbpt9/\
25+1JKLrgFBDxrHsCYcF2Tfqi+fgLxhGpqQYbXudXowsWyBoyugaDO3H7y4WiylBP812mtQwphhz7ASW8QmghW0T42i7Cf0XUYUoBTarU+BxBGBXJRHrIttW6IOMfRGTRtr04Pt9mPgiPoB/+ymIyhnYECtNH+jR\
aO7IUbblq9Nt5B/GxySJktXLUVszdpY14Z0JMNRT9Rc4EISihMTRyAlo0sky8UoP/VoQRDdMh1hc4vVOTGng5EpsRWF/8otY4haTDjrZrmNaKMhSPWOAGzppgGuBbCUQAsvAWSKlCZFB7J3CAGDOTsXNJaGbxh59\
YJDCFE49Phypl1P2psnBdXHKOoIK/BykaUCapZz0eEjlERHRuT8QAo9VfFoJmyMMa+gI4HtdsUiqDSKRMZZVfNRfG+fKmobXyf9gnZrHpOtj1j0rcfJCwrfEf0P14XddxQxPFXs8oKZNfy/ckfZ1+OJAqUbMnwBq\
fMkGAMjVdYObBH7dS7a1RTTUmlU3CMF6tpl5bLv07qNC47n4yq1oa/bmckKBlwlXau2hH1yx8a2RMBpOPI/Jx15ZiibRDthcq2A2IHFZMuw3Gw4Q+osgXqhkzo7XUlKPrB8IKYU6a3//gL16WObD2j9Tj/fh4b0L\
X+7Cl2X4sgpfQKI/M/jVqjMe2O8dm9FW6aPWMILVZXtGfGpEtcqLEe03PVrc3sBCs5aHbDzTS58jIM+1rP4DeKXxW3dAhrU8y1lONe2L4zeYqw9uPwTHhUQ+XiJJ851gJB7b5CPJXTNCS75DOna3Eh3NIVASP2Q3\
+iE3qfxAnV1qMH6Dru3xAwOIDXIJDCnceVW4di3oj4e/49Mz2l4o+ie4VCajMkMyHuPH45xQtWY2a5T8xXIzmyYHd5yTXMUrKDrKW+B4/p6X0aE44cu+F6V89IRccazVUDxUMqUIXNl7Wqaq3Nec9wPgwrAT2M++\
Xix/BMHAjNeCcz9wijny9lu2vEdOwYM17ddAwptd2AIkAUlv8pZEAlSAJReoI06SFUgSFBdtn5kp2g32P/ZpE65gNmEBnWsjKDSWwzxCB6bpCP/IsDHWta++PZ2eEbVcDICYETClAhxOeT68KF9KwBQhfTXIsDak\
Z6CopucSJr38cRiMIkkEvN2Lk9lesEIa1DBEiYSEgA3JLHmRyqc4T+9464ZR6Vpiysn7Q/RGJmGnpB2+UMtaan1D/yDAHPMygKAFsbMib6YoAHLe67pDu2/IkwPauSmak9C66ez3Ni4RyDi6Ub/nIGAWwo0mh4nT\
O0cwA6+pXsc5oEa+Q2dJO7wmk7A64vASg4dGQAQ2A41vtqQxlQTz8NmwtlJ1huhGAyrDOqgg+Px45j1svabiU0HgN6TgQHlYTXIqfBH7GPOAA3r9380oXJsNIrLZWpUqoaOqSjIlm04gDQaXiRDbJfQR1cYgOlVs\
cptxh5xKxfzV+py3DDoRjyyJB0LtUoeIt9n/OuorG8L64QhCIDhz0O06+aLHFgTR2f0atyNhp+sFpzSaTYHnGTs7jYP2sFPfXy6WN5e7lMODE9A2f6AVwFegjiGBMnt0zw2sFR3DGnfHUYuN07jbF0L09Oxy3o9V\
tN2eXULEBtAldQygGLSJ9BQobSg5b9s/3vQDWY1zcXcADSeE0joJ4x1YKvWuATqbZkc6d8iLte3cq6xS9z1BoeEVOT1IQEYQrtRD8ff2ZwkhSCdRQbKHqOXBNvsgnScgyhZhLZm/ZaFku9BoT7BT8KV96xetEPrm\
cadCWEAL6QGtx7ajB7d48P3ondScnICCHItmzUm3ZMmqt+RcUIuDsEIOb7Cxyma062FLoqddv5XhiyUSxAAB0R14OCj79MXJqT3QqztFoX37273ttOgk7P5Xlwug3UpZBvOhZCirRASlw2GKhuh8O+jU0qmJOJX2\
PVc3cDTk6CxIxGFAOhxwylEayuNe9HhOMYecis6vto8AMJabcukloRVwjaFdR9guFe2QWlyFjrFE0cxAkZJ7+Du6A6uPfK2qSq9IFQC4wFmUGALfHZM7xzJTyYGcCfwvPJCJG/1uLUp7Ad73nv0O5A4le3kHgXs+\
2O7yzq5UdkVVnbaZe8GX/LRYOVxLB68OKUc1+rUUVB8k5znnUJi2fzBUFrJByj1g5XojK8U6K1cU3oLMbHbH+EHgIyfBoGFseOpsGTlRYUo+OrB34M9mNlhLaf7UrZVJASYmFkpsJAStle5ChC493rRSKP5Co/dE\
BUkvYJ1IGElOSC9oymfe2YAnh7I3aRnrDFVvfyPltNkqUGcAzSo9OoVoRx+xfEMLoI1K2QhJXyV+q7slIRMgYMM10xK9yJRwHw4BIudG39QzsLsvPm3T0gWozOgziiP7tase37OX5hNxiy3DE8ywangLer+6gaXj\
GbsOiAELLiCH2XWhHt2fkTh7QQCIDM2wxlKSuaPJV6TuUJfGfrwoUSeHXMaUcsxYPCKXlWGs1hNoRDN/pp2Gka9kxU67ITm7UaxxqOOoY4YDUXDDBZbfEWG+C+/z6gDGE7HNvsCWFEAUJpV+c0XlRw1QmRzjwX4P\
tH4TBgc0JjQPzaipU8FvQU6MPw2YIyiHhRC6wqoZXsRostoharqlbzv1nM9CqN09lh1eSVoRc0o89kYyXBD9YxtkNiXIo6/UgBBo3s0E9H9K0U6R/G1x+0S1XdSMJtAMEIMoORgenAd431oHpcSmTwrpPeXsVq8O\
uMhdYFzCVVwAKryfSrYW97ukCBgiSWG3SZ5jjecJ2oCeGoGuDIu/LnC4B009JgWw7EurPKWgWnV19vY4iJjMJoNX82e9Q/DQZLOIta7OxbDfrGK+PwQpYt2h+QRvn1ljMn/3XclNWeM1CQO0qjNhytOx9ojeeQSB\
wG/QuKNt9NBF2+zqxisERvGNfCrwesYdx8nL8wn16TTUAfSdjlSp3LfdVcsjSbIwv7x+wJ1vmbT2EZz7HaX5Vp3kRI/ODihexRiuolIUZr7Zo4+pNVQ+wP/ofMqVgWbF0IGfO+O8CaKFcioxwB0BuhnddHSTMO7k\
oj4pQRMAhQ3XwhpQLNJLN6SRDw0BcNteSaWpS0bELbWYdUxO0NhJceafMGatfThLtZzdwA82nGeZwBCdCuw9cEaDwz5CP6yMMIu8B99V9oKDZMqAAzyrE6TqW3lN8fXjnK9Qmn48SPgjBZ4GlFWMF9SlXDPaLQqq\
UDxSNWqG6SvZc3QMMQHkrRqxGbPYaDsiVwEuBDeX86kqskXDRewhWKDLGvFFYUfxOrZh/huwWWOUAaTIHboPQEwM59BwIGutn1mJ+xrNwjyOyET3VVURDcOqKxNo+W61Gd/6igfm+M2fJIa/gp8K7RzvUNSx3C+m\
VNgDTNJc2ANtKOROKbwOKZKSQLIZ38hl1FZ3w7PHtwuZ2DfC7h5BLZgnZO/N+HLDFSRNBu5tKtwzx3j1teHYdHbeFfzxOvItXEfm53gdme/nE+BZnXFMWQo+kajYX3fSehBdeoC4gLZ2g23O5ULLYJrJbS6G983W\
rxwE0CX8fUkm3WBNRWE9K9vfFTQBHjE1xGT6hXdrJVtGg26teePvZa3e+olzkIImgpaCJpb6H1yN5XsaKyUDS3EStg1dg1Ep6ud1IVoMAiqzC9t98I6zZIW0eOMNXyV9qMiBlnDbbLgQCyMBTkDIlDZEdIHOr1gK\
jPyKWDAsySfhrZ6IuKH0s2z4blR+HRHcKorMKATY+YnVkOm2KJ0D/DmS/hIOZ98QxIMOYlDaam5wxFpnB1xDB62Dq5Oa9YXqawdsP6hl+dn1PPg9iH1JqEWY/ETy1WwwRTvdhrmioebge0BxAQKjt8KvxcH1/N/y\
dT++SggDyy7oOt6AXOCUEfH1X9e/Gj0dds5vux1AxaEAAL/p8Ju8/DN4zNYHYLurkbCS9W/Qut+ZiS52WQJX6uinIHTzbTFw4vt8DAdzHxcptLTRE3kdHyzu0nCV78Z0AmaQzq7dJKuDZ7IvWmgsC7dQ9KE94+5G\
es//WItIw9HP+V5n0211LT8iE9ay3tTYk9KX1d5RhD9m/OnjsryHnzRqlafjxAkxdV+a2+X9p65Tp0XmOutyWQa/feSa/x5/CRcaZWo8TtPP/wO9arcm\
""")))
ESP32S2ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
+8a6uqlW/iNGW+9t85ceoTxNlco+/xcolYoO\
""")))
ESP32S3BETA2ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32C3ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
eNrFWmt328YR/SuypEix29PugngqtUw6lChKlmvnpFGcA6UGdgHVSatTyVQtt+V/7955ECAl0vnWD3wBi93ZmTt3Hsv/7M+a+9n+wVa9X94bW97b8Kqz8B0v8/60vHd5+DYo76uivM/p6l64WL0Ob+l34S0Ol9Lw\
2WyHNydPx/R0ed/6dxnN8SK8mVdh/sEsXMXttrwt7xsTfkXDerwTFsh3WYY6mpb3Phq/nG6HZ01ShYWj8Apj83wY3gblfnmNFTDfXZghofloVJHNw9WwQBNktkX40oY7Lghft1m5T3L99yyM82F8zc+2bZatuaFL\
j1g1tNPw8j6jnfJ8ED4VnUUL5YVXUF0RXm6Az2/nIkt+gT0OIfxRt44Jn3kxYhU8vmjxYi5L8wrDngy6Uvfbum9hAJ49rLc0tVqbjJaNoaFgomLAUwejBjMV7kxtSyYnQ75mWerwtMvOsyfhIwjubYbtsOX5aZuf\
hGF+EiDhw4hGYGHdGJdZx8BUMxhNaR8zkcvIIAgD+znRZpWwzu/bTDRmus020F5bllDBzlYm2+trwDrIMyAQ70OzjrEd1joMCxU7uwI/wqMAUMC4hEGTLOQ1okcnXmMHPeM7aHAU3m3N8uFqblkVeTLBbTz6BNAH\
hAyDAMPa4uIQg/qrQpNwwEYWyItjXAnb83Kl9Re8X8gC9cEqNKXtZKsNHA8S02QDxhjkaxtMOlFv6cEhPIJ9pOJqRsBmuyG649Z2qwM6nlYjwWipiNHQuodTQLB/8MJ1dkSIFAfAiDw5Io2TL7x94aayM2Axm/Tw\
i10pZPJBzzOFxzxG40ePOl5iyIjHW5GcdGdwJ8WdKFioiGTqPAXR7Agudfv9yQt71YlDy8fdItBlwzgmvsCcRJFqowXkMUtdYTEh4d5miGhT2fl6HdBvYRATY+mg4AY/jPAWb0kQ6PUHNstLPeE7AZNsRCM4gBFX\
eUWm9P0pGxHYCzK/xDxENvg6UfrpM0+fZQA4sISAsgKAY6YVRxZ5OpY78N9iD36OGd4xxplqTtxfwjQd87hL7BWmKWfPiXBPhnuiw4adshDdgj/ALeTRhsNZ44QTEt3VkFyjpOHlkBUUZihJyHIsrBTj4nDMEfUL\
HHSb3LGjaMjuReC7b4RpXfPi7xok8fiYZV0nOxTW1MJMNmFKqKMJ74Kwmm0dsg0a+00/zkwUjMNH1vPMzMy3cPPoaJ0Q4HFQTwhizIaaHQgX9lQw0lRFYW8SRUsaYJc3Hex6DjFj2YBO1ycyosmrOTs9khhEbCd3\
gIXcHzDMACR8Iv6By3MMrnf4BhPhCIGgYG5jTbxhblz1lZXwvhw84KhG4kakEyl7b7KhH3SjHY3u1rVCI/zclUwCjUSrAYLYOu8Cw8YlbbckEz60W2cj9fMYLh3pVp8x2mtx5Jos4Vi6IuZlgszbyAGi8WragpzJ\
yCiKAPER4hhxy90NmMLOP+LWB6yc7IkxwxZ8wxv1SElAUpVIlM0BDFi2C0czVkYu5KusA8Hb6pAFZVbZZnAq+axTVIW8eXlyk/zSrUAkUE3CduuCmSRAshdg6QVBC9E0i7JGxEJEXH68txpMmmx/ic52ZISzR/gC\
P8M3k7hOYXU1fAZTVePyOvBUm75rf4KCf5ouvBX0WbiPGKSG+3DKlApD5MnrL/MqQAYgNH7pyY1beM/RDtGkKJRnd0efe9laAmnShulUQazZzFp/cdHNzfzjD/D9n0BAPwNFtbAKxb9tMN8e3PEUsiN4tSiXPOQZ\
/ML00AgqKzGjqnRJjugPnCzXUGWdMueAE1y0ad9greTKwYM/8Yx54hU/uRZpmyG+UblbIIj5v9dPSNhzB8ygBDj3cHBY9ZYfIZ7VXMdOD6UUS1YTqWzFZyhNsJLseUQdrNNGIZc0tbthfzKDG6TZUkVwHAcqk7fw\
pCR7zZiqvowpflnhlAKFcFG42QdGSq0kWiDzR1aRZ/07vxuxxXOScfBJKl66siTDD1/AtYsuOaP6rQpCjHKs9zZH8JoirJ2HqxhT6J32Cyav47mkR9BAEvOPpuLHvE/dnzvsIog00XMppNcLShfJ7N9xYn3NaTUJ\
moAuta5YJKEo8TUJtXbCF/OcMwhqMPx/2XNYzpqkz54+VPvJqmtvpjwwyQrVvnn2PUD3fXn9DvJPfwG7VKenZ7h59uwVbr4qr8/Bw5fnvaq1zi5G03c3nQWQ0WGzgdAPxSOEVCvEyFgS+Yjp0xse4ySGOomhdK9i\
8YFryg4jTmJRZICjAA5nrnnApjyicprR5S+yTyv1jKZGmqbgWo0nGqmJbZTAGvbob5z3VRhpk4SBs6ixluujJ1eHhLgtSXG1rkryoTbC8I0fmnBKZOyx1qRqy0WTyyZHNJfEubVu1LquyVaWk4e4qGlKVQDtL6OU\
sGTRN+GmrTYQh6lFydw0cn+VvVRsTFHx9aLBJs0ByAd/I7mBR0sXE/7CrY2jTdkPyBmucJWdycbQqbArXRPkvHnSjlq1Q9dkocwjiDpD1KVukf81e6VWXadocD4Rda0BicG7aA9ksu+c94f4TNfj7nooibiuvZaq\
lqjSsqEwGEqxnJPOMJTKJ7lfDZbH4+XTp9TNcdoXO6efqfxMBbyVPfqaVVIkVhg3izvZXbYrMLPtNnR4yaXWwmXAH6i72rrXgPsNlcR7LnKWy4hNpi16ExNzNZsRSs2pAjG5zrT1pf0Ky9+J8RPdaNz1WBa7rqRD\
4QupMDUMoHWB1cgPqJXovj2E1j9wmucKNRIbZcqXly2ZUHcNXpF9DSC0KW/LctsNG8uixY28vKXLtU7hfpU7cTmjO+Q725TadNGG2g6t9qKoLf4h1Z8Isu3b9HMhsppVEZ8TamizQ4c2ZxaRFhbQeg1xJ9IlbLI3\
5a1qSDhlMWfyGKyscByHCVF4ImmHlwoXpQ+12T3bq7C03ZjK7GsA6VjWlL4QrOJgKpMcSXeUqNeJsXx+srOrFIoCpKm0G8aZcsWmQQTKtpxQXirh1B93O/SaQgy+OqXKHHzjNAfYddIUpO12VnkvsiSMiortnUtS\
Xy0WiGQB6odAyWSkjDsd3iEU568QSU/CN3MC2hljovFmV8pb1McFmJV7IFIuU/zPzlUYQVmtKBsdC4PQjXQBP4T+BzoBLbHIUF22J/s3diydX8cqbqSTF08VN1kfNxNlYjTTfN0j1EJK+oL0u73DroPQo23YKqYF\
vxL+oLbiFrWZc+lCWkwAFl3YVnqPhtvRyFm9sJzesoveRSF9cbTjeK9GgUZZ1dEzxfVU+vdep5HpAfTspV4cLiYWpVDK26kA4ETrJUB41BWOsktxiKafwsRdmZnbSbcFogRI7bw80Sjvfd/30bFGxmmf+vmeF3hF\
D+7IoUWzQp2UPIsbNXknoRUJq4QVZJLxufRc12S94W0QK3TfSpdywME/lz07Q03PK2YR1UhBSYkdyfz2AO7kR2jjFH7OFW3r55q/+yFfanJJDWrK9KQvVwhIXbYU1+Y0xzUHrEZSw3rw6Ss5+iEPHn3o1nDXUhyZ\
+efu6mew1MaVwEqhNLZg5eKU12u1Kbbpscp8lsfYBrsjgWcrrKlxUSh0LOWQl3Mx0+cZXxyI7doHDyTdA4U7cewpGuQcDstoCpNEbCZOx/Rp8aWmWTwd9Z6GfXUK6W1TPoLwVERcic70BCKW9oacjtWZHFi0i5h/\
zBeoyE+kvrbS5PUCTOKv5EjmgC9LiBGB5byFvBb7WmSrj9vu7rkegX0S1jByjlkDrZeTXoSMO2STGK2/xKJ3cr6ZnIzXr5SDlawb/wg0x8M53FHO+fSopDBCQq539gkU5K3me7n4eiiVR+y4Xo/w7OSJnLfi1JwS\
GmLmJumSzbBJPXBSzhl8rWOl3iGOqtU0lJaLP7T50HHZrfzWSJ5NnbAGIdxzMN+VPrkRZxismNqa110CWCiDUQIh+6mWrSonE0Rng91OZCsnQIuqnngsrH8tSQV9IsbykbP4VC8L2OItIP5Q/6ifteXCiu6xm4Xp\
PWlXbpr+k3Y6kRSTyjvnNEDFRyuJLPWH1ubBSyC0D/NiKge8ZNWtHGA/CNXBs24lA4fyBm6MBp91P/JhvpZp/yRFsh2borM3R3o5rRx0FjRIkavF8YKEeK1HnF4f9uM2bVnyBBc/zEwR8OJLTgEfd6ytn7s/rfAO\
R5QufaRUb5s5nWBKwRtd1S5cG+JrKw7KeppRGP+hOyhU/XFwcG8IxAFVTdtl9d7ccbJMcyc6d5527QO12fpw8I4SrOXDRNF/W/RlmUpwreRsGkpfHqG64Ibf/oiK2qebBWjMxVMC4KzpNSmSm55+1UBmA885M136\
YxEC6Fl3ht3/I4Ul9VxIV9cSmO8Yyfq/EUW2/p2DheEsYCBHjlnXX3tcsafdfnJzuDZlOJb+dfx4Kq/2Llo2Jnl23F2v5Pwht/rXhn/piaUiKGWv2dClYp/M20Nuvi2wR2ETf3gKE15zk4776SF31E5/3K1gkhM5\
MW8577o9eYmW3YH060jkP1KdOolO5D8YDKSA1VJaEZKnahPF6msgxIyItk7rjP/ogna/L4kFsOOGYk3kMZTaedkw9WGjC8QOCvZUD+mBQiInsKb3DwKRiw4PPM729SiZ/k1BHFNw/GlS6XYvOW2x0/U0yDMSDg6S\
Rsy4sdmIbrFkFXXLS6TpOygsr8UtLe/kMaNNrcc57PlDn3t85LDY5Hh/YoeoK0nMiPZyCVbNRhEg48FvchMQhZZpMR/WEabTN3toWKcoXVK0rNNpi5Z1enoM90/P9lAnp69wG5VFdNksmtb7v9+ivyf+9eOsusWf\
FK3JstjaPDbhTnM9u/28uDgYRLjoq1ml/2YEpIIv7cvl/izGpnFh4vn/AOixonk=\
""")))


def _main():
    try:
        main()
    except FatalError as e:
        print('\nA fatal error occurred: %s' % e)
        sys.exit(2)


if __name__ == '__main__':
    _main()
