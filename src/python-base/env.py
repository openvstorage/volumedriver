# Copyright 2015 iNuron NV
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
