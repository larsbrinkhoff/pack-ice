#include <stdlib.h>
#include <string.h>
#include "ice_private.h"

#include <stdio.h>

typedef struct ice_crunch_state
{
  int last_copy_direct;
  size_t unpacked_length;
  char *unpacked_start;
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
  state->unpacked_start = data + length;
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

static int
search_string (state_t *state, int *ret_length)
{
  int offset, length, max_offset, max_length;

  max_length = 1;
  for (offset = 1; offset < 4383; offset++)
    {
      if (state->unpacked + offset >= state->unpacked_start)
	break;

      for (length = 0;
	   state->unpacked[offset - length - 1] ==
	     state->unpacked[-length - 1];
	   length++)
	{
	  if (length == 1033)
	    break;
	}

      if (length > max_length &&
	  !(length == 2 && offset >= 574))
	{
	  max_length = length;
	  max_offset = offset;
	  if (length == 1033)
	    break;
	}
    }

  *ret_length = max_length;
  return max_offset;
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
      fprintf (stderr, "bits: %02x @ %d\n", state->bits & 0xff,
	       state->packed_start - state->packed);
	}
      else
	{
	  *state->write_bits_here = state->bits & 0xff;
      fprintf (stderr, "bits: %02x @ %d.\n", state->bits & 0xff,
	       state->packed_start - state->write_bits_here);
	  state->write_bits_here = NULL;
	}
      state->bits = 1;
    }
}

static void
write_bits (state_t *state, int length, int bits)
{
  int i;

  for (i = length - 1; i >= 0; i--)
    write_bit (state, (bits >> i) & 1);
}

static void
write_next_bits_here (state_t *state)
{
  if (state->bits != 1 &&
      state->write_bits_here == NULL)
    state->write_bits_here = --state->packed;
}

static void
flush_bits (state_t *state)
{
  write_bits (state, 7, 0);
}

static void
pack_string (state_t *state, int length, int offset)
{
  if (length < 2)
    {
      fprintf (stderr, "shyte\n");
      exit (1);
    }
  else if (length < 3)
    {
      write_bit (state, 0);
    }
  else if (length < 4)
    {
      write_bit (state, 1);
      write_bit (state, 0);
    }
  else if (length < 6)
    {
      write_bit (state, 1);
      write_bit (state, 1);
      write_bit (state, 0);
      write_bits (state, 1, length - 4);
    }
  else if (length < 10)
    {
      write_bit (state, 1);
      write_bit (state, 1);
      write_bit (state, 1);
      write_bit (state, 0);
      write_bits (state, 2, length - 6);
    }
  else if (length < 1034)
    {
      write_bit (state, 1);
      write_bit (state, 1);
      write_bit (state, 1);
      write_bit (state, 1);
      write_bits (state, 10, length - 10);
    }
  else
    {
      fprintf (stderr, "bollox\n");
      exit (1);
    }

  offset -= length;

  if (length == 2)
    {
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
    }
  else /* length > 2 */
    {
      if (offset < 31)
	{
	  write_bit (state, 1);
	  write_bit (state, 0);
	  write_bits (state, 5, offset + 1);
	}
      else if (offset < 287)
	{
	  write_bit (state, 0);
	  write_bits (state, 8, offset - 31);
	}
      else if (offset < 4383)
	{
	  write_bit (state, 1);
	  write_bit (state, 1);
	  write_bits (state, 12, offset - 287);
	}
      else
	{
	  fprintf (stderr, "shit\n");
	  exit (1);
	}
    }

  state->unpacked -= length;
}

static void
copy_direct (state_t *state, int length)
{
  if (length == 0)
    {
      write_bit (state, 0);
      return;
    }

  write_bit (state, 1);

  if (length > 33037)
    {
      exit (1);
    }

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
  
  write_next_bits_here (state);
  state->packed -= length;
  state->unpacked -= length;
  memcpy (state->packed, state->unpacked, length);
}

static void
analyze (state_t *state, int level)
{
  int max_copy_direct_length;
  int max_pack_string_length;
  int max_pack_string_offset;
  double max_compression, current_compression;
  int i;

  char max_copy_direct[100];
  char max_pack_string[100];

  current_compression = 1.0;
  while (state->unpacked > state->unpacked_stop)
    {
      max_compression = 0;
      for (i = 0; i < 33038; i++)
	{
	  int length, offset;
	  state_t new_state = *state;
	  double compression;

	  if (new_state.unpacked - i < new_state.unpacked_stop)
	    break;

#if 0
	  copy_direct (&new_state, i);
#else
	  new_state.packed -= 1;
	  new_state.packed -= i;
	  new_state.unpacked -= i;
#endif
	  offset = search_string (&new_state, &length);
	  if (length < 2)
	    continue;
#if 0
	  pack_string (&new_state, length, offset);
#else
	  new_state.packed -= 2;
	  new_state.unpacked -= length;
#endif
	  compression =
	    (double)(new_state.unpacked_start - new_state.unpacked) /
	    (double)(new_state.packed_start - new_state.packed);
      
	  if (compression > current_compression) /* max_compression) */
	    {
	      max_compression = compression;
	      max_copy_direct_length = i;
	      max_pack_string_length = length;
	      max_pack_string_offset = offset;
	      if (i > 0)
		sprintf (max_copy_direct, "copy_direct: length = %d\n", i);
	      else
		max_copy_direct[0] = 0;
	      sprintf (max_pack_string,   "pack_string: length = %d, offset = %d\n", length, offset);
	      break;
	    }
	  else if (compression > max_compression)
	    {
	      max_compression = compression;
	      max_copy_direct_length = i;
	      max_pack_string_length = length;
	      max_pack_string_offset = offset;
	      if (i > 0)
		sprintf (max_copy_direct, "copy_direct: length = %d\n", i);
	      else
		max_copy_direct[0] = 0;
	      sprintf (max_pack_string,   "pack_string: length = %d, offset = %d\n", length, offset);
	    }
	}

      copy_direct (state, max_copy_direct_length);
      pack_string (state, max_pack_string_length, max_pack_string_offset);
      fprintf (stderr, "%s%s", max_copy_direct, max_pack_string);
      current_compression = max_compression;
      fprintf (stderr, "%d bytes, %2.0f%% done, compression = %2.2f\n",
	       state->unpacked_start - state->unpacked,
	       100.0 * (double)(state->unpacked_start - state->unpacked) /
	               (double)(state->unpacked_start - state->unpacked_stop),
	       current_compression);
    }

  fprintf (stderr, "foo\n");
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

      write_next_bits_here (state);
      state->packed -= length;
      state->unpacked -= length;
      memcpy (state->packed, state->unpacked, length);

      if (state->unpacked > state->unpacked_stop)
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
