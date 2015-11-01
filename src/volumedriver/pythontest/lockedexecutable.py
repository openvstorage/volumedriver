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

import os
import signal
import subprocess
from volumedriver.base import env

logger = env.getSublogger(__name__)

from volumedriver.toolcut.backend import BackendWithNamespace
LOCKED_BINARY = "locked_executable"


class LockNotTakenException(BaseException):
    pass


class WrongArgumentType(BaseException):
    pass


class BackendNotSupported(BaseException):
    pass

REGISTERED_PROCS = {}


def start(backendWithNamespace,
          time_milliseconds=0,
          lock_session_timeout=1):
    """
    starts locked binary, returns the PID of the process
    Normal options:
    --namespace arg                  namespace to grab a lock in
    --time-milliseconds arg (=0)     time in milliseconds the code should run
    -c [ --backend-config-file ] arg backend config file
    -B [ --backend ] arg (=LOCAL)    backend type: BUCHLA / REST / LOCAL

    BUCHLA options:
    --dss_host arg (=127.0.0.1) BUCHLA host
    --dss_port arg (=23509)     port
    --dss_timeout arg (=60)     BUCHLA connection timeout in seconds

    LOCAL options:
    --localbackend-path arg LocalBackend toplevel directory

    REST options:
    --dss_host arg (=127.0.0.1) REST host
    --dss_port arg (=23509)     port
    --dss_timeout arg (=60)     REST connection timeout in seconds
    """
    if(not isinstance(backendWithNamespace, BackendWithNamespace,)):
        raise WrongArgumentType(
            "Expected a BackendWithNamespace, got a " + str(type(backendWithNamespace)))

    namespace = backendWithNamespace.namespace()
    params = backendWithNamespace.params()
    command = [LOCKED_BINARY,
               '--namespace', namespace,
               '--lock-session-timeout', str(lock_session_timeout),
               '--time-milliseconds', str(time_milliseconds)]

    if (params['backend_type'] == "LOCAL"):
        command.extend(['--backend', 'LOCAL',
                        '--localbackend-path', params["local_connection_path"]])
    elif (params['backend_type'] == "REST"):
        command.extend(['--backend', 'REST',
                        '--dss_host', params['rest_connection_host'],
                        '--dss_port', str(params['rest_connection_port'])])
    else:
        raise BackendNotSupported(
            "Backend type not supported " + params["backend_type"])

    logger.info("Running locked executable like so: " + str(command))
    childprocess = subprocess.Popen(command,
                                    # stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE,
                                    close_fds=True,
                                    shell=False,
                                    env=os.environ)
    response = ''

    global REGISTERED_PROCS  # pylint: disable=W0602
    REGISTERED_PROCS[childprocess.pid] = childprocess

    while((not response) and
          (childprocess.poll() is None)):
        response = childprocess.stdout.readline()

    error = 'FAILURE to take lock, maybe another lock proccess is running for namespace[%s]\n stdout:%s\n stderr: %s'

    if(not childprocess.returncode is None):
        raise LockNotTakenException(
            error % (namespace, response, childprocess.stderr.readlines()))

    pid = childprocess.pid
    if 'OK' in response:
        logger.info("SUCCESS, pid " + str(pid))
        return pid
    elif 'FAILURE' in response:
        logger.info("FAILURE")
        raise LockNotTakenException(
            error % (namespace, response, childprocess.stderr.readlines()))
    else:
        logger.error("STRANGE LINE " + response)
        raise Exception(error %
                        (namespace, response, childprocess.stderr.readlines()))


def stop(pid):
    """ Stops the locked executable with given PID which
        is expected to release the lock first
    """
    global REGISTERED_PROCS  # pylint: disable=W0602
    REGISTERED_PROCS[pid].send_signal(signal.SIGUSR1)
    REGISTERED_PROCS[pid].wait()
    del REGISTERED_PROCS[pid]


def kill(pid):
    """ Stops the locked executable with given PID which
        is NOT expected to release the lock first
    """
    global REGISTERED_PROCS  # pylint: disable=W0602
    REGISTERED_PROCS[pid].kill()

# pkill would be better here probably, also kill only stuff in your own process group
# so as not to interfere with other testers that might be running.


def stopAll():
    """ Stops all started locked executables, also not REGISTERED ones
    """
    locked_executable_process = subprocess.Popen(
        'ps -eaf | grep locked_executable',
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        close_fds=True,
        shell=True,
        env=os.environ)
    output = locked_executable_process.stdout.readlines()
    locked_executable_process = [
        process for process in output if 'locked_executable' in process and 'grep' not in process]
    if locked_executable_process:
        for process in locked_executable_process:
            pid = [p for p in process.split(' ') if p != '']
            os.kill(int(pid[1]), signal.SIGUSR1)

    global REGISTERED_PROCS  # pylint: disable=W0603
    REGISTERED_PROCS = {}
