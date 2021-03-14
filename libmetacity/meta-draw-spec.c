/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <math.h>
#include <stdlib.h>

#include "meta-draw-spec-private.h"
#include "meta-theme.h"
#include "meta-theme-metacity-private.h"

typedef enum
{
  POS_TOKEN_INT,
  POS_TOKEN_DOUBLE,
  POS_TOKEN_OPERATOR,
  POS_TOKEN_VARIABLE,
  POS_TOKEN_OPEN_PAREN,
  POS_TOKEN_CLOSE_PAREN
} PosTokenType;

typedef enum
{
  POS_OP_NONE,
  POS_OP_ADD,
  POS_OP_SUBTRACT,
  POS_OP_MULTIPLY,
  POS_OP_DIVIDE,
  POS_OP_MOD,
  POS_OP_MAX,
  POS_OP_MIN
} PosOperatorType;

/**
 * A token, as output by the tokeniser.
 *
 * \ingroup tokenizer
 */
typedef struct
{
  PosTokenType type;

  union
  {
    struct {
      int val;
    } i;

    struct {
      double val;
    } d;

    struct {
      PosOperatorType op;
    } o;

    struct {
      char *name;
      GQuark name_quark;
    } v;

  } d;
} PosToken;

/**
 * A computed expression in our simple vector drawing language.
 * While it appears to take the form of a tree, this is actually
 * merely a list; concerns such as precedence of operators are
 * currently recomputed on every recalculation.
 *
 * Created by meta_draw_spec_new(), destroyed by meta_draw_spec_free().
 * pos_eval() fills this with ...FIXME. Are tokens a tree or a list?
 * \ingroup parser
 */
struct _MetaDrawSpec
{
  /**
   * If this spec is constant, this is the value of the constant;
   * otherwise it is zero.
   */
  gdouble value;

  /** A list of tokens in the expression. */
  PosToken *tokens;

  /** How many tokens are in the tokens list. */
  int n_tokens;

  /** Does the expression contain any variables? */
  gboolean constant : 1;
};

/**
 * The type of a PosExpr: either integer, double, or an operation.
 * \ingroup parser
 */
typedef enum
{
  POS_EXPR_INT,
  POS_EXPR_DOUBLE,
  POS_EXPR_OPERATOR
} PosExprType;

/**
 * Type and value of an expression in a parsed sequence. We don't
 * keep expressions in a tree; if this is of type POS_EXPR_OPERATOR,
 * the arguments of the operator will be in the array positions
 * immediately preceding and following this operator; they cannot
 * themselves be operators.
 *
 * \bug operator is char; it should really be of PosOperatorType.
 * \ingroup parser
 */
typedef struct
{
  PosExprType type;
  union
  {
    double double_val;
    int int_val;
    char operator;
  } d;
} PosExpr;

/**
 * Frees an array of tokens. All the tokens and their associated memory
 * will be freed.
 *
 * \param tokens  an array of tokens to be freed
 * \param n_tokens  how many tokens are in the array.
 */
static void
free_tokens (PosToken *tokens,
             int       n_tokens)
{
  int i;

  /* n_tokens can be 0 since tokens may have been allocated more than
   * it was initialized
   */

  for (i = 0; i < n_tokens; i++)
    if (tokens[i].type == POS_TOKEN_VARIABLE)
      g_free (tokens[i].d.v.name);

  g_free (tokens);
}

/**
 * Tokenises a number in an expression.
 *
 * \param p  a pointer into a string representing an operation; part of an
 *           expression somewhere, so not null-terminated
 * \param end_return  set to a pointer to the end of the number found; but
 *                    not updated if no number was found at all
 * \param next  set to either an integer or a float token
 * \param[out] err  set to the problem if there was a problem
 * \return TRUE if a valid number was found, FALSE otherwise (and "err" will
 *         have been set)
 *
 * \bug The "while (*start)..." part: what's wrong with strchr-ish things?
 * \bug The name is wrong: it doesn't parse anything.
 * \ingroup tokenizer
 */
static gboolean
parse_number (const char  *p,
              const char **end_return,
              PosToken    *next,
              GError     **err)
{
  const char *start = p;
  char *end;
  gboolean is_float;
  char *num_str;

  while (*p && (*p == '.' || g_ascii_isdigit (*p)))
    ++p;

  if (p == start)
    {
      char buf[7] = { '\0' };
      buf[g_unichar_to_utf8 (g_utf8_get_char (p), buf)] = '\0';
      g_set_error (err, META_THEME_ERROR, META_THEME_ERROR_BAD_CHARACTER,
                   _("Coordinate expression contains character '%s' which is not allowed"),
                   buf);
      return FALSE;
    }

  *end_return = p;

  /* we need this to exclude floats like "1e6" */
  num_str = g_strndup (start, p - start);
  start = num_str;
  is_float = FALSE;
  while (*start)
    {
      if (*start == '.')
        is_float = TRUE;
      ++start;
    }

  if (is_float)
    {
      next->type = POS_TOKEN_DOUBLE;
      next->d.d.val = g_ascii_strtod (num_str, &end);

      if (end == num_str)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression contains floating point number '%s' which could not be parsed"),
                       num_str);
          g_free (num_str);
          return FALSE;
        }
    }
  else
    {
      next->type = POS_TOKEN_INT;
      next->d.i.val = strtol (num_str, &end, 10);
      if (end == num_str)
        {
          g_set_error (err, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Coordinate expression contains integer '%s' which could not be parsed"),
                       num_str);
          g_free (num_str);
          return FALSE;
        }
    }

  g_free (num_str);

  g_assert (next->type == POS_TOKEN_INT || next->type == POS_TOKEN_DOUBLE);

  return TRUE;
}

/**
 * Parses a string and returns an operation.
 *
 * \param p  a pointer into a string representing an operation; part of an
 *           expression somewhere, so not null-terminated
 * \param len  set to the length of the string found. Set to 0 if none is.
 * \return  the operation found. If none was, returns POS_OP_NONE.
 */
static PosOperatorType
op_from_string (const char *p,
                int        *len)
{
  *len = 0;

  switch (*p)
    {
    case '+':
      *len = 1;
      return POS_OP_ADD;
    case '-':
      *len = 1;
      return POS_OP_SUBTRACT;
    case '*':
      *len = 1;
      return POS_OP_MULTIPLY;
    case '/':
      *len = 1;
      return POS_OP_DIVIDE;
    case '%':
      *len = 1;
      return POS_OP_MOD;

    case '`':
      if (strncmp (p, "`max`", 5) == 0)
        {
          *len = 5;
          return POS_OP_MAX;
        }
      else if (strncmp (p, "`min`", 5) == 0)
        {
          *len = 5;
          return POS_OP_MIN;
        }

    default:
      break;
    }

  return POS_OP_NONE;
}

/**
 * Tokenises an expression.
 *
 * \param      expr        The expression
 * \param[out] tokens_p    The resulting tokens
 * \param[out] n_tokens_p  The number of resulting tokens
 * \param[out] err  set to the problem if there was a problem
 *
 * \return  True if the expression was successfully tokenised; false otherwise.
 *
 * \ingroup tokenizer
 */
static gboolean
pos_tokenize (const char  *expr,
              PosToken   **tokens_p,
              int         *n_tokens_p,
              GError     **err)
{
  PosToken *tokens;
  int n_tokens;
  int allocated;
  const char *p;

  *tokens_p = NULL;
  *n_tokens_p = 0;

  allocated = 3;
  n_tokens = 0;
  tokens = g_new (PosToken, allocated);

  p = expr;
  while (*p)
    {
      PosToken *next;
      int len;

      if (n_tokens == allocated)
        {
          allocated *= 2;
          tokens = g_renew (PosToken, tokens, allocated);
        }

      next = &tokens[n_tokens];

      switch (*p)
        {
        case '*':
        case '/':
        case '+':
        case '-': /* negative numbers aren't allowed so this is easy */
        case '%':
        case '`':
          next->type = POS_TOKEN_OPERATOR;
          next->d.o.op = op_from_string (p, &len);
          if (next->d.o.op != POS_OP_NONE)
            {
              ++n_tokens;
              p = p + (len - 1); /* -1 since we ++p later */
            }
          else
            {
              g_set_error (err, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                           _("Coordinate expression contained unknown operator at the start of this text: \"%s\""),
                           p);

              goto error;
            }
          break;

        case '(':
          next->type = POS_TOKEN_OPEN_PAREN;
          ++n_tokens;
          break;

        case ')':
          next->type = POS_TOKEN_CLOSE_PAREN;
          ++n_tokens;
          break;

        case ' ':
        case '\t':
        case '\n':
          break;

        default:
          if (g_ascii_isalpha (*p) || *p == '_')
            {
              /* Assume variable */
              const char *start = p;
              while (*p && (g_ascii_isalpha (*p) || *p == '_'))
                ++p;
              g_assert (p != start);
              next->type = POS_TOKEN_VARIABLE;
              next->d.v.name = g_strndup (start, p - start);
              ++n_tokens;
              --p; /* since we ++p again at the end of while loop */
            }
          else
            {
              /* Assume number */
              const char *end;

              if (!parse_number (p, &end, next, err))
                goto error;

              ++n_tokens;
              p = end - 1; /* -1 since we ++p again at the end of while loop */
            }

          break;
        }

      ++p;
    }

  if (n_tokens == 0)
    {
      g_set_error (err, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Coordinate expression was empty or not understood"));

      goto error;
    }

  *tokens_p = tokens;
  *n_tokens_p = n_tokens;

  return TRUE;

 error:
  g_assert (err == NULL || *err != NULL);

  free_tokens (tokens, n_tokens);
  return FALSE;
}

/* To do this we tokenize, replace variable tokens
 * that are constants, then reassemble. The purpose
 * here is to optimize expressions so we don't do hash
 * lookups to eval them. Obviously it's a tradeoff that
 * slows down theme load times.
 */
static gboolean
replace_constants (MetaThemeMetacity  *metacity,
                   PosToken           *tokens,
                   int                 n_tokens,
                   GError            **err)
{
  int i;
  double dval;
  int ival;
  gboolean is_constant = TRUE;

  /* Loop through tokenized string looking for variables to replace */
  for (i = 0; i < n_tokens; i++)
    {
      PosToken *t = &tokens[i];

      if (t->type == POS_TOKEN_VARIABLE)
        {
          if (meta_theme_metacity_lookup_int (metacity, t->d.v.name, &ival))
            {
              g_free (t->d.v.name);
              t->type = POS_TOKEN_INT;
              t->d.i.val = ival;
            }
          else if (meta_theme_metacity_lookup_float (metacity, t->d.v.name, &dval))
            {
              g_free (t->d.v.name);
              t->type = POS_TOKEN_DOUBLE;
              t->d.d.val = dval;
            }
          else
            {
              /* If we've found a variable that cannot be replaced then the
                 expression is not a constant expression and we want to
                 replace it with a GQuark */

              t->d.v.name_quark = g_quark_from_string (t->d.v.name);
              is_constant = FALSE;
            }
        }
    }

  return is_constant;
}

/**
 * pos_eval_get_variable:
 * @token: The token representing a variable
 * @result: (out): The value of that variable; not set if the token did not
 *     represent a known variable
 * @env: The environment within which @token should be evaluated
 * @err: (out): Set to the problem if there was a problem
 *
 * There is a predefined set of variables which can appear in an expression.
 * Here we take a token representing a variable, and return the current value
 * of that variable in a particular environment.
 * (The value is always an integer.)
 *
 * Returns: %TRUE if we found the variable asked for, %FALSE if we didn't
 */
static gboolean
pos_eval_get_variable (const PosToken             *token,
                       gdouble                    *result,
                       const MetaPositionExprEnv  *env,
                       GError                    **err)
{
  GQuark quark;

  quark = token->d.v.name_quark;

  if (quark == g_quark_from_static_string ("width"))
    {
      *result = env->rect.width;
    }
  else if (quark == g_quark_from_static_string ("height"))
    {
      *result = env->rect.height;
    }
  else if (env->object_width >= 0 &&
           quark == g_quark_from_static_string ("object_width"))
    {
      *result = env->object_width;
    }
  else if (env->object_height >= 0 &&
           quark == g_quark_from_static_string ("object_height"))
    {
      *result = env->object_height;
    }
  else if (quark == g_quark_from_static_string ("left_width"))
    {
      *result = env->left_width;
    }
  else if (quark == g_quark_from_static_string ("right_width"))
    {
      *result = env->right_width;
    }
  else if (quark == g_quark_from_static_string ("top_height"))
    {
      *result = env->top_height;
    }
  else if (quark == g_quark_from_static_string ("bottom_height"))
    {
      *result = env->bottom_height;
    }
  else if (quark == g_quark_from_static_string ("mini_icon_width"))
    {
      *result = env->mini_icon_width;
    }
  else if (quark == g_quark_from_static_string ("mini_icon_height"))
    {
      *result = env->mini_icon_height;
    }
  else if (quark == g_quark_from_static_string ("icon_width"))
    {
      *result = env->icon_width;
    }
  else if (quark == g_quark_from_static_string ("icon_height"))
    {
      *result = env->icon_height;
    }
  else if (quark == g_quark_from_static_string ("title_width"))
    {
      *result = env->title_width;
    }
  else if (quark == g_quark_from_static_string ("title_height"))
    {
      *result = env->title_height;
    }
  else if (quark == g_quark_from_static_string ("frame_x_center"))
    {
      *result = env->frame_x_center;
    }
  else if (quark == g_quark_from_static_string ("frame_y_center"))
    {
      *result = env->frame_y_center;
    }
  else
    {
      g_set_error (err, META_THEME_ERROR, META_THEME_ERROR_UNKNOWN_VARIABLE,
                   _("Coordinate expression had unknown variable or constant '%s'"),
                   token->d.v.name);

      return FALSE;
    }

  return TRUE;
}

static gboolean
do_operation (PosExpr *a,
              PosExpr *b,
              PosOperatorType op,
              GError **err)
{
  /* Promote types to double if required */
  if (a->type == POS_EXPR_DOUBLE ||
      b->type == POS_EXPR_DOUBLE)
    {
      int int_val;

      if (a->type != POS_EXPR_DOUBLE)
        {
          int_val = a->d.int_val;

          a->type = POS_EXPR_DOUBLE;
          a->d.double_val = int_val;
        }

      if (b->type != POS_EXPR_DOUBLE)
        {
          int_val = b->d.int_val;

          b->type = POS_EXPR_DOUBLE;
          b->d.double_val = int_val;
        }
    }

  g_assert (a->type == b->type);

  if (a->type == POS_EXPR_INT)
    {
      switch (op)
        {
        case POS_OP_MULTIPLY:
          a->d.int_val = a->d.int_val * b->d.int_val;
          break;
        case POS_OP_DIVIDE:
          if (b->d.int_val == 0)
            {
              g_set_error (err, META_THEME_ERROR,
                           META_THEME_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.int_val = a->d.int_val / b->d.int_val;
          break;
        case POS_OP_MOD:
          if (b->d.int_val == 0)
            {
              g_set_error (err, META_THEME_ERROR,
                           META_THEME_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.int_val = a->d.int_val % b->d.int_val;
          break;
        case POS_OP_ADD:
          a->d.int_val = a->d.int_val + b->d.int_val;
          break;
        case POS_OP_SUBTRACT:
          a->d.int_val = a->d.int_val - b->d.int_val;
          break;
        case POS_OP_MAX:
          a->d.int_val = MAX (a->d.int_val, b->d.int_val);
          break;
        case POS_OP_MIN:
          a->d.int_val = MIN (a->d.int_val, b->d.int_val);
          break;
        case POS_OP_NONE:
        default:
          g_assert_not_reached ();
          break;
        }
    }
  else if (a->type == POS_EXPR_DOUBLE)
    {
      switch (op)
        {
        case POS_OP_MULTIPLY:
          a->d.double_val = a->d.double_val * b->d.double_val;
          break;
        case POS_OP_DIVIDE:
          if (b->d.double_val == 0.0)
            {
              g_set_error (err, META_THEME_ERROR,
                           META_THEME_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.double_val = a->d.double_val / b->d.double_val;
          break;
        case POS_OP_MOD:
          if (b->d.double_val == 0.0)
            {
              g_set_error (err, META_THEME_ERROR,
                           META_THEME_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.double_val = fmod (a->d.double_val, b->d.double_val);
          break;
        case POS_OP_ADD:
          a->d.double_val = a->d.double_val + b->d.double_val;
          break;
        case POS_OP_SUBTRACT:
          a->d.double_val = a->d.double_val - b->d.double_val;
          break;
        case POS_OP_MAX:
          a->d.double_val = MAX (a->d.double_val, b->d.double_val);
          break;
        case POS_OP_MIN:
          a->d.double_val = MIN (a->d.double_val, b->d.double_val);
          break;
        case POS_OP_NONE:
        default:
          g_assert_not_reached ();
          break;
        }
    }
  else
    g_assert_not_reached ();

  return TRUE;
}

/**
 * Represents an operation as a string.
 *
 * \param type  an operation, such as addition
 * \return  a string, such as "+"
 */
static const char*
op_name (PosOperatorType type)
{
  switch (type)
    {
    case POS_OP_ADD:
      return "+";
    case POS_OP_SUBTRACT:
      return "-";
    case POS_OP_MULTIPLY:
      return "*";
    case POS_OP_DIVIDE:
      return "/";
    case POS_OP_MOD:
      return "%";
    case POS_OP_MAX:
      return "`max`";
    case POS_OP_MIN:
      return "`min`";
    case POS_OP_NONE:
      break;
    default:
      break;
    }

  return "<unknown>";
}

static gboolean
do_operations (PosExpr *exprs,
               int     *n_exprs,
               int      precedence,
               GError **err)
{
  int i;

  i = 1;
  while (i < *n_exprs)
    {
      gboolean compress;

      /* exprs[i-1] first operand
       * exprs[i]   operator
       * exprs[i+1] second operand
       *
       * we replace first operand with result of mul/div/mod,
       * or skip over operator and second operand if we have
       * an add/subtract
       */

      if (exprs[i-1].type == POS_EXPR_OPERATOR)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression has an operator \"%s\" where an operand was expected"),
                       op_name (exprs[i-1].d.operator));
          return FALSE;
        }

      if (exprs[i].type != POS_EXPR_OPERATOR)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression had an operand where an operator was expected"));
          return FALSE;
        }

      if (i == (*n_exprs - 1))
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression ended with an operator instead of an operand"));
          return FALSE;
        }

      g_assert ((i+1) < *n_exprs);

      if (exprs[i+1].type == POS_EXPR_OPERATOR)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression has operator \"%c\" following operator \"%c\" with no operand in between"),
                       exprs[i+1].d.operator,
                       exprs[i].d.operator);
          return FALSE;
        }

      compress = FALSE;

      switch (precedence)
        {
        case 2:
          switch (exprs[i].d.operator)
            {
            case POS_OP_DIVIDE:
            case POS_OP_MOD:
            case POS_OP_MULTIPLY:
              compress = TRUE;
              if (!do_operation (&exprs[i-1], &exprs[i+1],
                                 exprs[i].d.operator,
                                 err))
                return FALSE;
              break;
            default:
              break;
            }
          break;
        case 1:
          switch (exprs[i].d.operator)
            {
            case POS_OP_ADD:
            case POS_OP_SUBTRACT:
              compress = TRUE;
              if (!do_operation (&exprs[i-1], &exprs[i+1],
                                 exprs[i].d.operator,
                                 err))
                return FALSE;
              break;
            default:
              break;
            }
          break;
          /* I have no rationale at all for making these low-precedence */
        case 0:
          switch (exprs[i].d.operator)
            {
            case POS_OP_MAX:
            case POS_OP_MIN:
              compress = TRUE;
              if (!do_operation (&exprs[i-1], &exprs[i+1],
                                 exprs[i].d.operator,
                                 err))
                return FALSE;
              break;
            default:
              break;
            }
          break;
        default:
          break;
        }

      if (compress)
        {
          /* exprs[i-1] first operand (now result)
           * exprs[i]   operator
           * exprs[i+1] second operand
           * exprs[i+2] new operator
           *
           * we move new operator just after first operand
           */
          if ((i+2) < *n_exprs)
            {
              memmove (&exprs[i], &exprs[i+2],
                       sizeof (PosExpr) * (*n_exprs - i - 2));
            }

          *n_exprs -= 2;
        }
      else
        {
          /* Skip operator and next operand */
          i += 2;
        }
    }

  return TRUE;
}

/**
 * Evaluates a sequence of tokens within a particular environment context,
 * and returns the current value. May recur if parantheses are found.
 *
 * \param tokens  A list of tokens to evaluate.
 * \param n_tokens  How many tokens are in the list.
 * \param env  The environment context in which to evaluate the expression.
 * \param[out] result  The current value of the expression
 *
 * \bug Yes, we really do reparse the expression every time it's evaluated.
 *      We should keep the parse tree around all the time and just
 *      run the new values through it.
 * \ingroup parser
 */
static gboolean
pos_eval_helper (PosToken                   *tokens,
                 int                         n_tokens,
                 const MetaPositionExprEnv  *env,
                 PosExpr                    *result,
                 GError                    **err)
{
  /* Lazy-ass hardcoded limit on number of terms in expression */
#define MAX_EXPRS 32
  int paren_level;
  int first_paren;
  int i;
  PosExpr exprs[MAX_EXPRS];
  int n_exprs;
  int precedence;

  /* Our first goal is to get a list of PosExpr, essentially
   * substituting variables and handling parentheses.
   */

  first_paren = 0;
  paren_level = 0;
  n_exprs = 0;
  for (i = 0; i < n_tokens; i++)
    {
      PosToken *t = &tokens[i];

      if (n_exprs >= MAX_EXPRS)
        {
          g_set_error (err, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Coordinate expression parser overflowed its buffer."));
          return FALSE;
        }

      if (paren_level == 0)
        {
          gdouble double_val;

          switch (t->type)
            {
            case POS_TOKEN_INT:
              exprs[n_exprs].type = POS_EXPR_INT;
              exprs[n_exprs].d.int_val = t->d.i.val;
              ++n_exprs;
              break;

            case POS_TOKEN_DOUBLE:
              exprs[n_exprs].type = POS_EXPR_DOUBLE;
              exprs[n_exprs].d.double_val = t->d.d.val;
              ++n_exprs;
              break;

            case POS_TOKEN_OPEN_PAREN:
              ++paren_level;
              if (paren_level == 1)
                first_paren = i;
              break;

            case POS_TOKEN_CLOSE_PAREN:
              g_set_error (err, META_THEME_ERROR, META_THEME_ERROR_BAD_PARENS,
                           _("Coordinate expression had a close parenthesis with no open parenthesis"));
              return FALSE;

            case POS_TOKEN_VARIABLE:
              exprs[n_exprs].type = env->scale > 1 ? POS_EXPR_DOUBLE : POS_EXPR_INT;

              /* FIXME we should just dump all this crap
               * in a hash, maybe keep width/height out
               * for optimization purposes
               */
              if (!pos_eval_get_variable (t, &double_val, env, err))
                return FALSE;

              if (env->scale > 1)
                exprs[n_exprs].d.double_val = double_val;
              else
                exprs[n_exprs].d.int_val = double_val;

              ++n_exprs;
              break;

            case POS_TOKEN_OPERATOR:
              exprs[n_exprs].type = POS_EXPR_OPERATOR;
              exprs[n_exprs].d.operator = t->d.o.op;
              ++n_exprs;
              break;

            default:
              break;
            }
        }
      else
        {
          g_assert (paren_level > 0);

          switch (t->type)
            {
            case POS_TOKEN_INT:
            case POS_TOKEN_DOUBLE:
            case POS_TOKEN_VARIABLE:
            case POS_TOKEN_OPERATOR:
              break;

            case POS_TOKEN_OPEN_PAREN:
              ++paren_level;
              break;

            case POS_TOKEN_CLOSE_PAREN:
              if (paren_level == 1)
                {
                  /* We closed a toplevel paren group, so recurse */
                  if (!pos_eval_helper (&tokens[first_paren+1],
                                        i - first_paren - 1,
                                        env,
                                        &exprs[n_exprs],
                                        err))
                    return FALSE;

                  ++n_exprs;
                }

              --paren_level;
              break;

            default:
              break;
            }
        }
    }

  if (paren_level > 0)
    {
      g_set_error (err, META_THEME_ERROR, META_THEME_ERROR_BAD_PARENS,
                   _("Coordinate expression had an open parenthesis with no close parenthesis"));
      return FALSE;
    }

  /* Now we have no parens and no vars; so we just do all the multiplies
   * and divides, then all the add and subtract.
   */
  if (n_exprs == 0)
    {
      g_set_error (err, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Coordinate expression doesn't seem to have any operators or operands"));
      return FALSE;
    }

  /* precedence 1 ops */
  precedence = 2;
  while (precedence >= 0)
    {
      if (!do_operations (exprs, &n_exprs, precedence, err))
        return FALSE;
      --precedence;
    }

  g_assert (n_exprs == 1);

  *result = *exprs;

  return TRUE;
}

/*
 *   expr = int | double | expr * expr | expr / expr |
 *          expr + expr | expr - expr | (expr)
 *
 *   so very not worth fooling with bison, yet so very painful by hand.
 */
/**
 * Evaluates an expression.
 *
 * \param spec  The expression to evaluate.
 * \param env   The environment context to evaluate the expression in.
 * \param[out] val_p  The integer value of the expression; if the expression
 *                    is of type float, this will be rounded. If we return
 *                    FALSE because the expression is invalid, this will be
 *                    zero.
 * \param[out] err    The error, if anything went wrong.
 *
 * \return  True if we evaluated the expression successfully; false otherwise.
 *
 * \bug Shouldn't spec be const?
 * \ingroup parser
 */
static gboolean
pos_eval (MetaDrawSpec              *spec,
          const MetaPositionExprEnv *env,
          gdouble                   *val_p,
          GError                   **err)
{
  PosExpr expr;

  *val_p = 0;

  if (pos_eval_helper (spec->tokens, spec->n_tokens, env, &expr, err))
    {
      switch (expr.type)
        {
        case POS_EXPR_INT:
          *val_p = expr.d.int_val;
          break;
        case POS_EXPR_DOUBLE:
          *val_p = expr.d.double_val;
          break;
        case POS_EXPR_OPERATOR:
        default:
          g_assert_not_reached ();
          break;
        }
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

/* We always return both X and Y, but only one will be meaningful in
 * most contexts.
 */
static gboolean
parse_position_expression (MetaDrawSpec               *spec,
                           const MetaPositionExprEnv  *env,
                           gdouble                    *x_return,
                           gdouble                    *y_return,
                           GError                    **err)
{
  /* All positions are in a coordinate system with x, y at the origin.
   * The expression can have -, +, *, / as operators, floating point
   * or integer constants, and the variables "width" and "height" and
   * optionally "object_width" and object_height". Negative numbers
   * aren't allowed.
   */
  gdouble val;

  if (spec->constant)
    val = spec->value;
  else
    {
      if (pos_eval (spec, env, &spec->value, err) == FALSE)
        {
          g_assert (err == NULL || *err != NULL);
          return FALSE;
        }

      val = spec->value;
    }

  if (x_return)
    *x_return = env->rect.x + val;
  if (y_return)
    *y_return = env->rect.y + val;

  return TRUE;
}

static gboolean
parse_size_expression (MetaDrawSpec               *spec,
                       const MetaPositionExprEnv  *env,
                       gdouble                    *val_return,
                       GError                    **err)
{
  gdouble val;

  if (spec->constant)
    val = spec->value;
  else
    {
      if (pos_eval (spec, env, &spec->value, err) == FALSE)
        {
          g_assert (err == NULL || *err != NULL);
          return FALSE;
        }

      val = spec->value;
    }

  if (val_return)
    *val_return = MAX (val, 1); /* require that sizes be at least 1x1 */

  return TRUE;
}

MetaDrawSpec *
meta_draw_spec_new (MetaThemeMetacity  *metacity,
                    const gchar        *expr,
                    GError            **error)
{
  MetaDrawSpec *spec;

  spec = g_new0 (MetaDrawSpec, 1);

  pos_tokenize (expr, &spec->tokens, &spec->n_tokens, NULL);

  spec->constant = replace_constants (metacity, spec->tokens,
                                      spec->n_tokens, NULL);

  if (spec->constant)
    {
      gboolean result;

      result = pos_eval (spec, NULL, &spec->value, error);

      if (result == FALSE)
        {
          meta_draw_spec_free (spec);
          return NULL;
        }
    }

  return spec;
}

void
meta_draw_spec_free (MetaDrawSpec *spec)
{
  if (!spec)
    return;

  free_tokens (spec->tokens, spec->n_tokens);
  g_free (spec);
}

gdouble
meta_draw_spec_parse_x_position (MetaDrawSpec              *spec,
                                 const MetaPositionExprEnv *env)
{
  gdouble retval;
  GError *error;

  retval = 0;
  error = NULL;
  if (!parse_position_expression (spec, env, &retval, NULL, &error))
    {
      g_warning (_("Theme contained an expression that resulted in an error: %s"),
                 error->message);

      g_error_free (error);
    }

  return retval;
}

gdouble
meta_draw_spec_parse_y_position (MetaDrawSpec              *spec,
                                 const MetaPositionExprEnv *env)
{
  gdouble retval;
  GError *error;

  retval = 0;
  error = NULL;
  if (!parse_position_expression (spec, env, NULL, &retval, &error))
    {
      g_warning (_("Theme contained an expression that resulted in an error: %s"),
                 error->message);

      g_error_free (error);
    }

  return retval;
}

gdouble
meta_draw_spec_parse_size (MetaDrawSpec              *spec,
                           const MetaPositionExprEnv *env)
{
  gdouble retval;
  GError *error;

  retval = 0;
  error = NULL;
  if (!parse_size_expression (spec, env, &retval, &error))
    {
      g_warning (_("Theme contained an expression that resulted in an error: %s"),
                 error->message);

      g_error_free (error);
    }

  return retval;
}
