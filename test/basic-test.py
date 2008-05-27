#!/usr/bin/python

import sys
import os
from Xlib import X, display, Xatom

def query_window_manager (type, details):
    d = display.Display ()
    s = d.screen ()
    r = s.root

    w = r.create_window (
        50, 50, 300, 200, 2,
        s.root_depth,
        X.InputOutput,
        X.CopyFromParent,
        background_pixel = s.white_pixel,
        event_mask = X.PropertyChangeMask,
        colormap = X.CopyFromParent,
        )

    w.map()

    testing_atom = d.intern_atom ("_METACITY_TESTING")
    utf8_string = d.intern_atom ("UTF8_STRING")

    query = '%c?%s' % (type, details)
    w.change_property (testing_atom, utf8_string, 8, query)

    event = None

    while 1:
        event = d.next_event()

        if event.type == X.DestroyNotify:
            sys.exit(0)

        if event.type == X.PropertyNotify and event.atom==testing_atom:
            break

    result = w.get_full_property (testing_atom, utf8_string).value

    if len(result)<2 or result[1]!='=':
      if result==query:
        raise KeyError ("No result was set by the window manager.")
      else:
        raise KeyError ("Received some weird test result from the window manager: %s" % (result))

    if result[0]!=type:
        raise KeyError ("Received a result of %c from the window manager, but wanted %c." % (result[0], type))

    return result[2:]

query = '/apps/metacity/general/disable_workarounds TRUE'

print 'Querying window manager with the query: ',query
print 'Received result: ',query_window_manager ('C', query)
