dnl **************************************************************************
dnl Define version info
dnl **************************************************************************

m4_define([m_major_version], [3])
m4_define([m_minor_version], [58])
m4_define([m_micro_version], [1])
m4_define([m_version], [m_major_version.m_minor_version.m_micro_version])

dnl **************************************************************************
dnl Define bugzilla address
dnl **************************************************************************

m4_define([m_bugzilla_url],
          [https://gitlab.gnome.org/GNOME/metacity/-/issues])

dnl **************************************************************************
dnl Initialize autoconf
dnl **************************************************************************

AC_INIT([metacity], m_version, [m_bugzilla_url])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/core/display.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

dnl **************************************************************************
dnl Set release policy
dnl **************************************************************************

AX_IS_RELEASE([git-directory])

dnl **************************************************************************
dnl Enable debug
dnl **************************************************************************

AX_CHECK_ENABLE_DEBUG([yes])

dnl **************************************************************************
dnl Add warning flags
dnl **************************************************************************

AX_COMPILER_FLAGS([WARN_CFLAGS], [WARN_LDFLAGS])

dnl **************************************************************************
dnl Initialize automake
dnl **************************************************************************

AM_INIT_AUTOMAKE([1.14 no-dist-gzip dist-xz tar-ustar -Wno-portability subdir-objects])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

dnl **************************************************************************
dnl Initialize libtool
dnl **************************************************************************

LT_INIT([disable-static])
LT_LIB_M

dnl **************************************************************************
dnl Check for required programs
dnl **************************************************************************

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources])

dnl **************************************************************************
dnl Internationalization
dnl **************************************************************************

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.6])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.6])

GETTEXT_PACKAGE=metacity
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"],
                   [Name of default gettext domain])

dnl **************************************************************************
dnl Metacity version
dnl **************************************************************************

AC_DEFINE([METACITY_MAJOR_VERSION], [m_major_version], [Metacity major version])
AC_DEFINE([METACITY_MINOR_VERSION], [m_minor_version], [Metacity minor version])
AC_DEFINE([METACITY_MICRO_VERSION], [m_micro_version], [Metacity micro version])

dnl **************************************************************************
dnl Check for required packages
dnl **************************************************************************

GLIB_REQUIRED_VERSION=2.67.3
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED_VERSION=42.0
GTK_REQUIRED_VERSION=3.24.37
PANGO_REQUIRED_VERSION=1.2.0
STARTUP_NOTIFICATION_VERSION=0.7
XCOMPOSITE_REQUIRED_VERSION=0.3
XRES_REQUIRED_VERSION=1.2

PKG_CHECK_MODULES([LIBMETACITY], [
  glib-2.0 >= $GLIB_REQUIRED_VERSION
  gtk+-3.0 >= $GTK_REQUIRED_VERSION
])

PKG_CHECK_MODULES([METACITY_DIALOG], [
  glib-2.0 >= $GLIB_REQUIRED_VERSION
  gtk+-3.0 >= $GTK_REQUIRED_VERSION
  x11
])

PKG_CHECK_MODULES([METACITY_THEME_VIEWER], [
  glib-2.0 >= $GLIB_REQUIRED_VERSION
  gtk+-3.0 >= $GTK_REQUIRED_VERSION
])

dnl **************************************************************************
dnl Get GTK+ data prefix, needed by libmetacity
dnl **************************************************************************

GTK_DATA_PREFIX=`$PKG_CONFIG --variable=prefix gtk+-3.0`
AC_DEFINE_UNQUOTED([GTK_DATA_PREFIX], ["$GTK_DATA_PREFIX"], [GTK+ data prefix])

dnl **************************************************************************
dnl systemd user unit dir
dnl **************************************************************************

AS_IF([$PKG_CONFIG systemd], [
  SYSTEMD_USER_UNIT_DIR=$($PKG_CONFIG \
    --variable=systemduserunitdir \
    --define-variable=prefix=${prefix} \
    systemd)
], [
  SYSTEMD_USER_UNIT_DIR="${prefix}/lib/systemd/user"
])

AC_SUBST([SYSTEMD_USER_UNIT_DIR], [$SYSTEMD_USER_UNIT_DIR])

dnl **************************************************************************
dnl Library version information
dnl **************************************************************************

dnl Before making a release, the library version should be modified. Follow
dnl these instructions sequentially:
dnl 1. If the library source code has changed at all since the last update,
dnl    then increment lt_revision.
dnl 2. If any interfaces have been added, removed, or changed since the last
dnl    update, increment lt_current, and set lt_revision to 0.
dnl 3. If any interfaces have been added since the last public release, then
dnl    increment lt_age.
dnl 4. If any interfaces have been removed or changed since the last public
dnl    release, then set lt_age to 0.

m4_define([lt_current], [3])
m4_define([lt_revision], [0])
m4_define([lt_age], [0])
m4_define([lt_version_info], [lt_current:lt_revision:lt_age])

AC_SUBST([LT_VERSION_INFO], [lt_version_info])

dnl **************************************************************************

# Honor aclocal flags
AC_SUBST(ACLOCAL_AMFLAGS, "\${ACLOCAL_FLAGS}")

AC_PROG_CC
AC_PROG_MKDIR_P

#### Integer sizes 

AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(__int64)

## byte order
AC_C_BIGENDIAN

METACITY_PC_MODULES="gtk+-3.0 >= $GTK_REQUIRED_VERSION gio-2.0 >= $GLIB_REQUIRED_VERSION pango >= $PANGO_REQUIRED_VERSION gsettings-desktop-schemas >= $GSETTINGS_DESKTOP_SCHEMAS_REQUIRED_VERSION libstartup-notification-1.0 >= $STARTUP_NOTIFICATION_VERSION xcomposite >= $XCOMPOSITE_REQUIRED_VERSION xfixes xrender xdamage xres >= $XRES_REQUIRED_VERSION xpresent"

GLIB_GSETTINGS

AC_ARG_ENABLE(canberra,
  AS_HELP_STRING([--disable-canberra],
                 [disable use of canberra for event sounds, for embedded non-GNOME builds]),,
  enable_canberra=yes)

if test x$enable_canberra = xyes; then
    AC_DEFINE(HAVE_CANBERRA,1,[Build with canberra support])
    METACITY_PC_MODULES="$METACITY_PC_MODULES libcanberra-gtk3"
fi

AC_ARG_ENABLE(sm,
  AS_HELP_STRING([--disable-sm],
                 [disable metacity's session management support, for embedded/size-sensitive custom non-GNOME builds]),,
  enable_sm=auto)

AC_ARG_ENABLE(render,
  AS_HELP_STRING([--disable-render],
                 [disable metacity's use of the RENDER extension]),,
  enable_render=auto)

## here we get the flags we'll actually use
PKG_CHECK_MODULES(ALL, glib-2.0 >= $GLIB_REQUIRED_VERSION gthread-2.0 >= $GLIB_REQUIRED_VERSION)
PKG_CHECK_MODULES(METACITY_MESSAGE, gtk+-3.0 >= $GTK_REQUIRED_VERSION)

# Unconditionally use this dir to avoid a circular dep with gnomecc
GNOME_KEYBINDINGS_KEYSDIR="${datadir}/gnome-control-center/keybindings"
AC_SUBST(GNOME_KEYBINDINGS_KEYSDIR)

AC_MSG_CHECKING([Xcursor])
if $PKG_CONFIG xcursor; then
     have_xcursor=yes
  else
     have_xcursor=no
  fi
  AC_MSG_RESULT($have_xcursor)

if test x$have_xcursor = xyes; then
  echo "Building with Xcursor"
  METACITY_PC_MODULES="$METACITY_PC_MODULES xcursor"
  AC_DEFINE(HAVE_XCURSOR, , [Building with Xcursor support]) 
fi

AC_MSG_CHECKING([libgtop])
if $PKG_CONFIG libgtop-2.0; then
     have_gtop=yes
  else
     have_gtop=no
  fi
  AC_MSG_RESULT($have_gtop)

if test x$have_gtop = xyes; then
  echo "Building with libgtop"
  METACITY_PC_MODULES="$METACITY_PC_MODULES libgtop-2.0"
  AC_DEFINE(HAVE_GTOP, , [Building with libgtop]) 
fi

PKG_CHECK_MODULES(METACITY, $METACITY_PC_MODULES)

AC_PATH_XTRA

ALL_X_LIBS="-lX11 $X_EXTRA_LIBS"

# Check for Xinerama extension
metacity_save_cppflags="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $X_CFLAGS"

AC_ARG_ENABLE(xinerama,
  AS_HELP_STRING([--disable-xinerama],
                 [disable metacity's use of the Xinerama extension]),
  try_xinerama=$enable_xinerama,try_xinerama=yes)

have_xinerama=no
if test "${try_xinerama}" != no; then
    have_xinerama=yes
    AC_CHECK_LIB(Xinerama, XineramaQueryExtension,
        [AC_CHECK_HEADER(X11/extensions/Xinerama.h,
                         X_EXTRA_LIBS="-lXinerama $X_EXTRA_LIBS"
                         if test -z "`echo $ALL_X_LIBS | grep "\-lXext" 2> /dev/null`"; then
                             X_EXTRA_LIBS="-lXext $X_EXTRA_LIBS"
                         fi
                         AC_DEFINE(HAVE_XINERAMA, , [Have the Xinerama extension library]),
                         have_xinerama=no,
                         [#include <X11/Xlib.h>])],
        have_xinerama=no, -lXext $ALL_X_LIBS)
    AC_MSG_CHECKING([Xinerama])
    AC_MSG_RESULT($have_xinerama)
fi

CPPFLAGS="$metacity_save_cppflags"

found_xkb=no
AC_CHECK_LIB(X11, XkbQueryExtension,
               [AC_CHECK_HEADER(X11/XKBlib.h,
                                found_xkb=yes)],
	, $ALL_X_LIBS)

if test "x$found_xkb" = "xyes"; then
   AC_DEFINE(HAVE_XKB, , [Have keyboard extension library])
fi


RANDR_LIBS=
found_randr=no
AC_CHECK_LIB(Xrandr, XRRUpdateConfiguration,
               [AC_CHECK_HEADER(X11/extensions/Xrandr.h,
                                RANDR_LIBS=-lXrandr found_randr=yes,,
				[#include <X11/Xlib.h>])],
               , -lXrender -lXext $ALL_X_LIBS)

if test "x$found_randr" = "xyes"; then
   AC_DEFINE(HAVE_RANDR, , [Have the Xrandr extension library])
fi

METACITY_LIBS="$ALL_LIBS $METACITY_LIBS $RANDR_LIBS -lX11 -lXext $X_EXTRA_LIBS $LIBM"
METACITY_MESSAGE_LIBS="$METACITY_MESSAGE_LIBS -lX11 $X_EXTRA_LIBS"

found_sm=no
case "$METACITY_LIBS" in
 *-lSM*)
    found_sm=yes
    ;;
 *)
  AC_CHECK_LIB(SM, SmcSaveYourselfDone, 
               [AC_CHECK_HEADERS(X11/SM/SMlib.h,
                                METACITY_LIBS="-lSM -lICE $METACITY_LIBS" found_sm=yes)],
               , $METACITY_LIBS)
    ;;
esac

if test x$enable_sm = xno; then
   found_sm=no
fi

if test x$enable_sm = xyes; then
   if test "$found_sm" = "no"; then
      AC_MSG_ERROR([--enable-sm forced and -lSM not found])
      exit 1
   fi
fi

if test "$found_sm" = "yes"; then
   AC_DEFINE(HAVE_SM, , [Building with SM support])
fi

AM_CONDITIONAL(HAVE_SM, test "$found_sm" = "yes") 

AC_SEARCH_LIBS([dlsym], [dl], [], [
  AC_MSG_ERROR([unable to find the dlsym() function])
])

dnl **************************************************************************
dnl Check for Vulkan support
dnl **************************************************************************

AC_ARG_ENABLE([vulkan],
              [AS_HELP_STRING([--enable-vulkan], [enable Vulkan support])],
              [enable_vulkan="$enableval"], [enable_vulkan="auto"])

AS_IF([test "x$enable_vulkan" != "xno"], [
  AC_CHECK_LIB([vulkan], [vkCreateInstance], [
    AC_CHECK_HEADER([vulkan/vulkan.h], [found_vulkan=yes], [found_vulkan=no])
  ], [found_vulkan=no])

  AS_IF([test "x$found_vulkan" = "xyes"], [
    AC_DEFINE([HAVE_VULKAN], 1, [Build with Vulkan support])
    METACITY_LIBS="$METACITY_LIBS -lvulkan"
  ], [
    AS_IF([test "x$enable_vulkan" = "xyes"], [
      AC_MSG_ERROR([Vulkan enabled but not found])
    ])
  ])
], [found_vulkan=no])

AM_CONDITIONAL([HAVE_VULKAN], [test "x$found_vulkan" = "xyes"])

dnl **************************************************************************
dnl Process .in files
dnl **************************************************************************

AC_CONFIG_FILES([
  Makefile

  data/Makefile
  data/applications/Makefile
  data/keybindings/Makefile
  data/pkgconfig/Makefile
  data/pkgconfig/libmetacity.pc
  data/schemas/Makefile
  data/systemd/Makefile
  data/ui/Makefile

  doc/Makefile
  doc/man/Makefile

  libmetacity/Makefile

  dialog/Makefile

  src/Makefile
  src/tools/Makefile

  theme-viewer/Makefile

  po/Makefile.in
])

AC_OUTPUT

dnl **************************************************************************
dnl Show summary
dnl **************************************************************************

echo ""
echo "  Metacity $VERSION"
echo "  `echo Metacity $VERSION | sed "s/./=/g"`"
echo ""
echo "  srcdir ......................: ${srcdir}"
echo ""
echo "  compiler ....................: ${CC}"
echo "  cflags ......................: ${CFLAGS}"
echo ""
echo "  prefix ......................: ${prefix}"
echo "  exec_prefix .................: ${exec_prefix}"
echo "  libdir ......................: ${libdir}"
echo "  sysconfdir ..................: ${sysconfdir}"
echo "  libexecdir ..................: ${libexecdir}"
echo ""
echo "  Canberra ....................: ${enable_canberra}"
echo "  Xinerama ....................: ${have_xinerama}"
echo "  Session management ..........: ${found_sm}"
echo "  Resize-and-rotate ...........: ${found_randr}"
echo "  Xcursor .....................: ${have_xcursor}"
echo "  Vulkan ......................: ${found_vulkan}"
echo ""
