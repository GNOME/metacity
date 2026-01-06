# Metacity Window Manager

Metacity is a window manager for the X window system.

1. [Usage](#usage)
2. [Dependencies](#dependencies)
3. [Building](#building)
4. [Features](#features)
5. [Extensibility](#extensibility)
6. [FAQ](#faq)
7. [History](#history)
8. [Contributing](#contributing)

# Usage

When installed, metacity will be started by your desktop environment if it is
configured as the default window manager. To explicitly start a new metacity
process, run metacity with the `--replace` option:

```bash
metacity --replace &
```

Note that if a window manager process such as metacity is stopped without
another instance replacing it, you will lose the ability to switch between
windows or minimize and maximize windows.

During development metacity can be started directly from the build directory:
```bash
./src/metacity --replace &
```

Metacity includes additional executables:

* `metacity-theme-viewer`: A simple application to preview metacity themes.
* `metacity-message`: A command to send a message to metacity.
* `metacity-dialog`: A helper that displays dialogs, it is used to show the
  window not responding dialog and the session save dialog.

# Dependencies

Metacity depends on the following libraries:

* `gtk+-3.0`
* `glib-2.0`
* `x11`
* `libstartup-notification`
* `gsettings-desktop-schemas`
* `pango`
* `xcomposite`
* `xfixes`
* `xrender`
* `xdamage`
* `xres`

# Building

Metacity uses autotools. To build it, ensure that all
[Dependencies](#dependencies) are installed in the required versions and run:

```
./autogen.sh --prefix=/usr
make
```

The `autogen.sh` script calls all the necessary tools to generate the
`configure` script and then also runs the `configure` script, so we can
directly run make afterward.

For startup notification to work you need `libstartup-notification` at
http://www.freedesktop.org/software/startup-notification/ or on the
GNOME ftp site.

## Built-time Options

Several features for metacity can be configured at build time using
configure options:

```
--disable-canberra # disable use of canberra for event sounds
--disable-sm       # disable metacity's session management support
--disable-render   # disable metacity's use of the RENDER extension
--disable-xinerama # disable metacity's use of the Xinerama extension
```

# Features

- Window management for X11, such as drawing window decorations and managing
  window size constraints.

- Uses GTK+ 3.0 for drawing window frames. This means colors, fonts,
  etc. come from GTK+ theme.

- Does not expose the concept of "window manager" to the user. Some
  features in the GNOME control panel and other parts of the
  desktop happen to be implemented in metacity, such as changing your
  window border theme, or changing your window navigation shortcuts,
  but the user doesn't need to know this.

- Includes only the window manager; does not try to be a desktop
  environment. The pager, configuration, etc. are all separate and
  modular. The [libwnck](https://gitlab.gnome.org/GNOME/libwnck) library can
  be used for writing metacity extensions, pagers, and so on. Libwnck
  is not metacity specific, or GNOME-dependent; it requires only GTK,
  and should work with KWin, fvwm2, and other EWMH-compliant WMs.

- Has a simple theme system and a couple of extra themes come with it.
  Change themes via `gsettings`:

  ```bash
  gsettings set org.gnome.desktop.wm.preferences theme Crux
  gsettings set org.gnome.desktop.wm.preferences theme Gorilla
  gsettings set org.gnome.desktop.wm.preferences theme Atlanta
  gsettings set org.gnome.desktop.wm.preferences theme Bright
  ```

  Use `metacity-theme-viewer` to preview themes.

- Change number of workspaces via gsettings:

  ```bash
  gsettings set org.gnome.desktop.wm.preferences num-workspaces 5
  ```

  Can also change workspaces from GNOME 2 pager.

- Change focus mode:

  ```bash
  gsettings set org.gnome.desktop.wm.preferences focus-mode mouse
  gsettings set org.gnome.desktop.wm.preferences focus-mode sloppy
  gsettings set org.gnome.desktop.wm.preferences focus-mode click
  ```

## Global Keybindings

Global keybinding defaults include:

    Alt-Tab                forward cycle window focus
    Alt-Shift-Tab          backward cycle focus
    Alt-Ctrl-Tab           forward cycle focus among panels
    Alt-Ctrl-Shift-Tab     backward cycle focus among panels
    Alt-Escape             cycle window focus without a popup thingy
    Ctrl-Alt-Left Arrow    previous workspace
    Ctrl-Alt-Right Arrow   next workspace
    Ctrl-Alt-D             minimize/unminimize all, to show desktop

Change keybindings via gsettings, for example:

```bash
gsettings set org.gnome.desktop.wm.keybindings switch_to_workspace_1 '<Alt>F1'
```

Also try the GNOME keyboard shortcuts control panel.

## Window Keybindings

* `Alt-space`: open window menu

* Mnemonics work in the menu. That is, Alt-space then underlined
  letter in the menu item works.

* Choose Move from menu, and arrow keys to move the window.

* While moving, hold down Control to move slower, and
  Shift to snap to edges.

* Choose Resize from menu to resize the window.

* Keybindings for things like maximize window, maximize vertically/horizontally,
  minimize window, etc. can be bound to keyboard hotkeys (see
  `metacity.schemas`).

## Window Mouse Bindings

- Clicking anywhere on frame with button 1 will raise/focus window

- If you click a window control, such as the close button, then the
  control will activate on button release if you are still over it
  on release (as with most GUI toolkits)

- If you click and drag borders with button 1 it resizes the window

- If you click and drag the titlebar with button 1 it moves the
  window.

- If you click anywhere on the frame with button 2 it lowers the
  window.

- If you click anywhere on the frame with button 3 it shows the
  window menu.

- If you hold down Super (windows key) and click inside a window, it
  will move the window (buttons 1 and 2) or show menu (button 3).
  Or you can configure a different modifier for this.

- If you pick up a window with button 1 and then switch workspaces
  the window will come with you to the new workspace, this is
  a feature copied from Enlightenment.

- If you hold down Shift while moving a window, the window snaps
  to edges of other windows and the screen.

## Session Management

- Metacity connects to the session manager and will set itself up to
  be respawned. It theoretically restores sizes/positions/workspace
  for session-aware applications.

- Metacity implements much of the EWMH window manager specification
  from freedesktop.org, as well as the older ICCCM. Please refer to
  the [COMPLIANCE](./compliance) file for information on metacity
  compliance with these standards.

## Other Features

- Uses Pango to render text and support i18n capabilities, including UTF-8
  support.

- There are simple animations for actions such as minimization,
  to help users see what is happening. Should probably
  have a few more of these and make them nicer.

- considers the panel when placing windows and maximizing
  them.

- handles the window manager selection from the ICCCM. Will exit if
  another WM claims it, and can claim it from another WM if you pass
  the `--replace` argument. So if you're running another
  ICCCM-compliant WM, you can run `metacity --replace` to replace it
  with Metacity.

- does basic colormap handling

# Extensibility

You can write a metacity "plugin" such as a pager, window list, icon
box, task menu, or even things like "window matching" using
Extended Window Manager Hints (EWMH). See http://www.freedesktop.org for the
EWMH specification. An easy-to-use library called `libwnck` is
available that uses EWMH and is specifically designed for writing
WM accessories.

You might be interested in existing accessories such as "Devil's Pie"
by Ross Burton, which add features to Metacity (or other
EWMH-compliant WMs).

# FAQ

Q: Will you add my feature?

A: If it makes sense to turn on unconditionally, or is genuinely a
   harmless preference that I would not be embarrassed to put in a
   simple, uncluttered, user-friendly configuration dialog.

   If the only rationale for your feature is that other window
   managers have it, or that you are personally used to it, or
   something like that, then I will not be impressed. Metacity is
   firmly in the "choose good defaults" camp rather than the "offer 6
   equally broken ways to do it, and let the user pick one" camp.

   This is part of a "no crackrock" policy, despite some exceptions
   I'm mildly embarrassed about. For example, multiple workspaces
   probably constitute crackrock, they confuse most users and really
   are not that useful if you have a decent tasklist and so on. But I
   am too used to them to turn them off.  Or alternatively
   iconification/tasklist is crack, and workspaces/pager are good. But
   having both is certainly a bit wrong.  Sloppy focus is probably
   crackrock too.

   But don't think unlimited crack is OK just because I slipped up a
   little. No slippery slope here.

   Don't let this discourage patches and fixes - I love those. ;-)
   Just be prepared to hear the above objections if your patch adds
   some crack-ridden configuration option.

   http://pobox.com/~hp/free-software-ui.html
   http://pobox.com/~hp/features.html

Q: Is Metacity a Red Hat project?

A: Metacity's original creation was in no way funded, endorsed, or
   encouraged by Red Hat, Inc. - I'm guessing Red Hat would not
   consider "insufficient number of window managers for Linux" an
   urgent problem. Just a wild guess though.

   Now that metacity is the default WM however, Red Hat supports some
   bugfixing and other work.
 
Q: Why does Metacity remember the workspace/position of some apps 
   but not others across logout/login?

A: Metacity only stores sizes/positions for apps that are session
   managed. As far as I can determine, there is no way to attempt to
   remember workspace/position for non-session-aware apps without
   causing a lot of weird effects.

   The reason is that you don't know which non-SM-aware apps were
   launched by the session. When you initially log in, Metacity sees a
   bunch of new windows appear. But it can't distinguish between
   windows that were stored in your session, or windows you just
   launched after logging in. If Metacity tried to guess that a window
   was from the session, it could e.g. end up maximizing a dialog, or
   put a window you just launched on another desktop or in a weird
   place. And in fact I see a lot of bugs like this in window managers
   that try to handle non-session-aware apps.

   However, for session-aware apps, Metacity can tell that the
   application instance is from the session and thus restore it
   reliably, assuming the app properly restores the windows it had 
   open on session save.
   
   So the correct way to fix the situation is to make apps
   session-aware. libSM has come with X for years, it's very
   standardized, it's shared by GNOME and KDE - even twm is
   session-aware. So anyone who won't take a patch to add SM is more
   archaic than twm - and you should flame them. ;-)

   Docs on session management:
    http://www.fifi.org/doc/xspecs/xsmp.txt.gz
    http://www.fifi.org/doc/xspecs/SMlib.txt.gz

   See also the ICCCM section on SM. For GNOME apps, use the
   GnomeClient object. For a simple example of using libSM directly,
   twm/session.c in the twm source code is pretty easy to understand.

Q: How about adding viewports in addition to workspaces?

A: I could conceivably be convinced to use viewports _instead_ of
   workspaces, though currently I'm not thinking that. But I don't
   think it makes any sense to have both; it's just confusing. They
   are functionally equivalent.

   You may think this means that you won't have certain keybindings, 
   or something like that. This is a misconception. The only 
   _fundamental_ difference between viewports and workspaces is that 
   with viewports, windows can "overlap" and appear partially on 
   one and partially on another. All other differences that
   traditionally exist in other window managers are accidental - 
   the features commonly associated with viewports can be implemented
   for workspaces, and vice versa.

   So I don't want to have two kinds of
   workspace/desktop/viewport/whatever, but I'm willing to add
   features traditionally associated with either kind if those
   features make sense.

Q: Why is there no edge flipping?

A: This one is also in rationales.txt. Because "ouija board" UI, where
   you just move the mouse around and the computer guesses what you
   mean, has a lot of issues. This includes mouse focus, shade-hover
   mode, edge flipping, autoraise, etc. Metacity has mouse focus and
   autoraise as a compromise, but these features are all confusing for
   many users, and cause problems with accessibility, fitt's law, and
   so on.

   Read these and the bugzilla stuff before asking/commenting:
     http://pobox.com/~hp/free-software-ui.html
     http://pobox.com/~hp/features.html   

Q: Why no XYZ?

A: You are probably getting the idea by now - check rationales.txt,
   query/search GitLab, and read http://pobox.com/~hp/features.html
   and http://pobox.com/~hp/free-software-ui.html

   Then sit down and answer the question for yourself.  Is the feature
   good? What's the rationale for it? Answer "why" not just "why not."
   Justify in terms of users as a whole, not just users like
   yourself. How else can you solve the same problem? etc. If that
   leads you to a strong opinion, then please, post the rationale for
   discussion to an appropriate ticket in the issue tracker.

   Please don't just "me too!" on tickets, please don't think
   flames will get you anywhere, and please don't repeat rationale
   that's already been offered.

Q: Your dumb web pages you made me read talk about solving problems in
   fundamental ways instead of adding preferences or workarounds.
   What are some examples where metacity has done this?

A: There are quite a few, though many opportunities remain.  Sometimes
   the real fix involves application changes. The metacity approach is
   that it's OK to require apps to change, though there are also
   plenty of workarounds in metacity for battles considered too hard
   to fight.

   Here are some examples:

   - fullscreen mode was introduced to allow position constraints,
     panel-on-top, and other such things to apply to normal windows
     while still allowing video players etc. to "just work"

   - "whether to include minimized windows in Alt+Tab" was solved 
     by putting minimized windows at the *end* of the tab order. 

   - Whether to pop up a feedback display during Alt+Tab was solved by
     having both Alt+Tab and Alt+Esc

   - Whether to have a "kill" feature was solved by automatically
     detecting and offering to kill stuck apps. Better, metacity
     actually does "kill -9" on the process, it doesn't just
     disconnect the process from the X server. You'll appreciate this
     if you ever did a "kill" on Netscape 4, and watched it keep
     eating 100% CPU even though the X server had booted it.

   - The workspaces vs. viewports mess was avoided by adding
     directional navigation and such to workspaces, see discussion
     earlier in this file.

   - Instead of configurable placement algorithms, there's just one 
     that works fairly well most of the time.

   - To avoid excess CPU use during opaque move/resize, we rate limit
     the updates to the application window's size.

   - Instead of configurable "show size of window while resizing,"
     it's only shown for windows where it matters, such as terminals.
     (Only use-case given for all windows is for web designers
     choosing their web browser size, but there are web sites and
     desktop backgrounds that do this for you.)

   - Using startup notification, applications open on the workspace
     where you launched them, not the active workspace when their
     window is opened.

Q: I think metacity sucks.

A: Feel free to use any WM you like. The reason metacity follows the
   ICCCM and EWMH specifications is that it makes metacity a modular,
   interchangeable part in the desktop. libwnck-based apps such as the
   GNOME window list will work just fine with any EWMH-compliant WM.

# History

Metacity is not a meta-City as in an urban center, but rather
Meta-ness as in the state of being meta, i.e., metacity: meta and
opacity: opaque. Also, it may have something to do with the Meta key
on UNIX keyboards.

Metacity was officially part of GNOME starting with GNOME 2.2. Prior to that,
it was unofficially shipped as the default GNOME WM by several
OS vendors. Starting with GNOME 3 Metacity was replaced by the Mutter window
manager.

# Contributing

Report new bugs on https://gitlab.gnome.org/GNOME/metacity/-/issues/.
Please always check for duplicates before you are reporting a
bug or feature request.

Patches with bug fixes or features are welcome but should align with the overall
strategy of metacity.

See the [HACKING](./HACKING) file for some notes on hacking Metacity.
