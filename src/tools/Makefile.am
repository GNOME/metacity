NULL =

bin_PROGRAMS = metacity-message

metacity_message_CFLAGS = \
	-DMETACITY_ICON_DIR=\"$(pkgdatadir)/icons\" \
	-DMETACITY_LOCALEDIR=\""$(localedir)"\" \
	$(METACITY_MESSAGE_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

metacity_message_SOURCES = \
	metacity-message.c \
	$(NULL)

metacity_message_LDADD = \
	$(METACITY_MESSAGE_LIBS) \
	$(NULL)

EXTRA_DIST=$(icon_DATA)

-include $(top_srcdir)/git.mk
