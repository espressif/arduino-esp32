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

def compile(tmp_dir, cache_dir, sketch, ide_path, f, args):
    cmd = ide_path + '/arduino-builder '
    cmd += '-compile -logger=human '
    cmd += '-hardware "' + ide_path + '/hardware" '
    if args.usr_path:
        cmd += '-hardware "' + args.usr_path + '/hardware" '
    if args.hardware_path:
        for hw_dir in args.hardware_path:
            cmd += '-hardware "' + hw_dir + '" '
    cmd += '-tools "' +  ide_path + '/tools-builder" '
    if args.tools_path:
        for tools_dir in args.tools_path:
            cmd += '-tools "' + tools_dir + '" '
    cmd += '-built-in-libraries "' +  ide_path + '/libraries" '
    if args.usr_path:
        cmd += '-libraries "' + args.usr_path + '/libraries" '
    if args.library_path:
        for lib_dir in args.library_path:
            cmd += '-libraries "' + lib_dir + '" '
    cmd += '-fqbn={fqbn} '.format(**vars(args))
    cmd += '-ide-version=10810 '
    cmd += '-build-path "' + tmp_dir + '" '
    cmd += '-warnings={warnings} '.format(**vars(args))
    cmd += '-build-cache "' + cache_dir + '" '
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
    parser.add_argument('-v', '--verbose', help='Enable verbose output', action='store_true')
    parser.add_argument('-i', '--ide_path', help='Arduino IDE path')
    parser.add_argument('-b', '--build_path', help='Build directory')
    parser.add_argument('-c', '--build_cache', help='Core Cache directory')
    parser.add_argument('-u', '--usr_path', help='Arduino Home directory (holds your sketches, libraries and hardware)')
    parser.add_argument('-f', '--fqbn', help='Arduino Board FQBN')
    parser.add_argument('-l', '--library_path', help='Additional library path', action='append')
    parser.add_argument('-d', '--hardware_path', help='Additional hardware path', action='append')
    parser.add_argument('-t', '--tools_path', help='Additional tools path', action='append')
    parser.add_argument('-w', '--warnings', help='Compilation warnings level', default='none', choices=['none', 'all', 'more', 'default'])
    parser.add_argument('-o', '--output_binary', help='File name for output binary')
    parser.add_argument('-k', '--keep', action='store_true', help='Don\'t delete temporary build directory')
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

    if not args.fqbn:
        print("Please specify Arduino Board FQBN using the --fqbn option", file=sys.stderr)
        return 3

    sketch_path = args.sketch_path
    
    tmp_dir = args.build_path
    created_tmp_dir = False
    if not tmp_dir:
        tmp_dir = tempfile.mkdtemp()
        created_tmp_dir = True

    cache_dir = args.build_cache
    created_cache_dir = False
    if not cache_dir:
        cache_dir = tempfile.mkdtemp()
        created_cache_dir = True
    
    output_name = tmp_dir + '/' + os.path.basename(sketch_path) + '.bin'
    
    if args.verbose:
        print("Sketch: ", sketch_path)
        print("Build dir: ", tmp_dir)
        print("Cache dir: ", cache_dir)
        print("Output: ", output_name)

    if args.verbose:
        f = sys.stdout
    else:
        f = open(tmp_dir + '/build.log', 'w')

    res = compile(tmp_dir, cache_dir, sketch_path, ide_path, f, args)
    if res != 0:
        return res

    if args.output_binary is not None:
        shutil.copy(output_name, args.output_binary)

    if created_tmp_dir and not args.keep:
        shutil.rmtree(tmp_dir, ignore_errors=True)

    if created_cache_dir and not args.keep:
        shutil.rmtree(cache_dir, ignore_errors=True)

if __name__ == '__main__':
    sys.exit(main())
