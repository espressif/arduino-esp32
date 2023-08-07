#!/usr/bin/env python

"""Script to download and extract tools

This script will download and extract required tools into the current directory.
Tools list is obtained from package/package_esp8266com_index.template.json file.
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

if sys.version_info[0] == 3:
    from urllib.request import urlretrieve
    from urllib.request import urlopen
    unicode = lambda s: str(s)
else:
    # Not Python 3 - today, it is most likely to be Python 2
    from urllib import urlretrieve
    from urllib import urlopen

if 'Windows' in platform.system():
    import requests

# determine if application is a script file or frozen exe
if getattr(sys, 'frozen', False):
    current_dir = os.path.dirname(os.path.realpath(unicode(sys.executable)))
elif __file__:
    current_dir = os.path.dirname(os.path.realpath(unicode(__file__)))

#current_dir = os.path.dirname(os.path.realpath(unicode(__file__)))
dist_dir = current_dir + '/dist/'

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

def report_progress(block_count, block_size, total_size):
    downloaded_size = block_count * block_size
    current_speed = downloaded_size / (time.time() - start_time)
    time_elapsed = time.time() - start_time

    if sys.stdout.isatty():
        if total_size > 0:
            percent_complete = min((downloaded_size / total_size) * 100, 100)
            sys.stdout.write(f"\rDownloading... {percent_complete:.2f}% - {downloaded_size / 1024 / 1024:.2f} MB downloaded - Elapsed Time: {time_elapsed:.2f} seconds - Speed: {current_speed / 1024 / 1024:.2f} MB/s")
        sys.stdout.flush()

def format_time(seconds):
    minutes, seconds = divmod(seconds, 60)
    return "{:02}:{:05.2f}".format(int(minutes), seconds)

def verify_files(filename, destination, rename_to):
    # Set the path of the extracted directory
    extracted_dir_path = destination

    t1 = time.time()

    if filename.endswith(".zip"):
        try:
            with zipfile.ZipFile(filename, 'r') as archive:
                first_dir = archive.namelist()[0].split('/')[0]
                total_files = len(archive.namelist())
                for i, zipped_file in enumerate(archive.namelist(), 1):
                    local_path = os.path.join(extracted_dir_path, zipped_file.replace(first_dir, rename_to, 1))
                    if not os.path.exists(local_path):
                        print(f'\nMissing {zipped_file} on location: {extracted_dir_path}')
                        print(f"\nVerification failed; aborted in {format_time(time.time() - t1)}")
                        return False
                    #print(f'\rVerification progress: {i/total_files*100:.2f}%', end='')
                    if sys.stdout.isatty():
                        sys.stdout.write(f'\rVerification progress: {i/total_files*100:.2f}%')
                        sys.stdout.flush()
        except zipfile.BadZipFile:
            print(f"\nVerification failed; aborted in {format_time(time.time() - t1)}")
            return False
    elif filename.endswith(".tar.gz"):
        try:
            with tarfile.open(filename, 'r:gz') as archive:
                first_dir = archive.getnames()[0].split('/')[0]
                total_files = len(archive.getnames())
                for i, zipped_file in enumerate(archive.getnames(), 1):
                    local_path = os.path.join(extracted_dir_path, zipped_file.replace(first_dir, rename_to, 1))
                    if not os.path.exists(local_path):
                        print(f'\nMissing {zipped_file} on location: {extracted_dir_path}')
                        print(f"\nVerification failed; aborted in {format_time(time.time() - t1)}")
                        return False
                    #print(f'\rVerification progress: {i/total_files*100:.2f}%', end='')
                    if sys.stdout.isatty():
                        sys.stdout.write(f'\rVerification progress: {i/total_files*100:.2f}%')
                        sys.stdout.flush()
        except tarfile.ReadError:
            print(f"\nVerification failed; aborted in {format_time(time.time() - t1)}")
            return False
    elif filename.endswith(".tar.xz"):
        try:
            with tarfile.open(filename, 'r:xz') as archive:
                first_dir = archive.getnames()[0].split('/')[0]
                total_files = len(archive.getnames())
                for i, zipped_file in enumerate(archive.getnames(), 1):
                    local_path = os.path.join(extracted_dir_path, zipped_file.replace(first_dir, rename_to, 1))
                    if not os.path.exists(local_path):
                        print(f'\nMissing {zipped_file} on location: {extracted_dir_path}')
                        print(f"\nVerification failed; aborted in {format_time(time.time() - t1)}")
                        return False
                    #print(f'\rVerification progress: {i/total_files*100:.2f}%', end='')
                    if sys.stdout.isatty():
                        sys.stdout.write(f'\rVerification progress: {i/total_files*100:.2f}%')
                        sys.stdout.flush()
        except tarfile.ReadError:
            print(f"\nVerification failed; aborted in {format_time(time.time() - t1)}")
            return False
    else:
        raise NotImplementedError('Unsupported archive type')

    if(verobse):
        print(f"\nVerification passed; completed in {format_time(time.time() - t1)}")

    return True

def unpack(filename, destination):
    dirname = ''
    print(' > Verify... ')

    if filename.endswith('tar.gz'):
        tfile = tarfile.open(filename, 'r:gz')
        dirname = tfile.getnames()[0]
    elif filename.endswith('tar.xz'):
        tfile = tarfile.open(filename, 'r:xz')
        dirname = tfile.getnames()[0]
    elif filename.endswith('zip'):
        zfile = zipfile.ZipFile(filename)
        dirname = zfile.namelist()[0]
    else:
        raise NotImplementedError('Unsupported archive type')

    # A little trick to rename tool directories so they don't contain version number
    rename_to = re.match(r'^([a-z][^\-]*\-*)+', dirname).group(0).strip('-')
    if rename_to == dirname and dirname.startswith('esp32-arduino-libs-'):
        rename_to = 'esp32-arduino-libs'

    if(verify_files(filename, destination, rename_to)):
        print(" Files ok. Skipping Extraction")
        return
    else:
        print(" Failed. extracting")

    if filename.endswith('tar.gz'):
        tfile = tarfile.open(filename, 'r:gz')
        tfile.extractall(destination)
        dirname = tfile.getnames()[0]
    elif filename.endswith('tar.xz'):
        tfile = tarfile.open(filename, 'r:xz')
        tfile.extractall(destination)
        dirname = tfile.getnames()[0]
    elif filename.endswith('zip'):
        zfile = zipfile.ZipFile(filename)
        zfile.extractall(destination)
        dirname = zfile.namelist()[0]
    else:
        raise NotImplementedError('Unsupported archive type')

    if rename_to != dirname:
        print('Renaming {0} to {1} ...'.format(dirname, rename_to))
        if os.path.isdir(rename_to):
            shutil.rmtree(rename_to)
        shutil.move(dirname, rename_to)

def download_file_with_progress(url,filename):
    import ssl
    import contextlib
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    with contextlib.closing(urlopen(url,context=ctx)) as fp:
        total_size = int(fp.getheader("Content-Length",fp.getheader("Content-length","0")))
        block_count = 0
        block_size = 1024 * 8
        block = fp.read(block_size)
        if block:
            with open(filename,'wb') as out_file:
                out_file.write(block)
                block_count += 1
                report_progress(block_count, block_size, total_size)
                while True:
                    block = fp.read(block_size)
                    if not block:
                        break
                    out_file.write(block)
                    block_count += 1
                    report_progress(block_count, block_size, total_size)
        else:
            raise Exception ('nonexisting file or connection error')

def download_file(url,filename):
    import ssl
    import contextlib
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    with contextlib.closing(urlopen(url,context=ctx)) as fp:
        block_size = 1024 * 8
        block = fp.read(block_size)
        if block:
            with open(filename,'wb') as out_file:
                out_file.write(block)
                while True:
                    block = fp.read(block_size)
                    if not block:
                        break
                    out_file.write(block)
        else:
            raise Exception ('nonexisting file or connection error')

def get_tool(tool):
    sys_name = platform.system()
    archive_name = tool['archiveFileName']
    local_path = dist_dir + archive_name
    url = tool['url']
    if not os.path.isfile(local_path):
        print('Downloading ' + archive_name + ' ...')
        sys.stdout.flush()
        if 'CYGWIN_NT' in sys_name:
            import ssl
            ctx = ssl.create_default_context()
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            urlretrieve(url, local_path, report_progress, context=ctx)
        elif 'Windows' in sys_name:
            r = requests.get(url)
            f = open(local_path, 'wb')
            f.write(r.content)
            f.close()
        else:
            is_ci = os.environ.get('GITHUB_WORKSPACE');
            if is_ci:
                download_file(url, local_path)
            else:
                try:
                    urlretrieve(url, local_path, report_progress)
                except:
                    download_file_with_progress(url, local_path)
                sys.stdout.write("\rDone   \n")
                sys.stdout.flush()
    else:
        print('Tool {0} already downloaded'.format(archive_name))
        sys.stdout.flush()
    unpack(local_path, '.')

def load_tools_list(filename, platform):
    tools_info = json.load(open(filename))['packages'][0]['tools']
    tools_to_download = []
    for t in tools_info:
        tool_platform = [p for p in t['systems'] if p['host'] == platform]
        if len(tool_platform) == 0:
            # Fallback to x86 on Apple ARM
            if platform == 'arm64-apple-darwin':
                tool_platform = [p for p in t['systems'] if p['host'] == 'x86_64-apple-darwin']
                if len(tool_platform) == 0:
                    continue
            # Fallback to 32bit on 64bit x86 Windows
            elif platform == 'x86_64-mingw32':
                tool_platform = [p for p in t['systems'] if p['host'] == 'i686-mingw32']
                if len(tool_platform) == 0:
                    continue
            else:
                continue
        tools_to_download.append(tool_platform[0])
    return tools_to_download

def identify_platform():
    arduino_platform_names = {'Darwin'   : {32 : 'i386-apple-darwin',   64 : 'x86_64-apple-darwin'},
                              'DarwinARM': {32 : 'arm64-apple-darwin',  64 : 'arm64-apple-darwin'},
                              'Linux'    : {32 : 'i686-pc-linux-gnu',   64 : 'x86_64-pc-linux-gnu'},
                              'LinuxARM' : {32 : 'arm-linux-gnueabihf', 64 : 'aarch64-linux-gnu'},
                              'Windows'  : {32 : 'i686-mingw32',        64 : 'x86_64-mingw32'}}
    bits = 32
    if sys.maxsize > 2**32:
        bits = 64
    sys_name = platform.system()
    sys_platform = platform.platform()
    if 'Darwin' in sys_name and (sys_platform.find('arm') > 0 or sys_platform.find('arm64') > 0):
        sys_name = 'DarwinARM'
    if 'Linux' in sys_name and (sys_platform.find('arm') > 0 or sys_platform.find('aarch64') > 0):
        sys_name = 'LinuxARM'
    if 'CYGWIN_NT' in sys_name:
        sys_name = 'Windows'
    print('System: %s, Bits: %d, Info: %s' % (sys_name, bits, sys_platform))
    return arduino_platform_names[sys_name][bits]

if __name__ == '__main__':
    verbose = "-v" in sys.argv
    is_test = (len(sys.argv) > 1 and sys.argv[1] == '-h')
    if is_test:
        print('Test run!')
    identified_platform = identify_platform()
    print('Platform: {0}'.format(identified_platform))
    tools_to_download = load_tools_list(current_dir + '/../package/package_esp32_index.template.json', identified_platform)
    mkdir_p(dist_dir)
    for tool in tools_to_download:
        if is_test:
            print('Would install: {0}'.format(tool['archiveFileName']))
        else:
            get_tool(tool)
    print('Platform Tools Installed')
