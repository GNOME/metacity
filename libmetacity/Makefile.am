NULL =

noinst_LTLIBRARIES = libmetacity.la

libmetacity_la_SOURCES = \
	meta-color.c \
	meta-color.h \
	meta-hsla.c \
	meta-hsla.h \
	meta-theme.c \
	meta-theme.h \
	$(NULL)

libmetacity_la_CPPFLAGS = \
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

-include $(top_srcdir)/git.mk
