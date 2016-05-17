# Copyright (C) 2016 iNuron NV
#
# This file is part of Open vStorage Open Source Edition (OSE),
# as available from
#
#      http://www.openvstorage.org and
#      http://www.openvstorage.com.
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
# as published by the Free Software Foundation, in version 3 as it comes in
# the LICENSE.txt file of the Open vStorage OSE distribution.
# Open vStorage is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY of any kind.
import os, sys
from stat import *
from sets import Set

skip_directories = Set([ \
'./license', \
'./.hg', \
'./src/xmlrpc++0.7', \
'./src/serverinterface/qemu', \
'./src/msgpack-c' \
])

extensions = Set(['.cpp', '.h', '.py'])

skip_files = Set([ \
'./src/youtils/ProCon.h', \
'./src/youtils/LRUCache.h', \
'./src/filesystem-python-client/IntegerConverter.h', \
'./src/filesystem-python-client/IterableConverter.h', \
'./src/filesystem-python-client/OptionalConverter.h', \
'./src/filesystem-python-client/PairConverter.h', \
'./src/filesystem-python-client/StringyConverter.h', \
'./src/filesystem-python-client/StrongArithmeticTypedefConverter.h' \
])

licenses = dict()

def walktree(top, callback):
    for f in os.listdir(top):
        pathname = os.path.join(top, f)
        mode = os.stat(pathname)[ST_MODE]
        if S_ISDIR(mode):
            # It's a directory, recurse into it
            if pathname not in skip_directories:
                walktree(pathname, callback)
        elif S_ISREG(mode):
            # It's a file, call the callback function
            visit_file(pathname, callback)
        else:
            # Unknown file type, print a message
            print 'Skipping %s' % pathname

def visit_file(pathname, callback):
    extension = os.path.splitext(pathname)[1]
    if extension in extensions:
        if pathname not in skip_files:
            license = licenses[extension]
            callback(pathname, license)

def add_license(pathname, license):
    with open(pathname, 'r') as f:
        data = f.read()
        if data.startswith(license):
            print "skipping %s : already done" % pathname
        else:
            with open(pathname, 'w') as f:
                f.write(license + data)

def remove_license(pathname, license):
    with open(pathname, 'r') as f:
        data = f.read()
        if data.startswith(license):
            with open(pathname, 'w') as f:
                f.write(data[len(license):])
        else:
            print "skipping %s : already done" % pathname

def load_licenses():
    for extension in extensions:
        lic_file = 'license/license' + extension
        with open(lic_file,'r') as f:
            licenses[extension] = f.read()
