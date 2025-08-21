NULL =

SUBDIRS = tools

bin_PROGRAMS = metacity

noinst_PROGRAMS = \
	testasyncgetprop \
	testboxes \
	$(NULL)

AM_CPPFLAGS = \
	-DMETACITY_LIBEXECDIR=\""$(libexecdir)"\" \
	-DMETACITY_LOCALEDIR=\""$(localedir)"\" \
	-DG_LOG_DOMAIN=\"metacity\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DSN_API_NOT_YET_FROZEN=1 \
	-I$(srcdir)/core \
	-I$(srcdir)/include \
	-I$(builddir)/core \
	-I$(top_srcdir) \
	$(NULL)

metacity_CFLAGS = \
	$(METACITY_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

metacity_SOURCES = \
	compositor/meta-compositor.c \
	compositor/meta-compositor-external.c \
	compositor/meta-compositor-external.h \
	compositor/meta-compositor-none.c \
	compositor/meta-compositor-none.h \
	compositor/meta-compositor-private.h \
	compositor/meta-compositor-xpresent.c \
	compositor/meta-compositor-xpresent.h \
	compositor/meta-compositor-xrender.c \
	compositor/meta-compositor-xrender.h \
	compositor/meta-shadow-xrender.c \
	compositor/meta-shadow-xrender.h \
	compositor/meta-surface.c \
	compositor/meta-surface.h \
	compositor/meta-surface-private.h \
	compositor/meta-surface-xrender.c \
	compositor/meta-surface-xrender.h \
	core/above-tab-keycode.c \
	core/async-getprop.c \
	core/async-getprop.h \
	core/atomnames.h \
	core/bell.c \
	core/bell.h \
	core/boxes.c \
	core/constraints.c \
	core/constraints.h \
	core/core.c \
	core/delete.c \
	core/display.c \
	core/display-private.h \
	core/edge-resistance.c \
	core/edge-resistance.h \
	core/effects.c \
	core/effects.h \
	core/errors.c \
	core/frame.c \
	core/frame-private.h \
	core/group.c \
	core/group.h \
	core/group-private.h \
	core/group-props.c \
	core/group-props.h \
	core/iconcache.c \
	core/iconcache.h \
	core/keybindings.c \
	core/keybindings.h \
	core/main.c \
	core/metacity-Xatomtype.h \
	core/place.c \
	core/place.h \
	core/prefs.c \
	core/screen.c \
	core/screen-private.h \
	core/session.c \
	core/session.h \
	core/stack.c \
	core/stack.h \
	core/stack-tracker.c \
	core/stack-tracker.h \
	core/util.c \
	core/window.c \
	core/window-private.h \
	core/window-props.c \
	core/window-props.h \
	core/workspace.c \
	core/workspace.h \
	core/xprops.c \
	include/boxes.h \
	include/common.h \
	include/core.h \
	include/display.h \
	include/errors.h \
	include/frame.h \
	include/main.h \
	include/meta-compositor.h \
	include/prefs.h \
	include/resizepopup.h \
	include/screen.h \
	include/tabpopup.h \
	include/tile-preview.h \
	include/types.h \
	include/ui.h \
	include/window.h \
	include/xprops.h \
	ui/draw-workspace.c \
	ui/draw-workspace.h \
	ui/fixedtip.c \
	ui/fixedtip.h \
	ui/frames.c \
	ui/frames.h \
	ui/menu.c \
	ui/menu.h \
	ui/metaaccellabel.c \
	ui/metaaccellabel.h \
	ui/meta-request-csd.c \
	ui/meta-request-csd.h \
	ui/meta-tooltip.c \
	ui/meta-tooltip.h \
	ui/resizepopup.c \
	ui/select-image.c \
	ui/select-image.h \
	ui/select-workspace.c \
	ui/select-workspace.h \
	ui/tabpopup.c \
	ui/tile-preview.c \
	ui/ui.c \
	ui/ui-private.h \
	$(BUILT_SOURCES) \
	$(NULL)

if HAVE_VULKAN
metacity_SOURCES += \
	compositor/meta-compositor-vulkan.c \
	compositor/meta-compositor-vulkan.h \
	compositor/meta-surface-vulkan.c \
	compositor/meta-surface-vulkan.h \
	$(NULL)
endif

metacity_LDADD = \
	$(top_builddir)/libmetacity/libmetacity.la \
	$(METACITY_LIBS) \
	$(NULL)

metacity_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

testasyncgetprop_SOURCES = \
	core/async-getprop.c \
	core/async-getprop.h \
	core/testasyncgetprop.c \
	$(NULL)

testasyncgetprop_LDADD = \
	$(METACITY_LIBS) \
	$(NULL)

testasyncgetprop_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

testboxes_CFLAGS = \
	$(METACITY_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

testboxes_SOURCES = \
	core/util.c \
	core/boxes.c \
	core/testboxes.c \
	include/boxes.h \
	include/util.h \
	$(NULL)

testboxes_LDADD = \
	$(METACITY_LIBS) \
	$(NULL)

testboxes_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

testasyncgetprop_CFLAGS = \
	$(METACITY_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/core/window-private.h \
	$(srcdir)/include/meta-compositor.h \
	$(NULL)

core/meta-enum-types.c: core/meta-enum-types.c.in $(ENUM_TYPES) Makefile.am
	$(MKDIR_P) core
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/core/meta-enum-types.c.in $(ENUM_TYPES) > \
		meta-enum-types.c.tmp && mv meta-enum-types.c.tmp core/meta-enum-types.c

core/meta-enum-types.h: core/meta-enum-types.h.in $(ENUM_TYPES) Makefile.am
	$(MKDIR_P) core
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/core/meta-enum-types.h.in $(ENUM_TYPES) > \
		meta-enum-types.h.tmp && mv meta-enum-types.h.tmp core/meta-enum-types.h

BUILT_SOURCES = \
	core/meta-enum-types.c \
	core/meta-enum-types.h \
	$(NULL)

EXTRA_DIST = \
	core/meta-enum-types.c.in \
	core/meta-enum-types.h.in \
	$(NULL)

-include $(top_srcdir)/git.mk
