/* kill.c -- kill ring management. */

/* Copyright (C) 1994 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 1, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>           /* for _POSIX_VERSION */
#endif /* HAVE_UNISTD_H */

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif /* HAVE_STDLIB_H */

#include <stdio.h>

/* System-specific feature definitions and include files. */
#include "rldefs.h"

/* Some standard library routines. */
#include "readline.h"
#include "history.h"

extern int _rl_last_command_was_kill;
extern int rl_editing_mode;
extern int rl_explicit_arg;
extern Function *rl_last_func;

extern void _rl_init_argument ();
extern int _rl_set_mark_at_pos ();
extern void _rl_fix_point ();
extern void _rl_abort_internal ();

extern char *xmalloc (), *xrealloc ();

/* **************************************************************** */
/*								    */
/*			Killing Mechanism			    */
/*								    */
/* **************************************************************** */

/* What we assume for a max number of kills. */
#define DEFAULT_MAX_KILLS 10

/* The real variable to look at to find out when to flush kills. */
static int rl_max_kills =  DEFAULT_MAX_KILLS;

/* Where to store killed text. */
static char **rl_kill_ring = (char **)NULL;

/* Where we are in the kill ring. */
static int rl_kill_index;

/* How many slots we have in the kill ring. */
static int rl_kill_ring_length;

/* How to say that you only want to save a certain amount
   of kill material. */
int
rl_set_retained_kills (num)
     int num;
{
  return 0;
}

/* Add TEXT to the kill ring, allocating a new kill ring slot as necessary.
   This uses TEXT directly, so the caller must not free it.  If APPEND is
   non-zero, and the last command was a kill, the text is appended to the
   current kill ring slot, otherwise prepended. */
static int
_rl_copy_to_kill_ring (text, append)
     char *text;
     int append;
{
  char *old, *new;
  int slot;

  /* First, find the slot to work with. */
  if (_rl_last_command_was_kill == 0)
    {
      /* Get a new slot.  */
      if (rl_kill_ring == 0)
	{
	  /* If we don't have any defined, then make one. */
	  rl_kill_ring = (char **)
	    xmalloc (((rl_kill_ring_length = 1) + 1) * sizeof (char *));
	  rl_kill_ring[slot = 0] = (char *)NULL;
	}
      else
	{
	  /* We have to add a new slot on the end, unless we have
	     exceeded the max limit for remembering kills. */
	  slot = rl_kill_ring_length;
	  if (slot == rl_max_kills)
	    {
	      register int i;
	      free (rl_kill_ring[0]);
	      for (i = 0; i < slot; i++)
		rl_kill_ring[i] = rl_kill_ring[i + 1];
	    }
	  else
	    {
	      slot = rl_kill_ring_length += 1;
	      rl_kill_ring = (char **)xrealloc (rl_kill_ring, slot * sizeof (char *));
	    }
	  rl_kill_ring[--slot] = (char *)NULL;
	}
    }
  else
    slot = rl_kill_ring_length - 1;

  /* If the last command was a kill, prepend or append. */
  if (_rl_last_command_was_kill && rl_editing_mode != vi_mode)
    {
      old = rl_kill_ring[slot];
      new = xmalloc (1 + strlen (old) + strlen (text));

      if (append)
	{
	  strcpy (new, old);
	  strcat (new, text);
	}
      else
	{
	  strcpy (new, text);
	  strcat (new, old);
	}
      free (old);
      free (text);
      rl_kill_ring[slot] = new;
    }
  else
    rl_kill_ring[slot] = text;

  rl_kill_index = slot;
  return 0;
}

/* The way to kill something.  This appends or prepends to the last
   kill, if the last command was a kill command.  if FROM is less
   than TO, then the text is appended, otherwise prepended.  If the
   last command was not a kill command, then a new slot is made for
   this kill. */
int
rl_kill_text (from, to)
     int from, to;
{
  char *text;

  /* Is there anything to kill? */
  if (from == to)
    {
      _rl_last_command_was_kill++;
      return 0;
    }

  text = rl_copy_text (from, to);

  /* Delete the copied text from the line. */
  rl_delete_text (from, to);

  _rl_copy_to_kill_ring (text, from < to);

  _rl_last_command_was_kill++;
  return 0;
}

/* Now REMEMBER!  In order to do prepending or appending correctly, kill
   commands always make rl_point's original position be the FROM argument,
   and rl_point's extent be the TO argument. */

/* **************************************************************** */
/*								    */
/*			Killing Commands			    */
/*								    */
/* **************************************************************** */

/* Delete the word at point, saving the text in the kill ring. */
int
rl_kill_word (count, key)
     int count, key;
{
  int orig_point = rl_point;

  if (count < 0)
    return (rl_backward_kill_word (-count, key));
  else
    {
      rl_forward_word (count, key);

      if (rl_point != orig_point)
	rl_kill_text (orig_point, rl_point);

      rl_point = orig_point;
    }
  return 0;
}

/* Rubout the word before point, placing it on the kill ring. */
int
rl_backward_kill_word (count, ignore)
     int count, ignore;
{
  int orig_point = rl_point;

  if (count < 0)
    return (rl_kill_word (-count, ignore));
  else
    {
      rl_backward_word (count, ignore);

      if (rl_point != orig_point)
	rl_kill_text (orig_point, rl_point);
    }
  return 0;
}

/* Kill from here to the end of the line.  If DIRECTION is negative, kill
   back to the line start instead. */
int
rl_kill_line (direction, ignore)
     int direction, ignore;
{
  int orig_point = rl_point;

  if (direction < 0)
    return (rl_backward_kill_line (1, ignore));
  else
    {
      rl_end_of_line (1, ignore);
      if (orig_point != rl_point)
	rl_kill_text (orig_point, rl_point);
      rl_point = orig_point;
    }
  return 0;
}

/* Kill backwards to the start of the line.  If DIRECTION is negative, kill
   forwards to the line end instead. */
int
rl_backward_kill_line (direction, ignore)
     int direction, ignore;
{
  int orig_point = rl_point;

  if (direction < 0)
    return (rl_kill_line (1, ignore));
  else
    {
      if (!rl_point)
	ding ();
      else
	{
	  rl_beg_of_line (1, ignore);
	  rl_kill_text (orig_point, rl_point);
	}
    }
  return 0;
}

/* Kill the whole line, no matter where point is. */
int
rl_kill_full_line (count, ignore)
     int count, ignore;
{
  rl_begin_undo_group ();
  rl_point = 0;
  rl_kill_text (rl_point, rl_end);
  rl_end_undo_group ();
  return 0;
}

/* The next two functions mimic unix line editing behaviour, except they
   save the deleted text on the kill ring.  This is safer than not saving
   it, and since we have a ring, nobody should get screwed. */

/* This does what C-w does in Unix.  We can't prevent people from
   using behaviour that they expect. */
int
rl_unix_word_rubout (count, key)
     int count, key;
{
  int orig_point;

  if (rl_point == 0)
    ding ();
  else
    {
      orig_point = rl_point;
      if (count <= 0)
	count = 1;

      while (count--)
	{
	  while (rl_point && whitespace (rl_line_buffer[rl_point - 1]))
	    rl_point--;

	  while (rl_point && (whitespace (rl_line_buffer[rl_point - 1]) == 0))
	    rl_point--;
	}

      rl_kill_text (orig_point, rl_point);
    }
  return 0;
}

/* Here is C-u doing what Unix does.  You don't *have* to use these
   key-bindings.  We have a choice of killing the entire line, or
   killing from where we are to the start of the line.  We choose the
   latter, because if you are a Unix weenie, then you haven't backspaced
   into the line at all, and if you aren't, then you know what you are
   doing. */
int
rl_unix_line_discard (count, key)
     int count, key;
{
  if (rl_point == 0)
    ding ();
  else
    {
      rl_kill_text (rl_point, 0);
      rl_point = 0;
    }
  return 0;
}

/* Copy the text in the `region' to the kill ring.  If DELETE is non-zero,
   delete the text from the line as well. */
static int
region_kill_internal (delete)
     int delete;
{
  char *text;

  if (rl_mark == rl_point)
    {
      _rl_last_command_was_kill++;
      return 0;
    }

  text = rl_copy_text (rl_point, rl_mark);
  if (delete)
    rl_delete_text (rl_point, rl_mark);
  _rl_copy_to_kill_ring (text, rl_point < rl_mark);

  _rl_last_command_was_kill++;
  return 0;
}

/* Copy the text in the region to the kill ring. */
int
rl_copy_region_to_kill (count, ignore)
     int count, ignore;
{
  return (region_kill_internal (0));
}

/* Kill the text between the point and mark. */
int
rl_kill_region (count, ignore)
     int count, ignore;
{
  int r;

  r = region_kill_internal (1);
  _rl_fix_point (1);
  return r;
}

/* Copy COUNT words to the kill ring.  DIR says which direction we look
   to find the words. */
static int
_rl_copy_word_as_kill (count, dir)
     int count, dir;
{
  int om, op, r;

  om = rl_mark;
  op = rl_point;

  if (dir > 0)
    rl_forward_word (count, 0);
  else
    rl_backward_word (count, 0);

  rl_mark = rl_point;

  if (dir > 0)
    rl_backward_word (count, 0);
  else
    rl_forward_word (count, 0);

  r = region_kill_internal (0);

  rl_mark = om;
  rl_point = op;

  return r;
}

int
rl_copy_forward_word (count, key)
     int count, key;
{
  if (count < 0)
    return (rl_copy_backward_word (-count, key));

  return (_rl_copy_word_as_kill (count, 1));
}

int
rl_copy_backward_word (count, key)
     int count, key;
{
  if (count < 0)
    return (rl_copy_forward_word (-count, key));

  return (_rl_copy_word_as_kill (count, -1));
}
  
/* Yank back the last killed text.  This ignores arguments. */
int
rl_yank (count, ignore)
     int count, ignore;
{
  if (rl_kill_ring == 0)
    {
      _rl_abort_internal ();
      return -1;
    }

  _rl_set_mark_at_pos (rl_point);
  rl_insert_text (rl_kill_ring[rl_kill_index]);
  return 0;
}

/* If the last command was yank, or yank_pop, and the text just
   before point is identical to the current kill item, then
   delete that text from the line, rotate the index down, and
   yank back some other text. */
int
rl_yank_pop (count, key)
     int count, key;
{
  int l, n;

  if (((rl_last_func != rl_yank_pop) && (rl_last_func != rl_yank)) ||
      !rl_kill_ring)
    {
      _rl_abort_internal ();
      return -1;
    }

  l = strlen (rl_kill_ring[rl_kill_index]);
  n = rl_point - l;
  if (n >= 0 && STREQN (rl_line_buffer + n, rl_kill_ring[rl_kill_index], l))
    {
      rl_delete_text (n, rl_point);
      rl_point = n;
      rl_kill_index--;
      if (rl_kill_index < 0)
	rl_kill_index = rl_kill_ring_length - 1;
      rl_yank (1, 0);
      return 0;
    }
  else
    {
      _rl_abort_internal ();
      return -1;
    }
}

/* Yank the COUNTth argument from the previous history line. */
int
rl_yank_nth_arg (count, ignore)
     int count, ignore;
{
  register HIST_ENTRY *entry;
  char *arg;

  entry = previous_history ();
  if (entry)
    next_history ();
  else
    {
      ding ();
      return -1;
    }

  arg = history_arg_extract (count, count, entry->line);
  if (!arg || !*arg)
    {
      ding ();
      return -1;
    }

  rl_begin_undo_group ();

#if defined (VI_MODE)
  /* Vi mode always inserts a space before yanking the argument, and it
     inserts it right *after* rl_point. */
  if (rl_editing_mode == vi_mode)
    {
      rl_vi_append_mode (1, ignore);
      rl_insert_text (" ");
    }
#endif /* VI_MODE */

  rl_insert_text (arg);
  free (arg);

  rl_end_undo_group ();
  return 0;
}

/* Yank the last argument from the previous history line.  This `knows'
   how rl_yank_nth_arg treats a count of `$'.  With an argument, this
   behaves the same as rl_yank_nth_arg. */
int
rl_yank_last_arg (count, key)
     int count, key;
{
  if (rl_explicit_arg)
    return (rl_yank_nth_arg (count, key));
  else
    return (rl_yank_nth_arg ('$', key));
}
