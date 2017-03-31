#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# build.py — build a sketch using arduino-builder
#
# Wrapper script around arduino-builder which accepts some ESP8266-specific
# options and translates them into FQBN
#
# Copyright © 2016 Ivan Grokhotkov
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
#


from __future__ import print_function
import sys
import os
import argparse
import subprocess
import tempfile
import shutil

def compile(tmp_dir, sketch, tools_dir, hardware_dir, ide_path, f, args):
    cmd = ide_path + '/arduino-builder '
    cmd += '-compile -logger=human '
    cmd += '-build-path "' + tmp_dir + '" '
    cmd += '-tools "' +  ide_path + '/tools-builder" '
    if args.library_path:
        for lib_dir in args.library_path:
            cmd += '-libraries "' + lib_dir + '" '
    cmd += '-hardware "' + ide_path + '/hardware" '
    if args.hardware_dir:
        for hw_dir in args.hardware_dir:
            cmd += '-hardware "' + hw_dir + '" '
    else:
        cmd += '-hardware "' + hardware_dir + '" '
    # Debug=Serial,DebugLevel=Core____
    cmd += '-fqbn=espressif:esp32:{board_name}:' \
            'FlashFreq={flash_freq},' \
            'UploadSpeed=921600'.format(**vars(args))
    cmd += ' '
    cmd += '-ide-version=10607 '
    cmd += '-warnings={warnings} '.format(**vars(args))
    if args.verbose:
        cmd += '-verbose '
    cmd += sketch

    if args.verbose:
        print('Building: ' + cmd, file=f)

    cmds = cmd.split(' ')
    p = subprocess.Popen(cmds, stdout=f, stderr=subprocess.STDOUT)
    p.wait()
    return p.returncode

def parse_args():
    parser = argparse.ArgumentParser(description='Sketch build helper')
    parser.add_argument('-v', '--verbose', help='Enable verbose output',
                        action='store_true')
    parser.add_argument('-i', '--ide_path', help='Arduino IDE path')
    parser.add_argument('-p', '--build_path', help='Build directory')
    parser.add_argument('-l', '--library_path', help='Additional library path',
                        action='append')
    parser.add_argument('-d', '--hardware_dir', help='Additional hardware path',
                        action='append')
    parser.add_argument('-b', '--board_name', help='Board name', default='esp32')
    parser.add_argument('-w', '--warnings', help='Compilation warnings level',
                        default='none', choices=['none', 'all', 'more'])
    parser.add_argument('-o', '--output_binary', help='File name for output binary')
    parser.add_argument('-k', '--keep', action='store_true',
                        help='Don\'t delete temporary build directory')
    parser.add_argument('--flash_freq', help='Flash frequency', default=40,
                        type=int, choices=[40, 80])
    parser.add_argument('sketch_path', help='Sketch file path')
    return parser.parse_args()

def main():
    args = parse_args()

    ide_path = args.ide_path
    if not ide_path:
        ide_path = os.environ.get('ARDUINO_IDE_PATH')
        if not ide_path:
            print("Please specify Arduino IDE path via --ide_path option"
                  "or ARDUINO_IDE_PATH environment variable.", file=sys.stderr)
            return 2

    sketch_path = args.sketch_path
    tmp_dir = args.build_path
    created_tmp_dir = False
    if not tmp_dir:
        tmp_dir = tempfile.mkdtemp()
        created_tmp_dir = True

    tools_dir = os.path.dirname(os.path.realpath(__file__)) + '/../tools'
    # this is not the correct hardware folder to add.
    hardware_dir = os.path.dirname(os.path.realpath(__file__)) + '/../cores'

    output_name = tmp_dir + '/' + os.path.basename(sketch_path) + '.bin'
    if args.verbose:
        print("Sketch: ", sketch_path)
        print("Build dir: ", tmp_dir)
        print("Output: ", output_name)

    if args.verbose:
        f = sys.stdout
    else:
        f = open(tmp_dir + '/build.log', 'w')

    res = compile(tmp_dir, sketch_path, tools_dir, hardware_dir, ide_path, f, args)
    if res != 0:
        return res

    if args.output_binary is not None:
        shutil.copy(output_name, args.output_binary)

    if created_tmp_dir and not args.keep:
        shutil.rmtree(tmp_dir, ignore_errors=True)

if __name__ == '__main__':
    sys.exit(main())
