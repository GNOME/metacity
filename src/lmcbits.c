#include "lmcbits.h"

LmcBits *
lmc_bits_new (LmcBitsFormat  format,
	      int            width,
	      int            height,
	      guchar        *data,
	      int            rowstride,
	      GDestroyNotify notify,
	      gpointer       notify_data)
{
  LmcBits *bits = g_new0 (LmcBits, 1);
  
  bits->ref_count = 1;
  
  bits->format = format;
  bits->width = width;
  bits->height = height;
  bits->data_ = data;
  bits->rowstride = rowstride;
  bits->notify = notify;
  bits->notify_data = notify_data;

  return bits;
}

LmcBits *
lmc_bits_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  return lmc_bits_new (gdk_pixbuf_get_has_alpha (pixbuf) ? LMC_BITS_RGBA_MSB_32 : LMC_BITS_RGB_24,
		       gdk_pixbuf_get_width (pixbuf),
		       gdk_pixbuf_get_height (pixbuf),
		       gdk_pixbuf_get_pixels (pixbuf),
		       gdk_pixbuf_get_rowstride (pixbuf),
		       (GDestroyNotify)g_object_unref,
		       g_object_ref (pixbuf));
}

LmcBits *
lmc_bits_ref (LmcBits *bits)
{
  g_atomic_int_inc (&bits->ref_count);

  return bits;
}

void
lmc_bits_unref (LmcBits *bits)
{
  if (g_atomic_int_dec_and_test (&bits->ref_count))
    {
      if (bits->notify)
	bits->notify (bits->notify_data);
      
      g_free (bits);
    }
}


guchar *
lmc_bits_lock (LmcBits *bits)
{
  guchar *result;
  
  result = bits->data_;

  return result;
}

void
lmc_bits_unlock (LmcBits *bits)
{
}

gboolean
lmc_bits_has_alpha (LmcBits *bits)
{
  switch (bits->format)
    {
    case LMC_BITS_RGB_16:
      return FALSE;
    case LMC_BITS_RGB_24:
      return FALSE;
    case LMC_BITS_RGB_32:
      return FALSE;
    case LMC_BITS_RGBA_MSB_32:
      return TRUE;
    case LMC_BITS_ARGB_32:
      return TRUE;
    default:
      return FALSE;
    }
}
