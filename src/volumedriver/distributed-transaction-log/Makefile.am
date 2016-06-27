ACLOCAL_AMFLAGS = -I ../../m4
bin_PROGRAMS = distributed_transaction_log

distributed_transaction_log_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
distributed_transaction_log_CPPFLAGS = -I@abs_top_srcdir@/..
distributed_transaction_log_LDADD = \
	../libvolumedriver.la \
	../../backend/libbackend.la \
	../../youtils/libyoutils.la \
	 $(BUILDTOOLS_LIBS)

distributed_transaction_log_LDFLAGS = -Wl,--as-needed
distributed_transaction_log_SOURCES = \
	server.cpp \
	DtlServer.cpp

noinst_DATA = .gdbinit

SUBDIRS = . test
