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
    functionality between the ESP8266 & ESP32/32S2 ROM loaders, and the
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
    """ Attribute for a function only supported by software stubs or ESP32/32S2 ROM """
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

    # Commands supported by ESP32-S2 ROM bootloader only
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

    UART_DATE_REG_ADDR = 0x60000078  # used to differentiate ESP8266 vs ESP32*
    UART_DATE_REG2_ADDR = 0x3f400074  # used to differentiate ESP32-S2 vs other models

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
            date_reg = detect_port.read_reg(ESPLoader.UART_DATE_REG_ADDR)
            date_reg2 = detect_port.read_reg(ESPLoader.UART_DATE_REG2_ADDR)

            for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM]:
                if date_reg == cls.DATE_REG_VALUE and (cls.DATE_REG2_VALUE is None or date_reg2 == cls.DATE_REG2_VALUE):
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
        raise FatalError("Unexpected UART datecode value 0x%08x. Failed to autodetect chip type." % (date_reg))

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
                date_reg = self.read_reg(self.UART_DATE_REG_ADDR)
                date_reg2 = self.read_reg(self.UART_DATE_REG2_ADDR)
                if date_reg != self.DATE_REG_VALUE or (self.DATE_REG2_VALUE is not None and date_reg2 != self.DATE_REG2_VALUE):
                    actually = None
                    for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM]:
                        if date_reg == cls.DATE_REG_VALUE and (cls.DATE_REG2_VALUE is None or date_reg2 == cls.DATE_REG2_VALUE):
                            actually = cls
                            break
                    if actually is None:
                        print(("WARNING: This chip doesn't appear to be a %s (date codes 0x%08x:0x%08x). " +
                               "Probably it is unsupported by this version of esptool.") % (self.CHIP_NAME, date_reg, date_reg2))
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
        if isinstance(self, ESP32S2ROM) and not self.IS_STUB:
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
        if isinstance(self, ESP32S2ROM) and not self.IS_STUB:
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

    DATE_REG_VALUE = 0x00062000
    DATE_REG2_VALUE = None

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

    DATE_REG_VALUE = 0x15122500
    DATE_REG2_VALUE = None

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

    DATE_REG_VALUE = 0x00000500   # This is actually UART_ID_REG(0) on ESP32-S2
    DATE_REG2_VALUE = 0x19031400  # this is the date register

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

    (Basically the same as ESP2StubLoader, but different base class.
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
                        choices=['auto', 'esp8266', 'esp32', 'esp32s2'],
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


def _main():
    try:
        main()
    except FatalError as e:
        print('\nA fatal error occurred: %s' % e)
        sys.exit(2)


if __name__ == '__main__':
    _main()
