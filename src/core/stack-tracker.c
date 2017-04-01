/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2009 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <string.h>

#include "frame-private.h"
#include "meta-compositor.h"
#include "screen-private.h"
#include "stack-tracker.h"
#include "util.h"

/* The complexity here comes from resolving two competing factors:
 *
 *  - We need to have a view of the stacking order that takes into
 *    account everything we have done without waiting for events
 *    back from the X server; we don't want to draw intermediate
 *    partially-stacked stack states just because we haven't received
 *    some notification yet.
 *
 *  - Only the X server has an accurate view of the complete stacking;
 *    when we make a request to restack windows, we don't know how
 *    it will affect override-redirect windows, because at any point
 *    applications may restack these windows without our involvement.
 *
 * The technique we use is that we keep three sets of information:
 *
 *  - The stacking order on the server as known from the last
 *    event we received.
 *  - A queue of stacking requests that *we* made subsequent to
 *    that last event.
 *  - A predicted stacking order, derived from applying the queued
 *    requests to the last state from the server.
 *
 * When we receive a new event: a) we compare the serial in the event to
 * the serial of the queued requests and remove any that are now
 * no longer pending b) if necessary, drop the predicted stacking
 * order to recompute it at the next opportunity.
 *
 * Possible optimizations:
 *  Keep the stacks as an array + reverse-mapping hash table to avoid
 *    linear lookups.
 *  Keep the stacks as a GList + reverse-mapping hash table to avoid
 *    linear lookups and to make restacking constant-time.
 */

typedef union _MetaStackOp MetaStackOp;

typedef enum {
  STACK_OP_ADD,
  STACK_OP_REMOVE,
  STACK_OP_RAISE_ABOVE,
  STACK_OP_LOWER_BELOW
} MetaStackOpType;

/* MetaStackOp represents a "stacking operation" - a change to
 * apply to a window stack. Depending on the context, it could
 * either reflect a request we have sent to the server, or a
 * notification event we received from the X server.
 */
union _MetaStackOp
{
  struct {
    MetaStackOpType type;
    gulong serial;
  } any;
  struct {
    MetaStackOpType type;
    gulong serial;
    Window window;
  } add;
  struct {
    MetaStackOpType type;
    gulong serial;
    Window window;
  } remove;
  struct {
    MetaStackOpType type;
    gulong serial;
    Window window;
    Window sibling;
  } raise_above;
  struct {
    MetaStackOpType type;
    gulong serial;
    Window window;
    Window sibling;
  } lower_below;
};

struct _MetaStackTracker
{
  MetaScreen *screen;

  /* This is the last state of the stack as based on events received
   * from the X server.
   */
  GArray *xserver_stack;

  /* This is the serial of the last request we made that was reflected
   * in xserver_stack.
   */
  gulong xserver_serial;

  /* A stack without any unverified operations applied.
   */
  GArray *verified_stack;

  /* This is a queue of requests we've made to change the stacking order,
   * where we haven't yet gotten a reply back from the server.
   */
  GQueue *unverified_predictions;

  /* This is how we think the stack is, based on verified_stack, and
   * on the unverified_predictions we've made subsequent to
   * verified_stack.
   */
  GArray *predicted_stack;

  /* Idle function used to sync the compositor's view of the window
   * stack up with our best guess before a frame is drawn.
   */
  guint sync_stack_idle;
};

static void
meta_stack_op_dump (MetaStackOp *op,
                    const char  *prefix,
                    const char  *suffix)
{
  switch (op->any.type)
    {
    case STACK_OP_ADD:
      meta_topic (META_DEBUG_STACK, "%sADD(%#lx; %ld)%s",
                  prefix, op->add.window, op->any.serial, suffix);
      break;
    case STACK_OP_REMOVE:
      meta_topic (META_DEBUG_STACK, "%sREMOVE(%#lx; %ld)%s",
                  prefix, op->add.window, op->any.serial, suffix);
      break;
    case STACK_OP_RAISE_ABOVE:
      meta_topic (META_DEBUG_STACK, "%sRAISE_ABOVE(%#lx, %#lx; %ld)%s",
                  prefix,
                  op->raise_above.window, op->raise_above.sibling,
                  op->any.serial,
                  suffix);
      break;
    case STACK_OP_LOWER_BELOW:
      meta_topic (META_DEBUG_STACK, "%sLOWER_BELOW(%#lx, %#lx; %ld)%s",
                  prefix,
                  op->lower_below.window, op->lower_below.sibling,
                  op->any.serial,
                  suffix);
      break;
    default:
      break;
    }
}

static void
stack_dump (GArray *stack)
{
  guint i;

  meta_push_no_msg_prefix ();
  for (i = 0; i < stack->len; i++)
    {
      meta_topic (META_DEBUG_STACK, " %#lx", g_array_index (stack, Window, i));
    }
  meta_topic (META_DEBUG_STACK, "\n");
  meta_pop_no_msg_prefix ();
}

static void
meta_stack_tracker_dump (MetaStackTracker *tracker)
{
  GList *l;

  meta_topic (META_DEBUG_STACK, "MetaStackTracker state (screen=%d)\n", tracker->screen->number);
  meta_push_no_msg_prefix ();
  meta_topic (META_DEBUG_STACK, "  xserver_serial: %ld\n", tracker->xserver_serial);
  meta_topic (META_DEBUG_STACK, "  xserver_stack: ");
  stack_dump (tracker->xserver_stack);
  meta_topic (META_DEBUG_STACK, "  verfied_stack: ");
  stack_dump (tracker->verified_stack);
  meta_topic (META_DEBUG_STACK, "  unverified_predictions: [");
  for (l = tracker->unverified_predictions->head; l; l = l->next)
    {
      MetaStackOp *op = l->data;
      meta_stack_op_dump (op, "", l->next ? ", " : "");
    }
  meta_topic (META_DEBUG_STACK, "]\n");
  if (tracker->predicted_stack)
    {
      meta_topic (META_DEBUG_STACK, "\n  predicted_stack: ");
      stack_dump (tracker->predicted_stack);
    }
  meta_pop_no_msg_prefix ();
}

static void
meta_stack_op_free (MetaStackOp *op)
{
  g_slice_free (MetaStackOp, op);
}

static int
find_window (GArray *window_stack,
             Window  window)
{
  guint i;

  for (i = 0; i < window_stack->len; i++)
    {
      if (g_array_index (window_stack, Window, i) == window)
        return i;
    }

  return -1;
}

/* Returns TRUE if stack was changed */
static gboolean
move_window_above (GArray *stack,
                   Window  window,
                   int     old_pos,
                   int     above_pos)
{
  int i;

  if (old_pos < above_pos)
    {
      for (i = old_pos; i < above_pos; i++)
        g_array_index (stack, Window, i) = g_array_index (stack, Window, i + 1);

      g_array_index (stack, Window, above_pos) = window;

      return TRUE;
    }
  else if (old_pos > above_pos + 1)
    {
      for (i = old_pos; i > above_pos + 1; i--)
        g_array_index (stack, Window, i) = g_array_index (stack, Window, i - 1);

      g_array_index (stack, Window, above_pos + 1) = window;

      return TRUE;
    }
  else
    return FALSE;
}

/* Returns TRUE if stack was changed */
static gboolean
meta_stack_op_apply (MetaStackOp *op,
                     GArray      *stack)
{
  switch (op->any.type)
    {
    case STACK_OP_ADD:
      {
        int old_pos = find_window (stack, op->add.window);
        if (old_pos >= 0)
          {
            g_warning ("STACK_OP_ADD: window %#lx already in stack",
                       op->add.window);
            return FALSE;
          }

        g_array_append_val (stack, op->add.window);
        return TRUE;
      }
      break;
    case STACK_OP_REMOVE:
      {
        int old_pos = find_window (stack, op->remove.window);
        if (old_pos < 0)
          {
            g_warning ("STACK_OP_REMOVE: window %#lx not in stack",
                       op->remove.window);
            return FALSE;
          }

        g_array_remove_index (stack, old_pos);
        return TRUE;
      }
      break;
    case STACK_OP_RAISE_ABOVE:
      {
        int old_pos = find_window (stack, op->raise_above.window);
        int above_pos;
        if (old_pos < 0)
          {
            g_warning ("STACK_OP_RAISE_ABOVE: window %#lx not in stack",
                       op->raise_above.window);
            return FALSE;
          }

        if (op->raise_above.sibling != None)
          {
            above_pos = find_window (stack, op->raise_above.sibling);
            if (above_pos < 0)
              {
                g_warning ("STACK_OP_RAISE_ABOVE: sibling window %#lx not in stack",
                           op->raise_above.sibling);
                return FALSE;
              }
          }
        else
          {
            above_pos = -1;
          }

        return move_window_above (stack, op->raise_above.window, old_pos, above_pos);
      }
      break;
    case STACK_OP_LOWER_BELOW:
      {
        int old_pos = find_window (stack, op->lower_below.window);
        int above_pos;
        if (old_pos < 0)
          {
            g_warning ("STACK_OP_LOWER_BELOW: window %#lx not in stack",
                       op->lower_below.window);
            return FALSE;
          }

        if (op->lower_below.sibling != None)
          {
            int below_pos = find_window (stack, op->lower_below.sibling);
            if (below_pos < 0)
              {
                g_warning ("STACK_OP_LOWER_BELOW: sibling window %#lx not in stack",
                           op->lower_below.sibling);
                return FALSE;
              }

            above_pos = below_pos - 1;
          }
        else
          {
            above_pos = stack->len - 1;
          }

        return move_window_above (stack, op->lower_below.window, old_pos, above_pos);
      }
      break;
    default:
      break;
    }

  g_assert_not_reached ();
  return FALSE;
}

static GArray *
copy_stack (GArray *stack)
{
  GArray *copy = g_array_sized_new (FALSE, FALSE, sizeof (Window), stack->len);

  g_array_set_size (copy, stack->len);

  memcpy (copy->data, stack->data, sizeof (Window) * stack->len);

  return copy;
}

static void
requery_xserver_stack (MetaStackTracker *tracker)
{
  MetaScreen *screen = tracker->screen;
  Window ignored1, ignored2;
  Window *children;
  guint n_children;
  guint i;

  if (tracker->xserver_stack)
    g_array_free (tracker->xserver_stack, TRUE);

  tracker->xserver_serial = XNextRequest (screen->display->xdisplay);

  XQueryTree (screen->display->xdisplay,
              screen->xroot,
              &ignored1, &ignored2, &children, &n_children);

  tracker->xserver_stack =  g_array_sized_new (FALSE, FALSE, sizeof (Window), n_children);
  g_array_set_size (tracker->xserver_stack, n_children);

  for (i = 0; i < n_children; i++)
    {
      g_array_index (tracker->xserver_stack, Window, i) = children[i];
    }

  XFree (children);
}

MetaStackTracker *
meta_stack_tracker_new (MetaScreen *screen)
{
  MetaStackTracker *tracker;

  tracker = g_new0 (MetaStackTracker, 1);
  tracker->screen = screen;

  requery_xserver_stack (tracker);

  tracker->verified_stack = copy_stack (tracker->xserver_stack);

  tracker->unverified_predictions = g_queue_new ();

  meta_stack_tracker_dump (tracker);

  return tracker;
}

void
meta_stack_tracker_free (MetaStackTracker *tracker)
{
  if (tracker->sync_stack_idle)
    g_source_remove (tracker->sync_stack_idle);

  g_array_free (tracker->xserver_stack, TRUE);
  g_array_free (tracker->verified_stack, TRUE);
  if (tracker->predicted_stack)
    g_array_free (tracker->predicted_stack, TRUE);

  g_queue_foreach (tracker->unverified_predictions, (GFunc)meta_stack_op_free, NULL);
  g_queue_free (tracker->unverified_predictions);
  tracker->unverified_predictions = NULL;

  g_free (tracker);
}

static void
stack_tracker_apply_prediction (MetaStackTracker *tracker,
                                MetaStackOp      *op)
{
  meta_stack_op_dump (op, "Predicting: ", "\n");
  g_queue_push_tail (tracker->unverified_predictions, op);

  if (!tracker->predicted_stack ||
      meta_stack_op_apply (op, tracker->predicted_stack))
    meta_stack_tracker_queue_sync_stack (tracker);

  meta_stack_tracker_dump (tracker);
}

void
meta_stack_tracker_record_add (MetaStackTracker *tracker,
                               Window            window,
                               gulong            serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_ADD;
  op->any.serial = serial;
  op->add.window = window;

  stack_tracker_apply_prediction (tracker, op);
}

void
meta_stack_tracker_record_remove (MetaStackTracker *tracker,
                                  Window            window,
                                  gulong            serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_REMOVE;
  op->any.serial = serial;
  op->remove.window = window;

  stack_tracker_apply_prediction (tracker, op);
}

void
meta_stack_tracker_record_restack_windows (MetaStackTracker *tracker,
                                           Window           *windows,
                                           int               n_windows,
                                           gulong            serial)
{
  int i;

  /* XRestackWindows() isn't actually a X requests - it's broken down
   * by XLib into a series of XConfigureWindow(StackMode=below); we
   * mirror that here.
   *
   * Aside: Having a separate StackOp for this would be possible to
   * get some extra efficiency in memory allocation and in applying
   * the op, at the expense of a code complexity. Implementation hint
   * for that - keep op->restack_window.n_complete, and when receiving
   * events with intermediate serials, set n_complete rather than
   * removing the op from the queue.
   */
  for (i = 0; i < n_windows - 1; i++)
    meta_stack_tracker_record_lower_below (tracker, windows[i + 1], windows[i],
                                           serial + i);
}

void
meta_stack_tracker_record_raise_above (MetaStackTracker *tracker,
                                       Window            window,
                                       Window            sibling,
                                       gulong            serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_RAISE_ABOVE;
  op->any.serial = serial;
  op->raise_above.window = window;
  op->raise_above.sibling = sibling;

  stack_tracker_apply_prediction (tracker, op);
}

void
meta_stack_tracker_record_lower_below (MetaStackTracker *tracker,
                                       Window            window,
                                       Window            sibling,
                                       gulong            serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_LOWER_BELOW;
  op->any.serial = serial;
  op->lower_below.window = window;
  op->lower_below.sibling = sibling;

  stack_tracker_apply_prediction (tracker, op);
}

void
meta_stack_tracker_record_lower (MetaStackTracker *tracker,
                                 Window            window,
                                 gulong            serial)
{
  meta_stack_tracker_record_raise_above (tracker, window, None, serial);
}

/* @op is an operation derived from an X event from the server and we
 * want to verify that our predicted operations are consistent with
 * what's being reported by the X server.
 *
 * This function applies all the unverified predicted operations up to
 * the given @serial onto the verified_stack so that we can check the
 * stack for consistency with the given X operation.
 *
 * Return value: %TRUE if the predicted state is consistent with
 * receiving the given @op from X, else %FALSE.
 */
static gboolean
stack_tracker_verify_predictions (MetaStackTracker *tracker,
                                  MetaStackOp      *op)
{
  GArray *tmp_predicted_stack = NULL;
  GArray *predicted_stack;
  gboolean modified_stack = FALSE;

  meta_topic (META_DEBUG_STACK, "Verifying predictions:\n");

  if (tracker->unverified_predictions->length)
    {
      GList *l;

      tmp_predicted_stack = predicted_stack = copy_stack (tracker->verified_stack);

      for (l = tracker->unverified_predictions->head; l; l = l->next)
        {
          MetaStackOp *current_op = l->data;

          if (current_op->any.serial > op->any.serial)
            break;

          modified_stack |= meta_stack_op_apply (current_op, predicted_stack);
        }
    }
  else
    predicted_stack = tracker->verified_stack;

  meta_topic (META_DEBUG_STACK, "  predicted_stack: ");
  stack_dump (predicted_stack);

  switch (op->any.type)
    {
    case STACK_OP_ADD:
      if (!find_window (predicted_stack, op->add.window))
        {
          meta_topic (META_DEBUG_STACK, "Verify STACK_OP_ADD: window %#lx not found\n", op->add.window);
          goto not_verified;
        }
      break;
    case STACK_OP_REMOVE:
      if (!find_window (predicted_stack, op->remove.window))
        {
          meta_topic (META_DEBUG_STACK, "Verify STACK_OP_REMOVE: window %#lx not found\n", op->remove.window);
          goto not_verified;
        }
      break;
    case STACK_OP_RAISE_ABOVE:
      {
        Window last_xwindow = None;
        guint i;

        for (i = 0; i < predicted_stack->len; i++)
          {
            Window xwindow = g_array_index (predicted_stack, Window, i);

            if (xwindow == op->raise_above.window)
              {
                if (last_xwindow == op->raise_above.sibling)
                  goto verified;
                else
                  goto not_verified;
              }

            last_xwindow = xwindow;
          }

        meta_topic (META_DEBUG_STACK, "Verify STACK_OP_RAISE_ABOVE: window %#lx not found\n", op->raise_above.window);
        goto not_verified;
      }
      break;
    case STACK_OP_LOWER_BELOW:
      g_warn_if_reached (); /* No X events currently lead to this path */
      goto not_verified;
      break;
    default:
      g_warn_if_reached ();
      goto not_verified;
      break;
    }

verified:

  /* We can free the operations which we have now verified... */
  while (tracker->unverified_predictions->head)
    {
      MetaStackOp *queued_op = tracker->unverified_predictions->head->data;

      if (queued_op->any.serial > op->any.serial)
        break;

      g_queue_pop_head (tracker->unverified_predictions);
      meta_stack_op_free (queued_op);
    }

  if (modified_stack)
    {
      g_array_free (tracker->verified_stack, TRUE);
      tracker->verified_stack = predicted_stack;
    }
  else if (tmp_predicted_stack)
    g_array_free (tmp_predicted_stack, TRUE);

  return TRUE;

not_verified:

  if (tmp_predicted_stack)
    g_array_free (tmp_predicted_stack, TRUE);

  if (tracker->predicted_stack)
    {
      g_array_free (tracker->predicted_stack, TRUE);
      tracker->predicted_stack = NULL;
    }

  return FALSE;
}

/* If we find that our predicted state is not consistent with what the
 * X server is reporting to us then this function can re-query and
 * re-synchronize verified_stack with the X server stack.
 *
 * Return value: %TRUE if the verified stack was modified with respect
 * to the predicted stack else %FALSE.
 *
 * Note: ->predicted_stack will be cleared by this function if
 * ->verified_stack had to be modified when re-synchronizing.
 */
static gboolean
resync_verified_stack_with_xserver_stack (MetaStackTracker *tracker)
{
  GList *l;
  unsigned int i, j;
  Window expected_xwindow;
  gboolean modified_stack = FALSE;

  /* Overview of the algorithm:
   *
   * - Re-query the complete X window stack from the X server via
   *   XQueryTree() and update xserver_stack.
   *
   * - Apply all operations in unverified_predictions to
   *   verified_stack so we have a predicted stack and free the
   *   queue of unverified_predictions.
   *
   * - Iterate through the x windows listed in verified_stack at the
   *   same time as iterating the windows in xserver_stack. (Stop
   *   when we reach the end of the xserver_stack)
   *     - If the window found doesn't match the window expected
   *     according to the order of xserver_stack then:
   *       - Look ahead for the window we were expecting and restack
   *       that above the previous X window. If we fail to find the
   *       expected window then create a new entry for it and stack
   *       that.
   *
   * - Continue to iterate through verified_stack for any remaining
   *   X windows that we now know aren't in the xserver_stack and
   *   remove them.
   *
   * - Free ->predicted_stack if any.
   */

  meta_topic (META_DEBUG_STACK, "Fully re-synchronizing X stack with verified stack\n");

  requery_xserver_stack (tracker);

  for (l = tracker->unverified_predictions->head; l; l = l->next)
    {
      meta_stack_op_apply (l->data, tracker->verified_stack);
      meta_stack_op_free (l->data);
    }
  g_queue_clear (tracker->unverified_predictions);

  j = 0;
  expected_xwindow = g_array_index (tracker->xserver_stack, Window, j);

  for (i = 0; i < tracker->verified_stack->len; )
    {
      Window current = g_array_index (tracker->verified_stack, Window, i);

      if (current != expected_xwindow)
        {
          Window new;
          Window expected;
          int expected_index;

          /* If the current window corresponds to a window that's not
           * in xserver_stack any more then the least disruptive thing
           * we can do is to simply remove it and take another look at
           * the same index.
           *
           * Technically we only need to look forward from j if we
           * wanted to optimize this a bit...
           */
          if (find_window (tracker->xserver_stack, current) < 0)
            {
              g_array_remove_index (tracker->verified_stack, i);
              continue;
            }

          /* Technically we only need to look forward from i if we
           * wanted to optimize this a bit...
           */
          expected_index = find_window (tracker->verified_stack, expected_xwindow);

          if (expected_index >= 0)
            {
              expected = g_array_index (tracker->verified_stack, Window, expected_index);
            }
          else
            {
              new = expected_xwindow;

              g_array_append_val (tracker->verified_stack, new);

              expected = new;
              expected_index = tracker->verified_stack->len - 1;
            }

          /* Note: that this move will effectively bump the index of
           * the current window.
           *
           * We want to continue by re-checking this window against
           * the next expected window though so we don't have to
           * update i to compensate here.
           */
          move_window_above (tracker->verified_stack, expected,
                             expected_index, /* current index */
                             i - 1); /* above */
          modified_stack = TRUE;
        }

      /* NB: we want to make sure that if we break the loop because j
       * reaches the end of xserver_stack that i has also been
       * incremented already so that we can run a final loop to remove
       * remaining windows based on the i index.
       */
      i++;

      j++;
      expected_xwindow = g_array_index (tracker->xserver_stack, Window, j);

      if (j >= tracker->xserver_stack->len)
        break;
    }

  /* We now know that any remaining X windows aren't listed in the
   * xserver_stack and so we can remove them.
   */
  while (i < tracker->verified_stack->len)
    {
      g_array_remove_index (tracker->verified_stack, i);

      modified_stack = TRUE;
    }

  /* If we get to the end of verified_list and there are any remaining
   * entries in xserver_stack then append them all to the end
   */
  for (; j < tracker->xserver_stack->len; j++)
    {
      Window current = g_array_index (tracker->xserver_stack, Window, j);
      g_array_append_val (tracker->verified_stack, current);

      modified_stack = TRUE;
    }

  if (modified_stack)
    {
      if (tracker->predicted_stack)
        {
          g_array_free (tracker->predicted_stack, TRUE);
          tracker->predicted_stack = NULL;
        }

      meta_stack_tracker_queue_sync_stack (tracker);
    }

  return modified_stack;
}

static void
stack_tracker_event_received (MetaStackTracker *tracker,
                              MetaStackOp      *op)
{
  /* If the event is older than our latest requery, then it's
   * already included in our tree. Just ignore it.
   */
  if (op->any.serial < tracker->xserver_serial)
    return;

  meta_stack_op_dump (op, "Stack op event received: ", "\n");

  tracker->xserver_serial = op->any.serial;

  /* XXX: With the design we have ended up with it looks like we've
   * ended up making it unnecessary to maintain tracker->xserver_stack
   * since we only need an xserver_stack during the
   * resync_verified_stack_with_xserver_stack() at which point we are
   * going to query the full stack from the X server using
   * XQueryTree() anyway.
   *
   * TODO: remove tracker->xserver_stack.
   */
  meta_stack_op_apply (op, tracker->xserver_stack);

  if (!stack_tracker_verify_predictions (tracker, op))
    resync_verified_stack_with_xserver_stack (tracker);

  meta_stack_tracker_dump (tracker);
}

void
meta_stack_tracker_create_event (MetaStackTracker    *tracker,
                                 XCreateWindowEvent  *event)
{
  MetaStackOp op;

  op.any.type = STACK_OP_ADD;
  op.any.serial = event->serial;
  op.add.window = event->window;

  stack_tracker_event_received (tracker, &op);
}

void
meta_stack_tracker_destroy_event (MetaStackTracker    *tracker,
                                  XDestroyWindowEvent *event)
{
  MetaStackOp op;

  op.any.type = STACK_OP_REMOVE;
  op.any.serial = event->serial;
  op.remove.window = event->window;

  stack_tracker_event_received (tracker, &op);
}

void
meta_stack_tracker_reparent_event (MetaStackTracker    *tracker,
                                   XReparentEvent      *event)
{
  if (event->parent == event->event)
    {
      MetaStackOp op;

      op.any.type = STACK_OP_ADD;
      op.any.serial = event->serial;
      op.add.window = event->window;

      stack_tracker_event_received (tracker, &op);
    }
  else
    {
      MetaStackOp op;

      op.any.type = STACK_OP_REMOVE;
      op.any.serial = event->serial;
      op.remove.window = event->window;

      stack_tracker_event_received (tracker, &op);
    }
}

void
meta_stack_tracker_configure_event (MetaStackTracker    *tracker,
                                    XConfigureEvent     *event)
{
  MetaStackOp op;

  op.any.type = STACK_OP_RAISE_ABOVE;
  op.any.serial = event->serial;
  op.raise_above.window = event->window;
  op.raise_above.sibling = event->above;

  stack_tracker_event_received (tracker, &op);
}

/**
 * meta_stack_tracker_get_stack:
 * @tracker: a #MetaStackTracker
 * @windows: location to store list of windows, or %NULL
 * @n_windows: location to store count of windows, or %NULL
 *
 * Returns the most current view we have of the stacking order
 * of the children of the root window. The returned array contains
 * everything: InputOnly windows, override-redirect windows,
 * hidden windows, etc. Some of these will correspond to MetaWindow
 * objects, others won't.
 *
 * Assuming that no other clients have made requests that change
 * the stacking order since we last received a notification, the
 * returned list of windows is exactly that you'd get as the
 * children when calling XQueryTree() on the root window.
 */
void
meta_stack_tracker_get_stack (MetaStackTracker *tracker,
                              Window          **windows,
                              int              *n_windows)
{
  GArray *stack;

  if (tracker->unverified_predictions->length == 0)
    {
      stack = tracker->verified_stack;
    }
  else
    {
      if (tracker->predicted_stack == NULL)
        {
          GList *l;

          tracker->predicted_stack = copy_stack (tracker->verified_stack);
          for (l = tracker->unverified_predictions->head; l; l = l->next)
            {
              MetaStackOp *op = l->data;
              meta_stack_op_apply (op, tracker->predicted_stack);
            }
        }

      stack = tracker->predicted_stack;
    }

  meta_topic (META_DEBUG_STACK, "Get Stack\n");
  meta_stack_tracker_dump (tracker);

  if (windows)
    *windows = (Window *)stack->data;
  if (n_windows)
    *n_windows = stack->len;
}

/**
 * meta_stack_tracker_sync_stack:
 * @tracker: a #MetaStackTracker
 *
 * Informs the compositor of the current stacking order of windows,
 * based on the predicted view maintained by the #MetaStackTracker.
 */
void
meta_stack_tracker_sync_stack (MetaStackTracker *tracker)
{
  GList *meta_windows;
  Window *windows;
  int n_windows;
  int i;

  if (tracker->sync_stack_idle)
    {
      g_source_remove (tracker->sync_stack_idle);
      tracker->sync_stack_idle = 0;
    }

  meta_stack_tracker_get_stack (tracker, &windows, &n_windows);

  meta_windows = NULL;
  for (i = 0; i < n_windows; i++)
    {
      MetaWindow *meta_window;
      MetaFrame *frame;

      meta_window = meta_display_lookup_x_window (tracker->screen->display,
                                                  windows[i]);

      if (meta_window == NULL)
        continue;

      frame = meta_window->frame;

      /* When mapping back from xwindow to MetaWindow we have to be a bit careful;
       * children of the root could include unmapped windows created by toolkits
       * for internal purposes, including ones that we have registered in our
       * XID => window table. (Wine uses a toplevel for _NET_WM_USER_TIME_WINDOW;
       * see window-prop.c:reload_net_wm_user_time_window() for registration.)
       */
      if (windows[i] == meta_window->xwindow || (frame && windows[i] == frame->xwindow))
        {
          meta_windows = g_list_prepend (meta_windows, meta_window);
        }
    }

  meta_compositor_sync_stack (tracker->screen->display->compositor, meta_windows);
  g_list_free (meta_windows);
}

static gboolean
stack_tracker_sync_stack_idle (gpointer data)
{
  meta_stack_tracker_sync_stack (data);

  return FALSE;
}

/**
 * meta_stack_tracker_queue_sync_stack:
 * @tracker: a #MetaStackTracker
 *
 * Queue informing the compositor of the new stacking order before the
 * next redraw. (See meta_stack_tracker_sync_stack()). This is called
 * internally when the stack of X windows changes, but also needs be
 * called directly when we an undecorated window is first shown or
 * withdrawn since the compositor's stacking order (which contains only
 * the windows that have a corresponding MetaWindow) will change without
 * any change to the stacking order of the X windows, if we are creating
 * or destroying MetaWindows.
 */
void
meta_stack_tracker_queue_sync_stack (MetaStackTracker *tracker)
{
  if (tracker->sync_stack_idle == 0)
    {
      tracker->sync_stack_idle = g_idle_add_full (META_PRIORITY_BEFORE_REDRAW,
                                                  stack_tracker_sync_stack_idle,
                                                  tracker, NULL);
    }
}
