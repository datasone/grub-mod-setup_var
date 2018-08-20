/* kern/i386/tsc.c - x86 TSC time source implementation
 * Requires Pentium or better x86 CPU that supports the RDTSC instruction.
 * This module uses the RTC (via grub_get_rtc()) to calibrate the TSC to
 * real time.
 *
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008  Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/time.h>
#include <grub/misc.h>
#include <grub/i386/tsc.h>
#include <grub/i386/pit.h>
#include <grub/cpu/io.h>

/* This defines the value TSC had at the epoch (that is, when we calibrated it). */
static grub_uint64_t tsc_boot_time;

/* Calibrated TSC rate.  (In ms per 2^32 ticks) */
/* We assume that the tick is less than 1 ms and hence this value fits
   in 32-bit.  */
grub_uint32_t grub_tsc_rate;

static void
grub_pit_wait (grub_uint16_t tics)
{
  /* Disable timer2 gate and speaker.  */
  grub_outb (grub_inb (GRUB_PIT_SPEAKER_PORT)
	     & ~ (GRUB_PIT_SPK_DATA | GRUB_PIT_SPK_TMR2),
             GRUB_PIT_SPEAKER_PORT);

  /* Set tics.  */
  grub_outb (GRUB_PIT_CTRL_SELECT_2 | GRUB_PIT_CTRL_READLOAD_WORD,
	     GRUB_PIT_CTRL);
  grub_outb (tics & 0xff, GRUB_PIT_COUNTER_2);
  grub_outb (tics >> 8, GRUB_PIT_COUNTER_2);

  /* Enable timer2 gate, keep speaker disabled.  */
  grub_outb ((grub_inb (GRUB_PIT_SPEAKER_PORT) & ~ GRUB_PIT_SPK_DATA)
	     | GRUB_PIT_SPK_TMR2,
             GRUB_PIT_SPEAKER_PORT);

  /* Wait.  */
  while ((grub_inb (GRUB_PIT_SPEAKER_PORT) & GRUB_PIT_SPK_TMR2_LATCH) == 0x00);

  /* Disable timer2 gate and speaker.  */
  grub_outb (grub_inb (GRUB_PIT_SPEAKER_PORT)
	     & ~ (GRUB_PIT_SPK_DATA | GRUB_PIT_SPK_TMR2),
             GRUB_PIT_SPEAKER_PORT);
}

grub_uint64_t
grub_tsc_get_time_ms (void)
{
  grub_uint64_t a = grub_get_tsc () - tsc_boot_time;
  grub_uint64_t ah = a >> 32;
  grub_uint64_t al = a & 0xffffffff;

  return ((al * grub_tsc_rate) >> 32) + ah * grub_tsc_rate;
}

/* Calibrate the TSC based on the RTC.  */
static void
calibrate_tsc (void)
{
  /* First calibrate the TSC rate (relative, not absolute time). */
  grub_uint64_t end_tsc;

  tsc_boot_time = grub_get_tsc ();
  grub_pit_wait (0xffff);
  end_tsc = grub_get_tsc ();

  grub_tsc_rate = grub_divmod64 ((55ULL << 32), end_tsc - tsc_boot_time, 0);
}

void
grub_tsc_init (void)
{
  if (grub_cpu_is_tsc_supported ())
    {
      calibrate_tsc ();
      grub_install_get_time_ms (grub_tsc_get_time_ms);
    }
  else
    {
#if defined (GRUB_MACHINE_PCBIOS) || defined (GRUB_MACHINE_IEEE1275)
      grub_install_get_time_ms (grub_rtc_get_time_ms);
#else
      grub_fatal ("no TSC found");
#endif
    }
}
