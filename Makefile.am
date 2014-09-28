SUBDIRS = \
	src \
	po \
	doc

EXTRA_DIST = \
	HACKING \
	MAINTAINERS \
	rationales.txt

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.h.in~ \
	$(srcdir)/config.sub \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	$(srcdir)/configure

GITIGNOREFILES = \
	$(PACKAGE)-\*.tar.{gz,bz2,xz}

-include $(top_srcdir)/git.mk
