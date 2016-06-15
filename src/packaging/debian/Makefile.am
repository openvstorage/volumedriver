AC_LOCAL_AMFLAGS = -I ../../m4

SUBDIRS = . \
	libovsvolumedriver \
	libovsvolumedriver-dev \
	volumedriver-base \
	volumedriver-server \
	volumedriver-pitreplication \
	volumedriver-test

noinst_data = add_installdir.sh

clean-local:
	-rm -rf *.deb *.dsc *.tar.gz

install-data-local:
	rm -rf $(prefix)/bin $(prefix)/lib $(prefix)/debian
	cd ../.. && make install

