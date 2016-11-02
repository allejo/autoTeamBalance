lib_LTLIBRARIES = autoTeamBalance.la

autoTeamBalance_la_SOURCES = autoTeamBalance.cpp
autoTeamBalance_la_CXXFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
autoTeamBalance_la_LDFLAGS = -module -avoid-version -shared
autoTeamBalance_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	CHANGELOG.md \
	LICENSE.md \
	README.md \
	autoTeamBalance.def

MAINTAINERCLEANFILES =	\
	Makefile.in
