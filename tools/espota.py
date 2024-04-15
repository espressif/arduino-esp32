#!/usr/bin/env python
#
# Original espota.py by Ivan Grokhotkov:
# https://gist.github.com/igrr/d35ab8446922179dc58c
#
# Modified since 2015-09-18 from Pascal Gollor (https://github.com/pgollor)
# Modified since 2015-11-09 from Hristo Gochkov (https://github.com/me-no-dev)
# Modified since 2016-01-03 from Matthew O'Gorman (https://githumb.com/mogorman)
#
# This script will push an OTA update to the ESP
# use it like:
# python espota.py -i <ESP_IP_addr> -I <Host_IP_addr> -p <ESP_port> -P <Host_port> [-a password] -f <sketch.bin>
# Or to upload SPIFFS image:
# python espota.py -i <ESP_IP_addr> -I <Host_IP_addr> -p <ESP_port> -P <HOST_port> [-a password] -s -f <spiffs.bin>
#
# Changes
# 2015-09-18:
# - Add option parser.
# - Add logging.
# - Send command to controller to differ between flashing and transmitting SPIFFS image.
#
# Changes
# 2015-11-09:
# - Added digest authentication
# - Enhanced error tracking and reporting
#
# Changes
# 2016-01-03:
# - Added more options to parser.
#
# Changes
# 2023-05-22:
# - Replaced the deprecated optparse module with argparse.
# - Adjusted the code style to conform to PEP 8 guidelines.
# - Used with statement for file handling to ensure proper resource cleanup.
# - Incorporated exception handling to catch and handle potential errors.
# - Made variable names more descriptive for better readability.
# - Introduced constants for better code maintainability.

from __future__ import print_function
import socket
import sys
import os
import argparse
import logging
import hashlib
import random

# Commands
FLASH = 0
SPIFFS = 100
AUTH = 200

# Constants
PROGRESS_BAR_LENGTH = 60


# update_progress(): Displays or updates a console progress bar
def update_progress(progress):
    if PROGRESS:
        status = ""
        if isinstance(progress, int):
            progress = float(progress)
        if not isinstance(progress, float):
            progress = 0
            status = "Error: progress var must be float\r\n"
        if progress < 0:
            progress = 0
            status = "Halt...\r\n"
        if progress >= 1:
            progress = 1
            status = "Done...\r\n"
        block = int(round(PROGRESS_BAR_LENGTH * progress))
        text = "\rUploading: [{0}] {1}% {2}".format(
            "=" * block + " " * (PROGRESS_BAR_LENGTH - block), int(progress * 100), status
        )
        sys.stderr.write(text)
        sys.stderr.flush()
    else:
        sys.stderr.write(".")
        sys.stderr.flush()


def serve(remote_addr, local_addr, remote_port, local_port, password, filename, command=FLASH):  # noqa: C901
    # Create a TCP/IP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_address = (local_addr, local_port)
    logging.info("Starting on %s:%s", str(server_address[0]), str(server_address[1]))
    try:
        sock.bind(server_address)
        sock.listen(1)
    except Exception as e:
        logging.error("Listen Failed: %s", str(e))
        return 1

    content_size = os.path.getsize(filename)
    file_md5 = hashlib.md5(open(filename, "rb").read()).hexdigest()
    logging.info("Upload size: %d", content_size)
    message = "%d %d %d %s\n" % (command, local_port, content_size, file_md5)

    # Wait for a connection
    inv_tries = 0
    data = ""
    msg = "Sending invitation to %s " % remote_addr
    sys.stderr.write(msg)
    sys.stderr.flush()
    while inv_tries < 10:
        inv_tries += 1
        sock2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        remote_address = (remote_addr, int(remote_port))
        try:
            sent = sock2.sendto(message.encode(), remote_address)  # noqa: F841
        except:  # noqa: E722
            sys.stderr.write("failed\n")
            sys.stderr.flush()
            sock2.close()
            logging.error("Host %s Not Found", remote_addr)
            return 1
        sock2.settimeout(TIMEOUT)
        try:
            data = sock2.recv(37).decode()
            break
        except:  # noqa: E722
            sys.stderr.write(".")
            sys.stderr.flush()
            sock2.close()
    sys.stderr.write("\n")
    sys.stderr.flush()
    if inv_tries == 10:
        logging.error("No response from the ESP")
        return 1
    if data != "OK":
        if data.startswith("AUTH"):
            nonce = data.split()[1]
            cnonce_text = "%s%u%s%s" % (filename, content_size, file_md5, remote_addr)
            cnonce = hashlib.md5(cnonce_text.encode()).hexdigest()
            passmd5 = hashlib.md5(password.encode()).hexdigest()
            result_text = "%s:%s:%s" % (passmd5, nonce, cnonce)
            result = hashlib.md5(result_text.encode()).hexdigest()
            sys.stderr.write("Authenticating...")
            sys.stderr.flush()
            message = "%d %s %s\n" % (AUTH, cnonce, result)
            sock2.sendto(message.encode(), remote_address)
            sock2.settimeout(10)
            try:
                data = sock2.recv(32).decode()
            except:  # noqa: E722
                sys.stderr.write("FAIL\n")
                logging.error("No Answer to our Authentication")
                sock2.close()
                return 1
            if data != "OK":
                sys.stderr.write("FAIL\n")
                logging.error("%s", data)
                sock2.close()
                sys.exit(1)
                return 1
            sys.stderr.write("OK\n")
        else:
            logging.error("Bad Answer: %s", data)
            sock2.close()
            return 1
    sock2.close()

    logging.info("Waiting for device...")
    try:
        sock.settimeout(10)
        connection, client_address = sock.accept()
        sock.settimeout(None)
        connection.settimeout(None)
    except:  # noqa: E722
        logging.error("No response from device")
        sock.close()
        return 1
    try:
        with open(filename, "rb") as f:
            if PROGRESS:
                update_progress(0)
            else:
                sys.stderr.write("Uploading")
                sys.stderr.flush()
            offset = 0
            while True:
                chunk = f.read(1024)
                if not chunk:
                    break
                offset += len(chunk)
                update_progress(offset / float(content_size))
                connection.settimeout(10)
                try:
                    connection.sendall(chunk)
                    res = connection.recv(10)
                    last_response_contained_ok = "OK" in res.decode()
                except Exception as e:
                    sys.stderr.write("\n")
                    logging.error("Error Uploading: %s", str(e))
                    connection.close()
                    return 1

            if last_response_contained_ok:
                logging.info("Success")
                connection.close()
                return 0

            sys.stderr.write("\n")
            logging.info("Waiting for result...")
            count = 0
            while count < 5:
                count += 1
                connection.settimeout(60)
                try:
                    data = connection.recv(32).decode()
                    logging.info("Result: %s", data)

                    if "OK" in data:
                        logging.info("Success")
                        connection.close()
                        return 0

                except Exception as e:
                    logging.error("Error receiving result: %s", str(e))
                    connection.close()
                    return 1

            logging.error("Error response from device")
            connection.close()
            return 1

    finally:
        connection.close()

    sock.close()
    return 1


def parse_args(unparsed_args):
    parser = argparse.ArgumentParser(description="Transmit image over the air to the ESP32 module with OTA support.")

    # destination ip and port
    parser.add_argument("-i", "--ip", dest="esp_ip", action="store", help="ESP32 IP Address.", default=False)
    parser.add_argument("-I", "--host_ip", dest="host_ip", action="store", help="Host IP Address.", default="0.0.0.0")
    parser.add_argument("-p", "--port", dest="esp_port", type=int, help="ESP32 OTA Port. Default: 3232", default=3232)
    parser.add_argument(
        "-P",
        "--host_port",
        dest="host_port",
        type=int,
        help="Host server OTA Port. Default: random 10000-60000",
        default=random.randint(10000, 60000),
    )

    # authentication
    parser.add_argument("-a", "--auth", dest="auth", help="Set authentication password.", action="store", default="")

    # image
    parser.add_argument("-f", "--file", dest="image", help="Image file.", metavar="FILE", default=None)
    parser.add_argument(
        "-s",
        "--spiffs",
        dest="spiffs",
        action="store_true",
        help="Transmit a SPIFFS image and do not flash the module.",
        default=False,
    )

    # output
    parser.add_argument(
        "-d",
        "--debug",
        dest="debug",
        action="store_true",
        help="Show debug output. Overrides loglevel with debug.",
        default=False,
    )
    parser.add_argument(
        "-r",
        "--progress",
        dest="progress",
        action="store_true",
        help="Show progress output. Does not work for Arduino IDE.",
        default=False,
    )
    parser.add_argument(
        "-t",
        "--timeout",
        dest="timeout",
        type=int,
        help="Timeout to wait for the ESP32 to accept invitation.",
        default=10,
    )

    return parser.parse_args(unparsed_args)


def main(args):
    options = parse_args(args)
    log_level = logging.WARNING
    if options.debug:
        log_level = logging.DEBUG

    logging.basicConfig(level=log_level, format="%(asctime)-8s [%(levelname)s]: %(message)s", datefmt="%H:%M:%S")
    logging.debug("Options: %s", str(options))

    # check options
    global PROGRESS
    PROGRESS = options.progress

    global TIMEOUT
    TIMEOUT = options.timeout

    if not options.esp_ip or not options.image:
        logging.critical("Not enough arguments.")
        return 1

    command = FLASH
    if options.spiffs:
        command = SPIFFS

    return serve(
        options.esp_ip, options.host_ip, options.esp_port, options.host_port, options.auth, options.image, command
    )


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
