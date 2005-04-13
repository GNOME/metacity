#ifndef __LMC_TYPES_H__
#define __LMC_TYPES_H__

#include <glib.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

typedef struct _LmcBorderInfo   LmcBorderInfo;
typedef struct _LmcPropertyValue LmcPropertyValue;

struct _LmcBorderInfo
{
  short left, right, top, bottom;
  short left_unscaled, right_unscaled, top_unscaled, bottom_unscaled;
};

struct _LmcPropertyValue
{
  Atom type;			/* None means property does not exist */
  int format;			/* 8, 16, 32 */
  
  union {
    unsigned char *b;
    short *s;
    long *l;
  } data;
  
  unsigned long n_items;	/* Number of 8, 16, or 32 bit quantities */
};

G_END_DECLS

#endif /* __LMC_TYPES_H__ */
