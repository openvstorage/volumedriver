ACLOCAL_AMFLAGS = -I ../../../m4
bin_PROGRAMS = distributed_transaction_log_helper_test

distributed_transaction_log_helper_test_CXXFLAGS = $(BUILDTOOLS_CFLAGS)
distributed_transaction_log_helper_test_CPPFLAGS = -I@abs_top_srcdir@/..
distributed_transaction_log_helper_test_LDADD = \
	../../libvolumedriverfs.la \
	../../../volumedriver/libvolumedriver.la \
	../../../xmlrpc++0.7/libxmlrpc.la \
	../../../backend/libbackend.la \
	../../../youtils/libyoutils.la \
	 $(BUILDTOOLS_LIBS)

distributed_transaction_log_helper_test_LDFLAGS = -Wl,--as-needed
distributed_transaction_log_helper_test_SOURCES = \
	failovercachehelpertest.cpp
