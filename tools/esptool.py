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

__version__ = "2.0-beta1"

MAX_UINT32 = 0xffffffff
MAX_UINT24 = 0xffffff


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
        self._port.baudrate = baud

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

    def connect(self, mode='default_reset'):
        """ Try connecting repeatedly until successful, or giving up """
        print('Connecting...', end='')
        sys.stdout.flush()
        last_error = None

        try:
            for _ in range(10):
                # issue reset-to-bootloader:
                # RTS = either CH_PD/EN or nRESET (both active low = chip in reset
                # DTR = GPIO0 (active low = boot to flasher)
                #
                # DTR & RTS are active low signals,
                # ie True = pin @ 0V, False = pin @ VCC.
                if mode != 'no_reset':
                    self._port.setDTR(False)  # IO0=HIGH
                    self._port.setRTS(True)   # EN=LOW, chip in reset
                    time.sleep(0.05)
                    self._port.setDTR(True)   # IO0=LOW
                    self._port.setRTS(False)  # EN=HIGH, chip out of reset
                    if mode == 'esp32r0':
                        # this is a workaround for a bug with the most
                        # common auto reset circuit and Windows, if the EN
                        # pin on the dev board does not have enough
                        # capacitance. This workaround only works on
                        # revision 0 ESP32 chips, it exploits a silicon
                        # bug spurious watchdog reset.
                        #
                        # Details: https://github.com/espressif/esptool/issues/136
                        time.sleep(0.4)  # allow watchdog reset to occur
                    time.sleep(0.05)
                    self._port.setDTR(False)  # IO0=HIGH, done

                self._port.timeout = 0.1
                for _ in range(4):
                    try:
                        self.flush_input()
                        self._port.flushOutput()
                        self.sync()
                        self._port.timeout = 5
                        return
                    except FatalError as e:
                        print('.', end='')
                        sys.stdout.flush()
                        time.sleep(0.05)
                        last_error = e
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
        old_tmo = self._port.timeout
        num_blocks = (size + self.FLASH_WRITE_SIZE - 1) // self.FLASH_WRITE_SIZE
        erase_size = self.get_erase_size(offset, size)

        self._port.timeout = 20
        t = time.time()
        self.check_command("enter Flash download mode", self.ESP_FLASH_BEGIN,
                           struct.pack('<IIII', erase_size, num_blocks, self.FLASH_WRITE_SIZE, offset))
        if size != 0 and not self.IS_STUB:
            print("Took %.2fs to erase flash block" % (time.time() - t))
        self._port.timeout = old_tmo
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
        old_tmo = self._port.timeout
        num_blocks = (compsize + self.FLASH_WRITE_SIZE - 1) // self.FLASH_WRITE_SIZE
        erase_blocks = (size + self.FLASH_WRITE_SIZE - 1) // self.FLASH_WRITE_SIZE

        self._port.timeout = 20
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
        self._port.timeout = old_tmo
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
        self._port.baudrate = baud
        time.sleep(0.05)  # get rid of crap sent during baud rate change
        self.flush_input()

    @stub_function_only
    def erase_flash(self):
        oldtimeout = self._port.timeout
        # depending on flash chip model the erase may take this long (maybe longer!)
        self._port.timeout = 128
        try:
            self.check_command("erase flash", self.ESP_ERASE_FLASH)
        finally:
            self._port.timeout = oldtimeout

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

    def flash_spi_attach(self,is_hspi,is_legacy):
        """Send SPI attach command to enable the SPI flash pins

        ESP8266 ROM does this when you send flash_begin, ESP32 ROM
        has it as a SPI command.
        """
        # last 3 bytes in ESP_SPI_ATTACH argument are reserved values
        arg = struct.pack('<IBBBB', 1 if is_hspi else 0, 1 if is_legacy else 0, 0, 0, 0)
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
            if len(data) % 4 != 0:  # pad to 32-bit multiple
                data += b'\0' * (4 - (len(data) % 4))
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
        '4MB-c2':0x70}

    FLASH_HEADER_OFFSET = 0

    def flash_spi_attach(self, is_spi, is_legacy):
        if self.IS_STUB:
            super(ESP8266ROM, self).flash_spi_attach(is_spi, is_legacy)
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
    DROM_MAP_END   = 0x3F700000

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

    FLASH_HEADER_OFFSET = 0x1000

    def read_efuse(self, n):
        """ Read the nth word of the ESP3x EFUSE region. """
        return self.read_reg(self.EFUSE_REG_BASE + (4 * n))

    def chip_id(self):
        word16 = self.read_efuse(1)
        word17 = self.read_efuse(2)
        return ((word17 & MAX_UINT24) << 24) | (word16 >> 8) & MAX_UINT24

    def read_mac(self):
        """ Read MAC from EFUSE region """
        words = [self.read_efuse(1), self.read_efuse(2)]
        bitstring = struct.pack(">II", *words)
        bitstring = bitstring[:6]  # trim 2 byte CRC
        try:
            return tuple(ord(b) for b in bitstring)  # trim 2 byte CRC
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
        pad_mod = len(data) % 4
        if pad_mod != 0:
            data += b"\x00" * (4 - pad_mod)
        self.data = data
        self.file_offs = file_offs
        self.include_in_checksum = True

    def copy_with_new_addr(self, new_addr):
        """ Return a new ImageSegment with same data, but mapped at
        a new address. """
        return ImageSegment(new_addr, self.data, 0)

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

    def __init__(self, load_file=None):
        super(ESP32FirmwareImage, self).__init__()
        self.flash_mode = 0
        self.flash_size_freq = 0
        self.version = 1
        self.encrypt_flag = False

        if load_file is not None:
            segments = self.load_common_header(load_file, ESPLoader.ESP_IMAGE_MAGIC)
            additional_header = list(struct.unpack("B" * 16, load_file.read(16)))
            self.encrypt_flag = (additional_header[0] == 0x01)

            # check remaining 14 bytes are unused
            if additional_header[2:] != [0] * 14:
                print("WARNING: ESP32 image header contains unknown flags. Possibly this image is from a newer version of esptool.py")

            for _ in range(segments):
                self.load_segment(load_file)
            self.checksum = self.read_checksum(load_file)

    def is_flash_addr(self, addr):
        return (ESP32ROM.IROM_MAP_START <= addr < ESP32ROM.IROM_MAP_END) \
            or (ESP32ROM.DROM_MAP_START <= addr < ESP32ROM.DROM_MAP_END)

    def default_output_name(self, input_file):
        """ Derive a default output name from the ELF name. """
        return "%s.bin" % (os.path.splitext(input_file)[0])

    def warn_if_unusual_segment(self, offset, size, is_irom_segment):
        pass  # TODO: add warnings for ESP32 segment offset/size combinations that are wrong

    def save(self, filename):
        padding_segments = 0
        with open(filename, 'wb') as f:
            self.write_common_header(f, self.segments)

            f.write(b'\x01' if self.encrypt_flag else b'\x00')
            # remaining 15 bytes of header are unused
            f.write(b'\x00' * 15)

            checksum = ESPLoader.ESP_CHECKSUM_MAGIC
            last_addr = None
            for segment in sorted(self.segments, key=lambda s:s.addr):
                # IROM/DROM segment flash mappings need to align on
                # 64kB boundaries.
                #
                # TODO: intelligently order segments to reduce wastage
                # by squeezing smaller DRAM/IRAM segments into the
                # 64kB padding space.
                IROM_ALIGN = 65536

                # check for multiple ELF sections that live in the same flash mapping region.
                # this is usually a sign of a broken linker script, but if you have a legitimate
                # use case then let us know (we can merge segments here, but as a rule you probably
                # want to merge them in your linker script.)
                if last_addr is not None and self.is_flash_addr(last_addr) \
                   and self.is_flash_addr(segment.addr) and segment.addr // IROM_ALIGN == last_addr // IROM_ALIGN:
                    raise FatalError(("Segment loaded at 0x%08x lands in same 64KB flash mapping as segment loaded at 0x%08x. " +
                                     "Can't generate binary. Suggest changing linker script or ELF to merge sections.") %
                                     (segment.addr, last_addr))
                last_addr = segment.addr

                if self.is_flash_addr(segment.addr):
                    # Actual alignment required for the segment header: positioned so that
                    # after we write the next 8 byte header, file_offs % IROM_ALIGN == segment.addr % IROM_ALIGN
                    #
                    # (this is because the segment's vaddr may not be IROM_ALIGNed, more likely is aligned
                    # IROM_ALIGN+0x10 to account for longest possible header.
                    align_past = (segment.addr % IROM_ALIGN) - self.SEG_HEADER_LEN
                    assert (align_past + self.SEG_HEADER_LEN) == (segment.addr % IROM_ALIGN)

                    # subtract SEG_HEADER_LEN a second time, as the padding block has a header as well
                    pad_len = (IROM_ALIGN - (f.tell() % IROM_ALIGN)) + align_past - self.SEG_HEADER_LEN
                    if pad_len < 0:
                        pad_len += IROM_ALIGN
                    if pad_len > 0:
                        null = ImageSegment(0, b'\x00' * pad_len, f.tell())
                        checksum = self.save_segment(f, null, checksum)
                        padding_segments += 1
                    # verify that after the 8 byte header is added, were are at the correct offset relative to the segment's vaddr
                    assert (f.tell() + 8) % IROM_ALIGN == segment.addr % IROM_ALIGN
                checksum = self.save_segment(f, segment, checksum)
            self.append_checksum(f, checksum)
            # kinda hacky: go back to the initial header and write the new segment count
            # that includes padding segments. Luckily(?) this header is not checksummed
            f.seek(1)
            try:
                f.write(chr(len(self.segments) + padding_segments))
            except TypeError:  # Python 3
                f.write(bytes([len(self.segments) + padding_segments]))


class ELFFile(object):
    SEC_TYPE_PROGBITS = 0x01
    SEC_TYPE_STRTAB = 0x03

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
             _ehsize, _phentsize,_phnum,_shentsize,
             _shnum,shstrndx) = struct.unpack("<16sHHLLLLLHHHHHH", f.read(LEN_FILE_HEADER))
        except struct.error as e:
            raise FatalError("Failed to read a valid ELF header from %s: %s" % (self.name, e))

        if byte(ident, 0) != 0x7f or ident[1:4] != b'ELF':
            raise FatalError("%s has invalid ELF magic header" % self.name)
        if machine != 0x5e:
            raise FatalError("%s does not appear to be an Xtensa ELF file. e_machine=%04x" % (self.name, machine))
        self._read_sections(f, shoff, shstrndx)

    def _read_sections(self, f, section_header_offs, shstrndx):
        f.seek(section_header_offs)
        section_header = f.read()
        LEN_SEC_HEADER = 0x28
        if len(section_header) == 0:
            raise FatalError("No section header found at offset %04x in ELF file." % section_header_offs)
        if len(section_header) % LEN_SEC_HEADER != 0:
            print('WARNING: Unexpected ELF section header length %04x is not mod-%02x' % (len(section_header),LEN_SEC_HEADER))

        # walk through the section header and extract all sections
        section_header_offsets = range(0, len(section_header), LEN_SEC_HEADER)

        def read_section_header(offs):
            name_offs,sec_type,_flags,lma,sec_offs,size = struct.unpack_from("<LLLLLL", section_header[offs:])
            return (name_offs, sec_type, lma, size, sec_offs)
        all_sections = [read_section_header(offs) for offs in section_header_offsets]
        prog_sections = [s for s in all_sections if s[1] == ELFFile.SEC_TYPE_PROGBITS]

        # search for the string table section
        if not shstrndx * LEN_SEC_HEADER in section_header_offsets:
            raise FatalError("ELF file has no STRTAB section at shstrndx %d" % shstrndx)
        _,sec_type,_,sec_size,sec_offs = read_section_header(shstrndx * LEN_SEC_HEADER)
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
        args.flash_size = {0x12: '256KB', 0x13: '512KB', 0x14: '1MB', 0x15: '2MB', 0x16: '4MB', 0x17: '8MB', 0x18: '16MB'}.get(size_id)
        if args.flash_size is None:
            print('Warning: Could not auto-detect Flash size (FlashID=0x%x, SizeID=0x%x), defaulting to 4MB' % (flash_id, size_id))
            args.flash_size = '4MB'
        else:
            print('Auto-detected Flash size:', args.flash_size)


def _get_flash_params(esp, args):
    """ Return binary flash parameters (bitstring length 2) for args """
    detect_flash_size(esp, args)

    flash_mode = {'qio':0, 'qout':1, 'dio':2, 'dout': 3}[args.flash_mode]
    flash_size_freq = esp.parse_flash_size_arg(args.flash_size)
    flash_size_freq += {'40m':0, '26m':1, '20m':2, '80m': 0xf}[args.flash_freq]
    return struct.pack(b'BB', flash_mode, flash_size_freq)


def _update_image_flash_params(esp, address, flash_params, image):
    """ Modify the flash mode & size bytes if this looks like an executable image """
    if address == esp.FLASH_HEADER_OFFSET and (image[0] == '\xe9' or image[0] == 0xE9):  # python 2/3 compat:
        print('Flash params set to 0x%04x' % struct.unpack(">H", flash_params))
        image = image[0:2] + flash_params + image[4:]
    return image


def write_flash(esp, args):
    flash_params = _get_flash_params(esp, args)

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
        image = argfile.read()
        image = _update_image_flash_params(esp, address, flash_params, image)
        calcmd5 = hashlib.md5(image).hexdigest()
        uncsize = len(image)
        if args.compress:
            uncimage = image
            image = zlib.compress(uncimage, 9)
            blocks = esp.flash_defl_begin(uncsize, len(image), address)
        else:
            blocks = esp.flash_begin(uncsize, address)
        argfile.seek(0)  # in case we need it again
        seq = 0
        written = 0
        t = time.time()
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
        _verify_flash(esp, args)


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

    if args.chip != 'esp32':
        if args.set_encrypt_flag:
            raise FatalError("--encrypt-flag only applies to ESP32 images")

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
    image.encrypt_flag = args.set_encrypt_flag

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
    print('Device: %02x%02x' % ((flash_id >> 8) & 0xff, (flash_id >> 16) & 0xff))


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


def verify_flash(esp, args, flash_params=None):
    _verify_flash(esp, args)


def _verify_flash(esp, args):
    differences = False
    flash_params = _get_flash_params(esp, args)

    for address, argfile in args.addr_filename:
        image = argfile.read()
        argfile.seek(0)  # rewind in case we need it again

        image = _update_image_flash_params(esp, address, flash_params, image)

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
        choices=['default_reset', 'no_reset', 'esp32r0'],
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

    def add_spi_flash_subparsers(parent, auto_detect=False):
        """ Add common parser arguments for SPI flash properties """
        parent.add_argument('--flash_freq', '-ff', help='SPI Flash frequency',
                            choices=['40m', '26m', '20m', '80m'],
                            default=os.environ.get('ESPTOOL_FF', '40m'))
        parent.add_argument('--flash_mode', '-fm', help='SPI Flash mode',
                            choices=['qio', 'qout', 'dio', 'dout'],
                            default=os.environ.get('ESPTOOL_FM', 'qio'))
        parent.add_argument('--flash_size', '-fs', help='SPI Flash size in MegaBytes (1MB, 2MB, 4MB, 8MB, 16M)'
                            ' plus ESP8266-only (256KB, 512KB, 2MB-c1, 4MB-c1, 4MB-2)',
                            action=FlashSizeAction, auto_detect=auto_detect,
                            default=os.environ.get('ESPTOOL_FS', 'detect' if auto_detect else '1MB'))
        parent.add_argument('--ucIsHspi', '-ih', help='Config SPI PORT/PINS (Espressif internal feature)',action='store_true')
        parent.add_argument('--ucIsLegacy', '-il', help='Config SPI LEGACY (Espressif internal feature)',action='store_true')

    parser_write_flash = subparsers.add_parser(
        'write_flash',
        help='Write a binary blob to flash')
    parser_write_flash.add_argument('addr_filename', metavar='<address> <filename>', help='Address followed by binary filename, separated by space',
                                    action=AddrFilenamePairAction)
    add_spi_flash_subparsers(parser_write_flash, auto_detect=True)
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
    parser_elf2image.add_argument('--set-encrypt-flag', help='Flag image to be encrypted by bootloader after flashing.', action="store_true")

    add_spi_flash_subparsers(parser_elf2image)

    subparsers.add_parser(
        'read_mac',
        help='Read MAC address from OTP ROM')

    subparsers.add_parser(
        'chip_id',
        help='Read Chip ID from OTP ROM')

    subparsers.add_parser(
        'flash_id',
        help='Read SPI flash manufacturer and device ID')

    parser_read_status = subparsers.add_parser(
        'read_flash_status',
        help='Read SPI flash status register')

    parser_read_status.add_argument('--bytes', help='Number of bytes to read (1-3)', type=int, choices=[1,2,3], default=2)

    parser_write_status = subparsers.add_parser(
        'write_flash_status',
        help='Write SPI flash status register')

    parser_write_status.add_argument('--non-volatile', help='Write non-volatile bits (use with caution)', action='store_true')
    parser_write_status.add_argument('--bytes', help='Number of status bytes to write (1-3)', type=int, choices=[1,2,3], default=2)
    parser_write_status.add_argument('value', help='New value', type=arg_auto_int)

    parser_read_flash = subparsers.add_parser(
        'read_flash',
        help='Read SPI flash content')
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
    add_spi_flash_subparsers(parser_verify_flash, auto_detect=True)

    subparsers.add_parser(
        'erase_flash',
        help='Perform Chip Erase on SPI flash')

    parser_erase_region = subparsers.add_parser(
        'erase_region',
        help='Erase a region of the flash')
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

        if not args.no_stub:
            esp = esp.run_stub()

        if args.baud > initial_baud:
            try:
                esp.change_baud(args.baud)
            except NotImplementedInROMError:
                print("WARNING: ROM doesn't support changing baud rate. Keeping initial baud rate %d" % initial_baud)

        # override common SPI flash parameter stuff as required
        if hasattr(args, "ucIsHspi"):
            print("Attaching SPI flash...")
            esp.flash_spi_attach(args.ucIsHspi,args.ucIsLegacy)
        else:
            esp.flash_spi_attach(0, 0)
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
                '32m-c2': '4MB-c2'
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
        setattr(namespace, self.dest, pairs)


# Binary stub code (see flasher_stub dir for source & details)
ESP8266ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))
ESP32ROM.STUB_CODE = eval(zlib.decompress(base64.b64decode(b"""
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\
""")))


if __name__ == '__main__':
    try:
        main()
    except FatalError as e:
        print('\nA fatal error occurred: %s' % e)
        sys.exit(2)
