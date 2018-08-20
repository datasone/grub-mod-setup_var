/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010,2012,2013 Free Software Foundation, Inc.
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

#include <config.h>

#include <grub/util/misc.h>
#include <grub/i18n.h>
#include <grub/term.h>
#include <grub/macho.h>

#define _GNU_SOURCE	1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <unistd.h>
#include <errno.h>

#include "progname.h"

struct arguments
{
  char *input32;
  char *input64;
  char *output;
  int verbosity;
};

static struct argp_option options[] = {
  {"input32",  '3', N_("FILE"), 0,
   N_("set input filename for 32-bit part."), 0},
  {"input64",  '6', N_("FILE"), 0,
   N_("set input filename for 64-bit part."), 0},
  {"output",  'o', N_("FILE"), 0,
   N_("set output filename. Default is STDOUT"), 0},
  {"verbose",     'v', 0,      0, N_("print verbose messages."), 0},
  { 0, 0, 0, 0, 0, 0 }
};

static error_t
argp_parser (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case '6':
      arguments->input64 = xstrdup (arg);
      break;
    case '3':
      arguments->input32 = xstrdup (arg);
      break;

    case 'o':
      arguments->output = xstrdup (arg);
      break;

    case 'v':
      arguments->verbosity++;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp = {
  options, argp_parser, N_("[OPTIONS]"),
  N_("Glue 32-bit and 64-binary into Apple fat one."),
  NULL, NULL, NULL
};

static void
write_fat (FILE *in32, FILE *in64, FILE *out, const char *out_filename,
	   const char *name32, const char *name64)
{
  struct grub_macho_fat_header head;
  struct grub_macho_fat_arch arch32, arch64;
  grub_uint32_t size32, size64;
  char *buf;

  fseek (in32, 0, SEEK_END);
  size32 = ftell (in32);
  fseek (in32, 0, SEEK_SET);
  fseek (in64, 0, SEEK_END);
  size64 = ftell (in64);
  fseek (in64, 0, SEEK_SET);

  head.magic = grub_cpu_to_le32_compile_time (GRUB_MACHO_FAT_EFI_MAGIC);
  head.nfat_arch = grub_cpu_to_le32_compile_time (2);
  arch32.cputype = grub_cpu_to_le32_compile_time (GRUB_MACHO_CPUTYPE_IA32);
  arch32.cpusubtype = grub_cpu_to_le32_compile_time (3);
  arch32.offset = grub_cpu_to_le32_compile_time (sizeof (head)
						 + sizeof (arch32)
						 + sizeof (arch64));
  arch32.size = grub_cpu_to_le32 (size32);
  arch32.align = 0;

  arch64.cputype = grub_cpu_to_le32_compile_time (GRUB_MACHO_CPUTYPE_AMD64);
  arch64.cpusubtype = grub_cpu_to_le32_compile_time (3);
  arch64.offset = grub_cpu_to_le32 (sizeof (head) + sizeof (arch32)
				    + sizeof (arch64) + size32);
  arch64.size = grub_cpu_to_le32 (size64);
  arch64.align = 0;
  if (fwrite (&head, 1, sizeof (head), out) != sizeof (head)
      || fwrite (&arch32, 1, sizeof (arch32), out) != sizeof (arch32)
      || fwrite (&arch64, 1, sizeof (arch64), out) != sizeof (arch64))
    {
      if (out_filename)
	grub_util_error ("cannot write to `%s': %s",
			 out_filename, strerror (errno));
      else
	grub_util_error ("cannot write to the stdout: %s", strerror (errno));
    }

  buf = xmalloc (size32);
  if (fread (buf, 1, size32, in32) != size32)
    grub_util_error (_("cannot read `%s': %s"), name32,
                     strerror (errno));
  if (fwrite (buf, 1, size32, out) != size32)
    {
      if (out_filename)
	grub_util_error ("cannot write to `%s': %s", 
			 out_filename, strerror (errno));
      else
	grub_util_error ("cannot write to the stdout: %s", strerror (errno));
    }
  free (buf);

  buf = xmalloc (size64);
  if (fread (buf, 1, size64, in64) != size64)
    grub_util_error (_("cannot read `%s': %s"), name64,
                     strerror (errno));
  if (fwrite (buf, 1, size64, out) != size64)
    {
      if (out_filename)
	grub_util_error ("cannot write to `%s': %s",
			 out_filename, strerror (errno));
      else
	grub_util_error ("cannot write to the stdout: %s", strerror (errno));
    }
  free (buf);
}

int
main (int argc, char *argv[])
{
  FILE *in32, *in64, *out;
  struct arguments arguments;

  set_program_name (argv[0]);

  /* Check for options.  */
  memset (&arguments, 0, sizeof (struct arguments));
  if (argp_parse (&argp, argc, argv, 0, 0, &arguments) != 0)
    {
      fprintf (stderr, "%s", _("Error in parsing command line arguments\n"));
      exit(1);
    }

  if (!arguments.input32 || !arguments.input64)
    {
      fprintf (stderr, "%s", _("Missing input file\n"));
      exit(1);
    }

  in32 = fopen (arguments.input32, "r");

  if (!in32)
    grub_util_error (_("cannot open `%s': %s"), arguments.input32,
		     strerror (errno));

  in64 = fopen (arguments.input64, "r");
  if (!in64)
    grub_util_error (_("cannot open `%s': %s"), arguments.input64,
		     strerror (errno));

  if (arguments.output)
    out = fopen (arguments.output, "wb");
  else
    out = stdout;

  if (!out)
    {
      grub_util_error (_("cannot open `%s': %s"), arguments.output ? : "stdout",
		       strerror (errno));
    }

  write_fat (in32, in64, out, arguments.output,
	     arguments.input32, arguments.input64);

  fclose (in32);
  fclose (in64);

  if (out != stdout)
    fclose (out);

  return 0;
}
