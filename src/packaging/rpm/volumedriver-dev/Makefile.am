AC_LOCAL_AMFLAGS = -I ../../../m4

BASE_NAME=volumedriver-dev

RPM_LOCATION = $(prefix)/rpm
ARTEFACT = ../$(BASE_NAME)_@VD_MAJOR_VERSION@.@VD_MINOR_VERSION@.@VD_PATCH_VERSION@.@VD_EXTRA_VERSION@-1.rpm

.PHONY: SPECS/$(BASE_NAME).spec $(ARTEFACT)

noinst_DATA = SPECS/$(BASE_NAME).spec

SPECS/$(BASE_NAME).spec:

clean-local:
	-rm -rf ../$(BASE_NAME)_*

$(ARTEFACT):
	mkdir -p BUILDROOT
	ln -nsf . ../../../../usr
	awk '/^\// { printf("."); print; }' SPECS/$(BASE_NAME).spec > file.list
	d="${PWD}/BUILDROOT"; (cd $(prefix); cpio -pdm "$$d") < file.list
	rpmbuild --define "_topdir ${PWD}" \
                 --define "buildroot ${PWD}/BUILDROOT/" \
                 -ba SPECS/$(BASE_NAME).spec
	mv RPMS/x86_64/$(BASE_NAME)-@VD_MAJOR_VERSION@.@VD_MINOR_VERSION@.@VD_PATCH_VERSION@.@VD_EXTRA_VERSION@-1.x86_64.rpm $(ARTEFACT)

install-data-local: $(ARTEFACT)
	$(INSTALL) -d $(RPM_LOCATION)
	$(INSTALL) $(ARTEFACT) $(RPM_LOCATION)
