ACLOCAL_AMFLAGS = -I ../../m4

# AR: we can't use 'volumedriver' as subdirectory here, as that will
# confuse python (e.g. from volumedriver.X import Y will look in that subdir
# and not the correct place).
# Try to come up with something better.
volumedrivertestdir=@prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedrivertest/voldrv

# volumedrivertest_PYTHON = \
# 	__init__.py \
# 	backend_test.py \
# 	backupWithMonitoring.py \
# 	backup_locking_test.py \
# 	backup_test.py \
# 	lockedexecutable.py \
# 	testbackend.py \
# 	toolcut_test.py

# nobase_dist_volumedrivertest_DATA = \
# 	data/5c17a9f4f16348b8bbc/00_00000001_00 \
# 	data/5c17a9f4f16348b8bbc/00_00000002_00 \
# 	data/5c17a9f4f16348b8bbc/00_00000003_00 \
# 	data/5c17a9f4f16348b8bbc/00_00000004_00 \
# 	data/5c17a9f4f16348b8bbc/00_00000005_00 \
# 	data/5c17a9f4f16348b8bbc/00_00000006_00 \
# 	data/5c17a9f4f16348b8bbc/00_00000007_00 \
# 	data/5c17a9f4f16348b8bbc/00_00000008_00 \
# 	data/5c17a9f4f16348b8bbc/00_00000009_00 \
# 	data/5c17a9f4f16348b8bbc/failovercache_configuration \
# 	data/5c17a9f4f16348b8bbc/replicationConfig \
# 	data/5c17a9f4f16348b8bbc/replicationMetaInitialized \
# 	data/5c17a9f4f16348b8bbc/sco_access_data \
# 	data/5c17a9f4f16348b8bbc/snapshots.xml \
# 	data/5c17a9f4f16348b8bbc/tlog_515b1f2e-2c89-458e-ac4c-ff3a7d7c9186 \
# 	data/5c17a9f4f16348b8bbc/tlog_587ed32d-f303-49f3-a0ae-9c3e688b308f \
# 	data/5c17a9f4f16348b8bbc/tlog_9b8b2e32-614a-4b89-9fcf-45b363cc61d2 \
# 	data/5c17a9f4f16348b8bbc/tlog_9f44a070-2b76-41e8-a6ef-cdb876037829 \
# 	data/5c17a9f4f16348b8bbc/tlog_cdb59ec2-cc86-4550-bc39-9f7a127564db \
# 	data/5c17a9f4f16348b8bbc/tlog_d1b88928-68d5-4a56-a47b-a6c0ab2956a3 \
# 	data/5c17a9f4f16348b8bbc/tlog_f50541f0-7838-488c-9051-6a9bfa583e41 \
# 	data/5c17a9f4f16348b8bbc/volume_configuration \
# 	data/5c17a9f4f16348b8bbc_version3/00_00000001_00 \
# 	data/5c17a9f4f16348b8bbc_version3/00_00000002_00 \
# 	data/5c17a9f4f16348b8bbc_version3/00_00000003_00 \
# 	data/5c17a9f4f16348b8bbc_version3/00_00000004_00 \
# 	data/5c17a9f4f16348b8bbc_version3/00_00000005_00 \
# 	data/5c17a9f4f16348b8bbc_version3/00_00000006_00 \
# 	data/5c17a9f4f16348b8bbc_version3/00_00000007_00 \
# 	data/5c17a9f4f16348b8bbc_version3/00_00000008_00 \
# 	data/5c17a9f4f16348b8bbc_version3/00_00000009_00 \
# 	data/5c17a9f4f16348b8bbc_version3/failovercache_configuration \
# 	data/5c17a9f4f16348b8bbc_version3/replicationConfig \
# 	data/5c17a9f4f16348b8bbc_version3/replicationMetaInitialized \
# 	data/5c17a9f4f16348b8bbc_version3/sco_access_data \
# 	data/5c17a9f4f16348b8bbc_version3/snapshots.xml \
# 	data/5c17a9f4f16348b8bbc_version3/tlog_515b1f2e-2c89-458e-ac4c-ff3a7d7c9186 \
# 	data/5c17a9f4f16348b8bbc_version3/tlog_587ed32d-f303-49f3-a0ae-9c3e688b308f \
# 	data/5c17a9f4f16348b8bbc_version3/tlog_9b8b2e32-614a-4b89-9fcf-45b363cc61d2 \
# 	data/5c17a9f4f16348b8bbc_version3/tlog_9f44a070-2b76-41e8-a6ef-cdb876037829 \
# 	data/5c17a9f4f16348b8bbc_version3/tlog_cdb59ec2-cc86-4550-bc39-9f7a127564db \
# 	data/5c17a9f4f16348b8bbc_version3/tlog_d1b88928-68d5-4a56-a47b-a6c0ab2956a3 \
# 	data/5c17a9f4f16348b8bbc_version3/tlog_f50541f0-7838-488c-9051-6a9bfa583e41 \
# 	data/5c17a9f4f16348b8bbc_version3/volume_configuration \
# 	data/5c17a9f4f16348b8bbc_version4/00_00000001_00 \
# 	data/5c17a9f4f16348b8bbc_version4/00_00000002_00 \
# 	data/5c17a9f4f16348b8bbc_version4/00_00000003_00 \
# 	data/5c17a9f4f16348b8bbc_version4/00_00000004_00 \
# 	data/5c17a9f4f16348b8bbc_version4/00_00000005_00 \
# 	data/5c17a9f4f16348b8bbc_version4/00_00000006_00 \
# 	data/5c17a9f4f16348b8bbc_version4/00_00000007_00 \
# 	data/5c17a9f4f16348b8bbc_version4/00_00000008_00 \
# 	data/5c17a9f4f16348b8bbc_version4/00_00000009_00 \
# 	data/5c17a9f4f16348b8bbc_version4/failovercache_configuration \
# 	data/5c17a9f4f16348b8bbc_version4/replicationConfig \
# 	data/5c17a9f4f16348b8bbc_version4/replicationMetaInitialized \
# 	data/5c17a9f4f16348b8bbc_version4/sco_access_data \
# 	data/5c17a9f4f16348b8bbc_version4/snapshots.xml \
# 	data/5c17a9f4f16348b8bbc_version4/tlog_515b1f2e-2c89-458e-ac4c-ff3a7d7c9186 \
# 	data/5c17a9f4f16348b8bbc_version4/tlog_587ed32d-f303-49f3-a0ae-9c3e688b308f \
# 	data/5c17a9f4f16348b8bbc_version4/tlog_9b8b2e32-614a-4b89-9fcf-45b363cc61d2 \
# 	data/5c17a9f4f16348b8bbc_version4/tlog_9f44a070-2b76-41e8-a6ef-cdb876037829 \
# 	data/5c17a9f4f16348b8bbc_version4/tlog_cdb59ec2-cc86-4550-bc39-9f7a127564db \
# 	data/5c17a9f4f16348b8bbc_version4/tlog_d1b88928-68d5-4a56-a47b-a6c0ab2956a3 \
# 	data/5c17a9f4f16348b8bbc_version4/tlog_f50541f0-7838-488c-9051-6a9bfa583e41 \
# 	data/5c17a9f4f16348b8bbc_version4/volume_configuration \
# 	data/failovercache.cfg

noinst_DATA = pylint_test.sh python_test.sh

TESTS =
# pylint_test.sh python_test.sh
