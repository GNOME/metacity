NULL =

bin_PROGRAMS = metacity-theme-viewer

metacity_theme_viewer_SOURCES = \
	theme-viewer-main.c \
	theme-viewer-window.c \
	theme-viewer-window.h \
	$(BUILT_SOURCES) \
	$(NULL)

metacity_theme_viewer_CPPFLAGS = \
	-DDATADIR=\""$(datadir)"\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DLOCALEDIR=\""$(localedir)"\" \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

metacity_theme_viewer_CFLAGS = \
	$(METACITY_THEME_VIEWER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

metacity_theme_viewer_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

metacity_theme_viewer_LDADD = \
	$(top_builddir)/libmetacity/libmetacity.la \
	$(METACITY_THEME_VIEWER_LIBS) \
	$(NULL)

theme-viewer-resources.c: theme-viewer.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/theme-viewer.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name theme_viewer $<

theme-viewer-resources.h: theme-viewer.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/theme-viewer.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name theme_viewer $<

BUILT_SOURCES = \
	theme-viewer-resources.c \
	theme-viewer-resources.h \
	$(NULL)

EXTRA_DIST = \
	theme-viewer.gresource.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
