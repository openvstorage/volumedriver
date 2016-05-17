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

import logging
import os
from os import environ
import subprocess
configFile = environ.get("PY_LOGGING", None)

if configFile:
    if not os.path.exists(configFile):
        raise Exception("env.py: logconfig file %s not found" % configFile)
    else:
        logging.config.fileConfig(configFile)

logger_name = "com.openvstorage"
logger = logging.getLogger(logger_name)


def getSublogger(modname):
    return logging.getLogger(logger_name + "." + modname)


class NullHandler(logging.Handler):

    def emit(self, record):
        pass

_h = NullHandler()
logger.addHandler(_h)
logger.setLevel(logging.DEBUG)

tmpDir = environ.get('TEMP', "/tmp")
logDir = environ.get('LOG_DIR', "/var/log/volumedriver")
pidDir = environ.get('PID_DIR', "/var/run")
varDir = environ.get('VAR_DIR', "/var")
cfgDir = environ.get('CFG_DIR', "/etc/volumedriver")

# q-stuff to make transition easier, should fade out eventually


class Stub():  # pylint: disable=W0232
    pass

q = Stub()
q.dirs = Stub()  # pylint: disable=W0201
q.dirs.tmpDir = tmpDir  # pylint: disable=W0201
q.dirs.logDir = logDir  # pylint: disable=W0201
q.dirs.pidDir = pidDir  # pylint: disable=W0201
q.dirs.varDir = varDir  # pylint: disable=W0201
q.dirs.cfgDir = cfgDir  # pylint: disable=W0201


class Executor:  # pylint: disable=W0232

    def execute(self, cmd):
        return subprocess.check_output(cmd, shell=True)

q.system = Stub()  # pylint: disable=W0201
q.system.process = Executor()  # pylint: disable=W0201


class OldLogger:  # pylint: disable=W0232

    def log(self, msg, _=None):
        logger.info(msg)

q.logger = OldLogger()  # pylint: disable=W0201

import ConfigParser


def getConfig(name):
    config = ConfigParser.ConfigParser()

    successList = config.read(name)
    if len(successList) != 1:
        raise Exception("getConfig: config file %s not found" % name)

    dictionary = {}
    for section in config.sections():
        dictionary[section] = {}
        for option in config.options(section):
            dictionary[section][option] = config.get(section, option)
    return dictionary
