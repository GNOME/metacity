NULL =

keybindingsdir = @GNOME_KEYBINDINGS_KEYSDIR@
keybindings_DATA = \
	50-metacity-navigation.xml \
	50-metacity-system.xml \
	50-metacity-windows.xml \
	$(NULL)

keybindings.pot: $(keybindings_DATA) keybindings.its
	$(AM_V_GEN) $(ITSTOOL) -i keybindings.its -o $@ $(keybindings_DATA)

EXTRA_DIST = \
	$(keybindings_DATA) \
	keybindings.its \
	keybindings.pot \
	$(NULL)

-include $(top_srcdir)/git.mk
