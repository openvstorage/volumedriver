# Copyright 2015 Open vStorage NV
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

load_licenses()
walktree('.', add_license)
