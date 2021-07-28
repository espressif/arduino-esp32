#!/usr/bin/env python
#
# ESP8266 & ESP32 family ROM Bootloader Utility
# Copyright (C) 2014-2021 Fredrik Ahlberg, Angus Gratton, Espressif Systems (Shanghai) CO LTD, other contributors as noted.
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


__version__ = "3.2-dev"

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
        'esp32s3': ESP32S3ROM,
        'esp32c3': ESP32C3ROM,
        'esp32c6beta': ESP32C6BETAROM,
        'esp32h2': ESP32H2ROM,
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
                        0x15: '2MB', 0x16: '4MB', 0x17: '8MB',
                        0x18: '16MB', 0x19: '32MB', 0x1a: '64MB'}


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


def esp32s3_or_newer_function_only(func):
    """ Attribute for a function only supported by ESP32S3/32C3 ROM """
    return check_supported_function(func, lambda o: isinstance(o, ESP32S3ROM) or isinstance(o, ESP32C3ROM))


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

    # Commands supported by ESP32-S2/S3/C3/C6 ROM bootloader only
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

    # Response to ESP_SYNC might indicate that flasher stub is running instead of the ROM bootloader
    sync_stub_detected = False

    # Device PIDs
    USB_JTAG_SERIAL_PID = 0x1001

    # Chip IDs that are no longer supported by esptool
    UNSUPPORTED_CHIPS = {6: "ESP32-S3(beta 3)"}

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
        self.stub_is_disabled = False  # flag is set to True if esptool detects conditions which require the stub to be disabled

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

            for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM, ESP32S3BETA2ROM, ESP32S3ROM, ESP32C3ROM, ESP32C6BETAROM, ESP32H2ROM]:
                if chip_magic_value in cls.CHIP_DETECT_MAGIC_VALUE:
                    # don't connect a second time
                    inst = cls(detect_port._port, baud, trace_enabled=trace_enabled)
                    inst._post_connect()
                    inst.check_chip_id()

                    print(' %s' % inst.CHIP_NAME, end='')
                    if detect_port.sync_stub_detected:
                        inst = inst.STUB_CLASS(inst)
                        inst.sync_stub_detected = True
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
        val, _ = self.command(self.ESP_SYNC, b'\x07\x07\x12\x20' + 32 * b'\x55',
                              timeout=SYNC_TIMEOUT)

        # ROM bootloaders send some non-zero "val" response. The flasher stub sends 0. If we receive 0 then it
        # probably indicates that the chip wasn't or couldn't be reseted properly and esptool is talking to the
        # flasher stub.
        self.sync_stub_detected = val == 0

        for _ in range(7):
            val, _ = self.command()
            self.sync_stub_detected &= val == 0

    def _setDTR(self, state):
        self._port.setDTR(state)

    def _setRTS(self, state):
        self._port.setRTS(state)
        # Work-around for adapters on Windows using the usbser.sys driver:
        # generate a dummy change to DTR so that the set-control-line-state
        # request is sent with the updated RTS state and the same DTR state
        self._port.setDTR(self._port.dtr)

    def _get_pid(self):
        if list_ports is None:
            print("\nListing all serial ports is currently not available. Can't get device PID.")
            return
        active_port = self._port.port

        # Pyserial only identifies regular ports, URL handlers are not supported
        if not active_port.startswith(("COM", "/dev/")):
            print("\nDevice PID identification is only supported on COM and /dev/ serial ports.")
            return
        # Return the real path if the active port is a symlink
        if active_port.startswith("/dev/") and os.path.islink(active_port):
            active_port = os.path.realpath(active_port)

        # The "cu" (call-up) device has to be used for outgoing communication on MacOS
        if sys.platform == "darwin" and "tty" in active_port:
            active_port = [active_port, active_port.replace("tty", "cu")]
        ports = list_ports.comports()
        for p in ports:
            if p.device in active_port:
                return p.pid
        print("\nFailed to get PID of a device on {}, using standard reset sequence.".format(active_port))

    def bootloader_reset(self, esp32r0_delay=False, usb_jtag_serial=False):
        """ Issue a reset-to-bootloader, with esp32r0 workaround options
        and USB-JTAG-Serial custom reset sequence option
        """
        # RTS = either CH_PD/EN or nRESET (both active low = chip in reset)
        # DTR = GPIO0 (active low = boot to flasher)
        #
        # DTR & RTS are active low signals,
        # ie True = pin @ 0V, False = pin @ VCC.
        if usb_jtag_serial:
            # Custom reset sequence, which is required when the device
            # is connecting via its USB-JTAG-Serial peripheral
            self._setRTS(False)
            self._setDTR(False)  # Idle
            time.sleep(0.1)
            self._setDTR(True)  # Set IO0
            self._setRTS(False)
            time.sleep(0.1)
            self._setRTS(True)  # Reset. Note dtr/rts calls inverted so we go through (1,1) instead of (0,0)
            self._setDTR(False)
            self._setRTS(True)  # Extra RTS set for RTS as Windows only propagates DTR on RTS setting
            time.sleep(0.1)
            self._setDTR(False)
            self._setRTS(False)
        else:
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

    def _connect_attempt(self, mode='default_reset', esp32r0_delay=False, usb_jtag_serial=False):
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

        if mode != 'no_reset':
            self.bootloader_reset(esp32r0_delay, usb_jtag_serial)

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
        if mode in ['no_reset', 'no_reset_no_sync']:
            print('WARNING: Pre-connection option "{}" was selected.'.format(mode),
                  'Connection may fail if the chip is not in bootloader or flasher stub mode.')
        print('Connecting...', end='')
        sys.stdout.flush()
        last_error = None

        usb_jtag_serial = (mode == 'usb_reset') or (self._get_pid() == self.USB_JTAG_SERIAL_PID)

        try:
            for _ in range(attempts) if attempts > 0 else itertools.count():
                last_error = self._connect_attempt(mode=mode, esp32r0_delay=False, usb_jtag_serial=usb_jtag_serial)
                if last_error is None:
                    break
                last_error = self._connect_attempt(mode=mode, esp32r0_delay=True, usb_jtag_serial=usb_jtag_serial)
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
                if chip_magic_value not in self.CHIP_DETECT_MAGIC_VALUE:
                    actually = None
                    for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM, ESP32S3BETA2ROM, ESP32S3ROM, ESP32C3ROM, ESP32H2ROM]:
                        if chip_magic_value in cls.CHIP_DETECT_MAGIC_VALUE:
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
            self.check_chip_id()

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
        if isinstance(self, (ESP32S2ROM, ESP32S3BETA2ROM, ESP32S3ROM, ESP32C3ROM, ESP32C6BETAROM, ESP32H2ROM)) and not self.IS_STUB:
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
        if isinstance(self, (ESP32S2ROM, ESP32C3ROM, ESP32H2ROM)) and not self.IS_STUB:
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

    @esp32s3_or_newer_function_only
    def get_chip_info(self):
        # TODO: this only works on the ESP32S3/C3 ROM code loader and needs to work in stub loader also
        res = self.check_command('get security info', self.ESP_GET_SECURITY_INFO, b'')
        res = struct.unpack("<IBBBBBBBBII", res)
        chip_id, eco_version = res[-2], res[-1]
        return (chip_id, eco_version)

    @classmethod
    def parse_flash_size_arg(cls, arg):
        try:
            return cls.FLASH_SIZES[arg]
        except KeyError:
            raise FatalError("Flash size '%s' is not supported by this chip type. Supported sizes: %s"
                             % (arg, ", ".join(cls.FLASH_SIZES.keys())))

    def run_stub(self, stub=None):
        if stub is None:
            stub = self.STUB_CODE

        if self.sync_stub_detected:
            print("Stub is already running. No upload is necessary.")
            return self.STUB_CLASS(self)

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
        if isinstance(self, (ESP32S2ROM, ESP32S3BETA2ROM, ESP32S3ROM, ESP32C3ROM, ESP32C6BETAROM, ESP32H2ROM)) and not self.IS_STUB:
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
        print('Hard resetting via RTS pin...')
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

    def check_chip_id(self):
        try:
            chip_id, _ = self.get_chip_info()
            if chip_id != self.IMAGE_CHIP_ID:
                print("WARNING: Chip ID {} ({}) doesn't match expected Chip ID {}. esptool may not work correctly."
                      .format(chip_id, self.UNSUPPORTED_CHIPS.get(chip_id, 'Unknown'), self.IMAGE_CHIP_ID))
                # Try to flash anyways by disabling stub
                self.stub_is_disabled = True
        except NotImplementedInROMError:
            pass


class ESP8266ROM(ESPLoader):
    """ Access class for ESP8266 ROM bootloader
    """
    CHIP_NAME = "ESP8266"
    IS_STUB = False

    CHIP_DETECT_MAGIC_VALUE = [0xfff0c101]

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

    CHIP_DETECT_MAGIC_VALUE = [0x00f01d83]

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

    CHIP_DETECT_MAGIC_VALUE = [0x000007c6]

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
        num_word = 4
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 0) & 0x0F
        return pkg_version

    def get_flash_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 21) & 0x0F
        return pkg_version

    def get_psram_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 28) & 0x0F
        return pkg_version

    def get_block2_version(self):
        num_word = 4
        block2_addr = self.EFUSE_BASE + 0x05C
        word4 = self.read_reg(block2_addr + (4 * num_word))
        block2_version = (word4 >> 4) & 0x07
        return block2_version

    def get_chip_description(self):
        chip_name = {
            0: "ESP32-S2",
            1: "ESP32-S2FH2",
            2: "ESP32-S2FH4",
            102: "ESP32-S2FNR2",
            100: "ESP32-S2R2",
        }.get(self.get_flash_version() + self.get_psram_version() * 100, "unknown ESP32-S2")

        return "%s" % (chip_name)

    def get_chip_features(self):
        features = ["WiFi"]

        if self.secure_download_mode:
            features += ["Secure Download Mode Enabled"]

        flash_version = {
            0: "No Embedded Flash",
            1: "Embedded Flash 2MB",
            2: "Embedded Flash 4MB",
        }.get(self.get_flash_version(), "Unknown Embedded Flash")
        features += [flash_version]

        psram_version = {
            0: "No Embedded PSRAM",
            1: "Embedded PSRAM 2MB",
            2: "Embedded PSRAM 4MB",
        }.get(self.get_psram_version(), "Unknown Embedded PSRAM")
        features += [psram_version]

        block2_version = {
            0: "No calibration in BLK2 of efuse",
            1: "ADC and temperature sensor calibration in BLK2 of efuse V1",
            2: "ADC and temperature sensor calibration in BLK2 of efuse V2",
        }.get(self.get_block2_version(), "Unknown Calibration in BLK2")
        features += [block2_version]

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
            print("WARNING: {} chip was placed into download mode using GPIO0.\n"
                  "esptool.py can not exit the download mode over USB. "
                  "To run the app, reset the chip manually.\n"
                  "To suppress this note, set --after option to 'no_reset'.".format(self.get_chip_description()))
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


class ESP32S3ROM(ESP32ROM):
    CHIP_NAME = "ESP32-S3"

    IMAGE_CHIP_ID = 9

    CHIP_DETECT_MAGIC_VALUE = [0x9]

    IROM_MAP_START = 0x42000000
    IROM_MAP_END   = 0x44000000
    DROM_MAP_START = 0x3c000000
    DROM_MAP_END   = 0x3e000000

    UART_DATE_REG_ADDR = 0x60000080

    SPI_REG_BASE = 0x60002000
    SPI_USR_OFFS    = 0x18
    SPI_USR1_OFFS   = 0x1c
    SPI_USR2_OFFS   = 0x20
    SPI_MOSI_DLEN_OFFS = 0x24
    SPI_MISO_DLEN_OFFS = 0x28
    SPI_W0_OFFS = 0x58

    FLASH_ENCRYPTED_WRITE_ALIGN = 16

    # todo: use espefuse APIs to get this info
    EFUSE_BASE = 0x60007000  # BLOCK0 read base address
    MAC_EFUSE_REG = EFUSE_BASE + 0x044

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

    PURPOSE_VAL_XTS_AES256_KEY_1 = 2
    PURPOSE_VAL_XTS_AES256_KEY_2 = 3
    PURPOSE_VAL_XTS_AES128_KEY = 4

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
        return "ESP32-S3"

    def get_chip_features(self):
        return ["WiFi", "BLE"]

    def get_crystal_freq(self):
        # ESP32S3 XTAL is fixed to 40MHz
        return 40

    def get_flash_crypt_config(self):
        return None  # doesn't exist on ESP32-S3

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


class ESP32S3BETA2ROM(ESP32S3ROM):
    CHIP_NAME = "ESP32-S3(beta2)"
    IMAGE_CHIP_ID = 4

    CHIP_DETECT_MAGIC_VALUE = [0xeb004136]

    EFUSE_BASE = 0x6001A000  # BLOCK0 read base address

    def get_chip_description(self):
        return "ESP32-S3(beta2)"


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

    # Magic value for ESP32C3 eco 1+2 and ESP32C3 eco3 respectivly
    CHIP_DETECT_MAGIC_VALUE = [0x6921506f, 0x1b31506f]

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
        pkg_version = (word3 >> 21) & 0x07
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


class ESP32H2ROM(ESP32ROM):
    CHIP_NAME = "ESP32-H2"
    IMAGE_CHIP_ID = 10

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

    CHIP_DETECT_MAGIC_VALUE = [0xca26cc22]

    UART_DATE_REG_ADDR = 0x60000000 + 0x7c

    EFUSE_BASE = 0x6001A000
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

    MEMORY_MAP = []

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
            0: "ESP32-H2",
        }.get(self.get_pkg_version(), "unknown ESP32-H2")
        chip_revision = self.get_chip_revision()

        return "%s (revision %d)" % (chip_name, chip_revision)

    def get_chip_features(self):
        return ["BLE/802.15.4"]

    def get_crystal_freq(self):
        return 40

    def override_vddsdio(self, new_voltage):
        raise NotImplementedInROMError("VDD_SDIO overrides are not supported for ESP32-H2")

    def read_mac(self):
        mac0 = self.read_reg(self.MAC_EFUSE_REG)
        mac1 = self.read_reg(self.MAC_EFUSE_REG + 4)  # only bottom 16 bits are MAC
        bitstring = struct.pack(">II", mac1, mac0)[2:]
        try:
            return tuple(ord(b) for b in bitstring)
        except TypeError:  # Python 3, bitstring elements are already bytes
            return tuple(bitstring)

    def get_flash_crypt_config(self):
        return None  # doesn't exist on ESP32-H2

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


class ESP32C6BETAROM(ESP32C3ROM):
    CHIP_NAME = "ESP32-C6(beta)"
    IMAGE_CHIP_ID = 7

    CHIP_DETECT_MAGIC_VALUE = [0x0da1806f]

    UART_DATE_REG_ADDR = 0x00000500

    def get_chip_description(self):
        chip_name = {
            0: "ESP32-C6",
        }.get(self.get_pkg_version(), "unknown ESP32-C6")
        chip_revision = self.get_chip_revision()

        return "%s (revision %d)" % (chip_name, chip_revision)


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


class ESP32S3StubLoader(ESP32S3ROM):
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


ESP32S3ROM.STUB_CLASS = ESP32S3StubLoader


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


class ESP32H2StubLoader(ESP32H2ROM):
    """ Access class for ESP32H2 stub loader, runs on top of ROM.

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


ESP32H2ROM.STUB_CLASS = ESP32H2StubLoader


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
        elif chip == "esp32s3":
            return ESP32S3FirmwareImage(f)
        elif chip == 'esp32c3':
            return ESP32C3FirmwareImage(f)
        elif chip == 'esp32c6beta':
            return ESP32C6BETAFirmwareImage(f)
        elif chip == 'esp32h2':
            return ESP32H2FirmwareImage(f)
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


class ESP32S3FirmwareImage(ESP32FirmwareImage):
    """ ESP32S3 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32S3ROM


ESP32S3ROM.BOOTLOADER_IMAGE = ESP32S3FirmwareImage


class ESP32C3FirmwareImage(ESP32FirmwareImage):
    """ ESP32C3 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32C3ROM


ESP32C3ROM.BOOTLOADER_IMAGE = ESP32C3FirmwareImage


class ESP32C6BETAFirmwareImage(ESP32FirmwareImage):
    """ ESP32C6 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32C6BETAROM


ESP32C6BETAROM.BOOTLOADER_IMAGE = ESP32C6BETAFirmwareImage


class ESP32H2FirmwareImage(ESP32FirmwareImage):
    """ ESP32H2 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32H2ROM


ESP32H2ROM.BOOTLOADER_IMAGE = ESP32H2FirmwareImage


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
            argfile.seek(0, os.SEEK_END)
            if address + argfile.tell() > flash_end:
                raise FatalError(("File %s (length %d) at offset %d will not fit in %d bytes of flash. "
                                  "Use --flash-size argument, or change flashing address.")
                                 % (argfile.name, argfile.tell(), address, flash_end))
            argfile.seek(0)

    if args.erase_all:
        erase_flash(esp, args)
    else:
        for address, argfile in args.addr_filename:
            argfile.seek(0, os.SEEK_END)
            write_end = address + argfile.tell()
            argfile.seek(0)
            bytes_over = address % esp.FLASH_SECTOR_SIZE
            if bytes_over != 0:
                print("WARNING: Flash address {:#010x} is not aligned to a {:#x} byte flash sector. "
                      "{:#x} bytes before this address will be erased."
                      .format(address, esp.FLASH_SECTOR_SIZE, bytes_over))
            # Print the address range of to-be-erased flash memory region
            print("Flash will be erased from {:#010x} to {:#010x}..."
                  .format(address - bytes_over, div_roundup(write_end, esp.FLASH_SECTOR_SIZE) * esp.FLASH_SECTOR_SIZE - 1))

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
    elif args.chip == 'esp32s2':
        image = ESP32S2FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
    elif args.chip == 'esp32s3beta2':
        image = ESP32S3BETA2FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
    elif args.chip == 'esp32s3':
        image = ESP32S3FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
    elif args.chip == 'esp32c3':
        image = ESP32C3FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
    elif args.chip == 'esp32c6beta':
        image = ESP32C6BETAFirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
    elif args.chip == 'esp32h2':
        image = ESP32H2FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
    elif args.version == '1':  # ESP8266
        image = ESP8266ROMFirmwareImage()
    else:
        image = ESP8266V2FirmwareImage()
    image.entrypoint = e.entrypoint
    image.flash_mode = {'qio': 0, 'qout': 1, 'dio': 2, 'dout': 3}[args.flash_mode]

    if args.chip != 'esp8266':
        image.min_rev = int(args.min_rev)

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
                        choices=['auto', 'esp8266', 'esp32', 'esp32s2', 'esp32s3beta2', 'esp32s3', 'esp32c3', 'esp32c6beta', 'esp32h2'],
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
        choices=['default_reset', 'usb_reset', 'no_reset', 'no_reset_no_sync'],
        default=os.environ.get('ESPTOOL_BEFORE', 'default_reset'))

    parser.add_argument(
        '--after', '-a',
        help='What to do after esptool.py is finished',
        choices=['hard_reset', 'soft_reset', 'no_reset', 'no_reset_stub'],
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
            elif not esp.IS_STUB and esp.stub_is_disabled:
                print("WARNING: Stub loader has been disabled for compatibility, setting --no-stub")
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
            esp.hard_reset()
        elif args.after == 'soft_reset':
            print('Soft resetting...')
            # flash_finish will trigger a soft reset
            esp.soft_reset(False)
        elif args.after == 'no_reset_stub':
            print('Staying in flasher stub.')
        else:  # args.after == 'no_reset'
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
8rTPzE3+5/8AACNy+Q==\
""")))
ESP32S3ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
eNqVW3tX3DYW/yrDEF557Fr2jC2xbQMlnZI2PQVCCMnSs5Flu8meLEvIdEPSdD/76r4k2R56sn8AtqzXff3uQ+L3rWV7s9zandRbFzdqdnGjs4ubrHjlf2XJi53du7hx9UP/GvuU+9xLXdx0tf/p/Lt77DtmE/pi\
jP+rfUNJA6EtjNA0InS2mjrjx4o+WmjPlr6RP7YZ/c2yNd+jHEyBvfILT0oLff1EqvFdsrhyFsbDj6dMueQl9oIv8YUoXYtk8oq4nP/W2V7zNlA78Y/G02z8Am3uW4AH83SN/UgB0NzOVzFkFmkPzGhTIupph0Tf\
tJ5nGuabwTywWdWb0FC3PkdO8dOr05ROv2cY2YyZuyTZ6+zO2SF9xZ72S3qOJXF/Etg+GQkAKJTtIGecUBU0jXUrrMoEmkQdUZfMYHsmf00PsQU5fPZxpTp99q05kDLNQKYgkZVale3RflvZrO/nxWFsIuIm4Zob\
bssMCOrvarim4b9klZr47exDYXxibbP0fW9Png5hLR5jZmE24XodjHSCeufpai2vXfLznFlcJnSVA9M1TIpOOV7nO4mqFTyced7raUBndaQZfkDFlRexM0wCt4VBLj/nEX6Xpk6NKn88FH6yAArJxt3IYsJSfK5A\
VgvuPIuUiwFbRfiD7ZaIjztD26iToTifVwYFc4ICZ9lnmgC+KAtAtoBBqbalWn+Z8tVcLMM0/fbL/qglbbpJNor67uBLwhxm5FD9Wi0YtEdsCS91/FISk3CM+5FsFghKPUYEs/v+V0VAqW4dpYejrk4PIhhgVz++\
ga3PaR4wOuCHNvvwS4H01gx9Q8xQsDAjim7AJdznlS0MLd/YN8cs7W64eq9jX39ip5fFy+nRI1LTtiQ/W1e86xEk/xnT2mK8AfRgJf+4nhMbscb2WUMUmx6TfBdrjoFJyK41YhL0bobMco63qPlrf2tEMhAfyXbN\
bWRfXLD4BUdZ4O1w2iloshKKzBSsneCUSJ3QjnXxgEasBM6ytxC4eqUA5ubR8WkHC2Xuh9v0dZUfOBzNbYWziU9N58AlxV3BYryZjdidlglcyQlcXTp5hTh2DfhGX5u6T01odwAPRR9NdDKIp0Jst/Pop8IEzUbi\
4PqaizvclWAnjwvAYHlX9TS4SF6rm62eKq7qJAwwt0UTfZ8XfhT7UfGUXRd/sMMsxB6OQJb84zIRYohxpuvs/0Gb84ul50RTRldfW/bEmu0OdivaDny18N4Cv9c5VmGPp9gPWd6yznZAgyeTJZHdYDwMy3FMV+t1\
djpIi9+zrhnSKkD7++RTkLm1j91r9Ll+AZC8eyLwssdf2N33oKa+BWryBRERWIzTpSP1KpQEji3QorLo+RuWhMPP3NYmblQNQzL0Y1p99X8GTyRbWEXwwQg+eGlNSAoOFziAl/W72zvrUc08dF2SQliO3EkQ57Oj\
gl2OAc7W59D35dOji4t9isaJ2g0Jy197TLQljchmr+/8ykaWtxCIDaPGG5jNPzkUf/YcFMP/qst/sTQzzqsC99+M3OPRklULfcrRGyDTr1oXzLCG4KBmJEFEYS+MOA+m4UqIfOt8cuU7VJMTTz7HOgZ9w+6zViKJ\
o/P9xOckOQ+oujPA6dxGmSm1IdrN8QfEdWBUYAbwLAGhxlgSbEX9E3bxiljgGYJpIeyyYFCcx/0NPOjxWDP1IC+7jSHo42Yr8DukqG53ymA12zndBtHtTnfgz/YMJnJZzlP3weuK8sPOPmTvlWRCHKvXjtUP48dU\
E4naGm01yXUUGvQEPhcT9j2jsOQn8m9kea0j/CIZsMrkUXSCUhQxESrVXRL259Fq5XmQEFp8mozC0JqXtMye2yItm90BUByDPwq6G7oOmEaLmibYanJw0kiX25dsME9TWWxROxpZDym8nd4tsq9EsfOdc3PYSiz6\
AAEJNUzEP1/tn2JB4Tx9gcCHqhB7E5bt69CyDE/l2hpJBJBHZbcJlRU64PNxmp1dooUeuF1y+ajqM1Z1DH/yiTgQUPoEkxxbZM/fqlhjWGkRnbs7XGRMQjsK8H6Y7qEln61TmICOgCGiTkaDSVvxOWbFPpIkC5lY\
xzGIEQXJj8KhEYEoTXdbss8/aCFvU3G+Tl+u0peb9GXZUwHM0QHtvKBfhRCSLI+by/01KwbOmZKy3QmqfS1MOWE1z6AdU8c5yS4UQ7KYnYOgwZw01poEJkYQ90604HvOWgB1y2+T6JYzFrdCBigbDPXeRSEhFutP\
ZHKxRvgAHdR7BnG1C52vlj1lraIeoZraiyWpbUsfEAmGsWcSgNfNn6ircS+j69I5euI/6C2NS9AR2EgJEKAtqZtFQPMWeJ14fuLLFXBvwtsBr+ekxjlHVCczYyIZv6HyWrJg6my49U/TT8+ACXOCHBQBwub1Ju1M\
MZ5TAfHqN45wqooAC4CidQOAxEB08ZZIDhEJe2vUFKnc6KRD3NQZM6eliNJ0MXvIyre0p5pzE82YDWkAZM0KDLL8PmKDy1OFEUI+URwQKjmq/rAQJPpMWmYqCqFcBlqbaeJIJQr8gbhmWpIaRskdBBTAbFgcYYU3\
YUYWscEVJ3Z3uo3YZLMUm3zHGj+20Vmiv56zv7ZOzBnHrlMY2dNY9SVpJ4cu9uHPh/uPwZqpXP8I5qggOYDidea4qq0zaFOxwH8qbbF+8ErvcSTSy5YGqdWobcaq1XN7e+OOcdK9lXTE8wJ88XvdSmacJWX6Lutv\
UWiP7MIZ6rRcitujWsNNAFhAZNlRKwHXUQi8TEwPuyRT1KFDliXAbXXy4sGd+r8N44xkmNtJ5ayJg/xvp/jQQUp0XjiXU0wxv43GmpaVe4cj5N2SoNWFbATsJv9xWoHa6zbqGJWyAGsVAKqVGGwu6LMPCXS7hr8V\
oVXX3b0TIjqceI0DwIbr8a0UEjrS99quRV+hODSHLbjRiRDPkHFtB5F3lJYqgj8i4LveIQzYEhpLdzSNcedO8PjLWOl2iWN0dhIrD1LjbtSKhcm6Tzi7UqPFcY7JisW1+sdqZzlG1DXApdH5Uk5JM4TswENXQLyi\
MqmGSXkMon/I+XXBbKx5mRWFRAQklnmjdnjJpLFOntuQUpJiMlq1K3ZvG9ZEKMR37d1HEK4U92JpJDlNw+LK+x61N0mOW/T4OIOzN4UaUjEeuW1sA1TOjy+WO8ebnJmDU3VSU4ccomVbw13KFAXFGpmEOZitYT2m\
mHQH8KwBiJINF+Z40cdC5dZhaVJ88X+ovrVYF4EvfkfefMny7IxYC+OBREUJFGBiLJa15K/Tul3bbkjjhqy7iBWYYTkwcpRKu+hRNdtjSdEL9q3MefdaosI4V1feTFELoVwkFbiuPLxYdqi+iz8kW9mWhbvDOI1F\
TO7OeYLGSu87aeGJCEq3BiBXt2FrwGioosinhqYNJ8V+QjmXKcmvRj50MuuC4zJesl7BDVtG/FdG9GDUMW7sXsd5L27sfSivLXnbAnmd+YWz02QiXb5INp3F2pwarfh70MnBjo1KssU6qcpJmXe05rOEq1kSMuKB\
AliVfpk0Kml8zWvMVqxRDNf4d1J8yMh19Tu8hRmYJS33Ij1Gfbx/OPjmsmjyQnqDW9k5FoCvJPMFg9RGTjwxqaDXlg6Gkoek/qb7Vp3VxQbZeted8dlms/jbIMoBYc3Pnkqo+2xVWvIUYHvxzTA+2mQ0F0xkIHM5\
VRa2dIUnPJN4glnPzvh8BjDAYjxw9omCdDeQv0bgb4/AV+8/Z2yhtOtyVOvdpjlG1YeCMRVjkIzLhSpLTh60+nZFXFvG5AGTk6aPTU5cQCnH30dyRHv2gmoFLjkEAxVXRqiBbvPnZL6qFYJcUkW1aQFeEdlD8oY3\
WQxkcnR+WvCKUhSzz7h7LiTnq0k2CcmNi0mmwlCggtx4vjtfRDn5ZS77QjJ8ALFKSIApfTKCHIJkeKfDYyF1Z/WOWxuVx7WpkCo6XHT48R0fFQdbFNVlkNdtYs0oNZ5TO1Z0qFJ3WMJoeL8koI4L2DXa4VaItuAJ\
Suk2RHMntCSeb865VpFUe9sgc14t7C2PMVmWk5PewuOik0lCUl5FHNflfzme1M8xA9iiwNfE+B01z/K63rg/rYAFjcTd19+zt0ctHMJaLkU6xLTf5Myj6i1uEEJyQrGWi7ZGck9UtYZcYat2HNrrvRfrXBzKE8Zp\
Tt6FIViwb8Qz5VE5wv7wiOMx16D0Oa6ylXPcKXdrXOILHAc0mL0XchonB5NQDtaDug6eVFcodj4QhkQOAgIqNGfVDmUiRsKZuTzwjQ46gMCHiXwqRR0l9kIQLcL3isMydFM+LAukCB2K6SiIDl2eSq7QJUxD9SpW\
Me2Q7+Eh8+WLPqP0SGGEeCBXMJ7Chn+K/l/rxJhA1fqu7wcZ9ytkCB8gs3wO80JhD48nUWtQwMUY+mjOX4DZUYNh2oMVdQDsu3kgAc7DcKNtK56shHRnuFDJXi3jOr3OZiO3fsYxXJud7pO7NdXXMMdncgRpDS6p\
BeLJnyExQXzWMNZjBjnYB97b4bwU++kdSuzBqTThSANbAKz8uOske23xFQN1PL1OD1GwxDTpp7rXoMAFqavDyPV5QYtjvIaBSHcQEcpg/XDEmMWdnggiqrlyjTURHB0Qa0D8mqACsKRQfH7DgU9WxIshWRIfY2Gv\
oX4EWbzPOlg4dchyZqPhozlQe1Mk1jeM5Vx5xpuZHzOJbfLVcJFkjsOqr58wM8Fi0pAPd0V3OHCAK9MTIgYyK8Hlmx9JNAYPl3HjnGIB9Y77WgeuGFowTjFUEcZTppq2ifWKmjCMSto3KfJ3fHpVcfUaKdOc9rHB\
RouXKgX6cUVDRUMt2t5OkNClcDHcEMUzVrh5Oj/F8gfd4DuVEuS5JPFYFjwjz1sXg7yvC/nyNxWmmF0waMwrOpf4S8wnNouYa7lZDBJUJyC1zTdMAij9B8MGqVewNvW7fB1DSywwtCturuSy1Z+Txpk0vl9QqQQ1\
G6EOT8rbE4oY5FAa5fnbEAnUr2wu8pCm/+I/syWEKdFpwEOGqYWmtJ5SiiOyR4fw93aMOZbvaOD1P9rjGByt6TOhodBkjvNu8XFbDIs4EPnMmZKdxdF1qDz0yg5UokfXWNcT6uZqsmvMdSoSk52/iGcuBkXzpaWM\
vw9JO8aaYQogEKd5RvL9CtB/iabUnG9WFuyfndy2UPFqgVMbTzgUZyC3+QZH+iVFJvisNziOgCCp2ZiOr+7SOIySC+EGc0CrJ2MZKfK013Ib4OMzqDLPdz98hD83c2CKywwWFhcrgnOdRwnphKFmICR0J0rO5+Fa\
RthWzuVUwUK49aUCAKq/wgLvCHbwiFV02byDTX+A/X20N5uCPx/iKX2GB3IcUFp2pXb+iFmFrH4HGhcQUUWY80DGX+omaCylzo6CNXwGhxuuXagVWRjmObXehAXfiQkSu+uaILs2mxIJcoPsARIzq/kgGLoDZkNl\
Gr25m3BkXXADcG1+FSeGLBRGg67ihbksxltQC7EtH6s6Op8PF0iYZ85DDajG/DGXOUN4Ad+vndw4w7t3NzyRlXC541q14avPTbnDx26YKJR0ecY6OfuHREiSXFeZZ4sEO91XdEsM9bE8IT6jrVV07BXugYUZtDlf\
/Czaecx3vcJX4+d/kmKTh54zAjSTbyRHZniM3bDXWJFmY3s9Mo3v+rjn56/YGyMQtH9JSp4BRPl4qOmvgM+FJPXfDCe+7T6iaWL8TsEGl/rpVjhFDs7l8WINFpmrGJNleH5afOYIOlzW2OTu880pn3AlGXnvgmYG\
N9NoPVjcTWXCjj0vLTeVC58c34XLPtj9QXJDa3bLbdBG/vVBaCt7U0zjnvrM2ro/wX8r+sf7pb2Gfy5SWVUVlcmq3H9pL5fXH6XRm7cqfWNjl5b/Cyk5ttviL+lEPuwzc5P/8T9O/npU\
""")))
ESP32C3ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32C6BETAROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
TB3fMw==\
""")))
ESP32H2ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
AE/V398=\
""")))


def _main():
    try:
        main()
    except FatalError as e:
        print('\nA fatal error occurred: %s' % e)
        sys.exit(2)


if __name__ == '__main__':
    _main()
