NULL =

desktop_in_files = \
  metacity.desktop.in \
  $(NULL)

desktopdir=$(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t \
		-d $(top_srcdir)/po && mv $@-t $@

EXTRA_DIST = \
	$(desktop_in_files) \
	$(NULL)

CLEANFILES = \
	$(desktop_DATA) \
	$(NULL)

-include $(top_srcdir)/git.mk
