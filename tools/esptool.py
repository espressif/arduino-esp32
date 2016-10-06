#!/usr/bin/env python
# NB: Before sending a PR to change the above line to '#!/usr/bin/env python2', please read https://github.com/themadinventor/esptool/issues/21
#
# ESP8266 & ESP32 ROM Bootloader Utility
# https://github.com/themadinventor/esptool
#
# Copyright (C) 2014-2016 Fredrik Ahlberg, Angus Gratton, Espressif Systems (Shanghai) PTE LTD, other contributors as noted.
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

__version__ = "2.0-dev"


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
            raise NotImplementedInROMError(obj)
    return inner


def stub_function_only(func):
    """ Attribute for a function only supported in the software stub loader """
    return check_supported_function(func, lambda o: o.IS_STUB)


def stub_and_esp32_function_only(func):
    """ Attribute for a function only supported by software stubs or ESP32 ROM """
    return check_supported_function(func, lambda o: o.IS_STUB or o.CHIP_NAME == "ESP32")


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
    ESP_GET_FLASH_ID = 0xD3

    # Maximum block sized for RAM and Flash writes, respectively.
    ESP_RAM_BLOCK   = 0x1800
    ESP_FLASH_BLOCK = 0x400

    FLASH_WRITE_SIZE = ESP_FLASH_BLOCK

    # Default baudrate. The ROM auto-bauds, so we can use more or less whatever we want.
    ESP_ROM_BAUD    = 115200

    # First byte of the application image
    ESP_IMAGE_MAGIC = 0xe9

    # Initial state for the checksum routine
    ESP_CHECKSUM_MAGIC = 0xef

    # Flash sector size, minimum unit of erase.
    ESP_FLASH_SECTOR = 0x1000

    UART_DATA_REG_ADDR = 0x60000078

    # SPI peripheral "command" bitmasks
    SPI_CMD_READ_ID = 0x10000000

    # Memory addresses
    IROM_MAP_START = 0x40200000
    IROM_MAP_END = 0x40300000

    # The number of bytes in the response that signify command status
    STATUS_BYTES_LENGTH = 2

    def __init__(self, port=DEFAULT_PORT, baud=ESP_ROM_BAUD, do_connect=True):
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
            self._port = serial.Serial(port)
        self._slip_reader = slip_reader(self._port)
        # setting baud rate in a separate step is a workaround for
        # CH341 driver on some Linux versions (this opens at 9600 then
        # sets), shouldn't matter for other platforms/drivers. See
        # https://github.com/themadinventor/esptool/issues/44#issuecomment-107094446
        self._port.baudrate = baud
        if do_connect:
            self.connect()

    @staticmethod
    def detect_chip(port=DEFAULT_PORT, baud=ESP_ROM_BAUD):
        """Use serial access to detect the chip type.

        We use the UART's datecode register for this, it's mapped at
        the same address on ESP8266 & ESP32 so we can use one
        memory read and compare to the datecode register for each chip
        type.

        """
        detect_port = ESPLoader(port, baud, True)
        sys.stdout.write('Detecting chip type... ')
        date_reg = detect_port.read_reg(ESPLoader.UART_DATA_REG_ADDR)

        for cls in [ESP8266ROM, ESP32ROM]:
            if date_reg == cls.DATE_REG_VALUE:
                # don't connect a second time
                inst = cls(detect_port._port, baud, False)
                print '%s' % inst.CHIP_NAME
                return inst
        print ''
        raise FatalError("Unexpected UART datecode value 0x%08x. Failed to autodetect chip type." % date_reg)

    """ Read a SLIP packet from the serial port """
    def read(self):
        r = self._slip_reader.next()
        return r

    """ Write bytes to the serial port while performing SLIP escaping """
    def write(self, packet):
        buf = '\xc0' \
              + (packet.replace('\xdb','\xdb\xdd').replace('\xc0','\xdb\xdc')) \
              + '\xc0'
        self._port.write(buf)

    """ Calculate checksum of a blob, as it is defined by the ROM """
    @staticmethod
    def checksum(data, state=ESP_CHECKSUM_MAGIC):
        for b in data:
            state ^= ord(b)
        return state

    """ Send a request and read the response """
    def command(self, op=None, data="", chk=0):
        if op is not None:
            pkt = struct.pack('<BBHI', 0x00, op, len(data), chk) + data
            self.write(pkt)

        # tries to get a response until that response has the
        # same operation as the request or a retries limit has
        # exceeded. This is needed for some esp8266s that
        # reply with more sync responses than expected.
        for retry in xrange(100):
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

    def check_command(self, op_description, op=None, data="", chk=0):
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
        if status_bytes[0] != '\0':
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
        """ Perform a connection test """
        self.command(self.ESP_SYNC, '\x07\x07\x12\x20' + 32 * '\x55')
        for i in xrange(7):
            self.command()

    def connect(self):
        """ Try connecting repeatedly until successful, or giving up """
        print 'Connecting...'

        for _ in xrange(4):
            # issue reset-to-bootloader:
            # RTS = either CH_PD or nRESET (both active low = chip in reset)
            # DTR = GPIO0 (active low = boot to flasher)
            self._port.setDTR(False)
            self._port.setRTS(True)
            time.sleep(0.05)
            self._port.setDTR(True)
            self._port.setRTS(False)
            time.sleep(0.05)
            self._port.setDTR(False)

            # worst-case latency timer should be 255ms (probably <20ms)
            self._port.timeout = 0.3
            last_exception = None
            for _ in xrange(4):
                try:
                    self.flush_input()
                    self._port.flushOutput()
                    self.sync()
                    self._port.timeout = 5
                    return
                except FatalError as e:
                    last_exception = e
                    time.sleep(0.05)
        raise FatalError('Failed to connect to %s: %s' % (self.CHIP_NAME, last_exception))

    """ Read memory address in target """
    def read_reg(self, addr):
        # we don't call check_command here because read_reg() function is called
        # when detecting chip type, and the way we check for success (STATUS_BYTES_LENGTH) is different
        # for different chip types (!)
        val, data = self.command(self.ESP_READ_REG, struct.pack('<I', addr))
        if data[0] != '\0':
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

    """ Start downloading to Flash (performs an erase) """
    def flash_begin(self, size, offset):
        old_tmo = self._port.timeout
        num_blocks = (size + self.ESP_FLASH_BLOCK - 1) / self.ESP_FLASH_BLOCK
        erase_size = self.get_erase_size(offset, size)

        self._port.timeout = 20
        t = time.time()
        self.check_command("enter Flash download mode", self.ESP_FLASH_BEGIN,
                           struct.pack('<IIII', erase_size, num_blocks, self.ESP_FLASH_BLOCK, offset))
        if size != 0:
            print "Took %.2fs to erase flash block" % (time.time() - t)
        self._port.timeout = old_tmo

    """ Write block to flash """
    def flash_block(self, data, seq):
        self.check_command("write to target Flash after seq %d" % seq,
                           self.ESP_FLASH_DATA,
                           struct.pack('<IIII', len(data), seq, 0, 0) + data,
                           self.checksum(data))

    """ Leave flash mode and run/reboot """
    def flash_finish(self, reboot=False):
        pkt = struct.pack('<I', int(not reboot))
        self.check_command("leave Flash mode", self.ESP_FLASH_END, pkt)

    """ Run application code in flash """
    def run(self, reboot=False):
        # Fake flash begin immediately followed by flash end
        self.flash_begin(0, 0)
        self.flash_finish(reboot)

    """ Read SPI flash manufacturer and device id """
    @stub_function_only
    def flash_id(self):
        resp = self.check_command("get flash id", self.ESP_GET_FLASH_ID)
        return struct.unpack('<I', resp)[0]

    def parse_flash_size_arg(self, arg):
        try:
            return self.FLASH_SIZES[arg]
        except KeyError:
            raise FatalError("Flash size '%s' is not supported by this chip type. Supported sizes: %s"
                             % (arg, ", ".join(self.FLASH_SIZES.keys())))

    """ Abuse the loader protocol to force flash to be left in write mode """
    @esp8266_function_only
    def flash_unlock_dio(self):
        # Enable flash write mode
        self.flash_begin(0, 0)
        # Reset the chip rather than call flash_finish(), which would have
        # write protected the chip again (why oh why does it do that?!)
        self.mem_begin(0,0,0,0x40100000)
        self.mem_finish(0x40000080)

    def run_stub(self, stub=None):
        if stub is None:
            if self.IS_STUB:
                raise FatalError("Not possible for a stub to load another stub (memory likely to overlap.)")
            stub = self.STUB_CODE

        # Upload
        print "Uploading stub..."
        for field in ['text', 'data']:
            if field in stub:
                offs = stub[field + "_start"]
                length = len(stub[field])
                blocks = (length + self.ESP_RAM_BLOCK - 1) / self.ESP_RAM_BLOCK
                self.mem_begin(length, blocks, self.ESP_RAM_BLOCK, offs)
                for seq in range(blocks):
                    from_offs = seq * self.ESP_RAM_BLOCK
                    to_offs = from_offs + self.ESP_RAM_BLOCK
                    self.mem_block(stub[field][from_offs:to_offs], seq)
        print "Running stub..."
        self.mem_finish(stub['entry'])

        p = self.read()
        if p != 'OHAI':
            raise FatalError("Failed to start stub. Unexpected response: %s" % p)
        print "Stub running..."
        return self.STUB_CLASS(self)

    @stub_and_esp32_function_only
    def flash_defl_begin(self, size, compsize, offset):
        """ Start downloading compressed data to Flash (performs an erase) """
        old_tmo = self._port.timeout
        num_blocks = (compsize + self.ESP_FLASH_BLOCK - 1) / self.ESP_FLASH_BLOCK
        erase_blocks = (size + self.ESP_FLASH_BLOCK - 1) / self.ESP_FLASH_BLOCK

        self._port.timeout = 20
        t = time.time()
        print "Unc size %d comp size %d comp blocks %d" % (size, compsize, num_blocks)
        self.check_command("enter compressed flash mode", self.ESP_FLASH_DEFL_BEGIN,
                           struct.pack('<IIII', erase_blocks * self.ESP_FLASH_BLOCK, num_blocks, self.ESP_FLASH_BLOCK, offset))
        if size != 0 and not self.IS_STUB:
            # (stub erases as it writes, but ROM loaders erase on begin)
            print "Took %.2fs to erase flash block" % (time.time() - t)
        self._port.timeout = old_tmo

    """ Write block to flash, send compressed """
    @stub_and_esp32_function_only
    def flash_defl_block(self, data, seq):
        self.check_command("write compressed data to flash after seq %d" % seq,
                           self.ESP_FLASH_DEFL_DATA, struct.pack('<IIII', len(data), seq, 0, 0) + data, self.checksum(data))

    """ Leave compressed flash mode and run/reboot """
    @stub_and_esp32_function_only
    def flash_defl_finish(self, reboot=False):
        pkt = struct.pack('<I', int(not reboot))
        self.check_command("leave compressed flash mode", self.ESP_FLASH_DEFL_END, pkt)
        self.in_bootloader = False

    @stub_and_esp32_function_only
    def flash_md5sum(self, addr, size):
        # the MD5 command returns additional bytes in the standard
        # command reply slot
        res = self.check_command('calculate md5sum', self.ESP_SPI_FLASH_MD5, struct.pack('<IIII', addr, size, 0, 0))

        if len(res) == 32:
            return res  # already hex formatted
        elif len(res) == 16:
            return hexify(res).lower()
        else:
            raise FatalError("MD5Sum command returned unexpected result: %r" % res)

    @stub_and_esp32_function_only
    def change_baud(self, baud):
        print "Changing baud rate to %d" % baud
        self.command(self.ESP_CHANGE_BAUDRATE, struct.pack('<II', baud, 0))
        print "Changed."
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
        if offset % self.ESP_FLASH_SECTOR != 0:
            raise FatalError("Offset to erase from must be a multiple of 4096")
        if size % self.ESP_FLASH_SECTOR != 0:
            raise FatalError("Size of data to erase must be a multiple of 4096")
        self.check_command("erase region", self.ESP_ERASE_REGION, struct.pack('<II', offset, size))

    @stub_function_only
    def read_flash(self, offset, length, progress_fn=None):
        # issue a standard bootloader command to trigger the read
        self.check_command("read flash", self.ESP_READ_FLASH,
                           struct.pack('<IIII',
                                       offset,
                                       length,
                                       self.ESP_FLASH_BLOCK,
                                       64))
        # now we expect (length / block_size) SLIP frames with the data
        data = ''
        while len(data) < length:
            p = self.read()
            data += p
            print "Read %d bytes total %d" % (len(p), len(data))
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

    SPI_CMD_REG_ADDR = 0x60000200
    SPI_W0_REG_ADDR = 0x60000240

    FLASH_SIZES = {
        '512KB':0x00,
        '256KB':0x10,
        '1MB':0x20,
        '2MB':0x30,
        '4MB':0x40,
        '2MB-c1': 0x50,
        '4MB-c1':0x60,
        '4MB-c2':0x70}

    def flash_spi_attach(self, is_spi, is_legacy):
        pass  # not implemented in ROM, but OK to silently skip

    def flash_set_parameters(self, size):
        pass  # not implemented in ROM, but OK to silently skip

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
        sector_size = self.ESP_FLASH_SECTOR
        num_sectors = (size + sector_size - 1) / sector_size
        start_sector = offset / sector_size

        head_sectors = sectors_per_block - (start_sector % sectors_per_block)
        if num_sectors < head_sectors:
            head_sectors = num_sectors

        if num_sectors < 2 * head_sectors:
            return (num_sectors + 1) / 2 * sector_size
        else:
            return (num_sectors - head_sectors) * sector_size


class ESP8266StubLoader(ESP8266ROM):
    """ Access class for ESP8266 stub loader, runs on top of ROM.
    """
    FLASH_WRITE_SIZE = 8192  # matches MAX_WRITE_BLOCK in stub_loader.c
    IS_STUB = True

    def __init__(self, rom_loader):
        self._port = rom_loader._port
        self.flush_input()  # resets _slip_reader

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

    SPI_CMD_REG_ADDR = 0x60003000
    SPI_W0_REG_ADDR = 0x60003040

    EFUSE_BASE = 0x6001a000

    FLASH_SIZES = {
        '1MB':0x00,
        '2MB':0x10,
        '4MB':0x20,
        '8MB':0x30,
        '16MB':0x40
    }

    def read_efuse(self, n):
        """ Read the nth word of the ESP3x EFUSE region. """
        return self.read_reg(self.EFUSE_BASE + (4 * n))

    def chip_id(self):
        word16 = self.read_efuse(16)
        word17 = self.read_efuse(17)
        return ((word17 & MAX_UINT24) << 24) | (word16 >> 8) & MAX_UINT24

    def read_mac(self):
        """ Read MAC from EFUSE region """
        word16 = self.read_efuse(16)
        word17 = self.read_efuse(17)
        word18 = self.read_efuse(18)
        word19 = self.read_efuse(19)
        wifi_mac = (((word17 >> 16) & 0xff), ((word17 >> 8) & 0xff), ((word17 >> 0) & 0xff),
                    ((word16 >> 24) & 0xff), ((word16 >> 16) & 0xff), ((word16 >> 8) & 0xff))
        bt_mac = (((word19 >> 16) & 0xff), ((word19 >> 8) & 0xff), ((word19 >> 0) & 0xff),
                  ((word18 >> 24) & 0xff), ((word18 >> 16) & 0xff), ((word18 >> 8) & 0xff))
        return (wifi_mac,bt_mac)

    def get_erase_size(self, offset, size):
        return size


class ESP32StubLoader(ESP32ROM):
    """ Access class for ESP32 stub loader, runs on top of ROM.
    """
    FLASH_WRITE_SIZE = 8192  # matches MAX_WRITE_BLOCK in stub_loader.c
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
            data += "\x00" * (4 - pad_mod)
        self.data = data
        self.file_offs = file_offs

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
        self.name = name

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
        if not is_irom_segment:
            if offset > 0x40200000 or offset < 0x3ffe0000 or size > 65536:
                print('WARNING: Suspicious segment 0x%x, length %d' % (offset, size))
        segment_data = f.read(size)
        if len(segment_data) < size:
            raise FatalError('End of file reading segment 0x%x, length %d (actual length %d)' % (offset, size, len(segment_data)))
        segment = ImageSegment(offset, segment_data, file_offs)
        self.segments.append(segment)
        return segment

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

    def append_checksum(self, f, checksum):
        """ Append ESPLoader checksum to the just-written image """
        align_file_position(f, 16)
        f.write(struct.pack('B', checksum))

    def write_common_header(self, f, segments):
        f.write(struct.pack('<BBBBI', ESPLoader.ESP_IMAGE_MAGIC, len(segments),
                            self.flash_mode, self.flash_size_freq, self.entrypoint))

    def is_irom_addr(self, addr):
        """ Returns True if an address starts in the irom region.
        Valid for ESP8266 only.
        """
        return ESPLoader.IROM_MAP_START <= addr < ESPLoader.IROM_MAP_END

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
    def __init__(self, load_file=None):
        super(ESPFirmwareImage, self).__init__()
        self.flash_mode = 0
        self.flash_size_freq = 0
        self.version = 1

        if load_file is not None:
            segments = self.load_common_header(load_file, ESPLoader.ESP_IMAGE_MAGIC)

            for _ in xrange(segments):
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
            with open("%s0x%05x.bin" % (basename, irom_segment.addr), "wb") as f:
                f.write(irom_segment.data)

        # everything but IROM goes at 0x00000 in an image file
        normal_segments = self.get_non_irom_segments()
        with open("%s0x00000.bin" % basename, 'wb') as f:
            self.write_common_header(f, normal_segments)
            checksum = ESPLoader.ESP_CHECKSUM_MAGIC
            for segment in self.segments:
                checksum = self.save_segment(f, segment, checksum)
            self.append_checksum(f, checksum)


class OTAFirmwareImage(BaseFirmwareImage):
    """ 'Version 2' firmware image, segments loaded by software bootloader stub
        (ie Espressif bootloader or rboot)
    """
    def __init__(self, load_file=None):
        super(OTAFirmwareImage, self).__init__()
        self.version = 2
        if load_file is not None:
            segments = self.load_common_header(load_file, ESPBOOTLOADER.IMAGE_V2_MAGIC)
            if segments != ESPBOOTLOADER.IMAGE_V2_SEGMENT:
                # segment count is not really segment count here, but we expect to see '4'
                print 'Warning: V2 header has unexpected "segment" count %d (usually 4)' % segments

            # irom segment comes before the second header
            #
            # the file is saved in the image with a zero load address
            # in the header, so we need to calculate a load address
            irom_offs = load_file.tell()
            irom_segment = self.load_segment(load_file, True)
            irom_segment.addr = irom_offs + self.IROM_MAP_START

            first_flash_mode = self.flash_mode
            first_flash_size_freq = self.flash_size_freq
            first_entrypoint = self.entrypoint
            # load the second header
            self.load_common_header(load_file, ESPLoader.ESP_IMAGE_MAGIC)
            (magic, segments, self.flash_mode, self.flash_size_freq, self.entrypoint) = struct.unpack('<BBBBI', load_file.read(8))

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
            for _ in xrange(segments):
                self.load_segment(load_file)
            self.checksum = self.read_checksum(load_file)

    def default_output_name(self, input_file):
        """ Derive a default output name from the ELF name. """
        irom_segment = self.get_irom_segment()
        if irom_segment is not None:
            irom_offs = irom_segment.addr - self.IROM_MAP_START
        else:
            irom_offs = 0
        return "%s-0x%05x.bin" % (os.path.splitext(input_file)[0],
                                  irom_offs & ~(ESPLoader.ESP_FLASH_SECTOR - 1))

    def save(self, filename):
        with open(filename, 'wb') as f:
            # Save first header for irom0 segment
            f.write(struct.pack('<BBBBI', ESPBOOTLOADER.IMAGE_V2_MAGIC, ESPBOOTLOADER.IMAGE_V2_SEGMENT,
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
    def __init__(self, load_file=None):
        super(ESP32FirmwareImage, self).__init__()
        self.flash_mode = 0
        self.flash_size_freq = 0
        self.version = 1
        self.additional_header = '\x00' * 16

        if load_file is not None:
            segments = self.load_common_header(load_file, ESPLoader.ESP_IMAGE_MAGIC)
            self.additional_header = load_file.read(16)

            for i in xrange(segments):
                self.load_segment(load_file)
            self.checksum = self.read_checksum(load_file)

    def is_flash_addr(self, addr):
        return (ESP32ROM.IROM_MAP_START <= addr < ESP32ROM.IROM_MAP_END) \
            or (ESP32ROM.DROM_MAP_START <= addr < ESP32ROM.DROM_MAP_END)

    def default_output_name(self, input_file):
        """ Derive a default output name from the ELF name. """
        return "%s.bin" % (os.path.splitext(input_file)[0])

    def save(self, filename):
        padding_segments = 0
        with open(filename, 'wb') as f:
            self.write_common_header(f, self.segments)
            f.write(self.additional_header)

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
                        null = ImageSegment(0, '\x00' * pad_len, f.tell())
                        checksum = self.save_segment(f, null, checksum)
                        padding_segments += 1
                    # verify that after the 8 byte header is added, were are at the correct offset relative to the segment's vaddr
                    assert (f.tell() + 8) % IROM_ALIGN == segment.addr % IROM_ALIGN
                checksum = self.save_segment(f, segment, checksum)
            self.append_checksum(f, checksum)
            # kinda hacky: go back to the initial header and write the new segment count
            # that includes padding segments. Luckily(?) this header is not checksummed
            f.seek(1)
            f.write(chr(len(self.segments) + padding_segments))


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

        if ident[0] != '\x7f' or ident[1:4] != 'ELF':
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
            print 'WARNING: Unexpected ELF section header length %04x is not mod-%02x' % (len(section_header),LEN_SEC_HEADER)

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
            print 'WARNING: ELF file has incorrect STRTAB section type 0x%02x' % sec_type
        f.seek(sec_offs)
        string_table = f.read(sec_size)

        # build the real list of ELFSections by reading the actual section names from the
        # string table section, and actual data for each section from the ELF file itself
        def lookup_string(offs):
            raw = string_table[offs:]
            return raw[:raw.index('\x00')]

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
        if read_bytes == '':
            raise FatalError("Timed out waiting for packet %s" % ("header" if partial_packet is None else "content"))
        for b in read_bytes:
            if partial_packet is None:  # waiting for packet header
                if b == '\xc0':
                    partial_packet = ""
                else:
                    raise FatalError('Invalid head of packet (%r)' % b)
            elif in_escape:  # part-way through escape sequence
                in_escape = False
                if b == '\xdc':
                    partial_packet += '\xc0'
                elif b == '\xdd':
                    partial_packet += '\xdb'
                else:
                    raise FatalError('Invalid SLIP escape (%r%r)' % ('\xdb', b))
            elif b == '\xdb':  # start of escape sequence
                in_escape = True
            elif b == '\xc0':  # end of packet
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
    return (int(a) + int(b) - 1) / int(b)


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
    return ''.join('%02X' % ord(c) for c in s)


def unhexify(hs):
    s = ''
    for i in range(0, len(hs) - 1, 2):
        s += chr(int(hs[i] + hs[i + 1], 16))
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
        message += " (result was %s)" % ", ".join(hex(ord(x)) for x in result)
        return FatalError(message)


class NotImplementedInROMError(FatalError):
    """
    Wrapper class for the error thrown when a particular ESP bootloader function
    is not implemented in the ROM bootloader.
    """
    def __init__(self, bootloader):
        FatalError.__init__(self, "%s ROM does not support this function." % bootloader.CHIP_NAME)

# "Operation" commands, executable at command line. One function each
#
# Each function takes either two args (<ESPLoader instance>, <args>) or a single <args>
# argument.


def load_ram(esp, args):
    image = LoadFirmwareImage(esp, args.filename)

    print 'RAM boot...'
    for (offset, size, data) in image.segments:
        print 'Downloading %d bytes at %08x...' % (size, offset),
        sys.stdout.flush()
        esp.mem_begin(size, div_roundup(size, esp.ESP_RAM_BLOCK), esp.ESP_RAM_BLOCK, offset)

        seq = 0
        while len(data) > 0:
            esp.mem_block(data[0:esp.ESP_RAM_BLOCK], seq)
            data = data[esp.ESP_RAM_BLOCK:]
            seq += 1
        print 'done!'

    print 'All segments done, executing at %08x' % image.entrypoint
    esp.mem_finish(image.entrypoint)


def read_mem(esp, args):
    print '0x%08x = 0x%08x' % (args.address, esp.read_reg(args.address))


def write_mem(esp, args):
    esp.write_reg(args.address, args.value, args.mask, 0)
    print 'Wrote %08x, mask %08x to %08x' % (args.value, args.mask, args.address)


def dump_mem(esp, args):
    f = file(args.filename, 'wb')
    for i in xrange(args.size / 4):
        d = esp.read_reg(args.address + (i * 4))
        f.write(struct.pack('<I', d))
        if f.tell() % 1024 == 0:
            print '\r%d bytes read... (%d %%)' % (f.tell(),
                                                  f.tell() * 100 / args.size),
        sys.stdout.flush()
    print 'Done!'


def write_flash(esp, args):
    """Write data to flash
    """
    flash_mode = {'qio':0, 'qout':1, 'dio':2, 'dout': 3}[args.flash_mode]
    flash_size_freq = esp.parse_flash_size_arg(args.flash_size)
    flash_size_freq += {'40m':0, '26m':1, '20m':2, '80m': 0xf}[args.flash_freq]
    flash_info = struct.pack('BB', flash_mode, flash_size_freq)

    # verify file sizes fit in flash
    flash_end = flash_size_bytes(args.flash_size)
    for address, argfile in args.addr_filename:
        argfile.seek(0,2)  # seek to end
        if address + argfile.tell() > flash_end:
            raise FatalError("File %s (length %d) at offset %d will not fit in %d bytes of flash." +
                             "Use --flash-size argument, or change flashing address."
                             % (argfile.name, argfile.tell(), address, flash_end))
        argfile.seek(0)

    for address, argfile in args.addr_filename:
        print 'Erasing flash...'
        if args.compress:
            uncimage = argfile.read()
            calcmd5 = hashlib.md5(uncimage).hexdigest()
            uncsize = len(uncimage)
            image = zlib.compress(uncimage, 9)
            blocks = div_roundup(len(image), esp.FLASH_WRITE_SIZE)
            esp.flash_defl_begin(len(uncimage),len(image), address)
        else:
            image = argfile.read()
            calcmd5 = hashlib.md5(image).hexdigest()
            uncsize = len(image)
            blocks = div_roundup(len(image), esp.FLASH_WRITE_SIZE)
            esp.flash_begin(blocks * esp.FLASH_WRITE_SIZE, address)
        argfile.seek(0)  # in case we need it again
        seq = 0
        written = 0
        t = time.time()
        header_block = None
        while len(image) > 0:
            print '\rWriting at 0x%08x... (%d %%)' % (address + seq * esp.FLASH_WRITE_SIZE, 100 * (seq + 1) / blocks),
            sys.stdout.flush()
            block = image[0:esp.FLASH_WRITE_SIZE]
            if args.compress:
                esp.flash_defl_block(block, seq)
            else:
                # Pad the last block
                block = block + '\xff' * (esp.FLASH_WRITE_SIZE - len(block))
                # Fix sflash config data
                if address == 0 and seq == 0 and block[0] == '\xe9':
                    block = block[0:2] + flash_info + block[4:]
                    header_block = block
                esp.flash_block(block, seq)
            image = image[esp.FLASH_WRITE_SIZE:]
            seq += 1
            written += len(block)
        t = time.time() - t
        print '\rWrote %d bytes at 0x%08x in %.1f seconds (%.1f kbit/s)...' % (written, address, t, written / t * 8 / 1000)
        res = esp.flash_md5sum(address, uncsize)
        if res != calcmd5:
            print 'File  md5: %s' % calcmd5
            print 'Flash md5: %s' % res
            raise FatalError("MD5 of file does not match data in flash!")
        else:
            print 'Hash of data verified.'
    print '\nLeaving...'
    if args.flash_mode == 'dio' and esp.CHIP_NAME == "ESP8266":
        esp.flash_unlock_dio()
    else:
        esp.flash_begin(0, 0)
        if args.compress:
            esp.flash_defl_finish(False)
        else:
            esp.flash_finish(False)
    if args.verify:
        print 'Verifying just-written flash...'
        verify_flash(esp, args, header_block)


def image_info(args):
    image = LoadFirmwareImage(args.chip, args.filename)
    print('Image version: %d' % image.version)
    print('Entry point: %08x' % image.entrypoint) if image.entrypoint != 0 else 'Entry point not set'
    print '%d segments' % len(image.segments)
    print
    checksum = ESPLoader.ESP_CHECKSUM_MAGIC
    idx = 0
    for seg in image.segments:
        idx += 1
        print 'Segment %d: %r' % (idx, seg)
        checksum = ESPLoader.checksum(seg.data, checksum)
    print
    print 'Checksum: %02x (%s)' % (image.checksum, 'valid' if image.checksum == checksum else 'invalid!')


def make_image(args):
    image = ESPFirmwareImage()
    if len(args.segfile) == 0:
        raise FatalError('No segments specified')
    if len(args.segfile) != len(args.segaddr):
        raise FatalError('Number of specified files does not match number of specified addresses')
    for (seg, addr) in zip(args.segfile, args.segaddr):
        data = file(seg, 'rb').read()
        image.segments.append(ImageSegment(addr, data))
    image.entrypoint = args.entrypoint
    image.save(args.output)


def elf2image(args):
    e = ELFFile(args.input)
    if args.chip == 'auto':  # Default to ESP8266 for backwards compatibility
        print "Creating image for ESP8266..."
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
    image.flash_size_freq = ESP8266ROM.FLASH_SIZES[args.flash_size]
    image.flash_size_freq += {'40m':0, '26m':1, '20m':2, '80m': 0xf}[args.flash_freq]

    if args.output is None:
        args.output = image.default_output_name(args.input)
    image.save(args.output)


def read_mac(esp, args):
    mac = esp.read_mac()
    print 'MAC: %s' % ':'.join(map(lambda x: '%02x' % x, mac))


def chip_id(esp, args):
    chipid = esp.chip_id()
    print 'Chip ID: 0x%08x' % chipid


def erase_flash(esp, args):
    print 'Erasing flash (this may take a while)...'
    t = time.time()
    esp.erase_flash()
    print 'Chip erase completed successfully in %.1fs' % (time.time() - t)


def erase_region(esp, args):
    print 'Erasing region (may be slow depending on size)...'
    esp.erase_region(args.address, args.size)
    print 'Erase completed successfully.'


def run(esp, args):
    esp.run()


def flash_id(esp, args):
    flash_id = esp.flash_id()
    print 'Manufacturer: %02x' % (flash_id & 0xff)
    print 'Device: %02x%02x' % ((flash_id >> 8) & 0xff, (flash_id >> 16) & 0xff)


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
    print ('\rRead %d bytes at 0x%x in %.1f seconds (%.1f kbit/s)...'
           % (len(data), args.address, t, len(data) / t * 8 / 1000))
    file(args.filename, 'wb').write(data)


def verify_flash(esp, args, flash_params=None):
    differences = False
    for address, argfile in args.addr_filename:
        image = argfile.read()
        argfile.seek(0)  # rewind in case we need it again
        if address == 0 and image[0] == '\xe9' and flash_params is not None:
            image = image[0:2] + flash_params + image[4:]
        image_size = len(image)
        print 'Verifying 0x%x (%d) bytes @ 0x%08x in flash against %s...' % (image_size, image_size, address, argfile.name)
        # Try digest first, only read if there are differences.
        digest = esp.flash_md5sum(address, image_size)
        expected_digest = hashlib.md5(image).hexdigest()
        if digest == expected_digest:
            print '-- verify OK (digest matched)'
            continue
        else:
            differences = True
            if getattr(args, 'diff', 'no') != 'yes':
                print '-- verify FAILED (digest mismatch)'
                continue

        flash = esp.read_flash(address, image_size)
        assert flash != image
        diff = [i for i in xrange(image_size) if flash[i] != image[i]]
        print '-- verify FAILED: %d differences, first @ 0x%08x' % (len(diff), address + diff[0])
        for d in diff:
            print '   %08x %02x %02x' % (address + d, ord(flash[d]), ord(image[d]))
    if differences:
        raise FatalError("Verify failed.")


def version(args):
    print __version__

#
# End of operations functions
#


def main():
    parser = argparse.ArgumentParser(description='esptool.py v%s - ESP8266 ROM Bootloader Utility' % __version__, prog='esptool')

    parser.add_argument('--chip', '-c',
                        help='Target chip type',
                        choices=['auto', 'esp8266', 'esp31', 'esp32'],
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

    def add_spi_flash_subparsers(parent):
        """ Add common parser arguments for SPI flash properties """
        parent.add_argument('--flash_freq', '-ff', help='SPI Flash frequency',
                            choices=['40m', '26m', '20m', '80m'],
                            default=os.environ.get('ESPTOOL_FF', '40m'))
        parent.add_argument('--flash_mode', '-fm', help='SPI Flash mode',
                            choices=['qio', 'qout', 'dio', 'dout'],
                            default=os.environ.get('ESPTOOL_FM', 'qio'))
        parent.add_argument('--flash_size', '-fs', help='SPI Flash size in MegaBytes (1MB, 2MB, 4MB, 8MB, 16M)'
                            ' plus ESP8266-only (256KB, 512KB, 2MB-c1, 4MB-c1, 4MB-2)',
                            action=FlashSizeAction,
                            default=os.environ.get('ESPTOOL_FS', '1MB'))
        parent.add_argument('--ucIsHspi', '-ih', help='Config SPI PORT/PINS (Espressif internal feature)',action='store_true')
        parent.add_argument('--ucIsLegacy', '-il', help='Config SPI LEGACY (Espressif internal feature)',action='store_true')

    parser_write_flash = subparsers.add_parser(
        'write_flash',
        help='Write a binary blob to flash')
    parser_write_flash.add_argument('addr_filename', metavar='<address> <filename>', help='Address followed by binary filename, separated by space',
                                    action=AddrFilenamePairAction)
    add_spi_flash_subparsers(parser_write_flash)
    parser_write_flash.add_argument('--no-progress', '-p', help='Suppress progress output', action="store_true")
    parser_write_flash.add_argument('--verify', help='Verify just-written data (only necessary if very cautious, data is already CRCed', action='store_true')
    parser_write_flash.add_argument('--compress', '-z', help='Compress data in transfer',action="store_true")

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

    args = parser.parse_args()

    print 'esptool.py v%s' % __version__

    # operation function can take 1 arg (args), 2 args (esp, arg)
    # or be a member function of the ESPLoader class.

    operation_func = globals()[args.operation]
    operation_args,_,_,_ = inspect.getargspec(operation_func)
    if operation_args[0] == 'esp':  # operation function takes an ESPLoader connection object
        initial_baud = min(ESPLoader.ESP_ROM_BAUD, args.baud)  # don't sync faster than the default baud rate
        chip_constructor_fun = {
            'auto': ESPLoader.detect_chip,
            'esp8266': ESP8266ROM,
            'esp32': ESP32ROM,
        }[args.chip]
        esp = chip_constructor_fun(args.port, initial_baud)

        if not args.no_stub:
            esp = esp.run_stub()

        if args.baud > initial_baud:
            esp.change_baud(args.baud)
            # TODO: handle a NotImplementedInROMError

        # override common SPI flash parameter stuff as required
        if hasattr(args, "ucIsHspi"):
            print "Attaching SPI flash..."
            esp.flash_spi_attach(args.ucIsHspi,args.ucIsLegacy)
        if hasattr(args, "flash_size"):
            print "Configuring flash size..."
            esp.flash_set_parameters(flash_size_bytes(args.flash_size))

        operation_func(esp, args)
    else:
        operation_func(args)


class FlashSizeAction(argparse.Action):
    """ Custom flash size parser class to support backwards compatibility with megabit size arguments.

    (At next major relase, remove deprecated sizes and this can become a 'normal' choices= argument again.)
    """
    def __init__(self, option_strings, dest, nargs=1, **kwargs):
        super(FlashSizeAction, self).__init__(option_strings, dest, nargs, **kwargs)

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
        print '\nA fatal error occurred: %s' % e
        sys.exit(2)
