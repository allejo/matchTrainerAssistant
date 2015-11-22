lib_LTLIBRARIES = matchTrainerAssistant.la

matchTrainerAssistant_la_SOURCES = matchTrainerAssistant.cpp
matchTrainerAssistant_la_CPPFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
matchTrainerAssistant_la_LDFLAGS = -module -avoid-version -shared
matchTrainerAssistant_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	README.matchTrainerAssistant.txt \
	matchTrainerAssistant.def \
	matchTrainerAssistant.sln \
	matchTrainerAssistant.vcxproj

MAINTAINERCLEANFILES =	\
	Makefile.in
