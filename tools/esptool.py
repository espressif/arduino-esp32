#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2014-2022 Fredrik Ahlberg, Angus Gratton, Espressif Systems (Shanghai) CO LTD, other contributors as noted.
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
import re
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


__version__ = "3.3-dev"

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

SUPPORTED_CHIPS = ['esp8266', 'esp32', 'esp32s2', 'esp32s3beta2', 'esp32s3', 'esp32c3', 'esp32c6beta', 'esp32h2beta1', 'esp32h2beta2', 'esp32c2']


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
        'esp32h2beta1': ESP32H2BETA1ROM,
        'esp32h2beta2': ESP32H2BETA2ROM,
        'esp32c2': ESP32C2ROM,
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
                        0x18: '16MB', 0x19: '32MB', 0x1a: '64MB', 0x21: '128MB'}


def check_supported_function(func, check_func):
    """
    Decorator implementation that wraps a check around an ESPLoader
    bootloader function to check if it's supported.

    This is used to capture the multidimensional differences in
    functionality between the ESP8266 & ESP32 (and later chips) ROM loaders, and the
    software stub that runs on these. Not possible to do this cleanly
    via inheritance alone.
    """
    def inner(*args, **kwargs):
        obj = args[0]
        if check_func(obj):
            return func(*args, **kwargs)
        else:
            raise NotImplementedInROMError(obj, func)
    return inner


def esp8266_function_only(func):
    """ Attribute for a function only supported on ESP8266 """
    return check_supported_function(func, lambda o: o.CHIP_NAME == "ESP8266")


def stub_function_only(func):
    """ Attribute for a function only supported in the software stub loader """
    return check_supported_function(func, lambda o: o.IS_STUB)


def stub_and_esp32_function_only(func):
    """ Attribute for a function only supported by software stubs or ESP32 and later chips ROM """
    return check_supported_function(func, lambda o: o.IS_STUB or isinstance(o, ESP32ROM))


def esp32s3_or_newer_function_only(func):
    """ Attribute for a function only supported by ESP32S3 and later chips ROM """
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


class ESPLoader(object):
    """ Base class providing access to ESP ROM & software stub bootloaders.
    Subclasses provide ESP8266 & ESP32 Family specific functionality.

    Don't instantiate this base class directly, either instantiate a subclass or
    call ESPLoader.detect_chip() which will interrogate the chip and return the
    appropriate subclass instance.

    """
    CHIP_NAME = "Espressif device"
    IS_STUB = False

    FPGA_SLOW_BOOT = False

    DEFAULT_PORT = "/dev/ttyUSB0"

    USES_RFC2217 = False

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

    # Some comands supported by ESP32 and later chips ROM bootloader (or -8266 w/ stub)
    ESP_SPI_SET_PARAMS = 0x0B
    ESP_SPI_ATTACH     = 0x0D
    ESP_READ_FLASH_SLOW  = 0x0e  # ROM only, much slower than the stub flash read
    ESP_CHANGE_BAUDRATE = 0x0F
    ESP_FLASH_DEFL_BEGIN = 0x10
    ESP_FLASH_DEFL_DATA  = 0x11
    ESP_FLASH_DEFL_END   = 0x12
    ESP_SPI_FLASH_MD5    = 0x13

    # Commands supported by ESP32-S2 and later chips ROM bootloader only
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

        First, get_security_info command is sent to detect the ID of the chip
        (supported only by ESP32-C3 and later, works even in the Secure Download Mode).
        If this fails, we reconnect and fall-back to reading the magic number.
        It's mapped at a specific ROM address and has a different value on each chip model.
        This way we can use one memory read and compare it to the magic number for each chip type.

        This routine automatically performs ESPLoader.connect() (passing
        connect_mode parameter) as part of querying the chip.
        """
        inst = None
        detect_port = ESPLoader(port, baud, trace_enabled=trace_enabled)
        if detect_port.serial_port.startswith("rfc2217:"):
            detect_port.USES_RFC2217 = True
        detect_port.connect(connect_mode, connect_attempts, detecting=True)
        try:
            print('Detecting chip type...', end='')
            res = detect_port.check_command('get security info', ESPLoader.ESP_GET_SECURITY_INFO, b'')
            res = struct.unpack("<IBBBBBBBBI", res[:16])  # 4b flags, 1b flash_crypt_cnt, 7*1b key_purposes, 4b chip_id
            chip_id = res[9]  # 2/4 status bytes invariant

            for cls in [ESP32S3BETA2ROM, ESP32S3ROM, ESP32C3ROM, ESP32C6BETAROM, ESP32H2BETA1ROM, ESP32C2ROM, ESP32H2BETA2ROM]:
                if chip_id == cls.IMAGE_CHIP_ID:
                    inst = cls(detect_port._port, baud, trace_enabled=trace_enabled)
                    inst._post_connect()
                    try:
                        inst.read_reg(ESPLoader.CHIP_DETECT_MAGIC_REG_ADDR)  # Dummy read to check Secure Download mode
                    except UnsupportedCommandError:
                        inst.secure_download_mode = True
        except (UnsupportedCommandError, struct.error, FatalError) as e:
            # UnsupportedCmdErr: ESP8266/ESP32 ROM | struct.err: ESP32-S2 | FatalErr: ESP8266/ESP32 STUB
            print(" Unsupported detection protocol, switching and trying again...")
            try:
                # ESP32/ESP8266 are reset after an unsupported command, need to connect again (not needed on ESP32-S2)
                if not isinstance(e, struct.error):
                    detect_port.connect(connect_mode, connect_attempts, detecting=True, warnings=False)
                print('Detecting chip type...', end='')
                sys.stdout.flush()
                chip_magic_value = detect_port.read_reg(ESPLoader.CHIP_DETECT_MAGIC_REG_ADDR)

                for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM, ESP32S3BETA2ROM, ESP32S3ROM,
                            ESP32C3ROM, ESP32C6BETAROM, ESP32H2BETA1ROM, ESP32C2ROM, ESP32H2BETA2ROM]:
                    if chip_magic_value in cls.CHIP_DETECT_MAGIC_VALUE:
                        inst = cls(detect_port._port, baud, trace_enabled=trace_enabled)
                        inst._post_connect()
                        inst.check_chip_id()
            except UnsupportedCommandError:
                raise FatalError("Unsupported Command Error received. Probably this means Secure Download Mode is enabled, "
                                 "autodetection will not work. Need to manually specify the chip.")
        finally:
            if inst is not None:
                print(' %s' % inst.CHIP_NAME, end='')
                if detect_port.sync_stub_detected:
                    inst = inst.STUB_CLASS(inst)
                    inst.sync_stub_detected = True
                print('')  # end line
                return inst
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
        if not active_port.lower().startswith(("com", "/dev/")):
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

    def bootloader_reset(self, usb_jtag_serial=False, extra_delay=False):
        """ Issue a reset-to-bootloader, with USB-JTAG-Serial custom reset sequence option
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
            # This fpga delay is for Espressif internal use
            fpga_delay = True if self.FPGA_SLOW_BOOT and os.environ.get("ESPTOOL_ENV_FPGA", "").strip() == "1" else False
            delay = 7 if fpga_delay else 0.5 if extra_delay else 0.05  # 0.5 needed for ESP32 rev0 and rev1

            self._setDTR(False)  # IO0=HIGH
            self._setRTS(True)   # EN=LOW, chip in reset
            time.sleep(0.1)
            self._setDTR(True)   # IO0=LOW
            self._setRTS(False)  # EN=HIGH, chip out of reset
            time.sleep(delay)
            self._setDTR(False)  # IO0=HIGH, done

    def _connect_attempt(self, mode='default_reset', usb_jtag_serial=False, extra_delay=False):
        """ A single connection attempt """
        last_error = None
        boot_log_detected = False
        download_mode = False

        # If we're doing no_sync, we're likely communicating as a pass through
        # with an intermediate device to the ESP32
        if mode == "no_reset_no_sync":
            return last_error

        if mode != 'no_reset':
            if not self.USES_RFC2217:  # Might block on rfc2217 ports
                self._port.reset_input_buffer()  # Empty serial buffer to isolate boot log
            self.bootloader_reset(usb_jtag_serial, extra_delay)

            # Detect the ROM boot log and check actual boot mode (ESP32 and later only)
            waiting = self._port.inWaiting()
            read_bytes = self._port.read(waiting)
            data = re.search(b'boot:(0x[0-9a-fA-F]+)(.*waiting for download)?', read_bytes, re.DOTALL)
            if data is not None:
                boot_log_detected = True
                boot_mode = data.group(1)
                download_mode = data.group(2) is not None

        for _ in range(5):
            try:
                self.flush_input()
                self._port.flushOutput()
                self.sync()
                return None
            except FatalError as e:
                print('.', end='')
                sys.stdout.flush()
                time.sleep(0.05)
                last_error = e

        if boot_log_detected:
            last_error = FatalError("Wrong boot mode detected ({})! The chip needs to be in download mode.".format(boot_mode.decode("utf-8")))
            if download_mode:
                last_error = FatalError("Download mode successfully detected, but getting no sync reply: The serial TX path seems to be down.")
        return last_error

    def get_memory_region(self, name):
        """ Returns a tuple of (start, end) for the memory map entry with the given name, or None if it doesn't exist
        """
        try:
            return [(start, end) for (start, end, n) in self.MEMORY_MAP if n == name][0]
        except IndexError:
            return None

    def connect(self, mode='default_reset', attempts=DEFAULT_CONNECT_ATTEMPTS, detecting=False, warnings=True):
        """ Try connecting repeatedly until successful, or giving up """
        if warnings and mode in ['no_reset', 'no_reset_no_sync']:
            print('WARNING: Pre-connection option "{}" was selected.'.format(mode),
                  'Connection may fail if the chip is not in bootloader or flasher stub mode.')
        print('Connecting...', end='')
        sys.stdout.flush()
        last_error = None

        usb_jtag_serial = (mode == 'usb_reset') or (self._get_pid() == self.USB_JTAG_SERIAL_PID)

        try:
            for _, extra_delay in zip(range(attempts) if attempts > 0 else itertools.count(), itertools.cycle((False, True))):
                last_error = self._connect_attempt(mode=mode, usb_jtag_serial=usb_jtag_serial, extra_delay=extra_delay)
                if last_error is None:
                    break
        finally:
            print('')  # end 'Connecting...' line

        if last_error is not None:
            raise FatalError('Failed to connect to {}: {}'
                             '\nFor troubleshooting steps visit: '
                             'https://docs.espressif.com/projects/esptool/en/latest/troubleshooting.html'.format(self.CHIP_NAME, last_error))

        if not detecting:
            try:
                # check the date code registers match what we expect to see
                chip_magic_value = self.read_reg(ESPLoader.CHIP_DETECT_MAGIC_REG_ADDR)
                if chip_magic_value not in self.CHIP_DETECT_MAGIC_VALUE:
                    actually = None
                    for cls in [ESP8266ROM, ESP32ROM, ESP32S2ROM, ESP32S3BETA2ROM, ESP32S3ROM,
                                ESP32C3ROM, ESP32H2BETA1ROM, ESP32H2BETA2ROM, ESP32C2ROM, ESP32C6BETAROM]:
                        if chip_magic_value in cls.CHIP_DETECT_MAGIC_VALUE:
                            actually = cls
                            break
                    if warnings and actually is None:
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
        if isinstance(self, (ESP32S2ROM, ESP32S3BETA2ROM, ESP32S3ROM, ESP32C3ROM,
                             ESP32C6BETAROM, ESP32H2BETA1ROM, ESP32C2ROM, ESP32H2BETA2ROM)) and not self.IS_STUB:
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
        if isinstance(self, (ESP32S2ROM, ESP32C3ROM, ESP32S3ROM, ESP32H2BETA1ROM, ESP32C2ROM, ESP32H2BETA2ROM)) and not self.IS_STUB:
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
        res = self.check_command('get security info', self.ESP_GET_SECURITY_INFO, b'')
        esp32s2 = True if len(res) == 12 else False
        res = struct.unpack("<IBBBBBBBB" if esp32s2 else "<IBBBBBBBBII", res)
        return {
            "flags": res[0],
            "flash_crypt_cnt": res[1],
            "key_purposes": res[2:9],
            "chip_id": None if esp32s2 else res[9],
            "api_version": None if esp32s2 else res[10],
        }

    @esp32s3_or_newer_function_only
    def get_chip_id(self):
        res = self.check_command('get security info', self.ESP_GET_SECURITY_INFO, b'')
        res = struct.unpack("<IBBBBBBBBI", res[:16])  # 4b flags, 1b flash_crypt_cnt, 7*1b key_purposes, 4b chip_id
        chip_id = res[9]  # 2/4 status bytes invariant
        return chip_id

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
        if isinstance(self, (ESP32S2ROM, ESP32S3BETA2ROM, ESP32S3ROM, ESP32C3ROM,
                             ESP32C6BETAROM, ESP32H2BETA1ROM, ESP32C2ROM, ESP32H2BETA2ROM)) and not self.IS_STUB:
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

    def run_spiflash_command(self, spiflash_command, data=b"", read_bits=0, addr=None, addr_len=0, dummy_len=0):
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
        SPI_USR_ADDR    = (1 << 30)
        SPI_USR_DUMMY   = (1 << 29)
        SPI_USR_MISO    = (1 << 28)
        SPI_USR_MOSI    = (1 << 27)

        # SPI registers, base address differs ESP32* vs 8266
        base = self.SPI_REG_BASE
        SPI_CMD_REG       = base + 0x00
        SPI_ADDR_REG      = base + 0x04
        SPI_USR_REG       = base + self.SPI_USR_OFFS
        SPI_USR1_REG      = base + self.SPI_USR1_OFFS
        SPI_USR2_REG      = base + self.SPI_USR2_OFFS
        SPI_W0_REG        = base + self.SPI_W0_OFFS

        # following two registers are ESP32 and later chips only
        if self.SPI_MOSI_DLEN_OFFS is not None:
            # ESP32 and later chips have a more sophisticated way to set up "user" commands
            def set_data_lengths(mosi_bits, miso_bits):
                SPI_MOSI_DLEN_REG = base + self.SPI_MOSI_DLEN_OFFS
                SPI_MISO_DLEN_REG = base + self.SPI_MISO_DLEN_OFFS
                if mosi_bits > 0:
                    self.write_reg(SPI_MOSI_DLEN_REG, mosi_bits - 1)
                if miso_bits > 0:
                    self.write_reg(SPI_MISO_DLEN_REG, miso_bits - 1)
                flags = 0
                if dummy_len > 0:
                    flags |= (dummy_len - 1)
                if addr_len > 0:
                    flags |= (addr_len - 1) << SPI_USR_ADDR_LEN_SHIFT
                if flags:
                    self.write_reg(SPI_USR1_REG, flags)
        else:
            def set_data_lengths(mosi_bits, miso_bits):
                SPI_DATA_LEN_REG = SPI_USR1_REG
                SPI_MOSI_BITLEN_S = 17
                SPI_MISO_BITLEN_S = 8
                mosi_mask = 0 if (mosi_bits == 0) else (mosi_bits - 1)
                miso_mask = 0 if (miso_bits == 0) else (miso_bits - 1)
                flags = (miso_mask << SPI_MISO_BITLEN_S) | (mosi_mask << SPI_MOSI_BITLEN_S)
                if dummy_len > 0:
                    flags |= (dummy_len - 1)
                if addr_len > 0:
                    flags |= (addr_len - 1) << SPI_USR_ADDR_LEN_SHIFT
                self.write_reg(SPI_DATA_LEN_REG, flags)

        # SPI peripheral "command" bitmasks for SPI_CMD_REG
        SPI_CMD_USR  = (1 << 18)

        # shift values
        SPI_USR2_COMMAND_LEN_SHIFT = 28
        SPI_USR_ADDR_LEN_SHIFT = 26

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
        if addr_len > 0:
            flags |= SPI_USR_ADDR
        if dummy_len > 0:
            flags |= SPI_USR_DUMMY
        set_data_lengths(data_bits, read_bits)
        self.write_reg(SPI_USR_REG, flags)
        self.write_reg(SPI_USR2_REG,
                       (7 << SPI_USR2_COMMAND_LEN_SHIFT) | spiflash_command)
        if addr and addr_len > 0:
            self.write_reg(SPI_ADDR_REG, addr)
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

    def read_spiflash_sfdp(self, addr, read_bits):
        CMD_RDSFDP = 0x5A
        return self.run_spiflash_command(CMD_RDSFDP, read_bits=read_bits, addr=addr, addr_len=24, dummy_len=8)

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
            chip_id = self.get_chip_id()
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

    FPGA_SLOW_BOOT = True

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
        '16MB': 0x40,
        '32MB': 0x50,
        '64MB': 0x60,
        '128MB': 0x70
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

    FPGA_SLOW_BOOT = False

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

        print('Hard resetting via RTS pin...')
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

    FPGA_SLOW_BOOT = False

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

    UARTDEV_BUF_NO = 0x3fcef14c  # Variable in ROM .bss which indicates the port in use
    UARTDEV_BUF_NO_USB = 3  # Value of the above variable indicating that USB is in use

    USB_RAM_BLOCK = 0x800  # Max block size USB CDC is used

    GPIO_STRAP_REG = 0x60004038
    GPIO_STRAP_SPI_BOOT_MASK = 0x8   # Not download mode
    RTC_CNTL_OPTION1_REG = 0x6000812C
    RTC_CNTL_FORCE_DOWNLOAD_BOOT_MASK = 0x1  # Is download mode forced over USB?

    UART_CLKDIV_REG = 0x60000014

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

        print('Hard resetting via RTS pin...')
        self._setRTS(True)  # EN->LOW
        if self.uses_usb():
            # Give the chip some time to come out of reset, to be able to handle further DTR/RTS transitions
            time.sleep(0.2)
            self._setRTS(False)
            time.sleep(0.2)
        else:
            self._setRTS(False)


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

    FPGA_SLOW_BOOT = False

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


class ESP32H2BETA1ROM(ESP32ROM):
    CHIP_NAME = "ESP32-H2(beta1)"
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
        return 32

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


class ESP32H2BETA2ROM(ESP32H2BETA1ROM):
    CHIP_NAME = "ESP32-H2(beta2)"
    IMAGE_CHIP_ID = 14


class ESP32C2ROM(ESP32C3ROM):
    CHIP_NAME = "ESP32-C2"
    IMAGE_CHIP_ID = 12

    IROM_MAP_START = 0x42000000
    IROM_MAP_END   = 0x42400000
    DROM_MAP_START = 0x3c000000
    DROM_MAP_END   = 0x3c400000

    CHIP_DETECT_MAGIC_VALUE = [0x6f51306f]

    EFUSE_BASE = 0x60008800
    MAC_EFUSE_REG  = EFUSE_BASE + 0x040

    def get_pkg_version(self):
        num_word = 3
        block1_addr = self.EFUSE_BASE + 0x044
        word3 = self.read_reg(block1_addr + (4 * num_word))
        pkg_version = (word3 >> 21) & 0x0F
        return pkg_version

    def get_chip_description(self):
        chip_name = {
            0: "ESP32-C2",
        }.get(self.get_pkg_version(), "unknown ESP32-C2")
        chip_revision = self.get_chip_revision()

        return "%s (revision %d)" % (chip_name, chip_revision)


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

        if rom_loader.uses_usb():
            self.ESP_RAM_BLOCK = self.USB_RAM_BLOCK
            self.FLASH_WRITE_SIZE = self.USB_RAM_BLOCK


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


class ESP32H2BETA1StubLoader(ESP32H2BETA1ROM):
    """ Access class for ESP32H2BETA1 stub loader, runs on top of ROM.

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


ESP32H2BETA1ROM.STUB_CLASS = ESP32H2BETA1StubLoader


class ESP32H2BETA2StubLoader(ESP32H2BETA2ROM):
    """ Access class for ESP32H2BETA2 stub loader, runs on top of ROM.

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


ESP32H2BETA2ROM.STUB_CLASS = ESP32H2BETA2StubLoader


class ESP32C2StubLoader(ESP32C2ROM):
    """ Access class for ESP32C2 stub loader, runs on top of ROM.

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


ESP32C2ROM.STUB_CLASS = ESP32C2StubLoader


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
        elif chip == 'esp32h2beta1':
            return ESP32H2BETA1FirmwareImage(f)
        elif chip == 'esp32h2beta2':
            return ESP32H2BETA2FirmwareImage(f)
        elif chip == 'esp32c2':
            return ESP32C2FirmwareImage(f)
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

    def set_mmu_page_size(self, size):
        """ If supported, this should be overridden by the chip-specific class. Gets called in elf2image. """
        print('WARNING: Changing MMU page size is not supported on {}! Defaulting to 64KB.'.format(self.ROM_LOADER.CHIP_NAME))


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


class ESP8266V3FirmwareImage(ESP32FirmwareImage):
    """ ESP8266 V3 firmware image is very similar to ESP32 image
    """

    EXTENDED_HEADER_STRUCT_FMT = "B" * 16

    def is_flash_addr(self, addr):
        return (addr > ESP8266ROM.IROM_MAP_START)

    def save(self, filename):
        total_segments = 0
        with io.BytesIO() as f:  # write file to memory first
            self.write_common_header(f, self.segments)

            checksum = ESPLoader.ESP_CHECKSUM_MAGIC

            # split segments into flash-mapped vs ram-loaded, and take copies so we can mutate them
            flash_segments = [copy.deepcopy(s) for s in sorted(self.segments, key=lambda s:s.addr) if self.is_flash_addr(s.addr) and len(s.data)]
            ram_segments = [copy.deepcopy(s) for s in sorted(self.segments, key=lambda s:s.addr) if not self.is_flash_addr(s.addr) and len(s.data)]

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

            # try to fit each flash segment on a 64kB aligned boundary
            # by padding with parts of the non-flash segments...
            while len(flash_segments) > 0:
                segment = flash_segments[0]
                # remove 8 bytes empty data for insert segment header
                if segment.name == '.flash.rodata':
                    segment.data = segment.data[8:]
                # write the flash segment
                checksum = self.save_segment(f, segment, checksum)
                flash_segments.pop(0)
                total_segments += 1

            # flash segments all written, so write any remaining RAM segments
            for segment in ram_segments:
                checksum = self.save_segment(f, segment, checksum)
                total_segments += 1

            # done writing segments
            self.append_checksum(f, checksum)
            image_length = f.tell()

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

    def load_extended_header(self, load_file):
        def split_byte(n):
            return (n & 0x0F, (n >> 4) & 0x0F)

        fields = list(struct.unpack(self.EXTENDED_HEADER_STRUCT_FMT, load_file.read(16)))

        self.wp_pin = fields[0]

        # SPI pin drive stengths are two per byte
        self.clk_drv, self.q_drv = split_byte(fields[1])
        self.d_drv, self.cs_drv = split_byte(fields[2])
        self.hd_drv, self.wp_drv = split_byte(fields[3])

        if fields[15] in [0, 1]:
            self.append_digest = (fields[15] == 1)
        else:
            raise RuntimeError("Invalid value for append_digest field (0x%02x). Should be 0 or 1.", fields[15])

        # remaining fields in the middle should all be zero
        if any(f for f in fields[4:15] if f != 0):
            print("Warning: some reserved header fields have non-zero values. This image may be from a newer esptool.py?")


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


class ESP32H2BETA1FirmwareImage(ESP32FirmwareImage):
    """ ESP32H2 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32H2BETA1ROM


ESP32H2BETA1ROM.BOOTLOADER_IMAGE = ESP32H2BETA1FirmwareImage


class ESP32H2BETA2FirmwareImage(ESP32FirmwareImage):
    """ ESP32H2 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32H2BETA2ROM


ESP32H2BETA2ROM.BOOTLOADER_IMAGE = ESP32H2BETA2FirmwareImage


class ESP32C2FirmwareImage(ESP32FirmwareImage):
    """ ESP32C2 Firmware Image almost exactly the same as ESP32FirmwareImage """
    ROM_LOADER = ESP32C2ROM

    def set_mmu_page_size(self, size):
        if size not in [16384, 32768, 65536]:
            raise FatalError("{} is not a valid page size.".format(size))
        self.IROM_ALIGN = size


ESP32C2ROM.BOOTLOADER_IMAGE = ESP32C2FirmwareImage


class ELFFile(object):
    SEC_TYPE_PROGBITS = 0x01
    SEC_TYPE_STRTAB = 0x03
    SEC_TYPE_INITARRAY = 0x0e
    SEC_TYPE_FINIARRAY = 0x0f

    PROG_SEC_TYPES = (SEC_TYPE_PROGBITS, SEC_TYPE_INITARRAY, SEC_TYPE_FINIARRAY)

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
        prog_sections = [s for s in all_sections if s[1] in ELFFile.PROG_SEC_TYPES]

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
    successful_slip = False
    while True:
        waiting = port.inWaiting()
        read_bytes = port.read(1 if waiting == 0 else waiting)
        if read_bytes == b'':
            if partial_packet is None:  # fail due to no data
                msg = "Serial data stream stopped: Possible serial noise or corruption." if successful_slip else "No serial data received."
            else:  # fail during packet transfer
                msg = "Packet content transfer stopped (received {} bytes)".format(len(partial_packet))
            trace_function(msg)
            raise FatalError(msg)
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
                    raise FatalError('Invalid head of packet (0x%s): Possible serial noise or corruption.' % hexify(b))
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
                successful_slip = True
            else:  # normal byte in packet
                partial_packet += b


def arg_auto_int(x):
    return int(x, 0)


def format_chip_name(c):
    """ Normalize chip name from user input """
    c = c.lower().replace('-', '')
    if c == 'esp8684':  # TODO: Delete alias, ESPTOOL-389
        print('WARNING: Chip name ESP8684 is deprecated in favor of ESP32-C2 and will be removed in a future release. Using ESP32-C2 instead.')
        return 'esp32c2'
    return c


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
    ESP ROM responses or input content.
    """
    def __init__(self, message):
        RuntimeError.__init__(self, message)

    @staticmethod
    def WithResult(message, result):
        """
        Return a fatal error object that appends the hex values of
        'result' and its meaning as a string formatted argument.
        """

        err_defs = {
            0x101: 'Out of memory',
            0x102: 'Invalid argument',
            0x103: 'Invalid state',
            0x104: 'Invalid size',
            0x105: 'Requested resource not found',
            0x106: 'Operation or feature not supported',
            0x107: 'Operation timed out',
            0x108: 'Received response was invalid',
            0x109: 'CRC or checksum was invalid',
            0x10A: 'Version was invalid',
            0x10B: 'MAC address was invalid',
        }

        err_code = struct.unpack(">H", result[:2])
        message += " (result was {}: {})".format(hexify(result), err_defs.get(err_code[0], 'Unknown result'))
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
                                  "Use --flash_size argument, or change flashing address.")
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
        args.chip = 'esp8266'

    print("Creating {} image...".format(args.chip))

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
    elif args.chip == 'esp32h2beta1':
        image = ESP32H2BETA1FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
    elif args.chip == 'esp32h2beta2':
        image = ESP32H2BETA2FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
    elif args.chip == 'esp32c2':
        image = ESP32C2FirmwareImage()
        if args.secure_pad_v2:
            image.secure_pad = '2'
    elif args.version == '1':  # ESP8266
        image = ESP8266ROMFirmwareImage()
    elif args.version == '2':
        image = ESP8266V2FirmwareImage()
    else:
        image = ESP8266V3FirmwareImage()
    image.entrypoint = e.entrypoint
    image.flash_mode = {'qio': 0, 'qout': 1, 'dio': 2, 'dout': 3}[args.flash_mode]

    if args.chip != 'esp8266':
        image.min_rev = int(args.min_rev)

    if args.flash_mmu_page_size:
        image.set_mmu_page_size(flash_size_bytes(args.flash_mmu_page_size))

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

    print("Successfully created {} image.".format(args.chip))


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
    si = esp.get_security_info()
    # TODO: better display and tests
    print('Flags: {:#010x} ({})'.format(si["flags"], bin(si["flags"])))
    print('Flash_Crypt_Cnt: {:#x}'.format(si["flash_crypt_cnt"]))
    print('Key_Purposes: {}'.format(si["key_purposes"]))
    if si["chip_id"] is not None and si["api_version"] is not None:
        print('Chip_ID: {}'.format(si["chip_id"]))
        print('Api_Version: {}'.format(si["api_version"]))


def merge_bin(args):
    try:
        chip_class = _chip_to_rom_loader(args.chip)
    except KeyError:
        msg = "Please specify the chip argument" if args.chip == "auto" else "Invalid chip choice: '{}'".format(args.chip)
        msg = msg + " (choose from {})".format(', '.join(SUPPORTED_CHIPS))
        raise FatalError(msg)

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

    parser = argparse.ArgumentParser(description='esptool.py v%s - Espressif chips ROM Bootloader Utility' % __version__, prog='esptool')

    parser.add_argument('--chip', '-c',
                        help='Target chip type',
                        type=format_chip_name,  # support ESP32-S2, etc.
                        choices=['auto'] + SUPPORTED_CHIPS,
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
        parent.add_argument('--flash_size', '-fs', help='SPI Flash size in MegaBytes (1MB, 2MB, 4MB, 8MB, 16MB, 32MB, 64MB, 128MB)'
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
    parser_elf2image.add_argument('--version', '-e', help='Output image version', choices=['1', '2', '3'], default='1')
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
    parser_elf2image.add_argument('--flash-mmu-page-size', help="Change flash MMU page size.", choices=['64KB', '32KB', '16KB'])

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

    subparsers.add_parser('get_security_info', help='Get some security-related data')

    subparsers.add_parser('version', help='Print esptool version')

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

        # XMC chip startup sequence
        XMC_VENDOR_ID = 0x20

        def is_xmc_chip_strict():
            id = esp.flash_id()
            rdid = ((id & 0xff) << 16) | ((id >> 16) & 0xff) | (id & 0xff00)

            vendor_id = ((rdid >> 16) & 0xFF)
            mfid = ((rdid >> 8) & 0xFF)
            cpid = (rdid & 0xFF)

            if vendor_id != XMC_VENDOR_ID:
                return False

            matched = False
            if mfid == 0x40:
                if cpid >= 0x13 and cpid <= 0x20:
                    matched = True
            elif mfid == 0x41:
                if cpid >= 0x17 and cpid <= 0x20:
                    matched = True
            elif mfid == 0x50:
                if cpid >= 0x15 and cpid <= 0x16:
                    matched = True
            return matched

        def flash_xmc_startup():
            # If the RDID value is a valid XMC one, may skip the flow
            fast_check = True
            if fast_check and is_xmc_chip_strict():
                return  # Successful XMC flash chip boot-up detected by RDID, skipping.

            sfdp_mfid_addr = 0x10
            mf_id = esp.read_spiflash_sfdp(sfdp_mfid_addr, 8)
            if mf_id != XMC_VENDOR_ID:  # Non-XMC chip detected by SFDP Read, skipping.
                return

            print("WARNING: XMC flash chip boot-up failure detected! Running XMC25QHxxC startup flow")
            esp.run_spiflash_command(0xB9)  # Enter DPD
            esp.run_spiflash_command(0x79)  # Enter UDPD
            esp.run_spiflash_command(0xFF)  # Exit UDPD
            time.sleep(0.002)               # Delay tXUDPD
            esp.run_spiflash_command(0xAB)  # Release Power-Down
            time.sleep(0.00002)
            # Check for success
            if not is_xmc_chip_strict():
                print("WARNING: XMC flash boot-up fix failed.")
            print("XMC flash chip boot-up fix successful!")

        # Check flash chip connection
        if not esp.secure_download_mode:
            try:
                flash_id = esp.flash_id()
                if flash_id in (0xffffff, 0x000000):
                    print('WARNING: Failed to communicate with the flash chip, read/write operations will fail. '
                          'Try checking the chip connections or removing any other hardware connected to IOs.')
            except Exception as e:
                esp.trace('Unable to verify flash chip connection ({}).'.format(e))

        # Check if XMC SPI flash chip booted-up successfully, fix if not
        if not esp.secure_download_mode:
            try:
                flash_xmc_startup()
            except Exception as e:
                esp.trace('Unable to perform XMC flash chip startup sequence ({}).'.format(e))

        if hasattr(args, "flash_size"):
            print("Configuring flash size...")
            detect_flash_size(esp, args)
            if args.flash_size != 'keep':  # TODO: should set this even with 'keep'
                esp.flash_set_parameters(flash_size_bytes(args.flash_size))
                # Check if stub supports chosen flash size
                if esp.IS_STUB and args.flash_size in ('32MB', '64MB', '128MB'):
                    print("WARNING: Flasher stub doesn't fully support flash size larger than 16MB, in case of failure use --no-stub.")

        if esp.IS_STUB and hasattr(args, "address") and hasattr(args, "size"):
            if args.address + args.size > 0x1000000:
                print("WARNING: Flasher stub doesn't fully support flash size larger than 16MB, in case of failure use --no-stub.")

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
5dwuzr3/OyU1/H+n+L8kk1ilxnz4f3giyVw=\
""")))
ESP32ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32S2ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
+rqe/Hebvj3Y4S8jQkWaGpP//j8geRE3\
""")))
ESP32S3BETA2ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32S3ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32C3ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
eNqVWmt7EzcW/itpEkjhaXcley4aaINNbZwLsNCHkg1rtsxoZlJomy3BLGG3+e+r91xGYwcb9kNiW9LoHJ3re47mv3uL5nKxd2er2hvPL63dnl/65Gb4N5hfmix8hr8qz8OP5BXmZ/PL2oXhKk9krMon4be7N5+P\
5peFnV+6ij+b8FQ1GG2H6WIwCbsbTIZ9nQnfXVjthvPLkvYOf2HQDt/PL9s0/Gh5ZREm6xQ0+Glr5Tv+zKsjEA7fsE0RnqBRMF8+Buc/4ixyjAYH8/I0kQust/VpfsTMX3rzEAwswmhyb34R2A8MtYNRNdkJu7td\
ZqAaHAaOBpP7h9vhQZOW8UROTrQ3P197mqswGog3gWFbhC9tmPGB86rNw3Ng6s/jsK5mIeLZts3zNRNKesxyUW3VdU7H5P3AfCYCG3SSg/zDdlDkEJ8/XAkv7oR0BuankY4Jn65Y0uAq0eLelZBmCqMeD0op/rb+\
B0ifdw/0rm9NdkiWVQTlFEPeN6gzKKjwx0pZLLUMD/v2ZW5ZnHbISubl1h0EInWwXGcnwQTqjG2TxJBMMAftMZUqYRUZ8wh003tgdtIXnOX9C9CnsbBlHo7RZLyALCwTKw9/RcG2pOO8icriCasAJPFZ5Tc7kd0n\
pYxn4k14LJ1hKY5TsnNYzzYOIq6RfdwGv7PD3lE8ZDkO/20wqiblUSeu60CswP72K3gALMmwLWBZW5zsY1HPpdOZZydU6briAUaCeGsZaesTVjykAXnbXLa0kbfKwP+8lc2GbGrgr22w6UydpmcY4RGcIxOPM2Jz\
Ni7RE7c2Uoc+aqJGjBGpAYec1l/fAoz9zoSrfEq2KX6AFS6dwtzYMJ/e84dyMoqgs54l41ReRTTsOajEshqr8aMXQe5jyZjXW+GcZGcwk2FmsAOFy9Yug4HsMJFSj9/fvLBnkR0in0QikGWQdathA3tSmFQdKf8l\
dlFrNCuHUZOnk6+XAf2WQGISkO7cScIXH0kssNYfOCyT+opngk2yEo3YAZS4Gl5ky7q/ZSMM12SZK7GtEDbhV4hX5Bzi3MQlQqVLNUGMgs5sCkml/MWYTZHTpzkPBEO7mPbZtSJRpnsm8h4iFHwRhyGI1A1ELsnT\
p/3da/GgT0bfuPVyvIBuyhgqaDJVh63tvTjkaShu3hRsDss+Tg5XRd/2No9biGOmnUquY5PbgeMQ8SqJ91Uq8RjbFAIq1kMQ6E2hB3lqgpjf1D1OUz35xz4IISzxyeRE4ANfZ4o7CF887glT8hIiEdKIUCgbOUpB\
sgvM3ZrIDDBGcZNjvB2ecvAj8fsD/1PYxujPif8ZTgCfhUXepZR8MLop7tWwma63lnYFtKjV8AlHFD/n9CCwXsmWNyeG5xNWA85Q5KMJpyXCTwKYBDwtYaYqvduwGSq6WwJrtrgrcM03937r29yEWd/kmnUjOQw+\
2JDtz/goFNXyrX1SysXdPiyZadAafYIa0oJrJekSuJ2uZ4EiVQWh5JI4FU9K2uwJYqzIViMkJXo28R1O7BqherFzwewhkPl+zqOMenbFXgAxegkimIF1uPoOGx5MC5+V5bTvsLja4Qn2+jE8oeA0yMJ4wml0NW68\
fwtLt1fvsOY1FJjeFNINK6NGZhX8VZdyvvxqwTGliHl2wcSdYhzxGphWW+6zZ7NXbLMo1Xk+rYstTijLm5v0TaRAhlvOxux+jLTOesiB/sBoITGBWVnDYiEsLj/eo9ZA6tufcUbrZYW3sLKU8gsCVuqjwKpydBux\
rZzMz4Nvtdlp+wICfnHY2Racv/DvsEgj3esjDghQhEsff4YRxLNrUaGpl/bYeBg2fzLGkqMYB4rd8et+pAVr2U+IBvbt1bvnYeshzmL/CWgfHMnXgtfabYS1m3CvI+Sxx/CBH+GeqGCqN/Ch884PVFgZm7ddUQph\
fSc5tK0/KJjZF99TT2id2Fz5F67BqoF82rsrGHMQPaarVZGxzP4XSAmQMD3zSI0fOHS5tEYNAzbVBJ0WsXmvEljyEizqKuKjTXS3oNSr/yiJ9QTojP4OhxCyYX99ceDigi2dVsmjzh7uS/WaroLOPFLm5WQbMwkt\
w28Re0GpzQLyNpUCeCS54U8QWimgs7PCoJHxUHJeSYlwowBGqNcpl5XyyeReM/qqhNmqQKHUDoR1mTH53bGkmoQy+geBgTSyxMTzz2ihKpb87BGj0f9LYDvfsCZgrKHWRaR/RJBxoePtZhMkMyyvBEQMpwl/a0qR\
bL3vR4KKBozemgErlioz8rZVM8r7ZiSDTP9HLlHOuUAhplNmVNFiB+hh3QroqQmFQdRCDVhM1NOTSHs5cFdZz4h7AfyLArc+/GWBu22vyPuntPCweAm92mI1GHCQaFYL1MH+57b3KzmgLZ+8eYaM+uzoFGI+vf0C\
tvxifv4PTB6/eYjJh0ePMPnoVh995SfjQ/DxNqoL2AjCCLlmXyBjxgZeSvouJX0jmFMrsJGUjvmk970kCM81SMOWAjSCkgnPwqhwuqbZVMSMmDBDI3cv/7BSQ6rgNK6Tq5aet28owN8oIKjpL7ANgpipKKMrapcL\
0q/O9skwtwQpaiGbu1HXLeQnYHIVrSu193Mk/JUQA60SUQmT59wd9LHqsIOuVeZfTQFAHYM6l06n0w1ygeCIsRSdR2MfaBtC65bNT7exq+uP8+jVWpE31CgwAnerzzCyLqKglmkbUSA3tlCkaENH/ZokD4teqZwR\
x6afiVdUO5/lxyJ5dJdsvly5clptx61KLPbyCFQVFCLDIReImb/mD1XNG+oLao3h5JXmRrbvrqsjgAf2gyOiuqLxJI6H+oTbEefSjIACkN+ogBuyXLgdOV9gKQ7VyDwSX3+9IQd8RE24VCuGp/RzqD9v4WfR9Fp0\
RCSJ/SGf70rdKd0VNAqo6rH0bBqfLTywp/9hH3TtPlQxPWBh+/Q73S7pE5K9CwsQZ/KXvHXnzOimwNyQCxqBFf0mw9tkucNw2vtNsL0ZI5CnCaB8rh1LZdcKySpd4zgN82zT6WaDrpt4BzGfz1Z6BQP1H7FDQyVy\
LkVoDTNVVbLqSi6Bl/WdauvU59/4cwASHA/RmiaoBM3gljTh5hc0LO0/6/xvMpPMFzSDKEvNZXsY0x61DrqqH3Tb7xP9CQjQ3kCtBWVSFs7iY8vcfk9mhoSWT/yvCJyZWgx4avJn4Jx+Urvt+fxCkngh3XgEEdoz\
/ZTBWAlsnJrEOFOBSLWUp0ibyN+1ZTkXlk5Ozb5dmO4DIZlygEXU9gATJp16DsQtK4xNu3YHO7sa3He2OacYI7mwTFWuYXckHC8SJ0REc8iA+dfwC6XcCuWs31QZeA6IRLxRRw7f6wnFJzqsnU6h3BQAvB5EX5Mr\
JW8HqkIo2AD9ldLiK4sep0hNq3IA4iJdoRenXNT+IRoKQA+4cSjMAeIbQKWbbMAnzqApVyCEc9eDenTfssfVrBFt4kJkaLeJoYbTRYXBqCWNq1QrlmrB0zBG3wlWGiFk0OlEesiUvumSA6KyZ+QUcAJv0KDCaDiM\
dK2cQLPCJggcnkAzdYsTqfQMOwWO5Tsv7FjppFrxYOM765ro1USOQB+FWNnvgM8VsWZbdInRHRB2QbeQWYwqcoN00ZlXD/FmZLsXy8qFAEtSLl1+6DVDOpXeaVn3OZCdyUzv6+Co2zOXPZG6hjPOmKJQqktK8pxx\
7F2XyaGEdNBq+mAtiVcyzs7iEUiFJLxanmg0lTzrR4aJ5vDDfirhuVqMc3BtZiF9+5XkRuWEY1E3SeTQCocAGdRdRi+QwEv7h8AKasooSIGNAAT0MwapsIyYpyYsimeblo9L0MCJ13KArjt6v8dWJ9ZSE28JAv0+\
THodRs1tQ1atEzGTxVfDMw6XqoSCUBkKkZYy/OAWossY7buivuISpa2vtGiqR1K1ODkyRZhcuofrOCzNGw7ObX3OBVYjTFbDDzc4nlNtl49fR0rIeY4a3Vcf4+hHROWN9LZ+kWBaHDGx7gJhE4+NaeP1H2i1hixg\
dyydjJy8SG6XDDtSk6ykU7bnp2F5Jb4PG0YI1kAQrzNbTQP+wIslSIbXuISnHOuryuWKrO2Q2gMeoC5JKnWqlV5xLXfa6qJsCr7tAVO5QsDSZrgjTlbFO0hx3O/12oM691PWIwyzkLzZK88l2BDnHWr/tLC9mg7c\
/oOEJcNskSUOX856iT+JdkyctPVL0F3wLnV9MNnkH0/oauTvsN1kdAV/l2tqvdApjIQ6LwUwngc0c60CVCfBJJ0vxqxrvT8jQWimdcMbR3LhR1dQ4vF0j9IuGcsrjit04WjlvRGK6FQbNoqyCSd8UAArL0yY4de6\
VuBrCKDiIy0gbaXWovdhTRGDbyNlA/VSM8CamgHOrnQ7paaqqhWbs+axgmjITCXyQNo1rXhINArJwDXF793IspU7rq75Qm4T6J8L0KJPIIjuLieVdlsUIPqWfszSW3JCpyE7lTZhf7LQjEOThxG1wka8oB+TTBWc\
SqWEDtD6SmnJRu31yomKi1r6yPQ6zeGqJzJLF/GNET/0E1xcWH/C77BoQfsHyYpV1RRRpYW2a3sNLD7tKXPEjwvEqMQeuo54zPGFvrklp0yu43Ek3OQl45/1fncc38ziQ8IrEv+OXqXaFsF3lzKV6QceQ8HbylYs\
qgUhiefxwlNFyGnCPyFTPY9pFS7nzXv2Mto71b1dFpslqrZ1JzFmRt2B5YtQUUFb9Hk5lDBWyssXkPvyCpUFN+j3xlT+39rEwGiKzl2Ao02vzkzf9oSrCjJrw+BWvvTeHJLo8eobSVJEkWBOpP9O2TB/z2Zcydtn\
atbSGd9jTggPYIhuSRVLrxWpi4dxZn/9So+9Efhg158qRFTZ9OkkdGlGNf/iXIzUz2/t/FtvWNV2Mm5Nb+gHyttV7b7cdumTVBLglb6w4Tn3Ovn6IyR9vahJIgWTHsidf8sI7OLgPjqfd1iD/XNU+fZscCBvGOn9\
wZ9SmQpM1laT1b+hhF5TrZPmK/wfnNDh9wRYwG68pgzkF8J4tZwXQaUanCA1EKjg2k474dKGML1XIHzMVBTx5AUCekuIokvJWanJ5CZCjw38VSfSsSnUIwUFSzZZcPOYsLqmIR+JSxrp++VLuXfwkxjc6LFSKf+N\
HPKab61xI4htPcy9/ZTJEdamyOYk6zgl91dpmnze9LduiAtJjVZRIfykRas/e/YArf7s9CZa/dkLWASa/dlxi3I9e/gAISN7NF903f69b7boTd+f3y3KC7zva02eJ9a6xISZ5nxx8bEbHCbOhcG6XJT0YjDEOiLX\
2JPh/i7GZklhkqv/AdnBtYo=\
""")))
ESP32C6BETAROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32H2BETA1ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32H2BETA2ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32C2ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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
