// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

// Copyright Ralf W. Grosse-Kunstleve 2002-2004. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "VolumeInfo.h"
#include "SnapshotToolCut.h"
#include "TLogToolCut.h"
#include "SnapshotPersistorToolCut.h"
#include "TLogReaderToolCut.h"
#include "SCOAccessDataInfo.h"
#include "ScrubbingResultToolCut.h"
#include "MetadataStoreToolCut.h"
#include "ClusterLocationToolCut.h"
#include "SCOToolCut.h"
#include "EntryToolCut.h"

#include "../FailOverCacheConfig.h"

#include <iostream>
#include <string>
#include <vector>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/import.hpp>
#include <boost/python/manage_new_object.hpp>
#include <boost/python/module.hpp>

#include <youtils/Logger.h>
#include <youtils/LoggerToolCut.h>
#include <youtils/LoggingToolCut.h>
#include <youtils/PythonBuildInfo.h>

#define MAKE_PYTHON_BOOLEAN_ENUM(name, doc)     \
    enum_<name>(#name, doc)                     \
    .value("F", name::F)                        \
    .value("T", name::T);

BOOST_PYTHON_MODULE(ToolCut)
{
    using namespace boost::python;
    using namespace toolcut;

    youtils::Logger::disableLogging();

    MAKE_PYTHON_BOOLEAN_ENUM(OverwriteObject, "Whether to overwrite an existing object in the backend, values are T and F")

    scope().attr("__doc__") = "Access the basic building blocks of VolumeDriver\n"
        "such as SCO, ClusterLocation, TLog, TLogReader, Snapshot, VolumeInfo, ScrubbingResult...";

    enum_<volumedriver::DtlConfigWrapper::CacheType>("FailOverCacheType",
                                                               "Type of Failover cache.\n"
                                                               "Values are NoFailOverCache or RemoteFailOverCache")
        .value("NoFailOverCache", volumedriver::DtlConfigWrapper::CacheType::None)
        .value("RemoteFailOverCache", volumedriver::DtlConfigWrapper::CacheType::Remote);


    enum_<volumedriver::VolumeConfig::WanBackupVolumeRole>("WanBackupVolumeRole",
                                                           "Role of a Volume in the backup scenario -- currently unused")
        .value("Normal",
               volumedriver::VolumeConfig::WanBackupVolumeRole::WanBackupNormal)
        .value("BackupIncremental",
               volumedriver::VolumeConfig::WanBackupVolumeRole::WanBackupIncremental)
        .value("BackupBase",
               volumedriver::VolumeConfig::WanBackupVolumeRole::WanBackupBase);

    enum_<volumedriver::Entry::Type>("EntryType",
                                     "Type entries in a TLog.\n"
                                     "Values are SyncTC, TLogCRC, SCOCRC or CLoc")
        .value("SyncTC", volumedriver::Entry::Type::SyncTC)
        .value("TLogCRC", volumedriver::Entry::Type::TLogCRC)
        .value("SCOCRC", volumedriver::Entry::Type::SCOCRC)
        .value("CLoc", volumedriver::Entry::Type::LOC);

#include <youtils/LoggerToolCut.incl>

    youtils::python::BuildInfo::registerize();

    class_<SnapshotToolCut>("Snapshot",
                            "Represents a snapshot as it is stored in the snapshots.xml",
                            no_init)
        .def("__str__", &SnapshotToolCut::str)
        .def("__repr__", &SnapshotToolCut::repr)
        .def("number", &SnapshotToolCut::getNum,
             "Get number of this snapshot.\n"
             "@returns the snapshot number.")
        .def("name", &SnapshotToolCut::name,
             "get the name of the snapshot.\n"
             "@returns the snapshot name")
        .def("uuid", &SnapshotToolCut::getUUID,
             "get the UUID of this snapshot.\n"
             "\returns a string, the UUID")
        .def("date", &SnapshotToolCut::getDate,
             "get the date this snapshot was taken.\n"
             "returns a string, the date of this snapshot")
        .def("stored", &SnapshotToolCut::stored,
             "get the amount of data referenced on the backend for this snapshot.\n"
             "@returns amount of data referenced in the backend")
        .def("inBackend",
             &SnapshotToolCut::inBackend,
             "Returns whether this snapshot is in the backend.\n"
             "@returns a boolean")
        .def("forEach", &SnapshotToolCut::forEach, (args( "onTLogName")),
             "Calls the object passed for each tlog.\n"
             "@param object, a callable python object that takes a tlog as argument.\n");

    class_<TLogToolCut>("TLog",
                        "This class represent info about a tlog as represented in the Snapshots file.\n"
                        "To read TLogs use TLogReader",
                        no_init)
        .def("__str__", &TLogToolCut::str)
        .def("__repr__", &TLogToolCut::repr)
        .def("writtenToBackend", &TLogToolCut::writtenToBackend,
             "true is the tlog is written to backend.\n"
             "@returns true if the tlog is written to backend")
        .def("name", &TLogToolCut::getName,
             "name of this tlog.\n"
             "@returns the tlog name, a string")
        .def("uuid", &TLogToolCut::getUUID,
             "The stringified UUID of this tlog.\n"
             "@returns the uuid of the tlog as a string")
        .def("isTLogString", &TLogToolCut::isTLogString,
             (args("tlogName")),
             "Checks if the string is the correct tlog format.\n"
             "@param tlogName a string\n"
             "@returns true if the string can be interpreted as a tlog name\n")
        .staticmethod("isTLogString")
        .def("getUUIDFromTLogName", &TLogToolCut::getUUIDFromTLogName,
             (args("tlogName")),
             "Return the UUID from a tlog name as a string.\n"
             "@param the tlog name, a string\n"
             "@returns the uuid of this tlog name as a string\n")
        .staticmethod("getUUIDFromTLogName");


    class_<VolumeInfo,
           boost::noncopyable>("VolumeInfo",
                               "Information about a volume",
                               init<boost::python::object&,
                               const std::string&>("Creation from a backend and a namespace.\n"
                                                   "@param a backend\n"
                                                   "@param a string, the namespace to connect to"))
        .def(init<const std::string&,
             const std::string&>("Creation from a VolumeConfig file and a FailOverCacheConfig file (as they are stored on the backend).\n"
                                 "@param a string, the path to the VolumeConfig file\n"
                                 "@param a string, the path to the FailoverCacheConfig file"))
        .def("__str__", &VolumeInfo::str)
        .def("__repr__", &VolumeInfo::repr)
        .def("volumeID", &VolumeInfo::volumeID,
             "Get the volume ID.\n"
             "@returns a string, the volume id.")
        .def("volumeNamespace", &VolumeInfo::volumeNamespace,
             "Get the volume namespace.\n"
             "@returns a string, the volume namespace.")
        .def("parentNamespace", &VolumeInfo::parentNamespace,
             "Get the parent namespace, empty if the volume doesn't have a parent."
             "@returns a string, the parent namespace")
        .def("parentSnapshot", &VolumeInfo::parentSnapshot,
             "Get the parent snapshot this volume is cloned from, empty if the volume is not a clone.\n"
             "@returns a string, parent snapshot this volume is cloned from")
        .def("lbaSize", &VolumeInfo::lbaSize,
             "Get the lbasize of the volume.\n",
             "@returns a positive number, lba size of the volume")
        .def("lbaCount", &VolumeInfo::lbaCount,
             "Get the number of lba's in this volume.\n"
             "@returns a positive number, the number of lba's in this volume")
        .def("clusterMultiplier", &VolumeInfo::clusterMultiplier,
             "number of lbas in a cluster.\n"
             "@returns a positive number, the number of lbas in a cluster.")
        .def("scoMultiplier", &VolumeInfo::scoMultiplier,
             "number of clusters in a sco.\n"
             "@returns a positive number, number of clusters in a sco.\n")
        .def("readCacheEnabled", &VolumeInfo::readCacheEnabled,
             "Does this volume have the readcache enabled.\n"
             "@returns a boolean, True if the readcache is enabled")
        .def("size", &VolumeInfo::size,
             "gets the size of this volume.\n"
             "@return a positive number, the size of this volume")
        .def("role", &VolumeInfo::role,
             "Get the volume role -- not used\n"
             "@returns a VolumeRole enum value")
        .def("failOverCacheType", &VolumeInfo::failOverCacheType,
             "get Failover Cache type.\n"
             "@returns a FailOverCacheConfig enum value")
        .def("failOverCacheHost", &VolumeInfo::failOverCacheHost,
             "Get the Failover cache host or the empty string.\n"
             "@returns a string, the failover cache host")
        .def("failOverCachePort", &VolumeInfo::failOverCachePort,
             "Get the Failover cache port.\n"
             "@returns the failover cache port");

    class_<SnapshotPersistorToolCut>("SnapshotPersistor",
                                     init<boost::python::object&,
                                     const std::string&>("Creation from a backend and a namespace.\n"
                                                         "@param a backend\n"
                                                         "@param a string, the namespace to connect to"))
        .def(init<const std::string&>("Creation from a snapshots xml file.\n"
                                      "@param a string, path to the snapshots file"))
        .def("__str__", &SnapshotPersistorToolCut::str)
        .def("__repr__", &SnapshotPersistorToolCut::repr)
        .def("forEach", &SnapshotPersistorToolCut::forEach,
             "Calls the object passed for each snapshot.\n"
             "@param object, a callable python object that takes a SnapShot.\n")
        .def("getSnapshotNum", &SnapshotPersistorToolCut::getSnapshotNum,
             "Get the number associated with this snapshot -- later snapshots have larger numbers.\n"
             "@param a string, the name of the snapshot\n"
             "@returns a number, the number of that snapshot")
        .def("getSnapshots", &SnapshotPersistorToolCut::getSnapshots,
              "Get all snapshots.\n"
              "@returns a list of snapshot objects")
        .def("getSnapshotName", &SnapshotPersistorToolCut::getSnapshotName,
             "get The name of a snapshot if you know it's number.\n"
             "@param a number, the number of the snapshot\n"
             "@returns a string, the name of the snapshot")
        .def("getSnapshotsTill", &SnapshotPersistorToolCut::getSnapshotsTill,
             "Shows all the snapshot up to a certain snapshot.\n"
             "@param a string, the snapshot name\n"
             "@param a bool, whether to include the given snapshot\n"
             "@returns a list of strings, the snapshots till the given snapshot")
        .def("getSnapshotsAfter", & SnapshotPersistorToolCut::getSnapshotsAfter,
             "Shows all the snapshots after a given snapshot.\n"
             "@param a string, the snapshot name\n"
             "@return a list of strings, the snapshots after the given snapshot")

        .def("currentStored", & SnapshotPersistorToolCut::currentStored,
             "Gives the amount of data stored beyond the last snapshot.\n"
             "@returns a number")
        .def("snapshotStored", & SnapshotPersistorToolCut::snapshotStored,
              "Gives the amount of data stored in this snapshot.\n"
              "@param a string, the snapshot name\n"
              "@returns a number")
        .def("stored", & SnapshotPersistorToolCut::stored,
             "Gives the total amount of data stored for the volume.\n"
             "@returns a number")

        .def("getAllTLogs", &SnapshotPersistorToolCut::getAllTLogs,
             "Returns a list of all TLogs.\n"
             "@param a bool, whether to get the current tlogs too\n"
             "@returns a list of strings, the tlogs")
        .def("getCurrentTLogs", &SnapshotPersistorToolCut::getCurrentTLogs,
             "returns a list of the current tlogs.\n"
             "@returns a list of the current tlogs")
        .def("getParentNamespace", &SnapshotPersistorToolCut::getParentNamespace,
             "Parent namespace of the volume, empty when no parent namespace.\n"
             "@returns a string, the parent namespace of this snapshots")
        .def("getParentSnapshot", &SnapshotPersistorToolCut::getParentSnapshot,
             "Parent snapshot of the volume, empty when no parent snapsshot.\n"
             "@returns a string, the parent snapshot of this volume")
        .def("getCurrentTLog", &SnapshotPersistorToolCut::getCurrentTLog,
             "gets the current tlogs -- not in a snapshot yet.\n"
             "@returns a list of strings, the tlogs in the current snap")
        .def("getTLogsInSnapshot", &SnapshotPersistorToolCut::getTLogsInSnapshot,
             "get the tlogs in a particular snapshot.\n",
             "@param a string, the snapshot name\n"
             "@returns a list of strings, the tlogs in the snapshot.")
        .def("getTLogsTillSnapshot", &SnapshotPersistorToolCut::getTLogsTillSnapshot,
             "get the tlogs until a particular snapshot.\n"
             "@param a string, the snapshot name\n"
             "@returns a list of strings, the tlogs until the snapshot\n")
        .def("getTLogsAfterSnapshot", &SnapshotPersistorToolCut::getTLogsAfterSnapshot,
             "get the tlogs after a particular snapshot.\n"
             "@param a string, the snapshot name\n"
             "@returns a list of strings, the tlogs after the snapshot")
        .def("snapshotExists", &SnapshotPersistorToolCut::snapshotExists,
             "check whether a snapshot exists.\n"
             "@param a string, the snapshot name\n"
             "@returns a bool, True if the snapshot exists.")
        .def("deleteSnapshot", &SnapshotPersistorToolCut::deleteSnapshot,
             "delete a snapshot.\n"
             "@param a string, the snapshot name\n")
        .def("trimToBackend", &SnapshotPersistorToolCut::trimToBackend,
             "remove all snapshot and tlogs that were not written to the backend.\n")
        .def("setTLogWrittenToBackend", &SnapshotPersistorToolCut::setTLogWrittenToBackend,
             "set a particular tlog written to the backend.\n"
             "@param a string, the tlog name")
        .def("isTLogWrittenToBackend", &SnapshotPersistorToolCut::isTLogWrittenToBackend,
             "check whether a particular tlog was written to the backend.\n"
             "@param a string, the tlog name\n"
             "@returns a bool, True if the tlog was written to the backend.")
        .def("getTLogsNotWrittenToBackend", &SnapshotPersistorToolCut::getTLogsNotWrittenToBackend,
             "get a list of the tlogs not written to backend.\n"
             "@returns a list of strings the tlogs not written to backend.")
        .def("isSnapshotScrubbed", &SnapshotPersistorToolCut::isSnapshotScrubbed,
             "check if a snapshot is scrubbed.\n",
             "@param a string, the snapshot name\n"
             "@returns a bool, True if the snapshot was scrubbed")
        .def("setSnapshotScrubbed", &SnapshotPersistorToolCut::setSnapshotScrubbed,
             "set a snapshot scrubbed.\n",
             "@param a string, the snapshot name\n"
             "@param a bool whether true is set the snapshot scrubbed")
        .def("saveToFile",
             &SnapshotPersistorToolCut::saveToFile,
             "save the snapshot persistor to a file."
             "@param a string, the path to save the snapshot persistor to")
        .def("snip", &SnapshotPersistorToolCut::snip,
             "remove all tlogs after the given one.\n"
             "@param a string, the tlog name")
        .def("getScrubbingWork",
             &SnapshotPersistorToolCut::getScrubbingWork,
             GetScrubbingWorkOverloads(args("startSnapshot",
                                            "endSnapshot"),
                                       "@param startSnapshot a string or None, the name of the start snapshot (non-inclusive)\n"
                                       "@param endSnapshot a string or None, the name of the end snapshot (inclusive)\n"
                                       "@result a list of strings, the snapshots that can be scrubbed"))
        ;

    class_<EntryToolCut>("Entry",
                         "Entries are what TLogs are made from",
                         no_init)
        .def("__str__", &EntryToolCut::str)
        .def("__repr__", &EntryToolCut::repr)
        .def("type", &EntryToolCut::type,
             "Get the type of entry.\n"
             "@returns ToolCut.EntryType")
        .def("checksum", &EntryToolCut::getCheckSum,
             "Get the checksum, only meaningfull if type is TLogCRC, or SCOCRC.\n"
             "@returns a string, the checksum")
        .def("clusterAddress",&EntryToolCut::clusterAddress,
             "Get the clusterAddress, only meaningfull is the type is CLoc.\n"
             "@returns a number, the clusteraddress")
        .def("clusterLocation", &EntryToolCut::clusterLocation,
             "Get the clusterLoc, only meaningful if the type is CLoc.\n"
             "@returns a ClusterLocation")
        .def("replace", &SnapshotPersistorToolCut::replace,
             "replace all tlog in a snapshot by new ones.\n"
             "@param a list of TLogs, the tlogs to replace in the snapshot\n"
             "@param a string, the name of the snapshots to replace the tlogs in\n"
             "@param a number, the size of the backend of the data referenced in the tlogs");
        ;

    class_<TLogReaderToolCut,
           boost::noncopyable>("TLogReader",
                               "Process the contents of a TLog",
                               init<const std::string&>("Construct based on a local tlog file.\n"))
        .def(init<boost::python::object&,
             const std::string&,
             const std::string&,
             const std::string&>("Constructor from a backend. the file the tlog is written to won't be removed.\n"
                                 "@param a  backend\n"
                                 "@param a string, the namespace to connect to\n"
                                 "@param a string, the tlog name\n"
                                 "@param a string, the path to write the tlog too"))
        .def("__str__", &TLogReaderToolCut::str)
        .def("__repr__", &TLogReaderToolCut::repr)
        .def("SCONames", &TLogReaderToolCut::SCONames,
             "List all SCONames in TLog.\n"
             "@returns a list of strings, the SCONames in the tlog")
        .def("rewind", &TLogReaderToolCut::rewind,
             "Go back to the beginning of the TLog.\n")
        .def("isTLogString", &TLogReaderToolCut::isTLog,
             "Check whether a name represents a tlog"
             "@param a string, the presumed TLog name.\n"
             "@returns a bool, True if the string is a tlog name")
        .staticmethod("isTLogString")
        .def("forEach",
             &TLogReaderToolCut::forEach,
             (args("onClusterEntry")=TLogReaderToolCut::object_,
              args("onTLogCRC")=TLogReaderToolCut::object_,
              args("onSCOCRC")=TLogReaderToolCut::object_,
              args("onSyncTC")=TLogReaderToolCut::object_),
             "Call a function for each entry in the TLog (if it is passed).\n"
             "@param onClusterEntry, a callable taking a number (cluster addres) and a ToolCut.ClusterLocation, \n"
             "called for each clusterEntry\n"
             "@param onTLogCRC, a callable taking a Checksum string, called for each TLogCRC\n"
             "@param onSCOCRC, a callable taking a Checksum string, called for each SCOCRC\n"
             "@param onSyncTC, a callable taking no argumentscalled for each SynTC\n")
        .def("forEachEntry",
             &TLogReaderToolCut::forEachEntry,
             "Calls the function passed as argument with for each entry in the tlog.\n"
             "@param a function taking a ToolCut.Entry as parameter");

    class_<SCOAccessDataInfo, boost::noncopyable>("SCOAccessDataInfo",
                                                  "Get SCOAccessData content",
                                                  init<std::string>())
        .def(init<const std::string&,
             boost::python::object&,
             const std::string&>())
        .def("namespace", &SCOAccessDataInfo::getNamespace,
             "Get namespace this SCOAccessData applies to")
        .def("readActivity", &SCOAccessDataInfo::getReadActivity,
             "Get read activity value of this SCOAccessData")
        .def("entries", &SCOAccessDataInfo::getEntries,
             "Get a list of SCOs and their associated access frequencies from this SCOAccessData");

    class_<ScrubbingResultToolCut,
        boost::noncopyable>("ScrubbingResult",
                            "Shows the information related to a scrubbing run",
                            init<std::string>())
        .def(init<boost::python::object&,
             const std::string&,
             const std::string&>("Takes a backend, a namespace and a filename.\n"
                                 "@param a backend\n"
                                 "@param a string, the namespace to get the scrubbing from\n"
                                 "@param a string, the scrubbing result object name"))
        .def("snapshotName",
             &ScrubbingResultToolCut::getSnapshotName,
             "Name of the snapshot that was scrubbed")
        .def("tlogsIn",
             &ScrubbingResultToolCut::getTLogsIn,
             "TLogs that were scrubbed")
        .def("tlogsOut",
             &ScrubbingResultToolCut::getTLogsOut,
             "Tlogs that resulted from the scrub")
        .def("relocations",
             &ScrubbingResultToolCut::getRelocations,
             "Relocations resulting from the scrub")
        .def("numRelocations",
             &ScrubbingResultToolCut::getNumberOfRelocations,
             "Number of relocations resulting from the scrub")
        .def("scosToBeDeleted",
             &ScrubbingResultToolCut::getSCONamesToBeDeleted,
             "Scos to be deleted")
        .def("prefetch",
             &ScrubbingResultToolCut::getPrefetch,
             "Prefetch data for this scrub")
        .def("newScos",
             &ScrubbingResultToolCut::getNewSCONames,
             "New scos created as a result of this scrub")
        .def("referenced",
             &ScrubbingResultToolCut::getReferenced,
             "Amount Data referenced on the backend in cluster units")
        .def("stored",
             &ScrubbingResultToolCut::getStored,
             "Amount of data stored on the backend in cluster units.")
        .def("version",
             &ScrubbingResultToolCut::getVersion,
             "Version of the scrubbing data")
        .def("isScrubbingResultString",
             &ScrubbingResultToolCut::isScrubbingResultString,
             "Check whether a string is a ScrubbingResult.\b"
             "@param a string, the name to be checked\n"
             "@returns a bool, whether the string is a scrubbing_result")
        .staticmethod("isScrubbingResultString");


    class_<MetadataStoreToolCut, boost::noncopyable>("MetadataStore",
                                                     "Interface to the tokyo cabinet metadastore",
                                                     init<std::string>())
        .def("readCluster", &MetadataStoreToolCut::readCluster,
             "Read the sco location of a particular cluster.\n"
             "@param a number, the clusteraddress of the cluster"
             "@return a ToolCut.ClusterLocation, the location of that cluster")
        .def("forEach", & MetadataStoreToolCut::forEach,
             "Apply a function to each entry in the metadatastore");

    class_<ClusterLocationToolCut>("ClusterLocation",
                                   "Interface to clusterlocation i.e. a sco and an offset in that sco",
                                   init<const std::string&>("Construct from a string (XX_XXXXXXXX_XX:XXXX)"))
        .def("__str__", &ClusterLocationToolCut::str)
        .def("__repr__", &ClusterLocationToolCut::repr)
        .def("offset", &ClusterLocationToolCut::offset,
             "Get the offset in the sco")
        .def("version", &ClusterLocationToolCut::version,
             "Get the version of the sco")
        .def("cloneID", &ClusterLocationToolCut::cloneID,
             "Get the cloneID of the sco")
        .def("number", &ClusterLocationToolCut::number,
             "Get the number of the sco")
        .def("str", &ClusterLocationToolCut::str,
             "Get the stringified form of the clusterlocation")
        .def("sco", &ClusterLocationToolCut::sco,
             return_value_policy<boost::python::manage_new_object>(),
             "Get the SCO associated with this clusterlocation")
        .def("isClusterLocationString", &ClusterLocationToolCut::isClusterLocationString,
             "Test whether the string is the standard representation of a clusterlocation")
        .staticmethod("isClusterLocationString");

    class_<SCOToolCut>("SCO",
                       "Interface to SCO which is a a container that is a storage unit",
                       init<const std::string&>("Construct on the basis of a string (XX_XXXXXXXX_XX"))
        .def("__str__", &SCOToolCut::str)
        .def("__repr__", &SCOToolCut::str)
        .def("version", &SCOToolCut::version,
             "Get the version of the sco")
        .def("cloneID", &SCOToolCut::cloneID,
             "Get the cloneID of the sco")
        .def("number", &SCOToolCut::number,
             "Get the number of the sco")
        .def("str", &SCOToolCut::str,
             "Get the stringified form of the SCO")
        .def("asBool", &SCOToolCut::asBool,
             "SCO 0 is never used as a storage unit")
        .def("isSCOString", &SCOToolCut::isSCOString,
             "test whether a string can be interpreted as a sco string")
        .staticmethod("isSCOString");
}

// Local Variables: **
// mode: c++ **
// End: **
