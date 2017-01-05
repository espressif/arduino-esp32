#!/usr/bin/env python
# NB: Before sending a PR to change the above line to '#!/usr/bin/env python2', please read https://github.com/espressif/esptool/issues/21
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
        sys.stdout.write('Detecting chip type... ')
        date_reg = detect_port.read_reg(ESPLoader.UART_DATA_REG_ADDR)

        for cls in [ESP8266ROM, ESP32ROM]:
            if date_reg == cls.DATE_REG_VALUE:
                # don't connect a second time
                inst = cls(detect_port._port, baud)
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
    def command(self, op=None, data="", chk=0, wait_response=True):
        if op is not None:
            pkt = struct.pack('<BBHI', 0x00, op, len(data), chk) + data
            self.write(pkt)

        if not wait_response:
            return

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

    def connect(self, mode='default_reset'):
        """ Try connecting repeatedly until successful, or giving up """
        print 'Connecting...'

        for _ in xrange(10):
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
        return tuple(ord(b) for b in bitstring[:6])  # trim 2 byte CRC

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


def detect_flash_size(esp, args):
    if args.flash_size == 'detect':
        flash_id = esp.flash_id()
        size_id = flash_id >> 16
        args.flash_size = {0x12: '256KB', 0x13: '512KB', 0x14: '1MB', 0x15: '2MB', 0x16: '4MB', 0x17: '8MB', 0x18: '16MB'}.get(size_id)
        if args.flash_size is None:
            print 'Warning: Could not auto-detect Flash size (FlashID=0x%x, SizeID=0x%x), defaulting to 4m' % (flash_id, size_id)
            args.flash_size = '4m'
        else:
            print 'Auto-detected Flash size:', args.flash_size


def write_flash(esp, args):
    """Write data to flash
    """
    detect_flash_size(esp, args)

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
        if address == esp.FLASH_HEADER_OFFSET and image[0] == '\xe9':
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

    if esp.IS_STUB:
        # skip sending flash_finish to ROM loader here,
        # as it causes the loader to exit and run user code
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
    print_mac("MAC", mac)


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

    expand_file_arguments()

    args = parser.parse_args()

    print 'esptool.py v%s' % __version__

    # operation function can take 1 arg (args), 2 args (esp, arg)
    # or be a member function of the ESPLoader class.

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
                print "WARNING: ROM doesn't support changing baud rate. Keeping initial baud rate %d" % initial_baud

        # override common SPI flash parameter stuff as required
        if hasattr(args, "ucIsHspi"):
            print "Attaching SPI flash..."
            esp.flash_spi_attach(args.ucIsHspi,args.ucIsLegacy)
        else:
            esp.flash_spi_attach(0, 0)
        if hasattr(args, "flash_size"):
            print "Configuring flash size..."
            detect_flash_size(esp, args)
            esp.flash_set_parameters(flash_size_bytes(args.flash_size))

        operation_func(esp, args)

        # finish execution based on args.after
        if args.after == 'hard_reset':
            print 'Hard resetting...'
            esp.hard_reset()
        elif args.after == 'soft_reset':
            print 'Soft resetting...'
            # flash_finish will trigger a soft reset
            esp.soft_reset(False)
        else:
            print 'Staying in bootloader.'
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
        print "esptool.py %s" % (" ".join(new_args[1:]))
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
        print '\nA fatal error occurred: %s' % e
        sys.exit(2)
