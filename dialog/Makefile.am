NULL =

libexec_PROGRAMS = metacity-dialog

metacity_dialog_SOURCES = \
	meta-delete-dialog.c \
	meta-delete-dialog.h \
	meta-dialog-main.c \
	meta-session-dialog.c \
	meta-session-dialog.h \
	$(BUILT_SOURCES) \
	$(NULL)

metacity_dialog_CPPFLAGS = \
	-DG_LOG_USE_STRUCTURED=1 \
	-DLOCALEDIR=\""$(localedir)"\" \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

metacity_dialog_CFLAGS = \
	$(METACITY_DIALOG_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

metacity_dialog_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

metacity_dialog_LDADD = \
	$(METACITY_DIALOG_LIBS) \
	$(NULL)

meta-dialog-resources.c: meta-dialog.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/meta-dialog.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name meta_dialog $<

meta-dialog-resources.h: meta-dialog.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/meta-dialog.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name meta_dialog $<


BUILT_SOURCES = \
	meta-dialog-resources.c \
	meta-dialog-resources.h \
	$(NULL)

EXTRA_DIST = \
	meta-dialog.gresource.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
