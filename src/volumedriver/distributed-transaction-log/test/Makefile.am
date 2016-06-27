ACLOCAL_AMFLAGS=-I ../../../m4
bin_PROGRAMS=distributed_transaction_log_test distributed_transaction_log_client_perftest

distributed_transaction_log_test_CXXFLAGS=$(BUILDTOOLS_CFLAGS)
distributed_transaction_log_test_LDADD=../../libvolumedriver.la  \
	../../../backend/libbackend.la \
	../../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

distributed_transaction_log_test_LDFLAGS=-Wl,--as-needed
distributed_transaction_log_test_CPPFLAGS=-I@abs_top_srcdir@/..
distributed_transaction_log_test_SOURCES=\
	DtlTest.cpp \
	DtlEnvironment.cpp \
	../DtlServer.cpp \
	DtlTestMain.cpp


distributed_transaction_log_client_perftest_CXXFLAGS=$(BUILDTOOLS_CFLAGS)
distributed_transaction_log_client_perftest_LDADD=../../libvolumedriver.la  \
	../../../backend/libbackend.la \
	../../../youtils/libyoutils.la \
	$(BUILDTOOLS_LIBS)

distributed_transaction_log_client_perftest_LDFLAGS=-Wl,--as-needed
distributed_transaction_log_client_perftest_CPPFLAGS=-I@abs_top_srcdir@/..
distributed_transaction_log_client_perftest_SOURCES=\
	DtlClientPerfTest.cpp

noinst_DATA = distributed_transaction_log_tests.sh
TESTS = distributed_transaction_log_tests.sh
