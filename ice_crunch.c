#include <stdlib.h>
#include <string.h>
#include "ice_private.h"

typedef struct ice_crunch_state
{
  int last_copy_direct;
  size_t unpacked_length;
  char *unpacked_stop;
  char *unpacked;
  char *packed_start;
  char *packed;
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

size_t
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

void
analyze (state_t *state)
{
}

void
crunch (state_t *state)
{
  analyze (state);

  state->packed -= 4;
  putinfo (state->packed, state->unpacked_length);
  state->packed -= 4;
  putinfo (state->packed, state->packed_start - state->packed + 4);
  state->packed -= 4;
  putinfo (state->packed, ICE_MAGIC);
}

char *
ice_crunch (char *data, size_t length)
{
  state_t state;
  size_t packed_length;
  char *packed, *packed_new;

  packed = malloc (length * 2);
  if (packed == NULL)
    return NULL;

  init_state (&state, data, length, packed + length * 2);
  crunch (&state);

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
