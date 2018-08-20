/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#define grub_video_render_target grub_video_fbrender_target

#include <grub/err.h>
#include <grub/types.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/video.h>
#include <grub/video_fb.h>
#include <grub/machine/lbio.h>

GRUB_MOD_LICENSE ("GPLv3+");

static struct grub_linuxbios_table_framebuffer *cbfb;

static struct
{
  struct grub_video_mode_info mode_info;
  grub_uint8_t *ptr;
} framebuffer;

static grub_err_t
grub_video_cbfb_init (void)
{
  grub_memset (&framebuffer, 0, sizeof(framebuffer));

  return grub_video_fb_init ();
}

static grub_err_t
grub_video_cbfb_fill_mode_info (struct grub_video_mode_info *out)
{
  grub_memset (out, 0, sizeof (*out));

  out->width = cbfb->width;
  out->height = cbfb->height;
  out->pitch = cbfb->pitch;

  out->red_field_pos = cbfb->red_field_pos;
  out->red_mask_size = cbfb->red_mask_size;
  out->green_field_pos = cbfb->green_field_pos;
  out->green_mask_size = cbfb->green_mask_size;
  out->blue_field_pos = cbfb->blue_field_pos;
  out->blue_mask_size = cbfb->blue_mask_size;
  out->reserved_field_pos = cbfb->reserved_field_pos;
  out->reserved_mask_size = cbfb->reserved_mask_size;

  out->mode_type = GRUB_VIDEO_MODE_TYPE_RGB;
  out->bpp = cbfb->bpp;
  out->bytes_per_pixel = (cbfb->bpp + 7) / 8;
  out->number_of_colors = 256;

  out->blit_format = grub_video_get_blit_format (out);
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_video_cbfb_setup (unsigned int width, unsigned int height,
			   unsigned int mode_type __attribute__ ((unused)),
			   unsigned int mode_mask __attribute__ ((unused)))
{
  grub_err_t err;

  if (!cbfb)
    return grub_error (GRUB_ERR_IO, "Couldn't find display device.");

  if (!((width == cbfb->width && height == cbfb->height)
	|| (width == 0 && height == 0)))
    return grub_error (GRUB_ERR_IO, "can't set mode %dx%d", width, height);

  err = grub_video_cbfb_fill_mode_info (&framebuffer.mode_info);
  if (err)
    {
      grub_dprintf ("video", "CBFB: couldn't fill mode info\n");
      return err;
    }

  framebuffer.ptr = (void *) (grub_addr_t) cbfb->lfb;

  grub_dprintf ("video", "CBFB: initialising FB @ %p %dx%dx%d\n",
		framebuffer.ptr, framebuffer.mode_info.width,
		framebuffer.mode_info.height, framebuffer.mode_info.bpp);

  err = grub_video_fb_setup (mode_type, mode_mask,
			     &framebuffer.mode_info,
			     framebuffer.ptr, NULL, NULL);
  if (err)
    return err;

  grub_video_fb_set_palette (0, GRUB_VIDEO_FBSTD_NUMCOLORS,
			     grub_video_fbstd_colors);
    
  return err;
}

static grub_err_t
grub_video_cbfb_get_info_and_fini (struct grub_video_mode_info *mode_info,
				  void **framebuf)
{
  grub_memcpy (mode_info, &(framebuffer.mode_info), sizeof (*mode_info));
  *framebuf = (char *) framebuffer.ptr;

  grub_video_fb_fini ();

  return GRUB_ERR_NONE;
}

static struct grub_video_adapter grub_video_cbfb_adapter =
  {
    .name = "Coreboot video driver",

    .prio = GRUB_VIDEO_ADAPTER_PRIO_FIRMWARE_DIRTY,
    .id = GRUB_VIDEO_DRIVER_COREBOOT,

    .init = grub_video_cbfb_init,
    .fini = grub_video_fb_fini,
    .setup = grub_video_cbfb_setup,
    .get_info = grub_video_fb_get_info,
    .get_info_and_fini = grub_video_cbfb_get_info_and_fini,
    .set_palette = grub_video_fb_set_palette,
    .get_palette = grub_video_fb_get_palette,
    .set_viewport = grub_video_fb_set_viewport,
    .get_viewport = grub_video_fb_get_viewport,
    .map_color = grub_video_fb_map_color,
    .map_rgb = grub_video_fb_map_rgb,
    .map_rgba = grub_video_fb_map_rgba,
    .unmap_color = grub_video_fb_unmap_color,
    .fill_rect = grub_video_fb_fill_rect,
    .blit_bitmap = grub_video_fb_blit_bitmap,
    .blit_render_target = grub_video_fb_blit_render_target,
    .scroll = grub_video_fb_scroll,
    .swap_buffers = grub_video_fb_swap_buffers,
    .create_render_target = grub_video_fb_create_render_target,
    .delete_render_target = grub_video_fb_delete_render_target,
    .set_active_render_target = grub_video_fb_set_active_render_target,
    .get_active_render_target = grub_video_fb_get_active_render_target,

    .next = 0
  };

static int
iterate_linuxbios_table (grub_linuxbios_table_item_t table_item,
			 void *data __attribute__ ((unused)))
{
  if (table_item->tag != GRUB_LINUXBIOS_MEMBER_FRAMEBUFFER)
    return 0;
  cbfb = (struct grub_linuxbios_table_framebuffer *) (table_item + 1);
  return 1;
}

GRUB_MOD_INIT(coreboot_fb)
{
  grub_linuxbios_table_iterate (iterate_linuxbios_table, 0);

  if (cbfb)
    grub_video_register (&grub_video_cbfb_adapter);
}

GRUB_MOD_FINI(coreboot_fb)
{
  if (cbfb)
    grub_video_unregister (&grub_video_cbfb_adapter);
}
