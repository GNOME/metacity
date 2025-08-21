NULL =

service_in_files = metacity.service.in
service_files = $(service_in_files:.service.in=.service)

systemduserdir = $(libdir)/systemd/user
systemduser_DATA = $(service_files)

%.service: %.service.in
	$(AM_V_GEN) sed -e 's,[@]bindir[@],$(bindir),g' < $< > $@

EXTRA_DIST = \
	$(service_in_files) \
	$(NULL)

CLEANFILES = \
	$(service_files) \
	$(NULL)

-include $(top_srcdir)/git.mk
