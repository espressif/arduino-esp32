#!/usr/bin/env python
#
# ESP8266 & ESP32 ROM Bootloader Utility
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

from __future__ import print_function, division

import argparse
import hashlib
import inspect
import os
import serial
import struct
import sys
import time
import base64
import zlib
import shlex
import copy
import io

__version__ = "2.1"

MAX_UINT32 = 0xffffffff
MAX_UINT24 = 0xffffff

DEFAULT_TIMEOUT = 3       # timeout for most flash operations
START_FLASH_TIMEOUT = 20  # timeout for starting flash (may perform erase)
CHIP_ERASE_TIMEOUT = 120  # timeout for full chip erase
SYNC_TIMEOUT = 0.1        # timeout for syncing with bootloader


DETECTED_FLASH_SIZES = {0x12: '256KB', 0x13: '512KB', 0x14: '1MB',
                        0x15: '2MB', 0x16: '4MB', 0x17: '8MB', 0x18: '16MB'}


def check_supported_function(func, check_func):
    """
    Decorator implementation that wraps a check around an ESPLoader
    bootloader function to check if it's supported.

    This is used to capture the multidimensional differences in
    functionality between the ESP8266 & ESP32 ROM loaders, and the
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
    """ Attribute for a function only supported by software stubs or ESP32 ROM """
    return check_supported_function(func, lambda o: o.IS_STUB or o.CHIP_NAME == "ESP32")


PYTHON2 = sys.version_info[0] < 3  # True if on pre-Python 3

# Function to return nth byte of a bitstring
# Different behaviour on Python 2 vs 3
if PYTHON2:
    def byte(bitstr, index):
        return ord(bitstr[index])
else:
    def byte(bitstr, index):
        return bitstr[index]


def esp8266_function_only(func):
    """ Attribute for a function only supported on ESP8266 """
    return check_supported_function(func, lambda o: o.CHIP_NAME == "ESP8266")


class ESPLoader(object):
    """ Base class providing access to ESP ROM & softtware stub bootloaders.
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
    ESP_CHANGE_BAUDRATE = 0x0F
    ESP_FLASH_DEFL_BEGIN = 0x10
    ESP_FLASH_DEFL_DATA  = 0x11
    ESP_FLASH_DEFL_END   = 0x12
    ESP_SPI_FLASH_MD5    = 0x13

    # Some commands supported by stub only
    ESP_ERASE_FLASH = 0xD0
    ESP_ERASE_REGION = 0xD1
    ESP_READ_FLASH = 0xD2
    ESP_RUN_USER_CODE = 0xD3

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

    UART_DATA_REG_ADDR = 0x60000078

    # Memory addresses
    IROM_MAP_START = 0x40200000
    IROM_MAP_END = 0x40300000

    # The number of bytes in the UART response that signify command status
    STATUS_BYTES_LENGTH = 2

    def __init__(self, port=DEFAULT_PORT, baud=ESP_ROM_BAUD):
        """Base constructor for ESPLoader bootloader interaction

        Don't call this constructor, either instantiate ESP8266ROM
        or ESP32ROM, or use ESPLoader.detect_chip().

        This base class has all of the instance methods for bootloader
        functionality supported across various chips & stub
        loaders. Subclasses replace the functions they don't support
        with ones which throw NotImplementedInROMError().

        """
        if isinstance(port, serial.Serial):
            self._port = port
        else:
            self._port = serial.serial_for_url(port)
        self._slip_reader = slip_reader(self._port)
        # setting baud rate in a separate step is a workaround for
        # CH341 driver on some Linux versions (this opens at 9600 then
        # sets), shouldn't matter for other platforms/drivers. See
        # https://github.com/espressif/esptool/issues/44#issuecomment-107094446
        self._set_port_baudrate(baud)

    def _set_port_baudrate(self, baud):
        try:
            self._port.baudrate = baud
        except IOError:
            raise FatalError("Failed to set baud rate %d. The driver may not support this rate." % baud)

    @staticmethod
    def detect_chip(port=DEFAULT_PORT, baud=ESP_ROM_BAUD, connect_mode='default_reset'):
        """ Use serial access to detect the chip type.

        We use the UART's datecode register for this, it's mapped at
        the same address on ESP8266 & ESP32 so we can use one
        memory read and compare to the datecode register for each chip
        type.

        This routine automatically performs ESPLoader.connect() (passing
        connect_mode parameter) as part of querying the chip.
        """
        detect_port = ESPLoader(port, baud)
        detect_port.connect(connect_mode)
        print('Detecting chip type...', end='')
        sys.stdout.flush()
        date_reg = detect_port.read_reg(ESPLoader.UART_DATA_REG_ADDR)

        for cls in [ESP8266ROM, ESP32ROM]:
            if date_reg == cls.DATE_REG_VALUE:
                # don't connect a second time
                inst = cls(detect_port._port, baud)
                print(' %s' % inst.CHIP_NAME)
                return inst
        print('')
        raise FatalError("Unexpected UART datecode value 0x%08x. Failed to autodetect chip type." % date_reg)

    """ Read a SLIP packet from the serial port """
    def read(self):
        return next(self._slip_reader)

    """ Write bytes to the serial port while performing SLIP escaping """
    def write(self, packet):
        buf = b'\xc0' \
              + (packet.replace(b'\xdb',b'\xdb\xdd').replace(b'\xc0',b'\xdb\xdc')) \
              + b'\xc0'
        self._port.write(buf)

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
    def command(self, op=None, data=b"", chk=0, wait_response=True):
        if op is not None:
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

        raise FatalError("Response doesn't match request")

    def check_command(self, op_description, op=None, data=b'', chk=0):
        """
        Execute a command with 'command', check the result code and throw an appropriate
        FatalError if it fails.

        Returns the "result" of a successful command.
        """
        val, data = self.command(op, data, chk)

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
        self._slip_reader = slip_reader(self._port)

    def sync(self):
        self.command(self.ESP_SYNC, b'\x07\x07\x12\x20' + 32 * b'\x55')
        for i in range(7):
            self.command()

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

        # issue reset-to-bootloader:
        # RTS = either CH_PD/EN or nRESET (both active low = chip in reset
        # DTR = GPIO0 (active low = boot to flasher)
        #
        # DTR & RTS are active low signals,
        # ie True = pin @ 0V, False = pin @ VCC.
        if mode != 'no_reset':
            self._port.setDTR(False)  # IO0=HIGH
            self._port.setRTS(True)   # EN=LOW, chip in reset
            time.sleep(0.1)
            if esp32r0_delay:
                # Some chips are more likely to trigger the esp32r0
                # watchdog reset silicon bug if they're held with EN=LOW
                # for a longer period
                time.sleep(1.2)
            self._port.setDTR(True)   # IO0=LOW
            self._port.setRTS(False)  # EN=HIGH, chip out of reset
            if esp32r0_delay:
                # Sleep longer after reset.
                # This workaround only works on revision 0 ESP32 chips,
                # it exploits a silicon bug spurious watchdog reset.
                time.sleep(0.4)  # allow watchdog reset to occur
            time.sleep(0.05)
            self._port.setDTR(False)  # IO0=HIGH, done

        self._port.timeout = SYNC_TIMEOUT
        for _ in range(5):
            try:
                self.flush_input()
                self._port.flushOutput()
                self.sync()
                self._port.timeout = DEFAULT_TIMEOUT
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

    def connect(self, mode='default_reset'):
        """ Try connecting repeatedly until successful, or giving up """
        print('Connecting...', end='')
        sys.stdout.flush()
        last_error = None

        try:
            for _ in range(10):
                last_error = self._connect_attempt(mode=mode, esp32r0_delay=False)
                if last_error is None:
                    return
                last_error = self._connect_attempt(mode=mode, esp32r0_delay=True)
                if last_error is None:
                    return
        finally:
            print('')  # end 'Connecting...' line
        raise FatalError('Failed to connect to %s: %s' % (self.CHIP_NAME, last_error))

    """ Read memory address in target """
    def read_reg(self, addr):
        # we don't call check_command here because read_reg() function is called
        # when detecting chip type, and the way we check for success (STATUS_BYTES_LENGTH) is different
        # for different chip types (!)
        val, data = self.command(self.ESP_READ_REG, struct.pack('<I', addr))
        if byte(data, 0) != 0:
            raise FatalError.WithResult("Failed to read register address %08x" % addr, data)
        return val

    """ Write to memory address in target """
    def write_reg(self, addr, value, mask=0xFFFFFFFF, delay_us=0):
        return self.check_command("write target memory", self.ESP_WRITE_REG,
                                  struct.pack('<IIII', addr, value, mask, delay_us))

    """ Start downloading an application image to RAM """
    def mem_begin(self, size, blocks, blocksize, offset):
        return self.check_command("enter RAM download mode", self.ESP_MEM_BEGIN,
                                  struct.pack('<IIII', size, blocks, blocksize, offset))

    """ Send a block of an image to RAM """
    def mem_block(self, data, seq):
        return self.check_command("write to target RAM", self.ESP_MEM_DATA,
                                  struct.pack('<IIII', len(data), seq, 0, 0) + data,
                                  self.checksum(data))

    """ Leave download mode and run the application """
    def mem_finish(self, entrypoint=0):
        return self.check_command("leave RAM download mode", self.ESP_MEM_END,
                                  struct.pack('<II', int(entrypoint == 0), entrypoint))

    """ Start downloading to Flash (performs an erase)

    Returns number of blocks (of size self.FLASH_WRITE_SIZE) to write.
    """
    def flash_begin(self, size, offset):
        num_blocks = (size + self.FLASH_WRITE_SIZE - 1) // self.FLASH_WRITE_SIZE
        erase_size = self.get_erase_size(offset, size)

        self._port.timeout = START_FLASH_TIMEOUT
        t = time.time()
        self.check_command("enter Flash download mode", self.ESP_FLASH_BEGIN,
                           struct.pack('<IIII', erase_size, num_blocks, self.FLASH_WRITE_SIZE, offset))
        if size != 0 and not self.IS_STUB:
            print("Took %.2fs to erase flash block" % (time.time() - t))
        self._port.timeout = DEFAULT_TIMEOUT
        return num_blocks

    """ Write block to flash """
    def flash_block(self, data, seq):
        self.check_command("write to target Flash after seq %d" % seq,
                           self.ESP_FLASH_DATA,
                           struct.pack('<IIII', len(data), seq, 0, 0) + data,
                           self.checksum(data))

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

        self._port.timeout = START_FLASH_TIMEOUT
        t = time.time()
        if self.IS_STUB:
            write_size = size  # stub expects number of bytes here, manages erasing internally
        else:
            write_size = erase_blocks * self.FLASH_WRITE_SIZE  # ROM expects rounded up to erase block size
        print("Compressed %d bytes to %d..." % (size, compsize))
        self.check_command("enter compressed flash mode", self.ESP_FLASH_DEFL_BEGIN,
                           struct.pack('<IIII', write_size, num_blocks, self.FLASH_WRITE_SIZE, offset))
        if size != 0 and not self.IS_STUB:
            # (stub erases as it writes, but ROM loaders erase on begin)
            print("Took %.2fs to erase flash block" % (time.time() - t))
        self._port.timeout = DEFAULT_TIMEOUT
        return num_blocks

    """ Write block to flash, send compressed """
    @stub_and_esp32_function_only
    def flash_defl_block(self, data, seq):
        self.check_command("write compressed data to flash after seq %d" % seq,
                           self.ESP_FLASH_DEFL_DATA, struct.pack('<IIII', len(data), seq, 0, 0) + data, self.checksum(data))

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
        res = self.check_command('calculate md5sum', self.ESP_SPI_FLASH_MD5, struct.pack('<IIII', addr, size, 0, 0))

        if len(res) == 32:
            return res.decode("utf-8")  # already hex formatted
        elif len(res) == 16:
            return hexify(res).lower()
        else:
            raise FatalError("MD5Sum command returned unexpected result: %r" % res)

    @stub_and_esp32_function_only
    def change_baud(self, baud):
        print("Changing baud rate to %d" % baud)
        self.command(self.ESP_CHANGE_BAUDRATE, struct.pack('<II', baud, 0))
        print("Changed.")
        self._set_port_baudrate(baud)
        time.sleep(0.05)  # get rid of crap sent during baud rate change
        self.flush_input()

    @stub_function_only
    def erase_flash(self):
        # depending on flash chip model the erase may take this long (maybe longer!)
        self._port.timeout = CHIP_ERASE_TIMEOUT
        try:
            self.check_command("erase flash", self.ESP_ERASE_FLASH)
        finally:
            self._port.timeout = DEFAULT_TIMEOUT

    @stub_function_only
    def erase_region(self, offset, size):
        if offset % self.FLASH_SECTOR_SIZE != 0:
            raise FatalError("Offset to erase from must be a multiple of 4096")
        if size % self.FLASH_SECTOR_SIZE != 0:
            raise FatalError("Size of data to erase must be a multiple of 4096")
        self.check_command("erase region", self.ESP_ERASE_REGION, struct.pack('<II', offset, size))

    @stub_function_only
    def read_flash(self, offset, length, progress_fn=None):
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

        # SPI registers, base address differs ESP32 vs 8266
        base = self.SPI_REG_BASE
        SPI_CMD_REG       = base + 0x00
        SPI_USR_REG       = base + 0x1C
        SPI_USR1_REG      = base + 0x20
        SPI_USR2_REG      = base + 0x24
        SPI_W0_REG        = base + self.SPI_W0_OFFS

        # following two registers are ESP32 only
        if self.SPI_HAS_MOSI_DLEN_REG:
            # ESP32 has a more sophisticated wayto set up "user" commands
            def set_data_lengths(mosi_bits, miso_bits):
                SPI_MOSI_DLEN_REG = base + 0x28
                SPI_MISO_DLEN_REG = base + 0x2C
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
        SPI_USR2_DLEN_SHIFT = 28

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
                       (7 << SPI_USR2_DLEN_SHIFT) | spiflash_command)
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

    def hard_reset(self):
        self._port.setRTS(True)  # EN->LOW
        time.sleep(0.1)
        self._port.setRTS(False)

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

    # OTP ROM addresses
    ESP_OTP_MAC0    = 0x3ff00050
    ESP_OTP_MAC1    = 0x3ff00054
    ESP_OTP_MAC3    = 0x3ff0005c

    SPI_REG_BASE    = 0x60000200
    SPI_W0_OFFS     = 0x40
    SPI_HAS_MOSI_DLEN_REG = False

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

    def get_chip_description(self):
        return "ESP8266"

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
        """ Read Chip ID from OTP ROM - see http://esp8266-re.foogod.com/wiki/System_get_chip_id_%28IoT_RTOS_SDK_0.9.9%29 """
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


class ESP8266StubLoader(ESP8266ROM):
    """ Access class for ESP8266 stub loader, runs on top of ROM.
    """
    FLASH_WRITE_SIZE = 0x4000  # matches MAX_WRITE_BLOCK in stub_loader.c
    IS_STUB = True

    def __init__(self, rom_loader):
        self._port = rom_loader._port
        self.flush_input()  # resets _slip_reader

    def get_erase_size(self, offset, size):
        return size  # stub doesn't have same size bug as ROM loader


ESP8266ROM.STUB_CLASS = ESP8266StubLoader


class ESP32ROM(ESPLoader):
    """Access class for ESP32 ROM bootloader

    """
    CHIP_NAME = "ESP32"
    IS_STUB = False

    DATE_REG_VALUE = 0x15122500

    IROM_MAP_START = 0x400d0000
    IROM_MAP_END   = 0x40400000
    DROM_MAP_START = 0x3F400000
    DROM_MAP_END   = 0x3F800000

    # ESP32 uses a 4 byte status reply
    STATUS_BYTES_LENGTH = 4

    SPI_REG_BASE   = 0x60002000
    EFUSE_REG_BASE = 0x6001a000

    SPI_W0_OFFS = 0x80
    SPI_HAS_MOSI_DLEN_REG = True

    FLASH_SIZES = {
        '1MB':0x00,
        '2MB':0x10,
        '4MB':0x20,
        '8MB':0x30,
        '16MB':0x40
    }

    BOOTLOADER_FLASH_OFFSET = 0x1000

    def get_chip_description(self):
        blk3 = self.read_efuse(3)
        chip_version = (blk3 >> 12) & 0xF
        pkg_version = (blk3 >> 9) & 0x07

        silicon_rev = {
            0: "0",
            8: "1"
        }.get(chip_version, "(unknown 0x%x)" % chip_version)

        chip_name = {
            0: "ESP32D0WDQ6",
            1: "ESP32D0WDQ5",
            2: "ESP32D2WDQ5",
        }.get(pkg_version, "unknown ESP32")

        return "%s (revision %s)" % (chip_name, silicon_rev)

    def read_efuse(self, n):
        """ Read the nth word of the ESP3x EFUSE region. """
        return self.read_reg(self.EFUSE_REG_BASE + (4 * n))

    def chip_id(self):
        word16 = self.read_efuse(1)
        word17 = self.read_efuse(2)
        return ((word17 & MAX_UINT24) << 24) | (word16 >> 8) & MAX_UINT24

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


class ESP32StubLoader(ESP32ROM):
    """ Access class for ESP32 stub loader, runs on top of ROM.
    """
    FLASH_WRITE_SIZE = 0x4000  # matches MAX_WRITE_BLOCK in stub_loader.c
    STATUS_BYTES_LENGTH = 2  # same as ESP8266, different to ESP32 ROM
    IS_STUB = True

    def __init__(self, rom_loader):
        self._port = rom_loader._port
        self.flush_input()  # resets _slip_reader


ESP32ROM.STUB_CLASS = ESP32StubLoader


class ESPBOOTLOADER(object):
    """ These are constants related to software ESP bootloader, working with 'v2' image files """

    # First byte of the "v2" application image
    IMAGE_V2_MAGIC = 0xea

    # First 'segment' value in a "v2" application image, appears to be a constant version value?
    IMAGE_V2_SEGMENT = 4


def LoadFirmwareImage(chip, filename):
    """ Load a firmware image. Can be for ESP8266 or ESP32. ESP8266 images will be examined to determine if they are
        original ROM firmware images (ESPFirmwareImage) or "v2" OTA bootloader images.

        Returns a BaseFirmwareImage subclass, either ESPFirmwareImage (v1) or OTAFirmwareImage (v2).
    """
    with open(filename, 'rb') as f:
        if chip == 'esp32':
            return ESP32FirmwareImage(f)
        else:  # Otherwise, ESP8266 so look at magic to determine the image type
            magic = ord(f.read(1))
            f.seek(0)
            if magic == ESPLoader.ESP_IMAGE_MAGIC:
                return ESPFirmwareImage(f)
            elif magic == ESPBOOTLOADER.IMAGE_V2_MAGIC:
                return OTAFirmwareImage(f)
            else:
                raise FatalError("Invalid image magic number: %d" % magic)


class ImageSegment(object):
    """ Wrapper class for a segment in an ESP image
    (very similar to a section in an ELFImage also) """
    def __init__(self, addr, data, file_offs=None):
        self.addr = addr
        # pad all ImageSegments to at least 4 bytes length
        self.data = pad_to(data, 4, b'\x00')
        self.file_offs = file_offs
        self.include_in_checksum = True

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

    """ Base class with common firmware image functions """
    def __init__(self):
        self.segments = []
        self.entrypoint = 0

    def load_common_header(self, load_file, expected_magic):
            (magic, segments, self.flash_mode, self.flash_size_freq, self.entrypoint) = struct.unpack('<BBBBI', load_file.read(8))

            if magic != expected_magic or segments > 16:
                raise FatalError('Invalid firmware image magic=%d segments=%d' % (magic, segments))
            return segments

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

    def save_segment(self, f, segment, checksum=None):
        """ Save the next segment to the image file, return next checksum value if provided """
        f.write(struct.pack('<II', segment.addr, len(segment.data)))
        f.write(segment.data)
        if checksum is not None:
            return ESPLoader.checksum(segment.data, checksum)

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


class ESPFirmwareImage(BaseFirmwareImage):
    """ 'Version 1' firmware image, segments loaded directly by the ROM bootloader. """

    ROM_LOADER = ESP8266ROM

    def __init__(self, load_file=None):
        super(ESPFirmwareImage, self).__init__()
        self.flash_mode = 0
        self.flash_size_freq = 0
        self.version = 1

        if load_file is not None:
            segments = self.load_common_header(load_file, ESPLoader.ESP_IMAGE_MAGIC)

            for _ in range(segments):
                self.load_segment(load_file)
            self.checksum = self.read_checksum(load_file)

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


class OTAFirmwareImage(BaseFirmwareImage):
    """ 'Version 2' firmware image, segments loaded by software bootloader stub
        (ie Espressif bootloader or rboot)
    """

    ROM_LOADER = ESP8266ROM

    def __init__(self, load_file=None):
        super(OTAFirmwareImage, self).__init__()
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
            # for actual mapped addr, add ESP8266ROM.IROM_MAP_START + flashing_Addr + 8
            irom_segment.addr = 0
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
                self.save_segment(f, irom_segment)

            # second header, matches V1 header and contains loadable segments
            normal_segments = self.get_non_irom_segments()
            self.write_common_header(f, normal_segments)
            checksum = ESPLoader.ESP_CHECKSUM_MAGIC
            for segment in normal_segments:
                checksum = self.save_segment(f, segment, checksum)
            self.append_checksum(f, checksum)


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

    EXTENDED_HEADER_STRUCT_FMT = "B" * 16

    def __init__(self, load_file=None):
        super(ESP32FirmwareImage, self).__init__()
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

    def is_flash_addr(self, addr):
        return (ESP32ROM.IROM_MAP_START <= addr < ESP32ROM.IROM_MAP_END) \
            or (ESP32ROM.DROM_MAP_START <= addr < ESP32ROM.DROM_MAP_END)

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

            IROM_ALIGN = 65536

            # check for multiple ELF sections that are mapped in the same flash mapping region.
            # this is usually a sign of a broken linker script, but if you have a legitimate
            # use case then let us know (we can merge segments here, but as a rule you probably
            # want to merge them in your linker script.)
            if len(flash_segments) > 0:
                last_addr = flash_segments[0].addr
                for segment in flash_segments[1:]:
                    if segment.addr // IROM_ALIGN == last_addr // IROM_ALIGN:
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
                align_past = (segment.addr % IROM_ALIGN) - self.SEG_HEADER_LEN
                pad_len = (IROM_ALIGN - (f.tell() % IROM_ALIGN)) + align_past
                if pad_len == 0 or pad_len == IROM_ALIGN:
                    return 0  # already aligned

                # subtract SEG_HEADER_LEN a second time, as the padding block has a header as well
                pad_len -= self.SEG_HEADER_LEN
                if pad_len < 0:
                    pad_len += IROM_ALIGN
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
                    assert (f.tell() + 8) % IROM_ALIGN == segment.addr % IROM_ALIGN
                    checksum = self.save_segment(f, segment, checksum)
                    flash_segments.pop(0)
                    total_segments += 1

            # flash segments all written, so write any remaining RAM segments
            for segment in ram_segments:
                checksum = self.save_segment(f, segment, checksum)
                total_segments += 1

            # done writing segments
            self.append_checksum(f, checksum)
            # kinda hacky: go back to the initial header and write the new segment count
            # that includes padding segments. This header is not checksummed
            image_length = f.tell()
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

    def save_extended_header(self, save_file):
        def join_byte(ln,hn):
            return (ln & 0x0F) + ((hn & 0x0F) << 4)

        append_digest = 1 if self.append_digest else 0

        fields = [self.wp_pin,
                  join_byte(self.clk_drv, self.q_drv),
                  join_byte(self.d_drv, self.cs_drv),
                  join_byte(self.hd_drv, self.wp_drv)]
        fields += [0] * 11
        fields += [append_digest]

        packed = struct.pack(self.EXTENDED_HEADER_STRUCT_FMT, *fields)
        save_file.write(packed)


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
            raise FatalError("%s has unexpected section header entry size 0x%x (not 0x28)" % (self.name, shentsize, self.LEN_SEC_HEADER))
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
                         if lma != 0]
        self.sections = prog_sections


def slip_reader(port):
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
            raise FatalError("Timed out waiting for packet %s" % ("header" if partial_packet is None else "content"))
        for b in read_bytes:

            if type(b) is int:
                b = bytes([b])  # python 2/3 compat

            if partial_packet is None:  # waiting for packet header
                if b == b'\xc0':
                    partial_packet = b""
                else:
                    raise FatalError('Invalid head of packet (%r)' % b)
            elif in_escape:  # part-way through escape sequence
                in_escape = False
                if b == b'\xdc':
                    partial_packet += b'\xc0'
                elif b == b'\xdd':
                    partial_packet += b'\xdb'
                else:
                    raise FatalError('Invalid SLIP escape (%r%r)' % (b'\xdb', b))
            elif b == b'\xdb':  # start of escape sequence
                in_escape = True
            elif b == b'\xc0':  # end of packet
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


def hexify(s):
    if not PYTHON2:
        return ''.join('%02X' % c for c in s)
    else:
        return ''.join('%02X' % ord(c) for c in s)


def unhexify(hs):
    s = bytes()

    for i in range(0, len(hs) - 1, 2):
        hex_string = hs[i:i + 2]

        if not PYTHON2:
            s += bytes([int(hex_string, 16)])
        else:
            s += chr(int(hex_string, 16))

    return s


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

# "Operation" commands, executable at command line. One function each
#
# Each function takes either two args (<ESPLoader instance>, <args>) or a single <args>
# argument.


def load_ram(esp, args):
    image = LoadFirmwareImage(esp, args.filename)

    print('RAM boot...')
    for (offset, size, data) in image.segments:
        print('Downloading %d bytes at %08x...' % (size, offset), end=' ')
        sys.stdout.flush()
        esp.mem_begin(size, div_roundup(size, esp.ESP_RAM_BLOCK), esp.ESP_RAM_BLOCK, offset)

        seq = 0
        while len(data) > 0:
            esp.mem_block(data[0:esp.ESP_RAM_BLOCK], seq)
            data = data[esp.ESP_RAM_BLOCK:]
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
    f = open(args.filename, 'wb')
    for i in range(args.size // 4):
        d = esp.read_reg(args.address + (i * 4))
        f.write(struct.pack(b'<I', d))
        if f.tell() % 1024 == 0:
            print('\r%d bytes read... (%d %%)' % (f.tell(),
                                                  f.tell() * 100 // args.size),
                  end=' ')
        sys.stdout.flush()
    print('Done!')


def detect_flash_size(esp, args):
    if args.flash_size == 'detect':
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
    if address != esp.BOOTLOADER_FLASH_OFFSET or magic != esp.ESP_IMAGE_MAGIC:
        return image  # not flashing a bootloader, so don't modify this

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

    # verify file sizes fit in flash
    flash_end = flash_size_bytes(args.flash_size)
    for address, argfile in args.addr_filename:
        argfile.seek(0,2)  # seek to end
        if address + argfile.tell() > flash_end:
            raise FatalError(("File %s (length %d) at offset %d will not fit in %d bytes of flash. " +
                             "Use --flash-size argument, or change flashing address.")
                             % (argfile.name, argfile.tell(), address, flash_end))
        argfile.seek(0)

    for address, argfile in args.addr_filename:
        if args.no_stub:
            print('Erasing flash...')
        image = pad_to(argfile.read(), 4)
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
            blocks = esp.flash_begin(uncsize, address)
        argfile.seek(0)  # in case we need it again
        seq = 0
        written = 0
        t = time.time()
        esp._port.timeout = min(DEFAULT_TIMEOUT * ratio,
                                CHIP_ERASE_TIMEOUT * 2)
        while len(image) > 0:
            print('\rWriting at 0x%08x... (%d %%)' % (address + seq * esp.FLASH_WRITE_SIZE, 100 * (seq + 1) // blocks), end='')
            sys.stdout.flush()
            block = image[0:esp.FLASH_WRITE_SIZE]
            if args.compress:
                esp.flash_defl_block(block, seq)
            else:
                # Pad the last block
                block = block + b'\xff' * (esp.FLASH_WRITE_SIZE - len(block))
                esp.flash_block(block, seq)
            image = image[esp.FLASH_WRITE_SIZE:]
            seq += 1
            written += len(block)
        t = time.time() - t
        speed_msg = ""
        if args.compress:
            if t > 0.0:
                speed_msg = " (effective %.1f kbit/s)" % (uncsize / t * 8 / 1000)
            print('\rWrote %d bytes (%d compressed) at 0x%08x in %.1f seconds%s...' % (uncsize, written, address, t, speed_msg))
        else:
            if t > 0.0:
                speed_msg = " (%.1f kbit/s)" % (written / t * 8 / 1000)
            print('\rWrote %d bytes at 0x%08x in %.1f seconds%s...' % (written, address, t, speed_msg))
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
        esp._port.timeout = DEFAULT_TIMEOUT

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
    print
    idx = 0
    for seg in image.segments:
        idx += 1
        print('Segment %d: %r' % (idx, seg))
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
    image = ESPFirmwareImage()
    if len(args.segfile) == 0:
        raise FatalError('No segments specified')
    if len(args.segfile) != len(args.segaddr):
        raise FatalError('Number of specified files does not match number of specified addresses')
    for (seg, addr) in zip(args.segfile, args.segaddr):
        data = open(seg, 'rb').read()
        image.segments.append(ImageSegment(addr, data))
    image.entrypoint = args.entrypoint
    image.save(args.output)


def elf2image(args):
    e = ELFFile(args.input)
    if args.chip == 'auto':  # Default to ESP8266 for backwards compatibility
        print("Creating image for ESP8266...")
        args.chip == 'esp8266'

    if args.chip == 'esp32':
        image = ESP32FirmwareImage()
    elif args.version == '1':  # ESP8266
        image = ESPFirmwareImage()
    else:
        image = OTAFirmwareImage()
    image.entrypoint = e.entrypoint
    image.segments = e.sections  # ELFSection is a subclass of ImageSegment
    image.flash_mode = {'qio':0, 'qout':1, 'dio':2, 'dout': 3}[args.flash_mode]
    image.flash_size_freq = image.ROM_LOADER.FLASH_SIZES[args.flash_size]
    image.flash_size_freq += {'40m':0, '26m':1, '20m':2, '80m': 0xf}[args.flash_freq]

    if args.output is None:
        args.output = image.default_output_name(args.input)
    image.save(args.output)


def read_mac(esp, args):
    mac = esp.read_mac()

    def print_mac(label, mac):
        print('%s: %s' % (label, ':'.join(map(lambda x: '%02x' % x, mac))))
    print_mac("MAC", mac)


def chip_id(esp, args):
    chipid = esp.chip_id()
    print('Chip ID: 0x%08x' % chipid)


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
    print('\rRead %d bytes at 0x%x in %.1f seconds (%.1f kbit/s)...'
          % (len(data), args.address, t, len(data) / t * 8 / 1000))
    open(args.filename, 'wb').write(data)


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


def version(args):
    print(__version__)

#
# End of operations functions
#


def main():
    parser = argparse.ArgumentParser(description='esptool.py v%s - ESP8266 ROM Bootloader Utility' % __version__, prog='esptool')

    parser.add_argument('--chip', '-c',
                        help='Target chip type',
                        choices=['auto', 'esp8266', 'esp32'],
                        default=os.environ.get('ESPTOOL_CHIP', 'auto'))

    parser.add_argument(
        '--port', '-p',
        help='Serial port device',
        default=os.environ.get('ESPTOOL_PORT', ESPLoader.DEFAULT_PORT))

    parser.add_argument(
        '--baud', '-b',
        help='Serial port baud rate used when flashing/reading',
        type=arg_auto_int,
        default=os.environ.get('ESPTOOL_BAUD', ESPLoader.ESP_ROM_BAUD))

    parser.add_argument(
        '--before',
        help='What to do before connecting to the chip',
        choices=['default_reset', 'no_reset'],
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

        parent.add_argument('--flash_freq', '-ff', help='SPI Flash frequency',
                            choices=extra_keep_args + ['40m', '26m', '20m', '80m'],
                            default=os.environ.get('ESPTOOL_FF', '40m' if is_elf2image else 'keep'))
        parent.add_argument('--flash_mode', '-fm', help='SPI Flash mode',
                            choices=extra_keep_args + ['qio', 'qout', 'dio', 'dout'],
                            default=os.environ.get('ESPTOOL_FM', 'qio' if is_elf2image else 'keep'))
        parent.add_argument('--flash_size', '-fs', help='SPI Flash size in MegaBytes (1MB, 2MB, 4MB, 8MB, 16M)'
                            ' plus ESP8266-only (256KB, 512KB, 2MB-c1, 4MB-c1)',
                            action=FlashSizeAction, auto_detect=auto_detect,
                            default=os.environ.get('ESPTOOL_FS', 'detect' if auto_detect else '1MB'))
        add_spi_connection_arg(parent)

    parser_write_flash = subparsers.add_parser(
        'write_flash',
        help='Write a binary blob to flash')
    parser_write_flash.add_argument('addr_filename', metavar='<address> <filename>', help='Address followed by binary filename, separated by space',
                                    action=AddrFilenamePairAction)
    add_spi_flash_subparsers(parser_write_flash, is_elf2image=False)
    parser_write_flash.add_argument('--no-progress', '-p', help='Suppress progress output', action="store_true")
    parser_write_flash.add_argument('--verify', help='Verify just-written data on flash ' +
                                    '(mostly superfluous, data is read back during flashing)', action='store_true')
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

    # internal sanity check - every operation matches a module function of the same name
    for operation in subparsers.choices.keys():
        assert operation in globals(), "%s should be a module function" % operation

    expand_file_arguments()

    args = parser.parse_args()

    print('esptool.py v%s' % __version__)

    # operation function can take 1 arg (args), 2 args (esp, arg)
    # or be a member function of the ESPLoader class.

    if args.operation is None:
        parser.print_help()
        sys.exit(1)

    operation_func = globals()[args.operation]
    operation_args,_,_,_ = inspect.getargspec(operation_func)
    if operation_args[0] == 'esp':  # operation function takes an ESPLoader connection object
        initial_baud = min(ESPLoader.ESP_ROM_BAUD, args.baud)  # don't sync faster than the default baud rate
        if args.chip == 'auto':
            esp = ESPLoader.detect_chip(args.port, initial_baud, args.before)
        else:
            chip_class = {
                'esp8266': ESP8266ROM,
                'esp32': ESP32ROM,
            }[args.chip]
            esp = chip_class(args.port, initial_baud)
            esp.connect(args.before)

        print("Chip is %s" % (esp.get_chip_description()))

        if not args.no_stub:
            esp = esp.run_stub()

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
            esp.flash_set_parameters(flash_size_bytes(args.flash_size))

        operation_func(esp, args)

        # finish execution based on args.after
        if args.after == 'hard_reset':
            print('Hard resetting...')
            esp.hard_reset()
        elif args.after == 'soft_reset':
            print('Soft resetting...')
            # flash_finish will trigger a soft reset
            esp.soft_reset(False)
        else:
            print('Staying in bootloader.')
            if esp.IS_STUB:
                esp.soft_reset(True)  # exit stub back to ROM loader

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
                                         'Values are SPI, HSPI, or a sequence of 5 pin numbers CLK,Q,D,HD,CS).' % values)
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
            except ValueError as e:
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


def _main():
    try:
        main()
    except FatalError as e:
        print('\nA fatal error occurred: %s' % e)
        sys.exit(2)


if __name__ == '__main__':
    _main()
