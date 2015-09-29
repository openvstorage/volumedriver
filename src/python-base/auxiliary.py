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

import volumedriver.base.env as env
import time
import subprocess
import random
import os
import shutil
import tempfile
import logging
import threading

logger = env.getSublogger(__name__)


def logCommand(args):
    command = 'Command: '
    for i in args:
        command += i + ' '

    logger.debug(command)


def handleProcessOutput(outp, msg):
    ret = outp[0]
    if(ret == 0):
        logger.info("Success: " + msg)
        # return true for backward compatibility
        return True
    else:
        errmsg = "Failure: %s, [ret, out, err]: %s" % (
            msg, map(str, outp))  # pylint: disable=W0141
        logger.error(errmsg)
        raise Exception(errmsg)

SINGLE_SECTION_NAME = "singlesection"


def createDirSafe(path):
    if os.path.exists(path):
        shutil.rmtree(path)
    os.makedirs(path)


def removeDirTreeSafe(path):
    if os.path.exists(path):
        shutil.rmtree(path)


def waitUntil(test, pollInterval=1, maxTime=float("inf"), desc=None, slowDown=False, slowDownRate=1.5, initialPollInterval=0.01):
    duration = 0
    testSuccess = test()
    slp = initialPollInterval if slowDown else pollInterval

    while not testSuccess and duration < maxTime:
        if desc != None and slp >= pollInterval:  # not logging during startup
            logger.debug("waitUntil (%.1f s): %s" % (duration, desc))

        time.sleep(slp)
        duration += slp
        slp = min(slowDownRate * slp, pollInterval)
        testSuccess = test()

    return testSuccess


def waitUntil2(test, maxTime=float("inf"), desc=None):
    return waitUntil(test, pollInterval=5, maxTime=maxTime, desc=desc, slowDown=True)


def mapDict(f, d):
    return dict(zip(d.keys(),
                    # pylint: disable=W0141
                    map(f, d.values())))


def keyConflicts(x, y):
    def f(k):
        return x[k] != y[k]
    # pylint: disable=W0141
    return map(lambda k: (k, (x[k], y[k])),
               filter(f, set(x.keys()).intersection(set(y.keys()))))


def overlayDict(bottom, top):
    z = dict(bottom)
    z.update(top)
    return z


def mergeDict(x, y):
    if not keyConflicts(x, y) == []:
        raise Exception("Incompatible keys in mergeDict: %s" %
                        keyConflicts(x, y))
    return overlayDict(x, y)


def mergeDictList(lst):
    return reduce(mergeDict, lst, {})


def liftToDict(oper):
    def f(d1, d2):
        z = dict(d1)
        for k in d2.keys():
            if z.has_key(k):
                z[k] = oper(d1[k], d2[k])
            else:
                z[k] = d2[k]
        return z
    return f


def removeKey(k, d):
    nd = d.copy()
    nd.pop(k)
    return nd


def recursiveOverlayDict(bottom, top, allowOverwrites=True):
    if isinstance(bottom, dict) != isinstance(top, dict):
        raise ValueError("combining non-dict with dict not allowed")
    elif not isinstance(top, dict):
        if not allowOverwrites:
            raise ValueError("overwrites not allowed")
        return top
    else:
        z = dict(bottom)
        for k, v in top.iteritems():
            if not z.has_key(k):
                z[k] = v
            else:
                z[k] = recursiveOverlayDict(z[k], v, allowOverwrites)
        return z


def isIntegral(val):
    return type(val) in [int, long]


class Enumerate(object):

    def __init__(self, names):
        for number, name in enumerate(names.split()):
            setattr(self, name, number)


def multinomial(xs):
    n = len(xs)
    u = random.random()
    ys = reduce(lambda l, x: l + [x + l[-1]], xs, [0])[1:]
    i = 0
    while ys[i] < u and i < n - 1:
        i += 1
    return i


def sum2One(xs):
    s = sum(xs)
    # pylint: disable=W0141
    return map(lambda x: x / s, xs)


class PatternIterator:

    def __init__(self, itera):
        self._x = None
        self._hasNext = True
        self._iter = itera
        self.moveAhead()

    def moveAhead(self):
        try:
            self._x = self._iter.next()
        except StopIteration:
            self._hasNext = False

    def isNil(self):
        return not self._hasNext

    def head(self):
        if self.isNil():
            raise Exception("Taking head of empty list")
        return self._x


def mergeIterators2(x, y, _):
    xs = PatternIterator(x)
    ys = PatternIterator(y)

    while True:
        if xs.isNil() and ys.isNil():
            raise StopIteration

        if not xs.isNil() and ys.isNil():
            yield xs.head()
            xs.moveAhead()

        if xs.isNil() and not ys.isNil():
            yield ys.head()
            ys.moveAhead()

        if not xs.isNil() and not ys.isNil():
            if xs.head() < ys.head():
                yield xs.head()
                xs.moveAhead()
            else:
                yield ys.head()
                ys.moveAhead()


def mergeIterators(iters, key):
    return reduce(lambda x, y:  mergeIterators2(x, y, key), iters, iter([]))


def poissonTimes(freq):
    nextEventTime = 0
    while True:
        nextEventTime += random.expovariate(freq)
        yield nextEventTime


def isApprox(x, y, relativeError=0.1):
    return abs(float(x - y)) / abs(float(x)) <= relativeError if x != 0 else (x == y)


def withNonBlockingLock(lock, action):
    if lock.acquire(False):
        action()
        lock.release()
        return True
    else:
        return False


def avoidNone(x, y):
    return y if x == None else x


def flatten(x):
    return sum(x, [])


def dictToCmdLineParams(d):
    return flatten([["--%s" % it[0], it[1]] for it in d.iteritems()])


def divCeiling(a, b):
    """ assumes integers a, b with b >0 and returns ceiling(a/b)
    """
    if not (isIntegral(a) and isIntegral(b)):
        raise TypeError()

    if not b > 0:
        raise ValueError()

    # python assumption
    assert a == (a / b) * b + a % b

    return a / b if a % b == 0 else a / b + 1


def multipleCeiling(a, b):
    """ assumes integers a, b with b >0 and returns the smallest number m >=a
        such that m is a multiple of b
    """
    return divCeiling(a, b) * b


def multipleFloor(a, b):
    """ assumes integers a, b with b >0 and returns the biggest number m <=a
        such that m is a multiple of b
    """
    return (a / b) * b


def getPIDS(pattern):
    p = subprocess.Popen(["pgrep", "-f", pattern],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    (out, err) = p.communicate()
    if p.returncode == 0:
        # pylint: disable=E1103
        return [int(pid) for pid in out.splitlines()]
    elif p.returncode == 1:
        return []
    else:
        raise Exception(err)

# to be used as json hook


def noUnicode(dct):
    def maybeConvert(x):
        return str(x) if isinstance(x, unicode) else x

    newdct = dict()
    for k, v in dct.iteritems():
        newdct[maybeConvert(k)] = maybeConvert(v)
    return newdct


# gotten from python 2.7
# def check_output(*popenargs, **kwargs):
#     r"""Run command with arguments and return its output as a byte string.

#     If the exit code was non-zero it raises a CalledProcessError.  The
#     CalledProcessError object will have the return code in the returncode
#     attribute and output in the output attribute.

#     The arguments are the same as for the Popen constructor.  Example:

#     >>> check_output(["ls", "-l", "/dev/null"])
#     'crw-rw-rw- 1 root root 1, 3 Oct 18  2007 /dev/null\n'

#     The stdout argument is not allowed as it is used internally.
#     To capture standard error in the result, use stderr=STDOUT.

#     >>> check_output(["/bin/sh", "-c",
#     ...               "ls -l non_existent_file ; exit 0"],
#     ...              stderr=STDOUT)
#     'ls: non_existent_file: No such file or directory\n'
#     """
#     if 'stdout' in kwargs:
#         raise ValueError('stdout argument not allowed, it will be overridden.')
#     process = subprocess.Popen(stdout=subprocess.PIPE, *popenargs, **kwargs)
#     output, _ = process.communicate()
#     retcode = process.poll()
#     if retcode:
#         cmd = kwargs.get("args")
#         if cmd is None:
#             cmd = popenargs[0]
# doesnt work in python 2.6:
# raise subprocess.CalledProcessError(retcode, cmd, output=output)
#         raise subprocess.CalledProcessError(retcode, cmd)
#     return output


def isPidAlive(pid):
    try:
        os.kill(pid, 0)
    except OSError:
        return False

    return True


def syscmd(cmd, desc=None):
    _logger = logging.getLogger(env.logger_name + ".syscmd")
    if not isinstance(cmd, str):
        raise TypeError("syscmd: arg must be a string: %s" % str(cmd))
    if desc:
        _logger.info(desc)
    else:
        _logger.info(cmd)
    return subprocess.check_output(cmd, shell=True, stderr=subprocess.PIPE)


def randomString(n):
    return "".join(chr(random.randrange(0, 256)) for x in xrange(n))


class ErrorReportingThread(threading.Thread):

    def __init__(self, *args, **kwargs):
        threading.Thread.__init__(self, *args, **kwargs)
        self.error = None

    def run(self):
        try:
            threading.Thread.run(self)
        except Exception, e:
            self.error = e
            raise

#------------------ Auxiliary with constructs ---------------------------


class createdTempFile(object):

    def __init__(self, prefix=""):
        (self.fd, self.fname) = tempfile.mkstemp(prefix=prefix)

    def __enter__(self):
        return (self.fd, self.fname)

    def __exit__(self, typ, value, traceback):
        try:
            os.close(self.fd)
        except:     # pylint: disable=W0702
            pass
        try:
            os.remove(self.fname)
        except:     # pylint: disable=W0702
            pass


class nonCreatedTempFile(object):

    def __init__(self, prefix=""):
        self.fname = tempfile.mktemp(prefix=prefix)

    def __enter__(self):
        return self.fname

    def __exit__(self, typ, value, traceback):
        try:
            os.remove(self.fname)
        except:     # pylint: disable=W0702
            pass


class cleanedUpTempDir:

    def __init__(self, parentDir, prefix, cleanupOnException=True):
        self._cleanupOnException = cleanupOnException
        self._tmpDir = tempfile.mkdtemp(prefix=prefix, dir=parentDir)

    def __enter__(self):
        return self._tmpDir

    def __exit__(self, typ, value, traceback):
        if typ == None or self._cleanupOnException:
            removeDirTreeSafe(self._tmpDir)


class cleanedUpDir:

    def __init__(self, dirName, cleanupOnException=True):
        self._cleanupOnException = cleanupOnException
        os.mkdir(dirName)
        self._tmpDir = dirName

    def __enter__(self):
        return self._tmpDir

    def __exit__(self, type_, value, traceback):
        if type_ == None or self._cleanupOnException:
            removeDirTreeSafe(self._tmpDir)


class updatedDict:

    def __init__(self, dct, other):
        self._dct = dct
        self._old = self._dct.copy()
        self._dct.update(other)

    def __enter__(self):
        return self._dct

    def __exit__(self, tp, value, traceback):
        self._dct.clear()
        self._dct.update(self._old)


class openfd:

    def __init__(self, filename, flags, *args, **kwargs):
        self._fd = os.open(filename, flags, *args, **kwargs)

    def __enter__(self):
        return self._fd

    def __exit__(self, tp, value, traceback):
        os.close(self._fd)


# Local Variables: **
# mode : python **
# python-top-dir: "../../../../../../.." **
# End: **
