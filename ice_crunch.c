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
  int bits_written;
  int bits_copied;
} state_t;

static void analyze (state_t *state, int level, int *copy_length,
		     int *pack_length, int *pack_offset);

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
  state->bits = 1;
  state->bits_written = 0;
  state->bits_copied = 0;
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
pack_length_bits (int length)
{
  if (length == 0)
    return 0;
  if (length < 2)
    return 1000000;
  else if (length < 3)
    return 1;
  else if (length < 4)
    return 2;
  else if (length < 6)
    return 4;
  else if (length < 10)
    return 6;
  else if (length < 1034)
    return 14;
  else
    return 1000000;
}

static int
pack_offset_bits (int length, int offset)
{
  offset -= length;

  if (length == 0)
    return 0;
  if (length == 2)
    {
      if (offset < 63)
	return 7;
      else if (offset < 575)
	return 10;
      else
	return 1000000;
    }
  else /* length > 2 */
    {
      if (offset < 31)
	return 7;
      else if (offset < 287)
	return 9;
      else if (offset < 4383)
	return 14;
      else
	return 1000000;
    }
}

static int
pack_bits (int length, int offset)
{
  return pack_length_bits (length) + pack_offset_bits (length, offset);
}

static int
copy_length_bits (int length)
{
  if (length < 1)
    return 1;
  if (length < 2)
    return 2;
  else if (length < 5)
    return 4;
  else if (length < 8)
    return 6;
  else if (length < 15)
    return 9;
  else if (length < 270)
    return 17;
  else if (length < 33038)
    return 32;
  else
    return 1000000;
}

static int
copy_bits (int length)
{
  return copy_length_bits (length) + 8 * length;
}

static int
search_string (state_t *state, int *ret_length, int level)
{
  int offset, length, max_offset, max_length;
  double compression, max_compression;

  {
    int i;

    for (i = 0; i < 1034; i++)
      {
	if (state->unpacked[-i - 1] != state->unpacked[-i])
	  break;
      }

    if (i > 1)
      {
	max_length = i;
	max_offset = -1;
	max_compression = 
	  (double)(8 * i) / (double)pack_bits (i, -1);
      }
    else
      {
	max_length = 1;
	max_offset = 0;
	max_compression = 0.0;
      }
  }

  for (offset = 1; offset < 4383; offset++)
    {
      if (state->unpacked + offset >= state->unpacked_start)
	break;

      for (length = 0;
	   state->unpacked[offset - length - 1] ==
	     state->unpacked[-length - 1];
	   length++)
	{
	  if (length == offset)
	    break;
	  if (length == 1033)
	    break;
	}

      if (length < 2)
	continue;

      if (level > 0  && state->unpacked - length > state->unpacked_stop)
	{
	  int copy_length, pack_length, pack_offset;
	  state_t new_state = *state;

	  new_state.unpacked -= length;
	  analyze (&new_state, level, &copy_length,
		   &pack_length, &pack_offset);

	  compression =
	    (double)(8 * (length + copy_length + pack_length)) /
	    (double)(pack_bits (length, offset) + copy_bits (copy_length) +
		     pack_bits (pack_length, pack_offset));
	}
      else
	{
	  compression =
	    (double)(8 * length) / (double)pack_bits (length, offset);
	}

      if (compression > max_compression &&
	  !(length == 2 && offset >= 574))
	{
	  max_compression = compression;
	  max_length = length;
	  max_offset = offset;

	  if (length == 1033)
	    break;
	}

      if (offset == -1)
        offset++;
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
	}
      else
	{
	  *state->write_bits_here = state->bits & 0xff;
	  state->write_bits_here = NULL;
	}
      state->bits = 1;
    }

  state->bits_written++;
  if (state->bits_written == 7)
    write_bit (state, 1);
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
  if (length == 0)
    {
      return;
    }
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

  if (offset > 0)
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
  state->bits_copied += 8 * length;

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
analyze (state_t *state, int level, int *copy_length, int *pack_length,
	 int *pack_offset)
{
  int max_copy_length;
  int max_pack_length;
  int max_pack_offset;
  double max_compression, current_compression;
  int i, N;

  max_copy_length = state->unpacked - state->unpacked_stop;
  max_pack_length = 0;
  current_compression = 1.0;

  max_compression = 0;
  N = 33038;
  for (i = 0; i < N; i++)
    {
      int compressed_bits, uncompressed_bits;
      int length, offset;
      state_t new_state = *state;
      double compression;

      if (new_state.unpacked - i < new_state.unpacked_stop)
	break;

      new_state.unpacked -= i;
      uncompressed_bits = 8 * i;
      compressed_bits = copy_bits (i);

      offset = search_string (&new_state, &length, level - 1);
      if (length < 2)
	continue;

      new_state.unpacked -= length;
      uncompressed_bits += 8 * length;
      compressed_bits += pack_bits (length, offset);

      compression = (double)uncompressed_bits /
	            (double)compressed_bits;

      if (compression > max_compression)
	{
	  max_compression = compression;
	  max_copy_length = i;
	  max_pack_length = length;
	  max_pack_offset = offset;
	  N = i + 2;
	}
    }

  *copy_length = max_copy_length;
  *pack_length = max_pack_length;
  *pack_offset = max_pack_offset;
}

static void
debug_print_info (state_t *state, const char *name, int length,
		  int offset, int bits)
{
  int i;

  if (length == 0 && offset >= -1)
    return;

  fprintf (stderr, "%s: %3d", name, length);
  if (offset < -1)
    fprintf (stderr, "      ");
  else
    fprintf (stderr, ", %3d ", offset);
  fprintf (stderr, "%3d->%2d \"", 8 * length, bits);
  for (i = 0; i < length; i++)
    {
      char c = state->unpacked[i];
      switch (c)
	{
	case '\n':
	  fprintf (stderr, "\\n");
	  break;
	case '\t':
	  fprintf (stderr, "\\t");
	  break;
	default:
	  if (c < 32 || c >= 127)
	    fprintf (stderr, "\\x%02x", c);
	  else
	    fputc (c, stderr);
	}
    }
  fprintf (stderr, "\"\n");
}

static void
crunch (state_t *state, int level)
{
  while (state->unpacked > state->unpacked_stop)
    {
      int copy_length, pack_length, pack_offset;

      analyze (state, level, &copy_length, &pack_length, &pack_offset);

      copy_direct (state, copy_length);
      debug_print_info (state, "copy", copy_length, -2,
			copy_bits (copy_length));
      pack_string (state, pack_length, pack_offset);
      debug_print_info (state, "pack", pack_length, pack_offset,
			pack_bits (pack_length, pack_offset));
    }

  fprintf (stderr,
	   "summary: %d bits encoded in %d bits, compression factor %.2f\n",
	   8 * state->unpacked_length,
	   state->bits_written + state->bits_copied,
	   (double)(8 * state->unpacked_length) /
	   (double)(state->bits_written + state->bits_copied));

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
  size_t packed_buffer_length;
  size_t packed_length;
  char *packed, *packed_new;

  packed_buffer_length = 2 * length;
  if (packed_buffer_length < 100)
    packed_buffer_length = 100;

  packed = malloc (packed_buffer_length);
  if (packed == NULL)
    return NULL;

  init_state (&state, data, length, packed + packed_buffer_length);
  crunch (&state, level);

  packed_length = ice_crunched_length (state.packed);
  if (packed_length == 0)
    {
      free (packed);
      packed = NULL;
    }
  else
    {
      memmove (packed,
	       packed + packed_buffer_length - packed_length,
	       packed_length);
      packed_new = realloc (packed, packed_length);
      if (packed_new != NULL)
	packed = packed_new;
    }

  return packed;
}
