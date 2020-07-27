NULL =

gsettings_ENUM_NAMESPACE = org.gnome.metacity
gsettings_ENUM_FILES = \
	$(top_srcdir)/src/include/meta-compositor.h \
	$(NULL)

gsettings_SCHEMAS = \
	org.gnome.metacity.gschema.xml \
	org.gnome.metacity.keybindings.gschema.xml \
	org.gnome.metacity.theme.gschema.xml \
	$(NULL)

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(gsettings_SCHEMAS) \
	$(NULL)

CLEANFILES = \
	*.gschema.valid \
	$(gsettings__enum_file) \
	$(NULL)

-include $(top_srcdir)/git.mk
