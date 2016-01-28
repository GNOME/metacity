NULL =

lib_LTLIBRARIES = libmetacity.la

libmetacity_la_SOURCES = \
	meta-color.c \
	meta-color.h \
	meta-color-private.h \
	meta-color-spec.c \
	meta-color-spec.h \
	meta-frame-borders.c \
	meta-frame-borders.h \
	meta-gradient.c \
	meta-gradient.h \
	meta-gradient-private.h \
	meta-gradient-spec.c \
	meta-gradient-spec.h \
	meta-hsla.c \
	meta-hsla-private.h \
	meta-theme.c \
	meta-theme.h \
	meta-theme-gtk.c \
	meta-theme-gtk.h \
	meta-theme-impl.c \
	meta-theme-impl.h \
	meta-theme-metacity.c \
	meta-theme-metacity.h \
	$(NULL)

libmetacity_la_CPPFLAGS = \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libmetacity_la_CFLAGS = \
	$(LIBMETACITY_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libmetacity_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libmetacity_la_LIBADD = \
	$(LIBMETACITY_LIBS) \
	$(NULL)

libmetacity_includedir = $(includedir)/metacity/libmetacity
libmetacity_include_HEADERS = \
	meta-color.h \
	meta-color-spec.h \
	meta-frame-borders.h \
	meta-gradient.h \
	meta-gradient-spec.h \
	meta-theme.h \
	meta-theme-gtk.h \
	meta-theme-impl.h \
	meta-theme-metacity.h \
	$(NULL)

-include $(top_srcdir)/git.mk
