/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2013  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/term.h>
#include <grub/mm.h>
#include <grub/dl.h>
#include <grub/test.h>

GRUB_MOD_LICENSE ("GPLv3+");

static int *seq;
static int seqptr, nseq;
static struct grub_term_input *saved;

static int
fake_getkey (struct grub_term_input *term __attribute__ ((unused)))
{
  if (seq && seqptr < nseq)
    return seq[seqptr++];
  return -1;
}

static struct grub_term_input fake_input_term =
  {
    .name = "fake",
    .getkey = fake_getkey
  };

void
grub_terminal_input_fake_sequence (int *seq_in, int nseq_in)
{
  if (!saved)
    saved = grub_term_inputs;
  if (seq)
    grub_free (seq);
  seq = grub_malloc (nseq_in * sizeof (seq[0]));
  if (seq)
    {
      grub_term_inputs = &fake_input_term;
      grub_memcpy (seq, seq_in, nseq_in * sizeof (seq[0]));
    }
  nseq = nseq_in;
  seqptr = 0;
}

void
grub_terminal_input_fake_sequence_end (void)
{
  grub_term_inputs = saved;
  grub_free (seq);
  seq = 0;
  nseq = 0;
  seqptr = 0;
}
