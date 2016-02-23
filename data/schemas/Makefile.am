NULL =

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
	$(NULL)

-include $(top_srcdir)/git.mk
