Bugdir=@prefix@/lib/python@PYTHON_VERSION@/dist-packages/volumedriver/bug

Bug_LTLIBRARIES = Bug1.la Bug2.la

Bug1_la_SOURCES = \
	Bug1.cpp

Bug1_la_LDFLAGS = -module -shared


Bug1_la_LIBADD = $(BUILDTOOLS_LIBS) -lpthread -lrt -lm

Bug1_la_CPPFLAGS = -I@abs_top_srcdir@/..
Bug1_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)


Bug2_la_SOURCES = \
	Bug2.cpp

Bug2_la_LDFLAGS = -module -shared

Bug2_la_LIBADD = $(BUILDTOOLS_LIBS) -lpthread -lrt -lm

Bug2_la_CPPFLAGS = -I@abs_top_srcdir@/..
Bug2_la_CXXFLAGS = $(BUILDTOOLS_CFLAGS)

Bug_PYTHON = __init__.py



