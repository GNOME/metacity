NULL =

lib_LTLIBRARIES = libmetacity.la

libmetacity_la_SOURCES = \
	meta-button.c \
	meta-button.h \
	meta-button-layout.c \
	meta-button-layout-private.h \
	meta-button-private.h \
	meta-color.c \
	meta-color.h \
	meta-color-private.h \
	meta-color-spec.c \
	meta-color-spec-private.h \
	meta-css-provider.c \
	meta-css-provider-private.h \
	meta-rectangle-private.h \
	meta-draw-op.c \
	meta-draw-op-private.h \
	meta-draw-spec.c \
	meta-draw-spec-private.h \
	meta-frame-borders.c \
	meta-frame-borders.h \
	meta-frame-enums.h \
	meta-frame-layout.c \
	meta-frame-layout-private.h \
	meta-frame-style.c \
	meta-frame-style-private.h \
	meta-gradient-spec.c \
	meta-gradient-spec-private.h \
	meta-hsla.c \
	meta-hsla-private.h \
	meta-style-info.c \
	meta-style-info-private.h \
	meta-theme.c \
	meta-theme.h \
	meta-theme-gtk.c \
	meta-theme-gtk-private.h \
	meta-theme-impl.c \
	meta-theme-impl-private.h \
	meta-theme-metacity.c \
	meta-theme-metacity-private.h \
	$(BUILT_SOURCES) \
	$(NULL)

libmetacity_la_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DG_LOG_DOMAIN=\"libmetacity\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libmetacity_la_CFLAGS = \
	$(LIBMETACITY_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libmetacity_la_LDFLAGS = \
	-version-info $(LT_VERSION_INFO) \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libmetacity_la_LIBADD = \
	$(LIBMETACITY_LIBS) \
	$(LIBM) \
	$(NULL)

libmetacity_includedir = $(includedir)/metacity/libmetacity
libmetacity_include_HEADERS = \
	meta-button.h \
	meta-color.h \
	meta-frame-borders.h \
	meta-frame-enums.h \
	meta-theme.h \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/meta-theme.h \
	$(NULL)

meta-enum-types.c: meta-enum-types.c.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/meta-enum-types.c.in $(ENUM_TYPES) > \
		meta-enum-types.c.tmp && mv meta-enum-types.c.tmp meta-enum-types.c

meta-enum-types.h: meta-enum-types.h.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/meta-enum-types.h.in $(ENUM_TYPES) > \
		meta-enum-types.h.tmp && mv meta-enum-types.h.tmp meta-enum-types.h

BUILT_SOURCES = \
	meta-enum-types.c \
	meta-enum-types.h \
	$(NULL)

EXTRA_DIST = \
	meta-enum-types.c.in \
	meta-enum-types.h.in \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
