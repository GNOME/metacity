#ifndef __LMC_BITS_H__
#define __LMC_BITS_H__

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef struct _LmcBits LmcBits;

typedef enum {
  LMC_BITS_RGB_16,
  LMC_BITS_RGB_24,
  LMC_BITS_RGB_32,
  LMC_BITS_RGBA_MSB_32,
  LMC_BITS_ARGB_32
} LmcBitsFormat;

struct _LmcBits
{
  int ref_count;
  
  LmcBitsFormat format;
  int width;
  int height;
  int rowstride;

  GDestroyNotify notify;
  gpointer notify_data;

  guchar *data_;
};

LmcBits *lmc_bits_new (LmcBitsFormat  format,
		       int            width,
		       int            height,
		       guchar        *data,
		       int            rowstride,
		       GDestroyNotify notify,
		       gpointer       notify_data);

LmcBits *lmc_bits_new_from_pixbuf (GdkPixbuf *pixbuf);

LmcBits *lmc_bits_ref (LmcBits *bits);
void lmc_bits_unref (LmcBits *bits);

guchar *lmc_bits_lock   (LmcBits *bits);
void    lmc_bits_unlock (LmcBits *bits);

gboolean lmc_bits_has_alpha (LmcBits *bits);

G_END_DECLS

#endif /* __LMC_BITS_H__ */
