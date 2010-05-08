# Copyright (c) 2008-2010 Thomas Thurman <tthurman@gnome.org>; GPL 2.0 or later.
# Originally based around example code in python-xlib
# by Peter Liljenberg <petli@ctrl-c.liu.se>.

import sys
import time
import re
from Xlib import X
from Xlib.protocol import display
from Xlib.protocol.event import ClientMessage
from Xlib.protocol.request import *

display = display.Display()
screen = display.info.roots[display.default_screen]
window = display.allocate_resource_id()
gc = display.allocate_resource_id()

CreateWindow(display, None,
             depth = screen.root_depth,
             wid = window,
             parent = screen.root,
             x = 100, y = 100, width = 250, height = 250, border_width = 2,
             window_class = X.InputOutput, visual = X.CopyFromParent,
             background_pixel = screen.white_pixel,
             event_mask = (X.ExposureMask |
                           X.StructureNotifyMask |
                           X.ButtonPressMask |
                           X.ButtonReleaseMask |
                           X.Button1MotionMask),
             colormap = X.CopyFromParent)

CreateGC(display, None, gc, window)

def do_event():
    event = display.next_event()

    if event.type == X.DestroyNotify:
        sys.exit(0)
        
    print event

for arg in sys.argv[1:]:
    if arg=='map':
        MapWindow(display, None, window)
    elif arg=='unmap':
        UnmapWindow(display, window=window)
    elif arg=='sleep':
        time.sleep(1)
    elif arg=='event':
        do_event()
    elif arg=='flush':
        display.flush()
    elif arg=='events':
        while 1:
            do_event()
    elif arg.startswith('send-'):
        subargs = arg.split('-')
        values = []
        for subarg in subargs[1:]:
            if subarg=='this':
                values.append(window)
            elif subarg=='root':
                values.append(screen.root)
            elif re.match('^\d+$', subarg):
                values.append(int(subarg))
            elif subarg.startswith('0x'):
                values.append(int(subarg, 16))
            else:
                values.append(InternAtom(display=display,
                                         name=subarg,
                                         only_if_exists=True).atom)
                if values[-1]==0:
                    print 'Warning: %s was unrecognised' % (subarg)

        event = ClientMessage(window=values[1],
                              client_type=values[2],
                              data=(32,(values[3:])))

        SendEvent(destination = values[0],
                  display = display,
                  propagate=True,
                  event = event,
                  event_mask = X.SubstructureRedirectMask|X.SubstructureNotifyMask)
    elif arg=='raise':
        CirculateWindow(display = display,
                        window = window,
                        direction = X.RaiseLowest)
    else:
        print 'Unknown argument: ',arg


