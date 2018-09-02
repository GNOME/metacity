NULL =

SUBDIRS = tools

AM_CPPFLAGS = \
	-DMETACITY_LOCALEDIR=\""$(localedir)"\" \
	-DG_LOG_DOMAIN=\"metacity\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DSN_API_NOT_YET_FROZEN=1 \
	-I$(srcdir)/core \
	-I$(srcdir)/include \
	-I$(top_srcdir) \
	@METACITY_CFLAGS@ \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

metacity_SOURCES= 				\
	core/async-getprop.c			\
	core/async-getprop.h			\
	core/atomnames.h                        \
	core/bell.c				\
	core/bell.h				\
	core/boxes.c				\
	include/boxes.h				\
	core/util.c				\
	include/util.h				\
	include/common.h			\
	compositor/meta-compositor.c		\
	compositor/meta-compositor-none.c	\
	compositor/meta-compositor-none.h	\
	compositor/meta-compositor-private.h	\
	compositor/meta-compositor-xrender.c	\
	compositor/meta-compositor-xrender.h	\
	compositor/meta-compositor-vulkan.c	\
	compositor/meta-compositor-vulkan.h	\
	include/meta-compositor.h		\
	core/above-tab-keycode.c		\
	core/constraints.c			\
	core/constraints.h			\
	core/core.c				\
	core/delete.c				\
	core/display.c				\
	core/display-private.h			\
	include/display.h			\
	ui/draw-workspace.c			\
	ui/draw-workspace.h			\
	core/edge-resistance.c			\
	core/edge-resistance.h			\
	core/effects.c				\
	core/effects.h				\
	core/errors.c				\
	include/errors.h			\
	core/frame.c				\
	core/frame-private.h			\
	include/frame.h				\
	core/group-private.h			\
	core/group-props.c			\
	core/group-props.h			\
	core/group.c				\
	core/group.h				\
	core/iconcache.c			\
	core/iconcache.h			\
	core/keybindings.c			\
	core/keybindings.h			\
	core/main.c				\
	include/main.h				\
	core/metacity-Xatomtype.h		\
	core/place.c				\
	core/place.h				\
	core/prefs.c				\
	include/prefs.h				\
	core/screen.c				\
	core/screen-private.h			\
	include/screen.h			\
	include/types.h                         \
	core/session.c				\
	core/session.h				\
	core/stack.c				\
	core/stack.h				\
	core/stack-tracker.c			\
	core/stack-tracker.h			\
	core/window-props.c			\
	core/window-props.h			\
	core/window.c				\
	core/window-private.h			\
	include/window.h			\
	core/workspace.c			\
	core/workspace.h			\
	core/xprops.c				\
	include/xprops.h			\
	include/core.h				\
	include/ui.h				\
	ui/fixedtip.c				\
	ui/fixedtip.h				\
	ui/frames.c				\
	ui/frames.h				\
	ui/menu.c				\
	ui/menu.h				\
	ui/metaaccellabel.c			\
	ui/metaaccellabel.h			\
	ui/meta-tooltip.c \
	ui/meta-tooltip.h \
	ui/resizepopup.c			\
	include/resizepopup.h			\
	ui/tabpopup.c				\
	include/tabpopup.h			\
	ui/select-image.c			\
	ui/select-image.h			\
	ui/select-workspace.c			\
	ui/select-workspace.h			\
	ui/tile-preview.c			\
	include/tile-preview.h			\
	ui/ui.c					\
	ui/ui-private.h				\
	$(NULL)

bin_PROGRAMS=metacity

metacity_LDADD=@METACITY_LIBS@ $(top_builddir)/libmetacity/libmetacity.la

testboxes_SOURCES=include/util.h core/util.c include/boxes.h core/boxes.c core/testboxes.c
testasyncgetprop_SOURCES=core/async-getprop.h core/async-getprop.c core/testasyncgetprop.c

noinst_PROGRAMS=testboxes testasyncgetprop

testboxes_LDADD= @METACITY_LIBS@
testasyncgetprop_LDADD= @METACITY_LIBS@

-include $(top_srcdir)/git.mk
