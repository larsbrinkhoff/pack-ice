#include <stdlib.h>
#include <string.h>
#include "ice_private.h"

#include <stdio.h>

typedef struct ice_crunch_state
{
  int last_copy_direct;
  size_t unpacked_length;
  char *unpacked_stop;
  char *unpacked;
  char *packed_start;
  char *packed;
  char *write_bits_here;
  int bits;
} state_t;

static void
init_state (state_t *state, char *data, size_t length, void *packed_start)
{
  state->unpacked_length = length;
  state->unpacked_stop = data;
  state->unpacked = data + length;
  state->packed_start = packed_start;
  state->packed = state->packed_start;
  state->last_copy_direct = 0;
  state->write_bits_here = NULL;
  state->bits = 3;
}

static void
putinfo (char *data, size_t info)
{
  int i;

  for (i = 0; i < 4; i++)
    {
      *data++ = (info >> 24) & 0xff;
      info <<= 8;
    }
}

static size_t
search_string (state_t *state, size_t *ret_length)
{
  size_t offset, length, max_offset, max_length;

  max_length = 0;
  for (offset = 1; offset < 33000; offset++)
    {
      for (length = 0;
	   state->unpacked[offset - length] == state->unpacked[length];
	   length++)
	;

      if (length > max_length)
	{
	  max_length = length;
	  max_offset = offset;
	}
    }

  *ret_length = length;
  return offset;
}

void
write_bit (state_t *state, int bit)
{
  state->bits = (state->bits << 1) | bit;
  if (state->bits & 0x100)
    {
      if (state->write_bits_here == NULL)
	{
	  *--state->packed = state->bits & 0xff;
	}
      else
	{
	  *state->write_bits_here = state->bits & 0xff;
	  state->write_bits_here = NULL;
	}
      state->bits = 1;
    }
}

void
write_bits (state_t *state, int length, int bits)
{
  int i;

  for (i = length - 1; i >= 0; i--)
    write_bit (state, (bits >> i) & 1);
}

static void
flush_bits (state_t *state)
{
  write_bits (state, 7, 0);
}

#if 0
static int
analyze (struct state *state, int level)
{
  struct state optimum_state;
  int optimum_bits;
  int bits;
  int i;

  if (level == 0)
    return 0;

  optimum_state = -1;
  optimum_bits = infinity;
  for (i = 0; i < N; i++)
    {
      struct *tmp_state = *state;



      /*

1. copy_direct: yes/no?
2.	use which copy length?
		{ 1, 2-4, 5-7, 8-14, 15-269, 270-33037 }?
3. (depack_bytes: always yes)
4.	use which depack length?
		{ 2, 3, 4-5, 6-9, 10-1033 }?
5.	use which depack offset:
6.		if length != 2:	use which offset?
			{ 8, 5, 12 } bits?
7.		if length == 2:	use which offset?
			{ 6, 9 } bits?

       */


      /* search for strings in the previously processed data matching
       * the current position */

      /* do we need a copy_direct? */
      /* if (tmp_state->last_copy_direct) => failure; */
      /* if so, calculate what to copy, and how many bits we got */
      /* set tmp_state->last_copy_direct = 1; */

      /* calculate what to depack, and add to bits */

      bits += analyze (tmp_state, level - 1);

      if (bits < optimum_bits)
	{
	  optimum_bits = bits;
	  optimum_state = *tmp_state;
	}
    }

  *state = optimum_state;
  return optimum_bits;
}
#endif

static void
analyze (state_t *state, int level)
{
}

static char *
search_string_2 (state_t *state)
{
  int offset;

  for (offset = 0; offset < 574; offset++)
    {
      if (memcmp (state->unpacked - 2, state->unpacked + offset, 2) == 0)
	return state->unpacked + offset;
    }

  return NULL;
}

static void
no_crunch (state_t *state)
{
  while (state->unpacked > state->unpacked_stop)
    {
      int length;

      write_bit (state, 1);

      length = state->unpacked - state->unpacked_stop;
      if (length > 33037)
	length = 33037;

      if (length < 2)
	{
	  write_bits (state, 1, 0);
	}
      else if (length < 5)
	{
	  write_bits (state, 1, 1);
	  write_bits (state, 2, length - 2);
	}
      else if (length < 8)
	{
	  write_bits (state, 1, 1);
	  write_bits (state, 2, 3);
	  write_bits (state, 2, length - 5);
	}
      else if (length < 15)
	{
	  write_bits (state, 1, 1);
	  write_bits (state, 2, 3);
	  write_bits (state, 2, 3);
	  write_bits (state, 3, length - 8);
	}
      else if (length < 270)
	{
	  write_bits (state, 1, 1);
	  write_bits (state, 2, 3);
	  write_bits (state, 2, 3);
	  write_bits (state, 3, 7);
	  write_bits (state, 8, length - 15);
	}
      else /* length < 33038 */
	{
	  write_bits (state, 1, 0x01);
	  write_bits (state, 2, 0x03);
	  write_bits (state, 2, 0x03);
	  write_bits (state, 3, 0x07);
	  write_bits (state, 8, 0xff);
	  write_bits (state, 15, length - 270);
	}

      state->write_bits_here = --state->packed;
      state->packed -= length;
      state->unpacked -= length;
      memcpy (state->packed, state->unpacked, length);

      {
	char *string;
	int offset;

	string = search_string_2 (state);
	if (string == NULL)
	  {
	    fprintf (stderr, "bollox\n");
	    exit (1);
	  }

	write_bit (state, 0); /* length == 2 */

	offset = string - state->unpacked;
	if (offset < 63)
	  {
	    write_bit (state, 0);
	    write_bits (state, 6, offset + 1);
	  }
	else if (offset < 575)
	  {
	    write_bit (state, 1);
	    write_bits (state, 9, offset - 63);
	  }
	else
	  {
	    fprintf (stderr, "bugger\n");
	    exit (1);
	  }

	state->unpacked -= 2;
      }
    }
}

static void
crunch (state_t *state, int level)
{
  if (level == 0)
    no_crunch (state);
  else
    analyze (state, level);

  /* flush unwritten bits */
  flush_bits (state);

  state->packed -= 4;
  putinfo (state->packed, state->unpacked_length);
  state->packed -= 4;
  putinfo (state->packed, state->packed_start - state->packed + 4);
  state->packed -= 4;
  putinfo (state->packed, ICE_MAGIC);
}

char *
ice_crunch (char *data, size_t length, int level)
{
  state_t state;
  size_t packed_length;
  char *packed, *packed_new;

  packed = malloc (length * 2);
  if (packed == NULL)
    return NULL;

  init_state (&state, data, length, packed + length * 2);
  crunch (&state, level);

  packed_length = ice_crunched_length (state.packed);
  if (packed_length == 0)
    {
      free (packed);
      packed = NULL;
    }
  else
    {
      memmove (packed, packed + length * 2 - packed_length, packed_length);
      packed_new = realloc (packed, packed_length);
      if (packed_new != NULL)
	packed = packed_new;
    }

  return packed;
}
