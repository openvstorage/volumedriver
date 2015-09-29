ACLOCAL_AMFLAGS = -I m4
BUILDTOOLS_DIR=@buildtoolsdir@

SUBDIRS = \
	scripts \
	youtils \
	fawltyfs \
	simple-nfs \
	backend \
	backend-python \
	xmlrpc++0.7 \
	VolumeTester \
	python-base \
	volumedriver \
	filedriver \
	filesystem \
	filesystem-python-client \
	ganesha-filesystem

docs: Doxyfile
	$(BUILDTOOLS_DIR)/bin/doxygen Doxyfile

# persistent_cache
#
#

# DONT RUN THE PACKAGING HERE. THE PACKAGING CLEANS THE INSTALL DIRS AND THEN RUNS MAKE INSTALL AGAIN.

#.PHONY: youtils_only backend_only persistent_cache_only python-base_only VolumeTester_only volumedriver_only xmlrpc++0.7_only kernlel_only dameon_only packaging_only

%.only : %
	$(MAKE) -C $*

# youtils_only:
# 	make -C youtils

# backend_only:
# 	make -C backend

# persistent_cache_only:
# 	make -C persistent_cache

# python-base_only :
# 	make -C python-base

# VolumeTester_only:
# 	make -C VolumeTester

# volumedriver_only:
# 	 make -C volumedriver

# xmlrpc++0.7_only:
# 	make -C xmlrpc++0.7

# kernel_only:
# 	make -C kernel

# daemon_only:
# 	make -C daemon

# packaging_only:
# 	make -C packaging
