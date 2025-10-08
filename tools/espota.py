#!/usr/bin/env python
#
# Original espota.py by Ivan Grokhotkov:
# https://gist.github.com/igrr/d35ab8446922179dc58c
#
# Modified since 2015-09-18 from Pascal Gollor (https://github.com/pgollor)
# Modified since 2015-11-09 from Hristo Gochkov (https://github.com/me-no-dev)
# Modified since 2016-01-03 from Matthew O'Gorman (https://githumb.com/mogorman)
# Modified since 2025-09-04 from Lucas Saavedra Vaz (https://github.com/lucasssvaz)
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
#
# Changes
# 2025-09-04:
# - Changed authentication to use PBKDF2-HMAC-SHA256 for challenge/response
#
# Changes
# 2025-09-18:
# - Fixed authentication when using old images with MD5 passwords
#
# Changes
# 2025-10-07:
# - Fixed authentication when images might use old MD5 hashes stored in the firmware


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


def send_invitation_and_get_auth_challenge(remote_addr, remote_port, message):
    """
    Send invitation to ESP device and get authentication challenge.
    Returns (success, auth_data, error_message) tuple.
    """
    remote_address = (remote_addr, int(remote_port))
    inv_tries = 0
    data = ""

    msg = "Sending invitation to %s " % remote_addr
    sys.stderr.write(msg)
    sys.stderr.flush()

    while inv_tries < 10:
        inv_tries += 1
        sock2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sent = sock2.sendto(message.encode(), remote_address)  # noqa: F841
        except:  # noqa: E722
            sys.stderr.write("failed\n")
            sys.stderr.flush()
            sock2.close()
            return False, None, "Host %s Not Found" % remote_addr

        sock2.settimeout(TIMEOUT)
        try:
            # Try to read up to 69 bytes for new protocol (SHA256)
            # If device sends less (37 bytes), it's using old MD5 protocol
            data = sock2.recv(69).decode()
            sock2.close()
            break
        except:  # noqa: E722
            sys.stderr.write(".")
            sys.stderr.flush()
            sock2.close()

    sys.stderr.write("\n")
    sys.stderr.flush()

    if inv_tries == 10:
        return False, None, "No response from the ESP"

    return True, data, None


def authenticate(
    remote_addr, remote_port, password, use_md5_password, use_old_protocol, filename, content_size, file_md5, nonce
):
    """
    Perform authentication with the ESP device.

    Args:
        use_md5_password: If True, hash password with MD5 instead of SHA256
        use_old_protocol: If True, use old MD5 challenge/response protocol (pre-3.3.1)

    Returns (success, error_message) tuple.
    """
    cnonce_text = "%s%u%s%s" % (filename, content_size, file_md5, remote_addr)
    remote_address = (remote_addr, int(remote_port))

    if use_old_protocol:
        # Generate client nonce (cnonce)
        cnonce = hashlib.md5(cnonce_text.encode()).hexdigest()

        # Old MD5 challenge/response protocol (pre-3.3.1)
        # 1. Hash the password with MD5
        password_hash = hashlib.md5(password.encode()).hexdigest()

        # 2. Create challenge response
        challenge = "%s:%s:%s" % (password_hash, nonce, cnonce)
        response = hashlib.md5(challenge.encode()).hexdigest()
        expected_response_length = 32
    else:
        # Generate client nonce (cnonce) using SHA256 for new protocol
        cnonce = hashlib.sha256(cnonce_text.encode()).hexdigest()

        # New PBKDF2-HMAC-SHA256 challenge/response protocol (3.3.1+)
        # The password can be hashed with either MD5 or SHA256
        if use_md5_password:
            # Use MD5 for password hash (for devices that stored MD5 hashes)
            logging.warning(
                "Using insecure MD5 hash for password due to legacy device support. "
                "Please upgrade devices to ESP32 Arduino Core 3.3.1+ for improved security."
            )
            password_hash = hashlib.md5(password.encode()).hexdigest()
        else:
            # Use SHA256 for password hash (recommended)
            password_hash = hashlib.sha256(password.encode()).hexdigest()

        # 2. Derive key using PBKDF2-HMAC-SHA256 with the password hash
        salt = nonce + ":" + cnonce
        derived_key = hashlib.pbkdf2_hmac("sha256", password_hash.encode(), salt.encode(), 10000)
        derived_key_hex = derived_key.hex()

        # 3. Create challenge response
        challenge = derived_key_hex + ":" + nonce + ":" + cnonce
        response = hashlib.sha256(challenge.encode()).hexdigest()
        expected_response_length = 64

    # Send authentication response
    sock2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        message = "%d %s %s\n" % (AUTH, cnonce, response)
        sock2.sendto(message.encode(), remote_address)
        sock2.settimeout(10)
        try:
            data = sock2.recv(expected_response_length).decode()
        except:  # noqa: E722
            sock2.close()
            return False, "No Answer to our Authentication"

        if data != "OK":
            sock2.close()
            return False, data

        sock2.close()
        return True, None
    except Exception as e:
        sock2.close()
        return False, str(e)


def serve(  # noqa: C901
    remote_addr, local_addr, remote_port, local_port, password, md5_target, filename, command=FLASH
):
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
    with open(filename, "rb") as f:
        file_md5 = hashlib.md5(f.read()).hexdigest()
    logging.info("Upload size: %d", content_size)
    message = "%d %d %d %s\n" % (command, local_port, content_size, file_md5)

    # Send invitation and get authentication challenge
    success, data, error = send_invitation_and_get_auth_challenge(remote_addr, remote_port, message)
    if not success:
        logging.error(error)
        return 1

    if data != "OK":
        if data.startswith("AUTH"):
            nonce = data.split()[1]
            nonce_length = len(nonce)

            # Detect protocol version based on nonce length:
            # - 32 chars = Old MD5 protocol (pre-3.3.1)
            # - 64 chars = New SHA256 protocol (3.3.1+)

            if nonce_length == 32:
                # Scenario 1: Old device (pre-3.3.1) using MD5 protocol
                logging.info("Detected old MD5 protocol (pre-3.3.1)")
                sys.stderr.write("Authenticating (MD5 protocol)...")
                sys.stderr.flush()
                auth_success, auth_error = authenticate(
                    remote_addr,
                    remote_port,
                    password,
                    use_md5_password=True,
                    use_old_protocol=True,
                    filename=filename,
                    content_size=content_size,
                    file_md5=file_md5,
                    nonce=nonce,
                )

                if not auth_success:
                    sys.stderr.write("FAIL\n")
                    logging.error("Authentication Failed: %s", auth_error)
                    return 1

                sys.stderr.write("OK\n")
                logging.warning("====================================================================")
                logging.warning("WARNING: Device is using old MD5 authentication protocol (pre-3.3.1)")
                logging.warning("Please update to ESP32 Arduino Core 3.3.1+ for improved security.")
                logging.warning("======================================================================")

            elif nonce_length == 64:
                # New protocol (3.3.1+) - try SHA256 password first, then MD5 if it fails

                # Scenario 2: Try SHA256 password hash first (recommended for new devices)
                if md5_target:
                    # User explicitly requested MD5 password hash
                    logging.info("Using MD5 password hash as requested")
                    sys.stderr.write("Authenticating (SHA256 protocol with MD5 password)...")
                    sys.stderr.flush()
                    auth_success, auth_error = authenticate(
                        remote_addr,
                        remote_port,
                        password,
                        use_md5_password=True,
                        use_old_protocol=False,
                        filename=filename,
                        content_size=content_size,
                        file_md5=file_md5,
                        nonce=nonce,
                    )
                else:
                    # Try SHA256 password hash first
                    sys.stderr.write("Authenticating...")
                    sys.stderr.flush()
                    auth_success, auth_error = authenticate(
                        remote_addr,
                        remote_port,
                        password,
                        use_md5_password=False,
                        use_old_protocol=False,
                        filename=filename,
                        content_size=content_size,
                        file_md5=file_md5,
                        nonce=nonce,
                    )

                    # Scenario 3: If SHA256 fails, try MD5 password hash (for devices with stored MD5 passwords)
                    if not auth_success:
                        logging.info("SHA256 password failed, trying MD5 password hash")
                        sys.stderr.write("Retrying with MD5 password...")
                        sys.stderr.flush()

                        # Device is back in OTA_IDLE after auth failure, need to send new invitation
                        success, data, error = send_invitation_and_get_auth_challenge(remote_addr, remote_port, message)
                        if not success:
                            sys.stderr.write("FAIL\n")
                            logging.error("Failed to get new challenge for MD5 retry: %s", error)
                            return 1

                        if not data.startswith("AUTH"):
                            sys.stderr.write("FAIL\n")
                            logging.error("Expected AUTH challenge for MD5 retry, got: %s", data)
                            return 1

                        # Get new nonce for second attempt
                        nonce = data.split()[1]

                        auth_success, auth_error = authenticate(
                            remote_addr,
                            remote_port,
                            password,
                            use_md5_password=True,
                            use_old_protocol=False,
                            filename=filename,
                            content_size=content_size,
                            file_md5=file_md5,
                            nonce=nonce,
                        )

                        if auth_success:
                            logging.warning("====================================================================")
                            logging.warning("WARNING: Device authenticated with MD5 password hash (deprecated)")
                            logging.warning("MD5 is cryptographically broken and should not be used.")
                            logging.warning(
                                "Please update your sketch to use either setPassword() or setPasswordHash()"
                            )
                            logging.warning(
                                "with SHA256, then upload again to migrate to the new secure SHA256 protocol."
                            )
                            logging.warning("======================================================================")

                if not auth_success:
                    sys.stderr.write("FAIL\n")
                    logging.error("Authentication Failed: %s", auth_error)
                    return 1

                sys.stderr.write("OK\n")
            else:
                logging.error("Invalid nonce length: %d (expected 32 or 64)", nonce_length)
                return 1
        else:
            logging.error("Bad Answer: %s", data)
            return 1

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
                    response_text = res.decode().strip()
                    last_response_contained_ok = "OK" in response_text
                    logging.debug("Chunk response: '%s'", response_text)
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
            received_any_response = False
            while count < 10:  # Increased from 5 to 10 attempts
                count += 1
                connection.settimeout(30)  # Reduced from 60s to 30s per attempt
                try:
                    data = connection.recv(32).decode().strip()
                    received_any_response = True
                    logging.info("Result attempt %d: '%s'", count, data)

                    if "OK" in data:
                        logging.info("Success")
                        connection.close()
                        return 0
                    elif data:  # Got some response but not OK
                        logging.warning("Unexpected response from device: '%s'", data)

                except socket.timeout:
                    logging.debug("Timeout waiting for result (attempt %d/10)", count)
                    continue
                except Exception as e:
                    logging.debug("Error receiving result (attempt %d/10): %s", count, str(e))
                    # Don't return error here, continue trying
                    continue

            # After all attempts, provide detailed error information
            if received_any_response:
                logging.warning(
                    "Upload completed but device sent unexpected response(s). This may still be successful."
                )
                logging.warning("Device might be rebooting to apply firmware - this is normal.")
                connection.close()
                return 0  # Consider it successful if we got any response and upload completed
            else:
                logging.error("No response from device after upload completion")
                logging.error("This could indicate device reboot (normal) or network issues")
                connection.close()
                return 1
    except Exception as e:  # noqa: E722
        logging.error("Error: %s", str(e))
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
    parser.add_argument(
        "-m",
        "--md5-target",
        dest="md5_target",
        help=(
            "Use MD5 for password hashing (for devices with stored MD5 passwords). "
            "By default, SHA256 is tried first, then MD5 as fallback."
        ),
        action="store_true",
        default=False,
    )

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
        options.esp_ip,
        options.host_ip,
        options.esp_port,
        options.host_port,
        options.auth,
        options.md5_target,
        options.image,
        command,
    )


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
