#!/usr/bin/env python

"""Script to download and extract tools

This script will download and extract required tools into the current directory.
Tools list is obtained from package/package_esp32_index.template.json file.
"""

from __future__ import print_function

__author__ = "Ivan Grokhotkov"
__version__ = "2015"

import os
import shutil
import errno
import os.path
import hashlib
import json
import platform
import sys
import tarfile
import zipfile
import re
import time
import argparse

# Initialize start_time globally
start_time = -1

if sys.version_info[0] == 3:
    from urllib.request import urlretrieve
    from urllib.request import urlopen

    unicode = lambda s: str(s)  # noqa: E731
else:
    # Not Python 3 - today, it is most likely to be Python 2
    from urllib import urlretrieve
    from urllib import urlopen

if "Windows" in platform.system():
    import requests

# determine if application is a script file or frozen exe
if getattr(sys, "frozen", False):
    current_dir = os.path.dirname(os.path.realpath(unicode(sys.executable)))
elif __file__:
    current_dir = os.path.dirname(os.path.realpath(unicode(__file__)))

dist_dir = current_dir + "/dist/"


def sha256sum(filename, blocksize=65536):
    hash = hashlib.sha256()
    with open(filename, "rb") as f:
        for block in iter(lambda: f.read(blocksize), b""):
            hash.update(block)
    return hash.hexdigest()


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno != errno.EEXIST or not os.path.isdir(path):
            raise


def format_time(seconds):
    minutes, seconds = divmod(seconds, 60)
    return "{:02}:{:05.2f}".format(int(minutes), seconds)


def report_progress(block_count, block_size, total_size, start_time):
    downloaded_size = block_count * block_size
    time_elapsed = time.time() - start_time
    current_speed = downloaded_size / (time_elapsed)

    if sys.stdout.isatty():
        if total_size > 0:
            percent_complete = min((downloaded_size / total_size) * 100, 100)
            sys.stdout.write(
                f"\rDownloading... {percent_complete:.2f}% - {downloaded_size / 1024 / 1024:.2f} MB downloaded - Elapsed Time: {format_time(time_elapsed)} - Speed: {current_speed / 1024 / 1024:.2f} MB/s"  # noqa: E501
            )
        else:
            sys.stdout.write(
                f"\rDownloading... {downloaded_size / 1024 / 1024:.2f} MB downloaded - Elapsed Time: {format_time(time_elapsed)} - Speed: {current_speed / 1024 / 1024:.2f} MB/s"  # noqa: E501
            )
        sys.stdout.flush()


def print_verification_progress(total_files, i, t1):
    if sys.stdout.isatty():
        sys.stdout.write(f"\rElapsed time {format_time(time.time() - t1)}")
        sys.stdout.flush()


def verify_files(filename, destination, rename_to):
    # Set the path of the extracted directory
    extracted_dir_path = destination
    t1 = time.time()
    if filename.endswith(".zip"):
        try:
            archive = zipfile.ZipFile(filename, "r")
            file_list = archive.namelist()
        except zipfile.BadZipFile:
            if verbose:
                print(f"Verification failed; aborted in {format_time(time.time() - t1)}")
            return False
    elif filename.endswith(".tar.gz"):
        try:
            archive = tarfile.open(filename, "r:gz")
            file_list = archive.getnames()
        except tarfile.ReadError:
            if verbose:
                print(f"Verification failed; aborted in {format_time(time.time() - t1)}")
            return False
    elif filename.endswith(".tar.xz"):
        try:
            archive = tarfile.open(filename, "r:xz")
            file_list = archive.getnames()
        except tarfile.ReadError:
            if verbose:
                print(f"Verification failed; aborted in {format_time(time.time() - t1)}")
            return False
    else:
        raise NotImplementedError("Unsupported archive type")

    try:
        first_dir = file_list[0].split("/")[0]
        total_files = len(file_list)
        for i, zipped_file in enumerate(file_list, 1):
            local_path = os.path.join(extracted_dir_path, zipped_file.replace(first_dir, rename_to, 1))
            if not os.path.exists(local_path):
                if verbose:
                    print(f"\nMissing {zipped_file} on location: {extracted_dir_path}")
                    print(f"Verification failed; aborted in {format_time(time.time() - t1)}")
                return False
            print_verification_progress(total_files, i, t1)
    except Exception as e:
        print(f"\nError: {e}")
        return False

    if verbose:
        print(f"\nVerification passed; completed in {format_time(time.time() - t1)}")

    return True


def is_latest_version(destination, dirname, rename_to, cfile, checksum):
    current_version = None
    expected_version = None

    try:
        expected_version = checksum
        with open(os.path.join(destination, rename_to, ".package_checksum"), "r") as f:
            current_version = f.read()

        if verbose:
            print(f"\nTool: {rename_to}")
            print(f"Current version: {current_version}")
            print(f"Expected version: {expected_version}")

        if current_version and current_version == expected_version:
            if verbose:
                print("Latest version already installed. Skipping extraction")
            return True

        if verbose:
            print("New version detected")

    except Exception as e:
        if verbose:
            print(f"Failed to verify version for {rename_to}: {e}")

    return False


def unpack(filename, destination, force_extract, checksum):  # noqa: C901
    dirname = ""
    cfile = None  # Compressed file
    file_is_corrupted = False
    if not force_extract:
        print(" > Verify archive... ", end="", flush=True)

    try:
        if filename.endswith("tar.gz"):
            if tarfile.is_tarfile(filename):
                cfile = tarfile.open(filename, "r:gz")
                dirname = cfile.getnames()[0].split("/")[0]
            else:
                print("File corrupted!")
                file_is_corrupted = True
        elif filename.endswith("tar.xz"):
            if tarfile.is_tarfile(filename):
                cfile = tarfile.open(filename, "r:xz")
                dirname = cfile.getnames()[0].split("/")[0]
            else:
                print("File corrupted!")
                file_is_corrupted = True
        elif filename.endswith("zip"):
            if zipfile.is_zipfile(filename):
                cfile = zipfile.ZipFile(filename)
                dirname = cfile.namelist()[0].split("/")[0]
            else:
                print("File corrupted!")
                file_is_corrupted = True
        else:
            raise NotImplementedError("Unsupported archive type")
    except EOFError:
        print("File corrupted or incomplete!")
        cfile = None
        file_is_corrupted = True

    if file_is_corrupted:
        corrupted_filename = filename + ".corrupted"
        os.rename(filename, corrupted_filename)
        if verbose:
            print(f"Renaming corrupted archive to {corrupted_filename}")
        return False

    # A little trick to rename tool directories so they don't contain version number
    rename_to = re.match(r"^([a-z][^\-]*\-*)+", dirname).group(0).strip("-")
    if rename_to == dirname and dirname.startswith("esp32-arduino-libs-"):
        rename_to = "esp32-arduino-libs"

    if not force_extract:
        if is_latest_version(destination, dirname, rename_to, cfile, checksum):
            if verify_files(filename, destination, rename_to):
                print(" Files ok. Skipping Extraction")
                return True
        print(" Extracting archive...")
    else:
        print(" Forcing extraction")

    if os.path.isdir(os.path.join(destination, rename_to)):
        print("Removing existing {0} ...".format(rename_to))
        shutil.rmtree(os.path.join(destination, rename_to), ignore_errors=True)

    if filename.endswith("tar.gz"):
        if not cfile:
            cfile = tarfile.open(filename, "r:gz")
        cfile.extractall(destination)
    elif filename.endswith("tar.xz"):
        if not cfile:
            cfile = tarfile.open(filename, "r:xz")
        cfile.extractall(destination)
    elif filename.endswith("zip"):
        if not cfile:
            cfile = zipfile.ZipFile(filename)
        cfile.extractall(destination)
    else:
        raise NotImplementedError("Unsupported archive type")

    if rename_to != dirname:
        print("Renaming {0} to {1} ...".format(dirname, rename_to))
        shutil.move(dirname, rename_to)

    with open(os.path.join(destination, rename_to, ".package_checksum"), "w") as f:
        f.write(checksum)

    if verify_files(filename, destination, rename_to):
        print(" Files extracted successfully.")
        return True
    else:
        print(" Failed to extract files.")
        return False


def download_file_with_progress(url, filename, start_time):
    import ssl
    import contextlib

    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    with contextlib.closing(urlopen(url, context=ctx)) as fp:
        total_size = int(fp.getheader("Content-Length", fp.getheader("Content-length", "0")))
        block_count = 0
        block_size = 1024 * 8
        block = fp.read(block_size)
        if block:
            with open(filename, "wb") as out_file:
                out_file.write(block)
                block_count += 1
                report_progress(block_count, block_size, total_size, start_time)
                while True:
                    block = fp.read(block_size)
                    if not block:
                        break
                    out_file.write(block)
                    block_count += 1
                    report_progress(block_count, block_size, total_size, start_time)
        else:
            raise Exception("Non-existing file or connection error")


def download_file(url, filename):
    import ssl
    import contextlib

    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    with contextlib.closing(urlopen(url, context=ctx)) as fp:
        block_size = 1024 * 8
        block = fp.read(block_size)
        if block:
            with open(filename, "wb") as out_file:
                out_file.write(block)
                while True:
                    block = fp.read(block_size)
                    if not block:
                        break
                    out_file.write(block)
        else:
            raise Exception("Non-existing file or connection error")


def get_tool(tool, force_download, force_extract):
    sys_name = platform.system()
    archive_name = tool["archiveFileName"]
    checksum = tool["checksum"][8:]
    local_path = dist_dir + archive_name
    url = tool["url"]
    start_time = time.time()
    print("")
    if not os.path.isfile(local_path) or force_download:
        if verbose:
            print("Downloading '" + archive_name + "' to '" + local_path + "'")
        else:
            print("Downloading '" + archive_name + "' ...")
        sys.stdout.flush()
        if "CYGWIN_NT" in sys_name:
            import ssl

            ctx = ssl.create_default_context()
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            urlretrieve(url, local_path, report_progress, context=ctx)
        elif "Windows" in sys_name:
            r = requests.get(url)
            f = open(local_path, "wb")
            f.write(r.content)
            f.close()
        else:
            is_ci = os.environ.get("GITHUB_WORKSPACE")
            if is_ci:
                download_file(url, local_path)
            else:
                try:
                    urlretrieve(url, local_path, report_progress)
                except:  # noqa: E722
                    download_file_with_progress(url, local_path, start_time)
                sys.stdout.write(" - Done\n")
                sys.stdout.flush()
    else:
        print("Tool {0} already downloaded".format(archive_name))
        sys.stdout.flush()

    if sha256sum(local_path) != checksum:
        print("Checksum mismatch for {0}".format(archive_name))
        return False

    return unpack(local_path, ".", force_extract, checksum)


def load_tools_list(filename, platform):
    tools_info = json.load(open(filename))["packages"][0]["tools"]
    tools_to_download = []
    for t in tools_info:
        tool_platform = [p for p in t["systems"] if p["host"] == platform]
        if len(tool_platform) == 0:
            # Fallback to x86 on Apple ARM
            if platform == "arm64-apple-darwin":
                tool_platform = [p for p in t["systems"] if p["host"] == "x86_64-apple-darwin"]
                if len(tool_platform) == 0:
                    continue
            # Fallback to 32bit on 64bit x86 Windows
            elif platform == "x86_64-mingw32":
                tool_platform = [p for p in t["systems"] if p["host"] == "i686-mingw32"]
                if len(tool_platform) == 0:
                    continue
            else:
                continue
        tools_to_download.append(tool_platform[0])
    return tools_to_download


def identify_platform():
    arduino_platform_names = {
        "Darwin": {32: "i386-apple-darwin", 64: "x86_64-apple-darwin"},
        "DarwinARM": {32: "arm64-apple-darwin", 64: "arm64-apple-darwin"},
        "Linux": {32: "i686-pc-linux-gnu", 64: "x86_64-pc-linux-gnu"},
        "LinuxARM": {32: "arm-linux-gnueabihf", 64: "aarch64-linux-gnu"},
        "Windows": {32: "i686-mingw32", 64: "x86_64-mingw32"},
    }
    bits = 32
    if sys.maxsize > 2**32:
        bits = 64
    sys_name = platform.system()
    sys_platform = platform.platform()
    if "Darwin" in sys_name and (sys_platform.find("arm") > 0 or sys_platform.find("arm64") > 0):
        sys_name = "DarwinARM"
    if "Linux" in sys_name and (sys_platform.find("arm") > 0 or sys_platform.find("aarch64") > 0):
        sys_name = "LinuxARM"
    if "CYGWIN_NT" in sys_name:
        sys_name = "Windows"
    print("System: %s, Bits: %d, Info: %s" % (sys_name, bits, sys_platform))
    return arduino_platform_names[sys_name][bits]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Download and extract tools")

    parser.add_argument("-v", "--verbose", action="store_true", required=False, help="Print verbose output")

    parser.add_argument("-d", "--force_download", action="store_true", required=False, help="Force download of tools")

    parser.add_argument("-e", "--force_extract", action="store_true", required=False, help="Force extraction of tools")

    parser.add_argument(
        "-f", "--force_all", action="store_true", required=False, help="Force download and extraction of tools"
    )

    parser.add_argument("-t", "--test", action="store_true", required=False, help=argparse.SUPPRESS)

    args = parser.parse_args()

    verbose = args.verbose
    force_download = args.force_download
    force_extract = args.force_extract
    force_all = args.force_all
    is_test = args.test

    if is_test and (force_download or force_extract or force_all):
        print("Cannot combine test (-t) and forced execution (-d | -e | -f)")
        parser.print_help(sys.stderr)
        sys.exit(1)

    if is_test:
        print("Test run!")

    if force_all:
        force_download = True
        force_extract = True

    identified_platform = identify_platform()
    print("Platform: {0}".format(identified_platform))
    tools_to_download = load_tools_list(
        current_dir + "/../package/package_esp32_index.template.json", identified_platform
    )
    mkdir_p(dist_dir)

    print("\nDownloading and extracting tools...")

    for tool in tools_to_download:
        if is_test:
            print("Would install: {0}".format(tool["archiveFileName"]))
        else:
            if not get_tool(tool, force_download, force_extract):
                if verbose:
                    print(f"Tool {tool['archiveFileName']} was corrupted. Re-downloading...\n")
                if not get_tool(tool, True, force_extract):
                    print(f"Tool {tool['archiveFileName']} was corrupted, but re-downloading did not help!\n")
                    sys.exit(1)

    print("\nPlatform Tools Installed")
