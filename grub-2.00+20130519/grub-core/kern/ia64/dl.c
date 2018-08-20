/* dl.c - arch-dependent part of loadable module support */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2004,2005,2007,2009  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/elf.h>
#include <grub/misc.h>
#include <grub/err.h>
#include <grub/mm.h>
#include <grub/i18n.h>
#include <grub/ia64/reloc.h>

#define MASK19 ((1 << 19) - 1)

/* Check if EHDR is a valid ELF header.  */
grub_err_t
grub_arch_dl_check_header (void *ehdr)
{
  Elf_Ehdr *e = ehdr;

  /* Check the magic numbers.  */
  if (e->e_ident[EI_CLASS] != ELFCLASS64
      || e->e_ident[EI_DATA] != ELFDATA2LSB
      || e->e_machine != EM_IA_64)
    return grub_error (GRUB_ERR_BAD_OS, N_("invalid arch-dependent ELF magic"));

  return GRUB_ERR_NONE;
}

#pragma GCC diagnostic ignored "-Wcast-align"

/* Relocate symbols.  */
grub_err_t
grub_arch_dl_relocate_symbols (grub_dl_t mod, void *ehdr)
{
  Elf_Ehdr *e = ehdr;
  Elf_Shdr *s;
  Elf_Word entsize;
  unsigned i;
  grub_uint64_t *gp, *gpptr;
  struct grub_ia64_trampoline *tr;

  gp = (grub_uint64_t *) mod->base;
  gpptr = (grub_uint64_t *) mod->got;
  tr = mod->tramp;

  /* Find a symbol table.  */
  for (i = 0, s = (Elf_Shdr *) ((char *) e + e->e_shoff);
       i < e->e_shnum;
       i++, s = (Elf_Shdr *) ((char *) s + e->e_shentsize))
    if (s->sh_type == SHT_SYMTAB)
      break;

  if (i == e->e_shnum)
    return grub_error (GRUB_ERR_BAD_MODULE, N_("no symbol table"));

  entsize = s->sh_entsize;

  for (i = 0, s = (Elf_Shdr *) ((char *) e + e->e_shoff);
       i < e->e_shnum;
       i++, s = (Elf_Shdr *) ((char *) s + e->e_shentsize))
    if (s->sh_type == SHT_RELA)
      {
	grub_dl_segment_t seg;

	/* Find the target segment.  */
	for (seg = mod->segment; seg; seg = seg->next)
	  if (seg->section == s->sh_info)
	    break;

	if (seg)
	  {
	    Elf_Rela *rel, *max;

	    for (rel = (Elf_Rela *) ((char *) e + s->sh_offset),
		   max = rel + s->sh_size / s->sh_entsize;
		 rel < max;
		 rel++)
	      {
		grub_addr_t addr;
		Elf_Sym *sym;
		grub_uint64_t value;

		if (seg->size < (rel->r_offset & ~3))
		  return grub_error (GRUB_ERR_BAD_MODULE,
				     "reloc offset is out of the segment");

		addr = (grub_addr_t) seg->addr + rel->r_offset;
		sym = (Elf_Sym *) ((char *) mod->symtab
				     + entsize * ELF_R_SYM (rel->r_info));

		/* On the PPC the value does not have an explicit
		   addend, add it.  */
		value = sym->st_value + rel->r_addend;

		switch (ELF_R_TYPE (rel->r_info))
		  {
		  case R_IA64_PCREL21B:
		    {
		      grub_uint64_t noff;
		      grub_ia64_make_trampoline (tr, value);
		      noff = ((char *) tr - (char *) (addr & ~3)) >> 4;
		      tr = (struct grub_ia64_trampoline *) ((char *) tr + GRUB_IA64_DL_TRAMP_SIZE);
		      if (noff & ~MASK19)
			return grub_error (GRUB_ERR_BAD_OS,
					   "trampoline offset too big (%lx)", noff);
		      grub_ia64_add_value_to_slot_20b (addr, noff);
		    }
		    break;
		  case R_IA64_SEGREL64LSB:
		    *(grub_uint64_t *) addr += value - (grub_addr_t) seg->addr;
		    break;
		  case R_IA64_FPTR64LSB:
		  case R_IA64_DIR64LSB:
		    *(grub_uint64_t *) addr += value;
		    break;
		  case R_IA64_PCREL64LSB:
		    *(grub_uint64_t *) addr += value - addr;
		    break;
		  case R_IA64_GPREL22:
		    grub_ia64_add_value_to_slot_21 (addr, value - (grub_addr_t) gp);
		    break;

		  case R_IA64_LTOFF22X:
		  case R_IA64_LTOFF22:
		    if (ELF_ST_TYPE (sym->st_info) == STT_FUNC)
		      value = *(grub_uint64_t *) sym->st_value + rel->r_addend;
		  case R_IA64_LTOFF_FPTR22:
		    *gpptr = value;
		    grub_ia64_add_value_to_slot_21 (addr, (grub_addr_t) gpptr - (grub_addr_t) gp);
		    gpptr++;
		    break;

		    /* We treat LTOFF22X as LTOFF22, so we can ignore LDXMOV.  */
		  case R_IA64_LDXMOV:
		    break;
		  default:
		    return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
				       N_("relocation 0x%x is not implemented yet"),
				       ELF_R_TYPE (rel->r_info));
		  }
	      }
	  }
      }

  return GRUB_ERR_NONE;
}
