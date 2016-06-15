AC_LOCAL_AMFLAGS = -I ../../m4

SUBDIRS = . \
	libovsvolumedriver \
	libovsvolumedriver-devel \
	volumedriver-base \
	volumedriver-pitreplication \
	volumedriver-server \
	volumedriver-test

noinst_data = add_installdir.sh

clean-local:
	-rm -rf *.rpm

install-data-local:
	rm -rf $(prefix)/bin $(prefix)/lib $(prefix)/rpm
	cd ../.. && make install

