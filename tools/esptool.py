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
            raise NotImplementedInROMError(obj, func)
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

    """ Start downloading to Flash (performs an erase)

    Returns number of blocks (of size self.FLASH_WRITE_SIZE) to write.
    """
    def flash_begin(self, size, offset):
        old_tmo = self._port.timeout
        num_blocks = (size + self.FLASH_WRITE_SIZE - 1) / self.FLASH_WRITE_SIZE
        erase_size = self.get_erase_size(offset, size)

        self._port.timeout = 20
        t = time.time()
        self.check_command("enter Flash download mode", self.ESP_FLASH_BEGIN,
                           struct.pack('<IIII', erase_size, num_blocks, self.FLASH_WRITE_SIZE, offset))
        if size != 0 and not self.IS_STUB:
            print "Took %.2fs to erase flash block" % (time.time() - t)
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
        """ Start downloading compressed data to Flash (performs an erase)

        Returns number of blocks (size self.FLASH_WRITE_SIZE) to write.
        """
        old_tmo = self._port.timeout
        num_blocks = (compsize + self.FLASH_WRITE_SIZE - 1) / self.FLASH_WRITE_SIZE
        erase_blocks = (size + self.FLASH_WRITE_SIZE - 1) / self.FLASH_WRITE_SIZE

        self._port.timeout = 20
        t = time.time()
        print "Compressed %d bytes to %d..." % (size, compsize)
        self.check_command("enter compressed flash mode", self.ESP_FLASH_DEFL_BEGIN,
                           struct.pack('<IIII', erase_blocks * self.FLASH_WRITE_SIZE, num_blocks, self.FLASH_WRITE_SIZE, offset))
        if size != 0 and not self.IS_STUB:
            # (stub erases as it writes, but ROM loaders erase on begin)
            print "Took %.2fs to erase flash block" % (time.time() - t)
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
        sector_size = self.FLASH_SECTOR_SIZE
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
                                  irom_offs & ~(ESPLoader.FLASH_SECTOR_SIZE - 1))

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
        self.encrypt_flag = False

        if load_file is not None:
            segments = self.load_common_header(load_file, ESPLoader.ESP_IMAGE_MAGIC)
            additional_header = list(struct.unpack("B" * 16, load_file.read(16)))
            self.encrypt_flag = (additional_header[0] == 0x01)

            # check remaining 14 bytes are unused
            if additional_header[2:] != [0] * 14:
                print "WARNING: ESP32 image header contains unknown flags. Possibly this image is from a newer version of esptool.py"

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
    def __init__(self, bootloader, func):
        FatalError.__init__(self, "%s ROM does not support function %s." % (bootloader.CHIP_NAME, func.func_name))

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
        if args.no_stub:
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
            blocks = esp.flash_defl_begin(uncsize, len(image), address)
        else:
            blocks = esp.flash_begin(uncsize, address)
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
        try:
            res = esp.flash_md5sum(address, uncsize)
            if res != calcmd5:
                print 'File  md5: %s' % calcmd5
                print 'Flash md5: %s' % res
                print 'MD5 of 0xFF is %s' % (hashlib.md5(b'\xFF' * uncsize).hexdigest())
                raise FatalError("MD5 of file does not match data in flash!")
            else:
                print 'Hash of data verified.'
        except NotImplementedInROMError:
            pass
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
        print '%s: %s' % (label, ':'.join(map(lambda x: '%02x' % x, mac)))
    print("%r" % (mac,))
    if len(mac) == 1:
        print_mac("MAC", mac)
    else:
        print_mac("WiFi MAC", mac[0])
        print_mac("BT MAC", mac[1])

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
            try:
                esp.change_baud(args.baud)
            except NotImplementedInROMError:
                print "WARNING: ROM doesn't support changing baud rate. Keeping initial baud rate %d" % initial_baud

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
JlmuEgVGbP3zYv5L1xmpPPrX/wFeThnJ\
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
