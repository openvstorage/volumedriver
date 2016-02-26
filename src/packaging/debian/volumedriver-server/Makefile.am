AC_LOCAL_AMFLAGS = -I ../../../m4

if USE_MD5_HASH
BASE_NAME=volumedriver-server
else
BASE_NAME=volumedriver-no-dedup-server
endif

DEBIAN_LOCATION = $(prefix)/debian
ARTEFACT = ../$(BASE_NAME)_@VD_MAJOR_VERSION@.@VD_MINOR_VERSION@.@VD_PATCH_VERSION@-@VD_EXTRA_VERSION@_amd64.deb
ARTEFACT_DBGSYM = ../$(BASE_NAME)-dbgsym_@VD_MAJOR_VERSION@.@VD_MINOR_VERSION@.@VD_PATCH_VERSION@-@VD_EXTRA_VERSION@_amd64.ddeb

noinst_DATA = debian/control-template \
	debian/source/format \
	debian/copyright \
	debian/changelog \
	debian/rules \
	debian/compat \
	debian/$(BASE_NAME).docs \
	debian/$(BASE_NAME)-head.install \
	debian/$(BASE_NAME)-head.postinst \
	add-python-dirs.sh

.PHONY : debian/control debian/$(BASE_NAME).install debian/$(BASE_NAME).postinst $(ARTEFACT)

debian/control : debian/control-template
if USE_MD5_HASH
	cp debian/control-template debian/control
else
	cp debian/control-template-no-dedup debian/control
endif
	../update-buildinfo.sh

debian/$(BASE_NAME).install: debian/$(BASE_NAME)-head.install
	cp debian/$(BASE_NAME)-head.install debian/$(BASE_NAME).install
	./add-python-dirs.sh

debian/$(BASE_NAME).postinst: debian/$(BASE_NAME)-head.postinst
	cp debian/$(BASE_NAME)-head.postinst debian/$(BASE_NAME).postinst

clean-local:
	-rm -rf ../$(BASE_NAME)_*


$(ARTEFACT) : debian/control debian/$(BASE_NAME).install debian/$(BASE_NAME).postinst
	dpkg-buildpackage -us -uc

install-data-local: $(ARTEFACT)
	$(INSTALL) -d $(DEBIAN_LOCATION)
	$(INSTALL) $(ARTEFACT) $(DEBIAN_LOCATION)
	$(INSTALL) $(ARTEFACT_DBGSYM) $(DEBIAN_LOCATION)
