/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Theme Rendering */

/*
 * Copyright (C) 2009 Collabora Ltd
 * Copyright (C) 2009 Thomas Thurman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "theme.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <cairo.h>
#include <ccss-cairo/ccss-cairo.h>
#include <gtk/gtk.h>

struct _MetaTheme {
  ccss_grammar_t        *grammar;
  ccss_stylesheet_t	*stylesheet;
  gchar                 *directory;
};

MetaTheme *the_theme = NULL;

/****************************************************************/

/**
 * The element names of each CopperClass.
 *
 * \bug For heaven's sake change the identifier
 */
char *names[] =
  {
    "frame",
    "area", "area",
    "title",
    "button",
    "button", "button", "button",
    "button", "button", "button",
    "button", "button", "button",
    "area",
    "last"
  };

/**
 * The parent class of each CopperClass, or
 * CC_LAST if it has none.
 */
CopperClasses parents[] =
  {
    CC_LAST,
    CC_FRAME, CC_FRAME,
    CC_TITLEBAR, CC_TITLEBAR,
    CC_TITLEBAR, CC_TITLEBAR, CC_TITLEBAR,
    CC_TITLEBAR, CC_TITLEBAR, CC_TITLEBAR,
    CC_TITLEBAR, CC_TITLEBAR, CC_TITLEBAR,
    CC_TITLEBAR,
    CC_LAST
  };

/**
 * The class (in the CSS sense) of each
 * CopperClass, or NULL if it has none.
 */
char *cowbell_classnames[] =
  {
    NULL,
    "content", "titlebar",
    NULL,
    "menu",
    "minimize", "maximize", "close",
    "shade", "above", "stick",
    "unshade", "unabove", "unstick",
    "filler",
    NULL
  };

/**
 * Names of each CopperClass, for debugging.
 */
char *cowbell_human_names[] =
  {
    "Frame",
    "Content area",
    "Titlebar",
    "Title",
    "Menu button",
    "Min",
    "Max",
    "Close",
    "Shade",
    "Above",
    "Stick",
    "Unshade",
    "Unabove",
    "Unstick",
    "Filler",
    NULL
  };

/**
 * Sometimes there are too many buttons to show in the space
 * on the titlebar; this is the order in which they should
 * be removed. The menu button should be the last
 * to go, since you can do any of the other operations using it.
 */
gint meta_theme_button_priorities[] = {
  CC_ABOVE,
  CC_STICK,
  CC_SHADE,
  CC_MINIMIZE,
  CC_MAXIMIZE,
  CC_CLOSE,
  CC_MENU,
  CC_LAST
};

typedef struct
{
  ccss_node_t basic;
  CopperClasses cowbell_class;
  /* the next two make no sense for any class but CC_FRAME */
  MetaFrameType type;
  MetaFrameFlags flags;
} CopperNode;

CopperNode cowbell_nodes[CC_LAST];

static void cowbell_style_title_text (ccss_stylesheet_t *stylesheet,
                                      PangoLayout *layout,
                                      cairo_t *cr);

static void cowbell_get_edge_sizes (MetaTheme *theme,
                                    MetaFrameType type,
                                    MetaFrameFlags flags,
                                    CopperClasses style_id,
                                    int *top,
                                    int *bottom,
                                    int *left,
                                    int *right,
                                    int *top_margin,
                                    int *bottom_margin,
                                    int *left_margin,
                                    int *right_margin);

/****************************************************************/

static char const*
cowbell_get_type (ccss_node_t const *self)
{
  g_warning ("It's %s", self->type_name);
  return self->type_name;
}

static ptrdiff_t
cowbell_get_instance (ccss_node_t const *self)
{
  return self->instance;
}

static char const*
cowbell_get_style (ccss_node_t const *self) {
  /* this is always ""; why don't we just return ""? */
  return self->inline_style;
}

static ccss_node_t*
cowbell_get_container (ccss_node_t const *self)
{
  CopperClasses candidate = parents[((CopperNode*)self)->cowbell_class];

  if (candidate==CC_LAST)
    return NULL;
  else
    /* or should we allocate a new one? */
    return (ccss_node_t*) &(cowbell_nodes[candidate]);
}

static const char*
cowbell_get_class (ccss_node_t const *self)
{
  return cowbell_classnames[((CopperNode*)self)->cowbell_class];
}

/* Attributes of nodes representing windows */
typedef struct {
  const char *name;
  MetaFrameFlags flag;
} MetaFrameFlagName;

/*
 * Rationale for attribute names:
 *  - The verb should be lexically identical at all times.
 *    (So we shouldn't have "can-stick", "is-stuck".)
 *    Otherwise people who aren't native English speakers
 *    might get confused.
 *  - The class name should nevertheless read as an
 *    English phrase.
 */

MetaFrameFlagName meta_flag_names[] = {
  { "can-be-closed", META_FRAME_ALLOWS_DELETE },
  { "can-have-menu", META_FRAME_ALLOWS_MENU },
  { "can-be-minimized", META_FRAME_ALLOWS_MINIMIZE },
  { "can-be-maximized", META_FRAME_ALLOWS_MAXIMIZE },
  { "can-be-vertically-resized", META_FRAME_ALLOWS_VERTICAL_RESIZE },
  { "can-be-horizontally-resized", META_FRAME_ALLOWS_HORIZONTAL_RESIZE },
  { "is-focused", META_FRAME_HAS_FOCUS },
  { "is-shaded", META_FRAME_SHADED },
  { "is-stuck", META_FRAME_STUCK },
  { "is-maximized", META_FRAME_MAXIMIZED },
  { "can-be-shaded", META_FRAME_ALLOWS_SHADE },
  { "can-be-moved", META_FRAME_ALLOWS_MOVE },
  { "is-fullscreen", META_FRAME_FULLSCREEN },
  { "is-flashing", META_FRAME_IS_FLASHING },
  { "is-above", META_FRAME_ABOVE },
  { NULL, 0 }
};

static char*
cowbell_get_attribute (ccss_node_t const *self,
                       char const *name)
{
  CopperNode *copper = (CopperNode*) self;

  if (strcmp(name, "type")==0)
    {
      /* Pretty simple: just look up the type. */
      switch (copper->type)
        {
        case META_FRAME_TYPE_NORMAL:
          return g_strdup ("normal");
        case META_FRAME_TYPE_DIALOG:
          return g_strdup ("dialog");
        case META_FRAME_TYPE_MODAL_DIALOG:
          return g_strdup ("modal-dialog");
        case META_FRAME_TYPE_UTILITY:
          return g_strdup ("utility");
        case META_FRAME_TYPE_MENU:
          return g_strdup ("menu");
        case META_FRAME_TYPE_BORDER:
          return g_strdup ("border");
        default:
          return g_strdup ("unknown");      
        }
    }
  else
    {
      /* So try the flag names. */
      MetaFrameFlagName *cursor;
      for (cursor = meta_flag_names; cursor->name; cursor++)
        {
          if (strcmp (name, cursor->name)==0)
            {
              if (copper->flags & cursor->flag)
                {
                  return g_strdup ("1");
                }
              else
                {
                  return g_strdup ("0");
                }
            }
        }
    }

  /* try some odd rare ones */

  if (strcmp(name, "cowbell1")==0)
    {
      return g_strdup ("1");
    }
  else if (strcmp(name, "wm")==0)
    {
      return g_strdup ("metacity");
    }

  /* giving up here */
  return NULL;
}

static ccss_node_class_t cowbell_node_class = {
  .get_type               = (ccss_node_get_type_f) cowbell_get_type,
  .get_instance           = (ccss_node_get_instance_f) cowbell_get_instance,
  .get_style              = (ccss_node_get_style_f) cowbell_get_style,
  .get_container          = (ccss_node_get_container_f) cowbell_get_container,
  .get_class              = (ccss_node_get_class_f) cowbell_get_class,
  .get_attribute          = (ccss_node_get_attribute_f) cowbell_get_attribute,
};

/****************************************************************/

static void
cowbell_initialise_classes (void)
{
  int i;

  for (i=0; i<CC_LAST; i++)
    {
      ccss_node_init ((ccss_node_t*) &cowbell_nodes[i], &cowbell_node_class);
      cowbell_nodes[i].basic.type_name = names[i];
      cowbell_nodes[i].basic.instance = 0;
      cowbell_nodes[i].basic.inline_style = NULL;
      cowbell_nodes[i].cowbell_class = i;
    }
}

/****************************************************************/

MetaTheme*
meta_theme_get_current (void)
{
  return the_theme;
}

/**
 * Filters image URLs.
 * 
 * Currently, we accept two kinds of image URLs:
 *  - "wm:icon", which is the current window icon
 *  - "file:NAME" which loads images from the theme directory.
 *    This may be abbreviated to just NAME.
 */
static char *
cowbell_url (GSList const	*args,
             void		*user_data)
{
  char *filename = NULL;
  MetaTheme **theme = (MetaTheme**) user_data;

  g_return_val_if_fail (args && args->data, NULL);
  filename = (char*) args->data;

  if (strcmp (filename, "wm:icon")==0)
    {
      return g_strdup ("file:///usr/share/icons/gnome/24x24/actions/gtk-edit.png");
    }
  else
    {
      return g_strdup_printf ("file://%s/%s", (*theme)->directory, filename);
    }
}

static ccss_function_t const cowbell_functions[] = 
{
  { "url",	cowbell_url,	&the_theme },
  { NULL }
};

void
meta_theme_set_current (const char *name,
                        gboolean    force_reload)
{
  /*
   * TODO: This does not check in $prefix/share/themes;
   * only in ~/.themes.
   */

  /*
   * Rationale for CSS filename:
   * v1/v2 used to call all the themes metacity-theme-N.xml.
   * This meant you couldn't put a group of them in the same
   * directory to compare them.  Therefore, we put the theme
   * name in the filename: Human.css, etc.
   */

  char *css_filename, *css_path;

  if (!the_theme)
    {
      cowbell_initialise_classes ();

      the_theme = g_new0 (MetaTheme, 1);
    }
  else
    {
      ccss_stylesheet_destroy (the_theme->stylesheet);
      ccss_grammar_destroy (the_theme->grammar);
      g_free (the_theme->directory);
    }

  the_theme->grammar = ccss_cairo_grammar_create ();
  ccss_grammar_add_functions (the_theme->grammar,
                              cowbell_functions);

  the_theme->directory =
    g_build_filename (g_get_home_dir (),
                      ".themes",
                      name,
                      "cowbell",
                      NULL);

  css_filename = g_strdup_printf ("%s.css", name);

  css_path = g_build_filename (the_theme->directory,
                               css_filename,
                               NULL);

  g_free (css_filename);

  the_theme->stylesheet =
    ccss_grammar_create_stylesheet_from_file (the_theme->grammar,
                                              css_path,
                                              NULL);

  /*
   * TODO: Bad things will happen if the file doesn't exist;
   * we should abort here if it doesn't.
   */

#if 0
  ccss_stylesheet_dump (the_theme->stylesheet);
#endif

  g_free (css_path);
}

double
meta_theme_get_title_scale (MetaTheme     *theme,
                            MetaFrameType  type,
                            MetaFrameFlags flags)
{
  /*
   * In v2 themes, the theme authors had the option of
   * asking for various sizes to scale the text by:
   *
   *    xx-small,x-small,small,medium,large,x-large,xx-large
   *
   * We don't currently support any of these, and we
   * pretend that it's always given as "medium".
   */

  return 1.0;
}

static void
cowbell_style_title_text (ccss_stylesheet_t *stylesheet,
                          PangoLayout *layout,
                          cairo_t *cr)
{
  PangoAttrList *attrs = NULL;
  ccss_style_t *style = ccss_stylesheet_query (stylesheet,
					       (ccss_node_t*) &cowbell_nodes[CC_TITLE]);
  char *align;
  ccss_color_t const *colour;

  attrs = pango_attr_list_new ();

  /* We have to handle CSS text properties ourselves here because
   * libccss doesn't know how to render them yet.  We don't try
   * to do all the funky effects like shadows and so on,
   * unfortunately.
   */

  if (ccss_style_get_property (style, "color",
			       (const ccss_property_base_t**) &colour))
    {
      pango_attr_list_insert (attrs,
			      pango_attr_foreground_new
			      ((int)65535.0*colour->red,
			       (int)65535.0*colour->green,
			       (int)65535.0*colour->blue));
    }
  
  /* Alignment */
  
  if (ccss_style_get_string (style, "text-align", &align))
    {
      if (strcmp (align, "left")==0 || strcmp (align, "justify")==0)
	{
	  pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
	}
      else if (strcmp (align, "center")==0)
	{
	  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	}
      else if (strcmp (align, "right")==0)
	{
	  pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);
	}
      else
	g_warning ("Unknown alignment: %s", align);
    }
  else
    {
      /* default to centring the text */
      pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
    }

  pango_layout_set_attributes (layout, attrs);

  ccss_style_destroy (style);
  pango_attr_list_unref (attrs);
}

void
meta_theme_draw_frame_with_style (MetaTheme              *theme,
                                  GtkStyle               *style_gtk,
                                  GtkWidget              *widget,
                                  GdkDrawable            *drawable,
                                  const GdkRectangle     *clip,
                                  int                     x_offset,
                                  int                     y_offset,
                                  MetaFrameType           type,
                                  MetaFrameFlags          flags,
                                  int                     client_width,
                                  int                     client_height,
                                  PangoLayout            *title_layout,
                                  int                     text_height,
                                  const MetaButtonLayout *button_layout,
                                  MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                                  GdkPixbuf              *mini_icon,
                                  GdkPixbuf              *icon)
{
  cairo_t *cr = gdk_cairo_create (drawable);
  ccss_stylesheet_t *stylesheet = theme->stylesheet;
  MetaFrameGeometry fgeom;
  int i;

  meta_theme_calc_geometry (theme, type, text_height, flags,
                            client_width, client_height,
                            button_layout,
                            &fgeom);

  for (i=0; i<CC_LAST; i++)
    {
      ccss_style_t *style = ccss_stylesheet_query (stylesheet,
                                                   (ccss_node_t*) &cowbell_nodes[i]);
      int x = fgeom.areas[i].x;
      int y = fgeom.areas[i].y;
      int width = fgeom.areas[i].width;
      int height = fgeom.areas[i].height;

      if (!style)
        continue;

      x += fgeom.areas[i].left_margin;
      width -= (fgeom.areas[i].left_margin + fgeom.areas[i].right_margin);
      y += fgeom.areas[i].top_margin;
      height -= (fgeom.areas[i].top_margin + fgeom.areas[i].bottom_margin);

      if (width > 0 && height > 0)
        {
          /*
          meta_warning ("Plotting %s at %d %d %dx%d\n",
                        cowbell_human_names[i],
                        x, y, width, height);
          */

          ccss_cairo_style_draw_rectangle (style, cr,
                                           x, y,
                                           width, height);
        }

      ccss_style_destroy (style);
    }

  /* may be worth moving this inline? */
  cowbell_style_title_text (stylesheet, title_layout, cr);

  cairo_translate (cr,
                   fgeom.areas[CC_TITLE].x + fgeom.areas[CC_TITLE].left_edge,
                   fgeom.areas[CC_TITLE].y + fgeom.areas[CC_TITLE].top_edge);

  pango_layout_set_width (title_layout, fgeom.areas[CC_TITLE].width);
  pango_layout_set_height (title_layout, fgeom.areas[CC_TITLE].height);

  switch (pango_layout_get_alignment (title_layout))
    {
    case PANGO_ALIGN_RIGHT:
      cairo_translate (cr,
                       fgeom.areas[CC_TITLE].width,
                       0);
      break;
    case PANGO_ALIGN_CENTER:
      cairo_translate (cr,
                       fgeom.areas[CC_TITLE].width/2,
                       0);
      break;
    default:
      /* nothing */
      break;
    }

  pango_cairo_show_layout (cr, title_layout);

  cairo_destroy (cr);
}

/**
 * Returns the style to use with the given type and flags.
 * When you're done with it, call ccss_style_destroy (style).
 *
 * Be warned: this function is not threadsafe.  It modifies
 * a static copy of the node hierarchy in order to set the
 * attributes of the frame.
 */
static ccss_style_t *
cowbell_get_current_style (MetaTheme *theme,
                           MetaFrameType type,
                           MetaFrameFlags flags,
                           CopperClasses style_id)
{
  ccss_stylesheet_t *stylesheet = theme->stylesheet;
  ccss_style_t *result;

  /*
   * Possibly we should set attributes on everything,
   * not just the frame.
   */
  cowbell_nodes[CC_FRAME].type = type;
  cowbell_nodes[CC_FRAME].flags = flags;

  result = ccss_stylesheet_query (stylesheet,
                                  (ccss_node_t*) &cowbell_nodes[style_id]);

  return result;
}

/**
 * Finds the size of the edges (borders, margins, and optionally padding) of
 * an element with a given CSS style.  Any of the out parameters can be NULL
 * in order not to receive the value; if they are not, the answer will be
 * added to the current value, rather than replacing it.
 *
 * \param style  The style.
 * \param ignore_padding  Whether to ignore padding in the calculation.
 * \param top    Receives the size of the top edge.
 * \param bottom Receives the size of the bottom edge.
 * \param left   Receives the size of the left edge.
 * \param right  Receives the size of the right edge.
 */
static void
cowbell_get_edge_sizes (MetaTheme *theme,
                        MetaFrameType type,
                        MetaFrameFlags flags,
                        CopperClasses style_id,
                        int *top,
                        int *bottom,
                        int *left,
                        int *right,
                        int *top_margin,
                        int *bottom_margin,
                        int *left_margin,
                        int *right_margin)
{
  double d;
  gboolean fallback_known;
  gint i, j, start;
  /* We are called far too often to mess around with string splicing */
  gchar *properties[3][5] = {
    {"margin-top-width", "margin-bottom-width", "margin-left-width", "margin-right-width", "margin-width"},
    {"border-top-width", "border-bottom-width", "border-left-width", "border-right-width", "border-width"},
    {"padding-top-width", "padding-bottom-width", "padding-left-width", "padding-right-width", "padding-width"},
  };
  double results[4] = {0.0, 0.0, 0.0, 0.0};
  double fallback = 0.0;
  ccss_style_t *style = cowbell_get_current_style (theme, type, flags, style_id);

  if (style_id==CC_FRAME)
    {
      /* the frame has no margin */
      start = 1;

      if (top_margin) *top_margin = 0;
      if (bottom_margin) *bottom_margin = 0;
      if (left_margin) *left_margin = 0;
      if (right_margin) *right_margin = 0;
    }
  else
    start = 0;

  for (i=start; i<3; i++)
    {
      fallback_known = FALSE;
      for (j=0; j<4; j++)
        {
          if (ccss_style_get_double (style, properties[i][j], &d))
            {
              /* it exists under foo-direction-width */
              results[j] += d;
            }
          else
            {
              /* it doesn't exist under foo-direction-width;
               * try foo-width
               */
              if (!fallback_known)
                {
                  /* we haven't cached it yet */
                  fallback_known = TRUE;
                  if (!ccss_style_get_double (style, properties[i][4], &fallback))
                    {
                      fallback = 0.0;
                    }
                }
              results[j] += fallback;
            }
        }

      if (i==0)
        {
          /* we have just checked the first entry,
           * which is the margin
           */
          if (top_margin) *top_margin = (int) results[0];
          if (bottom_margin) *bottom_margin = (int) results[1];
          if (left_margin) *left_margin = (int) results[2];
          if (right_margin) *right_margin = (int) results[3];
        }
    }

  if (top) *top += (int) results[0];
  if (bottom) *bottom += (int) results[1];
  if (left) *left += (int) results[2];
  if (right) *right += (int) results[3];

  ccss_style_destroy (style);
}

void
meta_theme_get_frame_borders (MetaTheme         *theme,
                              MetaFrameType      type,
                              int                text_height,
                              MetaFrameFlags     flags,
                              int               *top_height,
                              int               *bottom_height,
                              int               *left_width,
                              int               *right_width)
{
  *top_height = 0;
  *bottom_height = 0;
  *left_width = 0;
  *right_width = 0;

  cowbell_get_edge_sizes (theme, type, flags, CC_FRAME,
                          top_height,
                          bottom_height,
                          left_width,
                          right_width,
                          NULL, NULL, NULL, NULL);

  cowbell_get_edge_sizes (theme, type, flags, CC_CONTENT,
                          top_height,
                          bottom_height,
                          left_width,
                          right_width,
                          NULL, NULL, NULL, NULL);

  cowbell_get_edge_sizes (theme, type, flags, CC_TITLEBAR,
                          top_height, top_height, NULL, NULL,
                          NULL, NULL, NULL, NULL);

  cowbell_get_edge_sizes (theme, type, flags, CC_TITLE,
                          top_height, top_height, NULL, NULL,
                          NULL, NULL, NULL, NULL);

  if (top_height)
    {
      *top_height += text_height;
    }
}

#define MAX_MIDDLE_BACKGROUNDS (MAX_BUTTONS_PER_CORNER - 2)

/**
 * This annoying function exists because we have
 * several enums which describe buttons, and we
 * need to map between them.
 */
static CopperClasses
copper_class_for_button (int button)
{
  switch (button)
    {
    case META_BUTTON_FUNCTION_MENU:
      return CC_MENU;
    case META_BUTTON_FUNCTION_MINIMIZE:
      return CC_MINIMIZE;
    case META_BUTTON_FUNCTION_MAXIMIZE:
      return CC_MAXIMIZE;
    case META_BUTTON_FUNCTION_CLOSE:
      return CC_CLOSE;
    case META_BUTTON_FUNCTION_SHADE:
      return CC_SHADE;
    case META_BUTTON_FUNCTION_ABOVE:
      return CC_ABOVE;
    case META_BUTTON_FUNCTION_STICK:
      return CC_STICK;
    case META_BUTTON_FUNCTION_UNSHADE:
      return CC_UNSHADE;
    case META_BUTTON_FUNCTION_UNABOVE:
      return CC_UNABOVE;
    case META_BUTTON_FUNCTION_UNSTICK:
      return CC_UNSTICK;
    default:
      /* there is no way we should ever get here */
      meta_bug ("Unknown button type. %d", button);
      return CC_MENU;
    }
}

/**
 * Fills the public record of a button's dimensions
 * using data from private members.  If we used an
 * enum for the public members, this wouldn't be necessary.
 *
 * \bug Does not distinguish clickable and visible areas.
 *      Should we still do this with Cowbell?
 */
static void
fill_button_rect (MetaButtonSpace *button,
                  CopperClasses cclass,
                  MetaFrameGeometry *fgeom)
{
  button->visible.x      = fgeom->areas[cclass].x;
  button->visible.y      = fgeom->areas[cclass].y;
  button->visible.width  = fgeom->areas[cclass].width;
  button->visible.height = fgeom->areas[cclass].height;
  
  button->clickable.x      = button->visible.x;
  button->clickable.y      = button->visible.y;
  button->clickable.width  = button->visible.width;
  button->clickable.height = button->visible.height;
}

static int
cowbell_get_button_width (MetaTheme              *theme,
                          MetaFrameType           type,
                          MetaFrameFlags          flags,
                          CopperClasses           button,
                          int                     button_height)
{
  double min_width=0.0, max_width=0.0;
  double width=0.0, height=0.0;
  ccss_style_t *style = cowbell_get_current_style (theme, type, flags, button);
  double aspect_ratio;
  double result;

  ccss_style_get_double (style, "min-width", &min_width);
  ccss_style_get_double (style, "max-width", &max_width);

  if (min_width > max_width)
    {
      min_width = max_width;
    }

  if (min_width!=0.0 && min_width==max_width)
    {
      /* gosh, that was easy. */
      return (int) min_width;
    }

  ccss_style_get_double (style, "width",  &width);
  ccss_style_get_double (style, "height", &height);
  
  if (width==0.0 || height==0.0)
    {
      /* They didn't specify a width or height for the button.
       *
       * Ultimately I want to stop at this point and check for the
       * background image, and default to its dimensions.  But
       * for now, let's assume it's square.
       */
      aspect_ratio = 1.0;

      /* Warn them.  This could be more informative... */
      g_warning ("Button found with missing width or height");
    }
  else
    {
      aspect_ratio = width/height;
    }

  result = button_height * aspect_ratio;

  if (min_width!=0 && result<min_width) result = min_width;
  if (max_width!=0 && result>max_width) result = max_width;

  return (int) result;
}

void
meta_theme_calc_geometry (MetaTheme              *theme,
                          MetaFrameType           type,
                          int                     text_height,
                          MetaFrameFlags          flags,
                          int                     client_width,
                          int                     client_height,
                          const MetaButtonLayout *button_layout,
                          MetaFrameGeometry      *fgeom)
{
  const int allocated_left = -1;
  const int allocated_right = -2;
  const int allocated_not = -3;
  int i;
  int button_height, button_y;
  int x;

  /* TODO - We may want to cache the style objects in "areas" */

  memset (&(fgeom->areas), 0, sizeof(fgeom->areas));

  /* First of all, we need to calculate the edges for each element. */
  for (i=0; i<CC_LAST; i++)
    {
      /* We may want to do this inline if we end up not calling
       * it from anywhere but here.
       */
      cowbell_get_edge_sizes (theme, type, flags,
                              i,
                              &(fgeom->areas[i].top_edge),
                              &(fgeom->areas[i].bottom_edge),
                              &(fgeom->areas[i].left_edge),
                              &(fgeom->areas[i].right_edge),
                              &(fgeom->areas[i].top_margin),
                              &(fgeom->areas[i].bottom_margin),
                              &(fgeom->areas[i].left_margin),
                              &(fgeom->areas[i].right_margin));
    }

  /* Now let's look at the coordinates for each element. */
  /* For now, we are assuming that the clickable and visible
   * areas of a button are the same.  We may need to distinguish them
   * eventually. */

  /* Let's begin with the frame. */
  fgeom->areas[CC_FRAME].x = 0; /* It necessarily starts at the top left... */
  fgeom->areas[CC_FRAME].y = 0;
  /* The width is simply the width of the content area, plus the edges
   * of the content area and frame. */
  fgeom->areas[CC_FRAME].width =
    fgeom->areas[CC_FRAME].left_edge +
    fgeom->areas[CC_CONTENT].left_edge +
    client_width +
    fgeom->areas[CC_CONTENT].right_edge +
    fgeom->areas[CC_FRAME].right_edge;
  /* The height is the height of those elements, plus the height of the
   * title and its edges, and the edges of the titlebar area.
   */
  fgeom->areas[CC_FRAME].height =
    fgeom->areas[CC_FRAME].top_edge +
    fgeom->areas[CC_TITLEBAR].top_edge +
    fgeom->areas[CC_TITLE].top_edge +
    text_height +
    fgeom->areas[CC_TITLE].bottom_edge +
    fgeom->areas[CC_TITLEBAR].bottom_edge +
    fgeom->areas[CC_CONTENT].top_edge +
    client_height +
    fgeom->areas[CC_CONTENT].bottom_edge +
    fgeom->areas[CC_FRAME].bottom_edge;

  /* Next, the content area. */
  fgeom->areas[CC_CONTENT].x =
    fgeom->areas[CC_FRAME].left_edge;
  fgeom->areas[CC_CONTENT].y =
    fgeom->areas[CC_FRAME].top_edge +
    fgeom->areas[CC_TITLEBAR].top_edge +
    fgeom->areas[CC_TITLE].top_edge +
    text_height +
    fgeom->areas[CC_TITLE].bottom_edge +
    fgeom->areas[CC_TITLEBAR].bottom_edge;
  fgeom->areas[CC_CONTENT].width =
    fgeom->areas[CC_CONTENT].left_edge +
    client_width +
    fgeom->areas[CC_CONTENT].right_edge;
  fgeom->areas[CC_CONTENT].height =
    fgeom->areas[CC_CONTENT].top_edge +
    client_height +
    fgeom->areas[CC_CONTENT].bottom_edge;

  /* Next, the titlebar. */
  fgeom->areas[CC_TITLEBAR].x =
    fgeom->areas[CC_FRAME].left_edge;
  fgeom->areas[CC_TITLEBAR].y =
    fgeom->areas[CC_FRAME].top_edge;
  fgeom->areas[CC_TITLEBAR].width =
    fgeom->areas[CC_CONTENT].width;
  fgeom->areas[CC_TITLEBAR].height =
    fgeom->areas[CC_TITLEBAR].top_edge +
    fgeom->areas[CC_TITLE].top_edge +
    text_height +
    fgeom->areas[CC_TITLE].bottom_edge +
    fgeom->areas[CC_TITLEBAR].bottom_edge;

  /* Now the title. */
  fgeom->areas[CC_TITLE].x =
    fgeom->areas[CC_FRAME].left_edge +
    fgeom->areas[CC_TITLEBAR].left_edge;
  fgeom->areas[CC_TITLE].y =
    fgeom->areas[CC_FRAME].top_edge +
    fgeom->areas[CC_TITLEBAR].top_edge;
  fgeom->areas[CC_TITLE].width =
    /* This is the most it can be; we'll possibly subtract from it */
    fgeom->areas[CC_CONTENT].width;
  fgeom->areas[CC_TITLE].height =
    text_height; /* obviously */

  /* And finally, the buttons. */
  /* Just work out the width, height, and y coord first... */
  button_height = fgeom->areas[CC_TITLEBAR].height -
        (fgeom->areas[CC_TITLEBAR].top_edge +
         fgeom->areas[CC_TITLEBAR].bottom_edge);
  button_y = fgeom->areas[CC_TITLEBAR].y +
    fgeom->areas[CC_TITLEBAR].top_edge;

  /*
   * We temporarily use two magic negative values
   * for fgeom->areas[n].x:
   *
   *  allocated_left -- allocated on the left
   *  allocated_right -- allocated on the right
   *  allocated_not -- not allocated at all.
   */

  for (i=CC_BUTTON_FIRST; i<=CC_BUTTON_LAST; i++)
    {
      fgeom->areas[i].height = button_height;
      fgeom->areas[i].width = cowbell_get_button_width (theme, type, flags,
                                                        i,
                                                        button_height);
      fgeom->areas[i].width +=
        fgeom->areas[i].left_margin +
        fgeom->areas[i].right_margin;
      /*
       * but we do not adjust the height; setting top and bottom
       * margins on a button just compresses the button
       */
      fgeom->areas[i].x = allocated_not;
      fgeom->areas[i].y = button_y;
    }

  /*
   * If the total width is too wide,
   * knock out some of the buttons, in priority order.
   */

  {
    int total_width = 0;
    int available_width =
      fgeom->areas[CC_TITLEBAR].width -
      (fgeom->areas[CC_TITLEBAR].left_edge +
       fgeom->areas[CC_TITLEBAR].right_edge);

    for (i=0; i<MAX_BUTTONS_PER_CORNER; i++)
      {
        int button = button_layout->left_buttons[i];
        CopperClasses cc;

        if (button == META_BUTTON_FUNCTION_LAST)
          break;

        cc = copper_class_for_button(button);
        total_width += fgeom->areas[cc].width;
        fgeom->areas[cc].x = allocated_left;
      }

    for (i=0; i<MAX_BUTTONS_PER_CORNER; i++)
      {
        int button = button_layout->right_buttons[i];
        CopperClasses cc;

        if (button == META_BUTTON_FUNCTION_LAST)
          break;

        cc = copper_class_for_button(button);
        total_width += fgeom->areas[cc].width;
        fgeom->areas[cc].x = allocated_right;
      }

    i=0;
    while (total_width > available_width &&
           meta_theme_button_priorities[i]!=CC_LAST)
      {
        CopperClasses button = meta_theme_button_priorities[i];

        if (fgeom->areas[button].x == allocated_left ||
            fgeom->areas[button].x == allocated_right)
          {
            total_width -= fgeom->areas[button].width;

            fgeom->areas[button].x =
              fgeom->areas[button].y =
              fgeom->areas[button].width =
              fgeom->areas[button].height =
              0;
          }

        i++;
      }
  }
    

  /* And now allocate the buttons as necessary. */

  /*
   * We are not honouring spacers.
   * They should possibly be removed anyway.
   */

  /*
   * It's probably a little silly to loop over button_layout again
   * since we've already done it, but we do it this way at present
   * to get the ordering right.  TODO: Perhaps we need a separate array
   * that holds 0 if a button isn't displayed, and descending/ascending
   * +ve or -ve values for its ordering on the left or right side.
   */

  /* The left-hand side */

  x = fgeom->areas[CC_TITLEBAR].x +
    fgeom->areas[CC_TITLEBAR].left_edge;

  for (i=0; i<MAX_BUTTONS_PER_CORNER; i++)
    {
      int button = button_layout->left_buttons[i];
      CopperClasses cc;

      if (button == META_BUTTON_FUNCTION_LAST)
        break;

      cc = copper_class_for_button(button);

      if (fgeom->areas[cc].x==0)
        /* must have been knocked out */
        continue;

      /* so allocate it */
      fgeom->areas[cc].x = x;
      x += fgeom->areas[cc].width;

      fgeom->areas[CC_TITLE].x += fgeom->areas[cc].width;
      fgeom->areas[CC_TITLE].width -= fgeom->areas[cc].width;
    }

  /* The right-hand side */

  x = fgeom->areas[CC_TITLEBAR].x +
    (fgeom->areas[CC_TITLEBAR].width -
     (fgeom->areas[CC_TITLEBAR].left_edge +
      fgeom->areas[CC_TITLEBAR].right_edge));

  /* we want to go over them backwards */
  i=0;
  while (i+1<MAX_BUTTONS_PER_CORNER &&
         button_layout->right_buttons[i]!=META_BUTTON_FUNCTION_LAST)
    i++;

  for (i--; i>=0; i--)
    {
      int button = button_layout->right_buttons[i];
      CopperClasses cc;

      cc = copper_class_for_button(button);

      if (fgeom->areas[cc].x==0)
        /* must have been knocked out */
        continue;

      /* so allocate it */
      x -= fgeom->areas[cc].width;
      fgeom->areas[cc].x = x;

      fgeom->areas[CC_TITLE].width -= fgeom->areas[cc].width;
    }

  /* Now find the ones we didn't use, and zero them out */
  for (i=CC_BUTTON_FIRST; i<=CC_BUTTON_LAST; i++)
    {
      if (fgeom->areas[i].x == allocated_not)
        {
          fgeom->areas[i].x =
            fgeom->areas[i].y =
            fgeom->areas[i].width =
            fgeom->areas[i].height =
            0;
        }
    }

  /* We are not yet doing CC_FILLER. */
  fgeom->areas[CC_FILLER].x =
    fgeom->areas[CC_FILLER].y =
    fgeom->areas[CC_FILLER].width =
    fgeom->areas[CC_FILLER].height =
    0;

  /* Now we have our own private information.
   * Let's copy the necessary parts into the public members
   * of our struct.
   */

  fgeom->top_height =
    fgeom->areas[CC_FRAME].top_edge +
    fgeom->areas[CC_TITLEBAR].height +
    fgeom->areas[CC_CONTENT].top_edge;

  fgeom->right_width =
    fgeom->areas[CC_CONTENT].right_edge +
    fgeom->areas[CC_FRAME].right_edge;

  fgeom->bottom_height =
    fgeom->areas[CC_CONTENT].bottom_edge +
    fgeom->areas[CC_FRAME].bottom_edge;

  fgeom->left_width =
    fgeom->areas[CC_CONTENT].left_edge +
    fgeom->areas[CC_FRAME].left_edge;

  fgeom->width =
    fgeom->areas[CC_FRAME].width;

  fgeom->height =
    fgeom->areas[CC_FRAME].height;

  fgeom->title_rect.x =
    fgeom->areas[CC_TITLE].x;
  fgeom->title_rect.y =
    fgeom->areas[CC_TITLE].y;
  fgeom->title_rect.width =
    fgeom->areas[CC_TITLE].width;
  fgeom->title_rect.height =
    fgeom->areas[CC_TITLE].height;

  /* Are these correct? */
  fgeom->top_titlebar_edge =
    fgeom->areas[CC_TITLEBAR].left_edge;
  fgeom->right_titlebar_edge =
    fgeom->areas[CC_TITLEBAR].right_edge;
  fgeom->bottom_titlebar_edge =
    fgeom->areas[CC_TITLEBAR].bottom_edge;
  fgeom->left_titlebar_edge =
    fgeom->areas[CC_TITLEBAR].left_edge;

  /* I'll take "places we should have used an enum" for 25 */

  fill_button_rect (&(fgeom->close_rect),   CC_CLOSE,    fgeom);
  fill_button_rect (&(fgeom->max_rect),     CC_MAXIMIZE, fgeom);
  fill_button_rect (&(fgeom->min_rect),     CC_MINIMIZE, fgeom);
  fill_button_rect (&(fgeom->menu_rect),    CC_MENU,     fgeom);
  fill_button_rect (&(fgeom->shade_rect),   CC_SHADE,    fgeom);
  fill_button_rect (&(fgeom->above_rect),   CC_ABOVE,    fgeom);
  fill_button_rect (&(fgeom->stick_rect),   CC_STICK,    fgeom);
  fill_button_rect (&(fgeom->unshade_rect), CC_UNSHADE,  fgeom);
  fill_button_rect (&(fgeom->unabove_rect), CC_UNABOVE,  fgeom);
  fill_button_rect (&(fgeom->unstick_rect), CC_UNSTICK,  fgeom);

#if 0
  for (i=0; i<CC_LAST; i++)
    {
      g_warning("%20s %8d %8d %8d %8d",
                cowbell_human_names[i],
                fgeom->areas[i].x,
                fgeom->areas[i].y,
                fgeom->areas[i].width,
                fgeom->areas[i].height);
    }
#endif

  /* Finally, pick up the rounded corners. */
  {
    double d;
    int default_radius;
    ccss_style_t *style = ccss_stylesheet_query (theme->stylesheet,
                                                 (ccss_node_t*) &cowbell_nodes[CC_FRAME]);
   
    if (ccss_style_get_double (style, "border-radius", &d))
      default_radius = (int) d;
    else
      default_radius = 0;
    
    if (ccss_style_get_double (style, "border-top-left-radius", &d))
      fgeom->top_left_corner_rounded_radius = (int) d;
    else
      fgeom->top_left_corner_rounded_radius = default_radius;
    
    if (ccss_style_get_double (style, "border-top-right-radius", &d))
      fgeom->top_right_corner_rounded_radius = (int) d;
    else
      fgeom->top_right_corner_rounded_radius = default_radius;
    
    if (ccss_style_get_double (style, "border-bottom-left-radius", &d))
      fgeom->bottom_left_corner_rounded_radius = (int) d;
    else
      fgeom->bottom_left_corner_rounded_radius = default_radius;
    
    if (ccss_style_get_double (style, "border-bottom-right-radius", &d))
      fgeom->bottom_right_corner_rounded_radius = (int) d;
    else
      fgeom->bottom_right_corner_rounded_radius = default_radius;

  ccss_style_destroy (style);
  }
}

/*
 * This would be a stub, but actually the original code is
 * just what we need anyway.
 */
PangoFontDescription*
meta_gtk_widget_get_font_desc        (GtkWidget            *widget,
                                      double                scale,
                                      const PangoFontDescription *override)
{
  PangoFontDescription *font_desc;
  
  g_return_val_if_fail (GTK_WIDGET_REALIZED (widget), NULL);

  font_desc = pango_font_description_copy (widget->style->font_desc);

  if (override)
    pango_font_description_merge (font_desc, override, TRUE);

  pango_font_description_set_size (font_desc,
                                   MAX (pango_font_description_get_size (font_desc) * scale, 1));

  return font_desc;
}


/**
 * Returns the height of the letters in a particular font.
 *
 * \param font_desc  the font
 * \param context  the context of the font
 * \return  the height of the letters
 */
int
meta_pango_font_desc_get_text_height (const PangoFontDescription *font_desc,
                                      PangoContext         *context)
{
  PangoFontMetrics *metrics;
  PangoLanguage *lang;
  int retval;

  lang = pango_context_get_language (context);
  metrics = pango_context_get_metrics (context, font_desc, lang);

  retval = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) + 
                         pango_font_metrics_get_descent (metrics));
  
  pango_font_metrics_unref (metrics);
  
  return retval;
}
