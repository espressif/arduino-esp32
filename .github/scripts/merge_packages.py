#!/usr/bin/env python
# This script merges two Arduino Board Manager package json files.
# Usage:
#   python merge_packages.py package_esp8266com_index.json version/new/package_esp8266com_index.json
# Written by Ivan Grokhotkov, 2015
#
from __future__ import print_function
#from distutils.version import LooseVersion
from packaging.version import Version
import re
import json
import sys

def load_package(filename):
    pkg = json.load(open(filename))['packages'][0]
    print("Loaded package {0} from {1}".format(pkg['name'], filename), file=sys.stderr)
    print("{0} platform(s), {1} tools".format(len(pkg['platforms']), len(pkg['tools'])), file=sys.stderr)
    return pkg

def merge_objects(versions, obj):
    for o in obj:
        name = o['name'].encode('ascii')
        ver = o['version'].encode('ascii')
        if not name in versions:
            print("found new object, {0}".format(name), file=sys.stderr)
            versions[name] = {}
        if not ver in versions[name]:
            print("found new version {0} for object {1}".format(ver, name), file=sys.stderr)
            versions[name][ver] = o
    return versions

# Normalize ESP release version string (x.x.x) by adding '-rc<MAXINT>' (x.x.x-rc9223372036854775807) to ensure having REL above any RC
# Dummy approach, functional anyway for current ESP package versioning (unlike NormalizedVersion/LooseVersion/StrictVersion & similar crap)
def pkgVersionNormalized(versionString):

    verStr = str(versionString)
    verParts = re.split('\.|-rc|-alpha', verStr, flags=re.IGNORECASE)
    
    if len(verParts) == 3:
        if (sys.version_info > (3, 0)): # Python 3
            verStr = str(versionString) + '-rc' + str(sys.maxsize)
        else: # Python 2
            verStr = str(versionString) + '-rc' + str(sys.maxint)
        
    elif len(verParts) != 4:
        print("pkgVersionNormalized WARNING: unexpected version format: {0})".format(verStr), file=sys.stderr)
        
    return verStr


def main(args):
    if len(args) < 3:
        print("Usage: {0} <package1> <package2>".format(args[0]), file=sys.stderr)
        return 1

    tools = {}
    platforms = {} 
    pkg1 = load_package(args[1])
    tools = merge_objects(tools, pkg1['tools']);
    platforms = merge_objects(platforms, pkg1['platforms']);
    pkg2 = load_package(args[2])
    tools = merge_objects(tools, pkg2['tools']);
    platforms = merge_objects(platforms, pkg2['platforms']);

    pkg1['tools'] = []
    pkg1['platforms'] = []

    for name in tools:
        for version in tools[name]:
            print("Adding tool {0}-{1}".format(name, version), file=sys.stderr)
            pkg1['tools'].append(tools[name][version])

    for name in platforms:
        for version in platforms[name]:
            print("Adding platform {0}-{1}".format(name, version), file=sys.stderr)
            pkg1['platforms'].append(platforms[name][version])
                
    #pkg1['platforms'] = sorted(pkg1['platforms'], key=lambda k: LooseVersion(pkgVersionNormalized(k['version'])), reverse=True)
    pkg1['platforms'] = sorted(pkg1['platforms'], key=lambda k: Version(pkgVersionNormalized(k['version'])), reverse=True)

    json.dump({'packages':[pkg1]}, sys.stdout, indent=2)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
