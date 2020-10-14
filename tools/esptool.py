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
import itertools
import io
import os
import shlex
import struct
import sys
import time
import zlib
import string

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


__version__ = "3.0-dev"

MAX_UINT32 = 0xffffffff
MAX_UINT24 = 0xffffff

DEFAULT_TIMEOUT = 3                   # timeout for most flash operations
START_FLASH_TIMEOUT = 20              # timeout for starting flash (may perform erase)
CHIP_ERASE_TIMEOUT = 120              # timeout for full chip erase
MAX_TIMEOUT = CHIP_ERASE_TIMEOUT * 2  # longest any command can run
SYNC_TIMEOUT = 0.1                    # timeout for syncing with bootloader
MD5_TIMEOUT_PER_MB = 8                # timeout (per megabyte) for calculating md5sum
ERASE_REGION_TIMEOUT_PER_MB = 30      # timeout (per megabyte) for erasing a region
MEM_END_ROM_TIMEOUT = 0.05            # special short timeout for ESP_MEM_END, as it may never respond
DEFAULT_SERIAL_WRITE_TIMEOUT = 10     # timeout for serial port write
DEFAULT_CONNECT_ATTEMPTS = 7          # default number of times to try connection


def timeout_per_mb(seconds_per_mb, size_bytes):
    """ Scales timeouts which are size-specific """
    result = seconds_per_mb * (size_bytes / 1e6)
    if result < DEFAULT_TIMEOUT:
        return DEFAULT_TIMEOUT
    return result


DETECTED_FLASH_SIZES = {0x12: '256KB', 0x13: '512KB', 0x14: '1MB',
                        0x15: '2MB', 0x16: '4MB', 0x17: '8MB', 0x18: '16MB'}


def check_supported_function(func, check_func):
    """
    Decorator implementation that wraps a check around an ESPLoader
    bootloader function to check if it's supported.

    This is used to capture the multidimensional differences in
    functionality between the ESP8266 & ESP32/32S2/32S3 ROM loaders, and the
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
    """ Attribute for a function only supported by software stubs or ESP32/32S2/32S3 ROM """
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

    # Commands supported by ESP32-S2/S3 ROM bootloader only
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

            for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM, ESP32S3BETA2ROM]:
                if chip_magic_value == cls.CHIP_DETECT_MAGIC_VALUE:
                    # don't connect a second time
                    inst = cls(detect_port._port, baud, trace_enabled=trace_enabled)
                    inst._post_connect()
                    print(' %s' % inst.CHIP_NAME, end='')
                    return inst
        except UnsupportedCommandError:
            raise FatalError("Unsupported Command Error received. Probably this means Secure Download Mode is enabled, " +
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
              + (packet.replace(b'\xdb',b'\xdb\xdd').replace(b'\xc0',b'\xdb\xdc')) \
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
                    for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM, ESP32S3BETA2ROM]:
                        if chip_magic_value == cls.CHIP_DETECT_MAGIC_VALUE:
                            actually = cls
                            break
                    if actually is None:
                        print(("WARNING: This chip doesn't appear to be a %s (chip magic value 0x%08x). " +
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

    def read_reg(self, addr):
        """ Read memory address in target """
        # we don't call check_command here because read_reg() function is called
        # when detecting chip type, and the way we check for success (STATUS_BYTES_LENGTH) is different
        # for different chip types (!)
        val, data = self.command(self.ESP_READ_REG, struct.pack('<I', addr))
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
                    raise FatalError(("Software loader is resident at 0x%08x-0x%08x. " +
                                      "Can't load binary at overlapping address range 0x%08x-0x%08x. " +
                                      "Either change binary loading address, or use the --no-stub " +
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
        if isinstance(self, (ESP32S2ROM, ESP32S3BETA2ROM)) and not self.IS_STUB:
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
        if isinstance(self, ESP32S2ROM) and not self.IS_STUB:
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

    def parse_flash_size_arg(self, arg):
        try:
            return self.FLASH_SIZES[arg]
        except KeyError:
            raise FatalError("Flash size '%s' is not supported by this chip type. Supported sizes: %s"
                             % (arg, ", ".join(self.FLASH_SIZES.keys())))

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
        if isinstance(self, (ESP32S2ROM, ESP32S3BETA2ROM)) and not self.IS_STUB:
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

        # following two registers are ESP32 & 32S2 only
        if self.SPI_MOSI_DLEN_OFFS is not None:
            # ESP32/32S2 has a more sophisticated way to set up "user" commands
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
        '512KB':0x00,
        '256KB':0x10,
        '1MB':0x20,
        '2MB':0x30,
        '4MB':0x40,
        '2MB-c1': 0x50,
        '4MB-c1':0x60,
        '8MB':0x80,
        '16MB':0x90,
    }

    BOOTLOADER_FLASH_OFFSET = 0

    MEMORY_MAP = [[0x3FF00000, 0x3FF00010, "DPORT"],
                  [0x3FFE8000, 0x40000000, "DRAM"],
                  [0x40100000, 0x40108000, "IRAM"],
                  [0x40201010, 0x402E1010, "IROM"]]

    def get_efuses(self):
        # Return the 128 bits of ESP8266 efuse as a single Python integer
        return (self.read_reg(0x3ff0005c) << 96 |
                self.read_reg(0x3ff00058) << 64 |
                self.read_reg(0x3ff00054) << 32 |
                self.read_reg(0x3ff00050))

    def get_chip_description(self):
        efuses = self.get_efuses()
        is_8285 = (efuses & ((1 << 4) | 1 << 80)) != 0  # One or the other efuse bit is set for ESP8285
        return "ESP8285" if is_8285 else "ESP8266EX"

    def get_chip_features(self):
        features = ["WiFi"]
        if self.get_chip_description() == "ESP8285":
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
        '1MB':0x00,
        '2MB':0x10,
        '4MB':0x20,
        '8MB':0x30,
        '16MB':0x40
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


class ESPBOOTLOADER(object):
    """ These are constants related to software ESP bootloader, working with 'v2' image files """

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
                raise FatalError('Cannot place SHA256 digest on segment boundary' +
                                 '(elf_sha256_offset=%d, file_pos=%d, segment_size=%d)' %
                                 (self.elf_sha256_offset, file_pos, segment_len))
            if segment_data[patch_offset:patch_offset + self.SHA256_DIGEST_LEN] != b'\x00' * self.SHA256_DIGEST_LEN:
                raise FatalError('Contents of segment at SHA256 digest offset 0x%x are not all zero. Refusing to overwrite.' %
                                 self.elf_sha256_offset)
            assert(len(self.elf_sha256) == self.SHA256_DIGEST_LEN)
            # offset relative to the data part
            patch_offset -= self.SEG_HEADER_LEN
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
            # use case then let us know (we can merge segments here, but as a rule you probably
            # want to merge them in your linker script.)
            if len(flash_segments) > 0:
                last_addr = flash_segments[0].addr
                for segment in flash_segments[1:]:
                    if segment.addr // self.IROM_ALIGN == last_addr // self.IROM_ALIGN:
                        raise FatalError(("Segment loaded at 0x%08x lands in same 64KB flash mapping as segment loaded at 0x%08x. " +
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
            print(("Unexpected chip id in image. Expected %d but value was %d. " +
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
        def join_byte(ln,hn):
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


class ELFFile(object):
    SEC_TYPE_PROGBITS = 0x01
    SEC_TYPE_STRTAB = 0x03

    LEN_SEC_HEADER = 0x28

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
            (ident,_type,machine,_version,
             self.entrypoint,_phoff,shoff,_flags,
             _ehsize, _phentsize,_phnum, shentsize,
             shnum,shstrndx) = struct.unpack("<16sHHLLLLLHHHHHH", f.read(LEN_FILE_HEADER))
        except struct.error as e:
            raise FatalError("Failed to read a valid ELF header from %s: %s" % (self.name, e))

        if byte(ident, 0) != 0x7f or ident[1:4] != b'ELF':
            raise FatalError("%s has invalid ELF magic header" % self.name)
        if machine != 0x5e:
            raise FatalError("%s does not appear to be an Xtensa ELF file. e_machine=%04x" % (self.name, machine))
        if shentsize != self.LEN_SEC_HEADER:
            raise FatalError("%s has unexpected section header entry size 0x%x (not 0x%x)" % (self.name, shentsize, self.LEN_SEC_HEADER))
        if shnum == 0:
            raise FatalError("%s has 0 section headers" % (self.name))
        self._read_sections(f, shoff, shnum, shstrndx)

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
            name_offs,sec_type,_flags,lma,sec_offs,size = struct.unpack_from("<LLLLLL", section_header[offs:])
            return (name_offs, sec_type, lma, size, sec_offs)
        all_sections = [read_section_header(offs) for offs in section_header_offsets]
        prog_sections = [s for s in all_sections if s[1] == ELFFile.SEC_TYPE_PROGBITS]

        # search for the string table section
        if not (shstrndx * self.LEN_SEC_HEADER) in section_header_offsets:
            raise FatalError("ELF file has no STRTAB section at shstrndx %d" % shstrndx)
        _,sec_type,_,sec_size,sec_offs = read_section_header(shstrndx * self.LEN_SEC_HEADER)
        if sec_type != ELFFile.SEC_TYPE_STRTAB:
            print('WARNING: ELF file has incorrect STRTAB section type 0x%02x' % sec_type)
        f.seek(sec_offs)
        string_table = f.read(sec_size)

        # build the real list of ELFSections by reading the actual section names from the
        # string table section, and actual data for each section from the ELF file itself
        def lookup_string(offs):
            raw = string_table[offs:]
            return raw[:raw.index(b'\x00')]

        def read_data(offs,size):
            f.seek(offs)
            return f.read(size)

        prog_sections = [ELFSection(lookup_string(n_offs), lma, read_data(offs, size)) for (n_offs, _type, lma, size, offs) in prog_sections
                         if lma != 0 and size > 0]
        self.sections = prog_sections

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
        print("Warning: Image file at 0x%x is not a valid %s image, so not changing any flash settings." % (address,esp.CHIP_NAME))
        return image

    if args.flash_mode != 'keep':
        flash_mode = {'qio':0, 'qout':1, 'dio':2, 'dout': 3}[args.flash_mode]

    flash_freq = flash_size_freq & 0x0F
    if args.flash_freq != 'keep':
        flash_freq = {'40m':0, '26m':1, '20m':2, '80m': 0xf}[args.flash_freq]

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

    # For encrypt option we do few sanity checks before actual flash write
    if args.encrypt:
        do_write = True

        if not esp.secure_download_mode:
            if esp.get_encrypted_download_disabled():
                raise FatalError("This chip has encrypt functionality in UART download mode disabled. "
                                 + "This is the Flash Encryption configuration for Production mode instead of Development mode.")

            crypt_cfg_efuse = esp.get_flash_crypt_config()

            if crypt_cfg_efuse is not None and crypt_cfg_efuse != 0xF:
                print('Unexpected FLASH_CRYPT_CONFIG value: 0x%x' % (crypt_cfg_efuse))
                do_write = False

            enc_key_valid = esp.is_flash_encryption_key_valid()

            if not enc_key_valid:
                print('Flash encryption key is not programmed')
                do_write = False

        for address, argfile in args.addr_filename:
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
            argfile.seek(0,2)  # seek to end
            if address + argfile.tell() > flash_end:
                raise FatalError(("File %s (length %d) at offset %d will not fit in %d bytes of flash. " +
                                  "Use --flash-size argument, or change flashing address.")
                                 % (argfile.name, argfile.tell(), address, flash_end))
            argfile.seek(0)

    if args.erase_all:
        erase_flash(esp, args)

    if args.encrypt and args.compress:
        print('\nWARNING: - compress and encrypt options are mutually exclusive ')
        print('Will flash uncompressed')
        args.compress = False

    for address, argfile in args.addr_filename:
        if args.no_stub:
            print('Erasing flash...')
        image = pad_to(argfile.read(), esp.FLASH_ENCRYPTED_WRITE_ALIGN if args.encrypt else 4)
        if len(image) == 0:
            print('WARNING: File %s is empty' % argfile.name)
            continue
        image = _update_image_flash_params(esp, address, args, image)
        calcmd5 = hashlib.md5(image).hexdigest()
        uncsize = len(image)
        if args.compress:
            uncimage = image
            image = zlib.compress(uncimage, 9)
            ratio = uncsize / len(image)
            blocks = esp.flash_defl_begin(uncsize, len(image), address)
        else:
            ratio = 1.0
            blocks = esp.flash_begin(uncsize, address, begin_rom_encrypted=args.encrypt)
        argfile.seek(0)  # in case we need it again
        seq = 0
        written = 0
        t = time.time()
        while len(image) > 0:
            print_overwrite('Writing at 0x%08x... (%d %%)' % (address + seq * esp.FLASH_WRITE_SIZE, 100 * (seq + 1) // blocks))
            sys.stdout.flush()
            block = image[0:esp.FLASH_WRITE_SIZE]
            if args.compress:
                esp.flash_defl_block(block, seq, timeout=DEFAULT_TIMEOUT * ratio * 2)
            else:
                # Pad the last block
                block = block + b'\xff' * (esp.FLASH_WRITE_SIZE - len(block))
                if args.encrypt:
                    esp.flash_encrypt_block(block, seq)
                else:
                    esp.flash_block(block, seq)
            image = image[esp.FLASH_WRITE_SIZE:]
            seq += 1
            written += len(block)
        t = time.time() - t
        speed_msg = ""
        if args.compress:
            if t > 0.0:
                speed_msg = " (effective %.1f kbit/s)" % (uncsize / t * 8 / 1000)
            print_overwrite('Wrote %d bytes (%d compressed) at 0x%08x in %.1f seconds%s...' % (uncsize, written, address, t, speed_msg), last_line=True)
        else:
            if t > 0.0:
                speed_msg = " (%.1f kbit/s)" % (written / t * 8 / 1000)
            print_overwrite('Wrote %d bytes at 0x%08x in %.1f seconds%s...' % (written, address, t, speed_msg), last_line=True)

        if not args.encrypt and not esp.secure_download_mode:
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
        if args.compress:
            esp.flash_defl_finish(False)
        else:
            esp.flash_finish(False)

    if args.verify:
        print('Verifying just-written flash...')
        print('(This option is deprecated, flash contents are now always read back after flashing.)')
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
        seg_name = ", ".join([seg_range[2] for seg_range in image.ROM_LOADER.MEMORY_MAP if seg_range[0] <= seg.addr < seg_range[1]])
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
    elif args.version == '1':  # ESP8266
        image = ESP8266ROMFirmwareImage()
    else:
        image = ESP8266V2FirmwareImage()
    image.entrypoint = e.entrypoint
    image.segments = e.sections  # ELFSection is a subclass of ImageSegment
    image.flash_mode = {'qio':0, 'qout':1, 'dio':2, 'dout': 3}[args.flash_mode]
    image.flash_size_freq = image.ROM_LOADER.FLASH_SIZES[args.flash_size]
    image.flash_size_freq += {'40m':0, '26m':1, '20m':2, '80m': 0xf}[args.flash_freq]

    if args.elf_sha256_offset:
        image.elf_sha256 = e.sha256()
        image.elf_sha256_offset = args.elf_sha256_offset

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


def version(args):
    print(__version__)

#
# End of operations functions
#


def main(custom_commandline=None):
    """
    Main function for esptool

    custom_commandline - Optional override for default arguments parsing (that uses sys.argv), can be a list of custom arguments
    as strings. Arguments and their values need to be added as individual items to the list e.g. "-b 115200" thus
    becomes ['-b', '115200'].
    """
    parser = argparse.ArgumentParser(description='esptool.py v%s - ESP8266 ROM Bootloader Utility' % __version__, prog='esptool')

    parser.add_argument('--chip', '-c',
                        help='Target chip type',
                        type=lambda c: c.lower().replace('-', ''),  # support ESP32-S2, etc.
                        choices=['auto', 'esp8266', 'esp32', 'esp32s2', 'esp32s3beta2'],
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
        parent.add_argument('--spi-connection', '-sc', help='ESP32-only argument. Override default SPI Flash connection. ' +
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
    parser_write_mem.add_argument('mask', help='Mask of bits to write', type=arg_auto_int)

    def add_spi_flash_subparsers(parent, is_elf2image):
        """ Add common parser arguments for SPI flash properties """
        extra_keep_args = [] if is_elf2image else ['keep']
        auto_detect = not is_elf2image

        if auto_detect:
            extra_fs_message = ", detect, or keep"
        else:
            extra_fs_message = ""

        parent.add_argument('--flash_freq', '-ff', help='SPI Flash frequency',
                            choices=extra_keep_args + ['40m', '26m', '20m', '80m'],
                            default=os.environ.get('ESPTOOL_FF', '40m' if is_elf2image else 'keep'))
        parent.add_argument('--flash_mode', '-fm', help='SPI Flash mode',
                            choices=extra_keep_args + ['qio', 'qout', 'dio', 'dout'],
                            default=os.environ.get('ESPTOOL_FM', 'qio' if is_elf2image else 'keep'))
        parent.add_argument('--flash_size', '-fs', help='SPI Flash size in MegaBytes (1MB, 2MB, 4MB, 8MB, 16M)'
                            ' plus ESP8266-only (256KB, 512KB, 2MB-c1, 4MB-c1)' + extra_fs_message,
                            action=FlashSizeAction, auto_detect=auto_detect,
                            default=os.environ.get('ESPTOOL_FS', 'detect' if auto_detect else '1MB'))
        add_spi_connection_arg(parent)

    parser_write_flash = subparsers.add_parser(
        'write_flash',
        help='Write a binary blob to flash')

    parser_write_flash.add_argument('addr_filename', metavar='<address> <filename>', help='Address followed by binary filename, separated by space',
                                    action=AddrFilenamePairAction)
    parser_write_flash.add_argument('--erase-all', '-e',
                                    help='Erase all regions of flash (not just write areas) before programming',
                                    action="store_true")

    add_spi_flash_subparsers(parser_write_flash, is_elf2image=False)
    parser_write_flash.add_argument('--no-progress', '-p', help='Suppress progress output', action="store_true")
    parser_write_flash.add_argument('--verify', help='Verify just-written data on flash ' +
                                    '(mostly superfluous, data is read back during flashing)', action='store_true')
    parser_write_flash.add_argument('--encrypt', help='Apply flash encryption when writing data (required correct efuse settings)',
                                    action='store_true')
    parser_write_flash.add_argument('--ignore-flash-encryption-efuse-setting', help='Ignore flash encryption efuse settings ',
                                    action='store_true')

    compress_args = parser_write_flash.add_mutually_exclusive_group(required=False)
    compress_args.add_argument('--compress', '-z', help='Compress data in transfer (default unless --no-stub is specified)',action="store_true", default=None)
    compress_args.add_argument('--no-compress', '-u', help='Disable data compression during transfer (default if --no-stub is specified)',action="store_true")

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
    parser_elf2image.add_argument('--version', '-e', help='Output image version', choices=['1','2'], default='1')
    parser_elf2image.add_argument('--min-rev', '-r', help='Minimum chip revision', choices=['0','1','2','3'], default='0')
    parser_elf2image.add_argument('--secure-pad', action='store_true',
                                  help='Pad image so once signed it will end on a 64KB boundary. For Secure Boot v1 images only.')
    parser_elf2image.add_argument('--secure-pad-v2', action='store_true',
                                  help='Pad image to 64KB, so once signed its signature sector will start at the next 64K block. '
                                  'For Secure Boot v2 images only.')
    parser_elf2image.add_argument('--elf-sha256-offset', help='If set, insert SHA256 hash (32 bytes) of the input ELF file at specified offset in the binary.',
                                  type=arg_auto_int, default=None)

    add_spi_flash_subparsers(parser_elf2image, is_elf2image=True)

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
    parser_read_status.add_argument('--bytes', help='Number of bytes to read (1-3)', type=int, choices=[1,2,3], default=2)

    parser_write_status = subparsers.add_parser(
        'write_flash_status',
        help='Write SPI flash status register')

    add_spi_connection_arg(parser_write_status)
    parser_write_status.add_argument('--non-volatile', help='Write non-volatile bits (use with caution)', action='store_true')
    parser_write_status.add_argument('--bytes', help='Number of status bytes to write (1-3)', type=int, choices=[1,2,3], default=2)
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
    add_spi_flash_subparsers(parser_verify_flash, is_elf2image=False)

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

    subparsers.add_parser(
        'version', help='Print esptool version')

    subparsers.add_parser('get_security_info', help='Get some security-related data')

    # internal sanity check - every operation matches a module function of the same name
    for operation in subparsers.choices.keys():
        assert operation in globals(), "%s should be a module function" % operation

    expand_file_arguments()

    args = parser.parse_args(custom_commandline)

    print('esptool.py v%s' % __version__)

    # operation function can take 1 arg (args), 2 args (esp, arg)
    # or be a member function of the ESPLoader class.

    if args.operation is None:
        parser.print_help()
        sys.exit(1)

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
            if list_ports is None:
                raise FatalError("Listing all serial ports is currently not available on this operating system version. "
                                 "Specify the port when running esptool.py")
            ser_list = sorted(ports.device for ports in list_ports.comports())
            print("Found %d serial ports" % len(ser_list))
        else:
            ser_list = [args.port]
        esp = None
        for each_port in reversed(ser_list):
            print("Serial port %s" % each_port)
            try:
                if args.chip == 'auto':
                    esp = ESPLoader.detect_chip(each_port, initial_baud, args.before, args.trace,
                                                args.connect_attempts)
                else:
                    chip_class = {
                        'esp8266': ESP8266ROM,
                        'esp32': ESP32ROM,
                        'esp32s2': ESP32S2ROM,
                        'esp32s3beta2': ESP32S3BETA2ROM,
                    }[args.chip]
                    esp = chip_class(each_port, initial_baud, args.trace)
                    esp.connect(args.before, args.connect_attempts)
                break
            except (FatalError, OSError) as err:
                if args.port is not None:
                    raise
                print("%s failed to connect: %s" % (each_port, err))
                esp = None
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

        esp._port.close()

    else:
        operation_func(args)


def expand_file_arguments():
    """ Any argument starting with "@" gets replaced with all values read from a text file.
    Text file arguments can be split by newline or by space.
    Values are added "as-is", as if they were specified in this order on the command line.
    """
    new_args = []
    expanded = False
    for arg in sys.argv:
        if arg.startswith("@"):
            expanded = True
            with open(arg[1:],"r") as f:
                for line in f.readlines():
                    new_args += shlex.split(line)
        else:
            new_args.append(arg)
    if expanded:
        print("esptool.py %s" % (" ".join(new_args[1:])))
        sys.argv = new_args


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
                values = tuple(int(v,0) for v in values)
            except ValueError:
                raise argparse.ArgumentError(self, '%s is not a valid argument. All pins must be numeric values' % values)
            if any([v for v in values if v > 33 or v < 0]):
                raise argparse.ArgumentError(self, 'Pin numbers must be in the range 0-33.')
            # encode the pin numbers as a 32-bit integer with packed 6-bit values, the same way ESP32 ROM takes them
            # TODO: make this less ESP32 ROM specific somehow...
            clk,q,d,hd,cs = values
            value = (hd << 24) | (cs << 18) | (d << 12) | (q << 6) | clk
        else:
            raise argparse.ArgumentError(self, '%s is not a valid spi-connection value. ' +
                                         'Values are SPI, HSPI, or a sequence of 5 pin numbers CLK,Q,D,HD,CS).' % value)
        setattr(namespace, self.dest, value)


class AddrFilenamePairAction(argparse.Action):
    """ Custom parser class for the address/filename pairs passed as arguments """
    def __init__(self, option_strings, dest, nargs='+', **kwargs):
        super(AddrFilenamePairAction, self).__init__(option_strings, dest, nargs, **kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        # validate pair arguments
        pairs = []
        for i in range(0,len(values),2):
            try:
                address = int(values[i],0)
            except ValueError:
                raise argparse.ArgumentError(self,'Address "%s" must be a number' % values[i])
            try:
                argfile = open(values[i + 1], 'rb')
            except IOError as e:
                raise argparse.ArgumentError(self, e)
            except IndexError:
                raise argparse.ArgumentError(self,'Must be pairs of an address and the binary filename to write there')
            pairs.append((address, argfile))

        # Sort the addresses and check for overlapping
        end = 0
        for address, argfile in sorted(pairs):
            argfile.seek(0,2)  # seek to end
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
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\
""")))
ESP32ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32S2ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32S3BETA2ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))


def _main():
    try:
        main()
    except FatalError as e:
        print('\nA fatal error occurred: %s' % e)
        sys.exit(2)


if __name__ == '__main__':
    _main()
