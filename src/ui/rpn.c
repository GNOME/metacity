/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
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

/**
 * \file rpn.c    Internal representation of expressions
 *
 * Expressions found in theme files, such as
 *   "1 `max` ((height-title_height)/2)-1"
 * are stored internally in reverse Polish notation, here as
 *   1.000 height title_height - 2.000 / 1.000 - `max`
 *
 * This file provides functions for turning Metacity expressions
 * into RPN, and for evaluating RPN.
 */

#include <config.h>
#include "rpn.h"
#include "util.h"
#include <glib.h>
#include <string.h>
#include <stdio.h>

/* integers are stored this many times larger.
 * this lets us not use float arithmetic, for speed. */
#define SCALE_BITS 16
#define SCALE (1<<SCALE_BITS)

/* this is the size of the stack used while converting expressions
 * to RPN, and also the size of the evaluation stack, and the
 * maximum size of the final result.  if you go over this,
 * a run-time error will result.
 */
#define STACK_SIZE 100

enum MetaToken {
  META_TOKEN_INT = 0x7FFFFFFF,
  META_TOKEN_OP_BASE = 0x80000000,
  META_TOKEN_DONE,
  META_TOKEN_ADD,
  META_TOKEN_SUBTRACT,
  META_TOKEN_MULTIPLY,
  META_TOKEN_DIVIDE,
  META_TOKEN_MIN,
  META_TOKEN_MAX,
  /**
   * HALVE is an optimisation: it halves its first argument, since
   * division is slow and bitshifting is fast, and halving is something
   * that's very common in themes.  It's still a binary operator,
   * to save us having to invent unary operators for this one case.
   * The second argument (which should be 2) is thrown away.
   */
  META_TOKEN_HALVE,
  META_TOKEN_OPEN, /* only during parse */
  META_TOKEN_CLOSE, /* only during parse */
  /* Variables, which are treated as 0-ary operators */
  META_TOKEN_width,
  META_TOKEN_height,
  META_TOKEN_object_width,
  META_TOKEN_object_height,
  META_TOKEN_left_width,
  META_TOKEN_right_width,
  META_TOKEN_top_height,
  META_TOKEN_bottom_height,
  META_TOKEN_mini_icon_width,
  META_TOKEN_mini_icon_height,
  META_TOKEN_icon_width,
  META_TOKEN_icon_height,
  META_TOKEN_title_width,
  META_TOKEN_title_height
};

#ifdef DEBUG_RPN
static char *
meta_token_as_string (MetaToken token)
{
  if (token & META_TOKEN_OP_BASE)
    {
      switch (token)
	{
	case META_TOKEN_ADD: return g_strdup_printf ("+");
	case META_TOKEN_SUBTRACT: return g_strdup_printf ("-");
	case META_TOKEN_MULTIPLY: return g_strdup_printf ("*");
	case META_TOKEN_DIVIDE: return g_strdup_printf ("/");
	case META_TOKEN_MIN: return g_strdup_printf ("`min`");
	case META_TOKEN_MAX: return g_strdup_printf ("`max`");
	case META_TOKEN_HALVE: return g_strdup_printf ("/(halve)");
	case META_TOKEN_OPEN: return g_strdup_printf ("(");
	case META_TOKEN_CLOSE: return g_strdup_printf (")");

#define name_token(t) case META_TOKEN_##t: return g_strdup_printf(#t);
	name_token (width);
	name_token (height);
	name_token (object_width);
	name_token (object_height);
	name_token (left_width);
	name_token (right_width);
	name_token (top_height);
	name_token (bottom_height);
	name_token (mini_icon_width);
	name_token (mini_icon_height);
	name_token (icon_width);
	name_token (icon_height);
	name_token (title_width);
	name_token (title_height);
#undef name_token
	default: return g_strdup_printf ("unknown-op(%x)", token);
	}
    }
  else
    return g_strdup_printf ("%0.3f", ((double)token/SCALE));
}

static char *
meta_token_list_as_string (MetaToken *token)
{
  char *result = NULL;

  while (*token != META_TOKEN_DONE) {

    gchar *temp = meta_token_as_string (*token);

    gchar *new_result = g_strconcat (result? result: "",
			  result? " " : "",
			  temp,
			  NULL);
    g_free (result);
    result = new_result;

    g_free (temp);

    token++;
  }

  return result;
}
#endif

static void
parse_error (const char* message, GScanner *scanner, const char *expr)
{
  gchar *expr2 = g_strdup (expr);
  gint position = g_scanner_cur_position (scanner);

  expr2[position] = 0; /* why doesn't g_warning understand %*s? */

  meta_bug ("%s at %s<-- %s %s",
            message,
            expr2,
            /* Translators: This appears beside an error inside a
             * theme to show where the error is.
             */
            _("HERE"),
            expr+position);
  g_free (expr2);
}

static guint64 add(guint64 a, guint64 b) { return a+b; }
static guint64 subtract(guint64 a, guint64 b) { return b>a?0:a-b; }
static guint64 multiply(guint64 a, guint64 b) { return a*(b>>SCALE_BITS); }
static guint64 divide(guint64 a, guint64 b) { return b==0?0:a/(b>>SCALE_BITS); }
static guint64 min(guint64 a, guint64 b) { return a<b?a:b; }
static guint64 max(guint64 a, guint64 b) { return a>b?a:b; }
static guint64 halve(guint64 a, guint64 b) { return a>>1; }

static guint64 get_width(const MetaPositionExprEnv *env) { return env->rect.width; }
static guint64 get_height(const MetaPositionExprEnv *env) { return env->rect.height; }
#define name_token(t) static guint64 get_##t(const MetaPositionExprEnv *env) { return env->t; }
name_token (object_width)
name_token (object_height)
name_token (left_width)
name_token (right_width)
name_token (top_height)
name_token (bottom_height)
name_token (mini_icon_width)
name_token (mini_icon_height)
name_token (icon_width)
name_token (icon_height)
name_token (title_width)
name_token (title_height)
#undef name_token

typedef guint64 (BinaryOp (guint64, guint64));
typedef guint64 (Variable (const MetaPositionExprEnv*));

typedef struct TokenHandler {
  BinaryOp *binary;
  Variable *variable;
} TokenHandler;

const TokenHandler handlers[] = {
  { NULL }, /* BASE */
  { NULL }, /* DONE */
  { add, NULL },
  { subtract, NULL },
  { multiply, NULL },
  { divide, NULL },
  { min, NULL },
  { max, NULL },
  { halve, NULL },
  { NULL }, /* OPEN */
  { NULL }, /* CLOSE */
#define name_token(t) { NULL, get_##t },
name_token (width)
name_token (height)
name_token (object_width)
name_token (object_height)
name_token (left_width)
name_token (right_width)
name_token (top_height)
name_token (bottom_height)
name_token (mini_icon_width)
name_token (mini_icon_height)
name_token (icon_width)
name_token (icon_height)
name_token (title_width)
name_token (title_height)
#undef name_token
};

#ifdef DEBUG_RPN
static void
token_verbose_real (const char* message, MetaToken token)
{
  gchar *str = meta_token_as_string (token);
  gchar *message2 = g_strconcat (message, ": %s\n", NULL);

  g_print (message2, str);

  g_free (message2);
  g_free (str);
}

#define token_verbose(a,b) token_verbose_real(a,b)
#else
#define token_verbose(a,b) ;
#endif

/**
 * If the top two entries on the output are constants, performs "op" on the
 * constants, replaces them with the result, and returns META_TOKEN_DONE.
 * Otherwise returns "tokenop".
 * This means that we don't waste our time outputting, say, "2 2 add"
 * when we could be outputting "4".
 *
 * @param output  an array of MetaTokens
 * @param output_pointer  pointer to an index in the array
 *                        (it's a pointer so that we can change its value)
 * @param op  what operation needs to be carried out
 *
 * @bug there is no effective way yet to debug this
 */
static void
accept (MetaToken *output, gint *output_pointer, MetaToken item)
{
  if (
      (item & META_TOKEN_OP_BASE) && /* it's an op */
      handlers[item ^ META_TOKEN_OP_BASE].binary && /* it's a binary op */
      *output_pointer >= 2 && /* there's more than two things output */
      !(output[(*output_pointer)-1] & META_TOKEN_OP_BASE) &&
      !(output[(*output_pointer)-2] & META_TOKEN_OP_BASE)
      /* the most recent two are constants */
      )
    {
      MetaToken result =
        handlers[item ^ META_TOKEN_OP_BASE].binary(
                                                   output[(*output_pointer)-1],
                                                   output[(*output_pointer)-2]
                                                   );

      token_verbose ("Accept calculated", result & META_TOKEN_INT);

      output[(*output_pointer)-2] = result & META_TOKEN_INT;
      (*output_pointer)--;
    }
  else
    {
      if (*output_pointer>STACK_SIZE)
          meta_bug ("Parse stack overflow"); /* FIXME THIS IS BAD */

      /* Special case: */
      if (item==META_TOKEN_DIVIDE &&
          *output_pointer != 0 &&
          output[(*output_pointer)-1]==2<<SCALE_BITS)
        item = META_TOKEN_HALVE;

      token_verbose ("Accept", item);
      output[(*output_pointer)++] = item;
    }
}

static gint
precedence (MetaToken token)
{
  switch (token)
    {
    case META_TOKEN_MULTIPLY:
    case META_TOKEN_DIVIDE:
    case META_TOKEN_HALVE:
      return 4;
    case META_TOKEN_ADD:
    case META_TOKEN_SUBTRACT:
      return 3;
    case META_TOKEN_MIN:
    case META_TOKEN_MAX:
      return 2;
    case META_TOKEN_OPEN:
    case META_TOKEN_CLOSE:
      return 1;
    default:
      return 0;
    }
}

static gboolean
is_bracket (MetaToken token)
{
  return token==META_TOKEN_OPEN || token==META_TOKEN_CLOSE;
}

#define check_for_overflow(v) { if (v+1>=STACK_SIZE) { \
      parse_error ("Stack overflow", scanner, expr); return NULL; } }

MetaToken *
meta_rpn_parse (MetaTheme *theme,
                const char *expr,
                GError **err)
{
  GScannerConfig scanner_config = {
    " \n\t", /* cset_skip_characters; */
    G_CSET_a_2_z G_CSET_A_2_Z, /* cset_identifier_first */
    G_CSET_a_2_z G_CSET_A_2_Z "_", /* cset_identifier_nth */
    "", /* cpair_comment_single -- no comments! */
    TRUE, /* case_sensitive */
    FALSE, /* no comments */
    FALSE, /* no comments */
    FALSE, /* no comments */
    TRUE, /* pick up identifiers */
    TRUE, /* even single-char ones */
    FALSE, /* "NULL" isn't special */
    TRUE, /* we have symbols */
    FALSE, /* no binary */
    FALSE, /* no octal */
    TRUE, /* we do have floats */
    FALSE, /* but no hex */
    FALSE, /* still no hex */
    FALSE, /* also no strings */
    FALSE, /* still no strings */
    TRUE, /* makes no difference whether hex is an int */
    FALSE, /* we don't allow chars anyway */
    FALSE, /* return actual symbols */
    FALSE, /* don't bother with default scope */
    FALSE, /* just ordinary ints */
  };
  GScanner *scanner = g_scanner_new (&scanner_config);
  MetaToken stack[STACK_SIZE];
  gint stack_pointer = 0;
  MetaToken result[STACK_SIZE];
  gint result_pointer = 0;

  char *identifiers[] = {
			"min",
			"max",
#define name_token(t) #t,
	name_token (width)
	name_token (height)
	name_token (object_width)
	name_token (object_height)
	name_token (left_width)
	name_token (right_width)
	name_token (top_height)
	name_token (bottom_height)
	name_token (mini_icon_width)
	name_token (mini_icon_height)
	name_token (icon_width)
	name_token (icon_height)
	name_token (title_width)
	name_token (title_height)
#undef name_token
			NULL,
  };
  char **cursor;

  for (cursor = identifiers; *cursor; cursor++) {
    g_scanner_scope_add_symbol (scanner, 0, *cursor, *cursor);
  }

  g_scanner_input_text (scanner, expr, strlen (expr));

  while (g_scanner_get_next_token (scanner))
    {
      MetaToken token;

      switch (g_scanner_cur_token (scanner)) {
      case G_TOKEN_FLOAT:
	token =
	  (glong) (g_scanner_cur_value (scanner).v_float*SCALE) &
	  META_TOKEN_INT;

	token_verbose ("Accept float", token);
	check_for_overflow (result_pointer);
	result[result_pointer++] = token;
	break;
      case G_TOKEN_INT:
	token = g_scanner_cur_value (scanner).v_int<<SCALE_BITS &
	  META_TOKEN_INT;

	token_verbose ("Accept integer", token);
	check_for_overflow (result_pointer);
	result[result_pointer++] = token;
	break;
      case G_TOKEN_CHAR:
	switch (g_scanner_cur_value (scanner).v_char)
	  {

	  case '+': token = META_TOKEN_ADD; break;
	  case '-': token = META_TOKEN_SUBTRACT; break;
	  case '*': token = META_TOKEN_MULTIPLY; break;
	  case '/': token = META_TOKEN_DIVIDE; break;

	  case '(': token = META_TOKEN_OPEN; break;
	  case ')': token = META_TOKEN_CLOSE; break;
          case '`':
            /* used as a separator so people can continue to write "foo`max`bar" */
            token = META_TOKEN_DONE; break;
	  default:
	    parse_error ("Unknown character", scanner, expr);
	    return NULL;
	  }

        if (token==META_TOKEN_DONE) break; /* see above */

	switch (token)
	  {
	  case META_TOKEN_OPEN:
	    /* do nothing */
	    break;

	  case META_TOKEN_CLOSE:
            token_verbose ("Closing up", token);
	    while (stack_pointer>0 && stack[stack_pointer-1] != META_TOKEN_OPEN) {
	      stack_pointer--;
              accept (result, &result_pointer, stack[stack_pointer]);
	    }

            if (stack_pointer>0) stack_pointer--; /* and skip the open bracket */
	    break;

	  default:
	    while (stack_pointer>0 && precedence(stack[stack_pointer-1]) >= precedence(token)) {
	      stack_pointer--;
	      token_verbose ("Pop for precedence", stack[stack_pointer]);
	      if (!is_bracket(stack[stack_pointer])) {
                accept (result, &result_pointer, stack[stack_pointer]);
	      }
	    }
	  }

	if (token!=META_TOKEN_CLOSE) {
	  token_verbose ("Push", token);
	  check_for_overflow (stack_pointer);
	  stack[stack_pointer++] = token;
	}
        break;
      case G_TOKEN_SYMBOL:      

#define check_token(t) (strcmp (g_scanner_cur_value (scanner).v_symbol, #t)==0) \
	  token=META_TOKEN_##t 

	if (strcmp (g_scanner_cur_value (scanner).v_symbol, "min")==0)
	  token = META_TOKEN_MIN;
	else if (strcmp (g_scanner_cur_value (scanner).v_symbol, "max")==0)
	  token = META_TOKEN_MAX;
	else if check_token (width);
	else if check_token (height);
	else if check_token (object_width);
	else if check_token (object_height);
	else if check_token (left_width);
	else if check_token (right_width);
	else if check_token (top_height);
	else if check_token (bottom_height);
	else if check_token (mini_icon_width);
	else if check_token (mini_icon_height);
	else if check_token (icon_width);
	else if check_token (icon_height);
	else if check_token (title_width);
	else if check_token (title_height);

#undef check_token

	else
	  {
	    parse_error ("Unknown symbol", scanner, expr);
	    return NULL;
	  }

        if (token==META_TOKEN_DONE) break; /* it was constant */            

	if (token==META_TOKEN_MIN || token==META_TOKEN_MAX)
	  {
	    while (stack_pointer>0 && precedence(stack[stack_pointer-1]) >= precedence(token)) {
	      stack_pointer--;
	      token_verbose ("Pop for precedence", stack[stack_pointer]);
	      if (!is_bracket(stack[stack_pointer])) {
		token_verbose ("  ... and accept", stack[stack_pointer]);
                accept (result, &result_pointer, stack[stack_pointer]);
	      }
	    }
	    token_verbose ("Push", token);
	    check_for_overflow (stack_pointer);
	    stack[stack_pointer++] = token;
	  }
	else {
	  token_verbose ("Accept token", token);
          /* don't bother using accept(): it can never be >0-ary */
	  check_for_overflow (result_pointer);
	  result[result_pointer++] = token;
	}
	break;

      case G_TOKEN_IDENTIFIER:
        {
          double f;
          int i;

          if (meta_theme_lookup_int_constant (theme,
                                              g_scanner_cur_value (scanner).v_identifier,
                                              &i))
            {
              token = i<<SCALE_BITS & META_TOKEN_INT;
              token_verbose ("Accept const integer", token);
              check_for_overflow (result_pointer);
              result[result_pointer++] = token;
            }
          else if (meta_theme_lookup_float_constant (theme,
                                                     g_scanner_cur_value (scanner).v_identifier,
                                                     &f))
            {
              token = (glong)(f*SCALE) & META_TOKEN_INT;

              token_verbose ("Accept const float", token);
              check_for_overflow (result_pointer);
              result[result_pointer++] = token;
            }
          else
            {
              parse_error ("Unknown identifier", scanner, expr);
              return NULL;
            }
        }
        break;

      default:
	parse_error ("Unknown state", scanner, expr);
	return NULL;
      }

    }

  while (stack_pointer>0)
    if (!is_bracket (stack[--stack_pointer])) {
      token_verbose ("Final accept", stack[stack_pointer]);
      accept (result, &result_pointer, stack[stack_pointer]);
    }

  result[result_pointer++] = META_TOKEN_DONE;

  g_scanner_destroy (scanner);

  return g_memdup (result, sizeof(MetaToken)*result_pointer);
}

gboolean inline
meta_rpn_eval (MetaToken *expr,
	       const MetaPositionExprEnv *env,
	       int *result,
	       GError **err)
{
  if (*(expr+1) == META_TOKEN_DONE)
    {
      /* commonest case is "number, DONE";
       * there must be at least two tokens, so we know
       * we're okay to look ahead by one
       */
      if (*(expr) & META_TOKEN_OP_BASE)
        {
          /* must be a variable; you can't legally
           * do, say, nothing but "add, DONE"
           */
          *result = handlers[*expr ^ META_TOKEN_OP_BASE].variable (env);
        }
      else
          *result = (*expr) >> SCALE_BITS;

      return TRUE;
    }
  else
    {
      MetaToken *cursor = expr;
      /* This is not a stack of MetaTokens because... FIXME */
      gint64 stack[STACK_SIZE];
      gint64* stack_pointer = &(stack[0]);
      
      while (*cursor != META_TOKEN_DONE)
        {
          if (*cursor & META_TOKEN_OP_BASE)
            {
              guint64 h = *cursor ^ META_TOKEN_OP_BASE;
              if (handlers[h].binary)
                {
                  *(stack_pointer-2) = 
                    handlers[h].binary (*(stack_pointer-2),
                                        *(stack_pointer-1));
                  stack_pointer--;
                }
              else if (handlers[h].variable)
                {
                  *(stack_pointer++) =
                    handlers[h].variable (env) << SCALE_BITS;
                }
              else
                {
                  /* should never happen */
                  meta_bug ("Invalid token found %x", (unsigned) h);
                  return FALSE;
                }
            }
          else
            *(stack_pointer++) = *cursor;

          cursor++;
        }

      *result = *(--stack_pointer) >> SCALE_BITS;

      return TRUE;
    }
}
