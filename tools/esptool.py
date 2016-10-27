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

    # Memory addresses
    IROM_MAP_START = 0x40200000
    IROM_MAP_END = 0x40300000

    # The number of bytes in the UART response that signify command status
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

        for _ in xrange(10):
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

            self._port.timeout = 0.1
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
    def flash_id(self):
        SPIFLASH_RDID = 0x9F
        return self.run_spiflash_command(SPIFLASH_RDID, b"", 24)

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
        print "Compressed %d bytes to %d..." % (size, compsize)
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
            words = struct.unpack("I" * (len(data) / 4), data)
            next_reg = SPI_W0_REG
            for word in words:
                self.write_reg(next_reg, word)
                next_reg += 4
        self.write_reg(SPI_CMD_REG, SPI_CMD_USR)

        def wait_done():
            for _ in xrange(10):
                if (self.read_reg(SPI_CMD_REG) & SPI_CMD_USR) == 0:
                    return
            raise FatalError("SPI command did not complete in time")
        wait_done()

        status = self.read_reg(SPI_W0_REG)
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
    FLASH_WRITE_SIZE = 0x4000  # matches MAX_WRITE_BLOCK in stub_loader.c
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

    def read_efuse(self, n):
        """ Read the nth word of the ESP3x EFUSE region. """
        return self.read_reg(self.EFUSE_REG_BASE + (4 * n))

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
            data += "\x00" * (4 - pad_mod)
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
        f.write(struct.pack('B', checksum))

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
                print 'Warning: V2 header has unexpected "segment" count %d (usually 4)' % segments

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
            for _ in xrange(segments):
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

    ROM_LOADER = ESP32ROM

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

    def warn_if_unusual_segment(self, offset, size, is_irom_segment):
        pass  # TODO: add warnings for ESP32 segment offset/size combinations that are wrong

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
            raise FatalError(("File %s (length %d) at offset %d will not fit in %d bytes of flash. " +
                             "Use --flash-size argument, or change flashing address.")
                             % (argfile.name, argfile.tell(), address, flash_end))
        argfile.seek(0)

    for address, argfile in args.addr_filename:
        print 'Erasing flash...'
        image = argfile.read()
        # Update header with flash parameters
        if address == 0 and image[0] == '\xe9':
            image = image[0:2] + flash_info + image[4:]
        calcmd5 = hashlib.md5(image).hexdigest()
        uncsize = len(image)
        if args.compress:
            uncimage = image
            image = zlib.compress(uncimage, 9)
            blocks = div_roundup(len(image), esp.FLASH_WRITE_SIZE)
            esp.flash_defl_begin(len(uncimage),len(image), address)
        else:
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
                esp.flash_block(block, seq)
            image = image[esp.FLASH_WRITE_SIZE:]
            seq += 1
            written += len(block)
        t = time.time() - t
        speed_msg = ""
        if args.compress:
            if t > 0.0:
                speed_msg = " (effective %.1f kbit/s)" % (uncsize / t * 8 / 1000)
            print '\rWrote %d bytes (%d compressed) at 0x%08x in %.1f seconds%s...' % (uncsize, written, address, t, speed_msg)
        else:
            if t > 0.0:
                speed_msg = " (%.1f kbit/s)" % (written / t * 8 / 1000)
            print '\rWrote %d bytes at 0x%08x in %.1f seconds%s...' % (written, address, t, speed_msg)
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
    idx = 0
    for seg in image.segments:
        idx += 1
        print 'Segment %d: %r' % (idx, seg)
    calc_checksum = image.calculate_checksum()
    print 'Checksum: %02x (%s)' % (image.checksum,
                                   'valid' if image.checksum == calc_checksum else 'invalid - calculated %02x' % calc_checksum)


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
    image.flash_size_freq = image.ROM_LOADER.FLASH_SIZES[args.flash_size]
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
    t = time.time()
    esp.erase_region(args.address, args.size)
    print 'Erase completed successfully in %.1f seconds.' % (time.time() - t)


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


def read_flash_status(esp, args):
    print ('Status value: 0x%04x' % esp.read_status(args.bytes))


def write_flash_status(esp, args):
    fmt = "0x%%0%dx" % (args.bytes * 2)
    args.value = args.value & ((1 << (args.bytes * 8)) - 1)
    print (('Initial flash status: ' + fmt) % esp.read_status(args.bytes))
    print (('Setting flash status: ' + fmt) % args.value)
    esp.write_status(args.value, args.bytes, args.non_volatile)
    print (('After flash status:   ' + fmt) % esp.read_status(args.bytes))


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
        else:
            esp.flash_spi_attach(0, 0)
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
