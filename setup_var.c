/* setup_var.c - InsydeH2o Setup variable modification tool, can modify single
 *               bytes within the Setup variable */
/*  (c) 2009 by Bernhard Froemel
 *  (c) 2021 by datasone (patches after setup_var_3)
 *
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2005,2006,2007,2008,2009,2008  Free Software Foundation, Inc.
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
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/file.h>
#include <grub/efi/efi.h>
#include <grub/pci.h>

#define INSYDE_SETUP_VAR			((grub_efi_char16_t*)"S\0e\0t\0u\0p\0\0\0")
#define INSYDE_SETUP_VAR_NSIZE		(12)
#define INSYDE_CUSTOM_VAR			((grub_efi_char16_t*)"C\0u\0s\0t\0o\0m\0\0\0")
#define INSYDE_CUSTOM_VAR_NSIZE		(14)
#define INSYDE_SETUP_VAR_SIZE		(0x2bc)
#define INSYDE_SETUP_VAR_GUID		{ 0xa04a27f4, 0xdf00, 0x4d42, { 0xb5, 0x52, 0x39, 0x51, 0x13, 0x02, 0x11, 0x3d } }
#define MAX_VARIABLE_SIZE			(1024)
#define MAX_VAR_DATA_SIZE			(65536)

#define CMDNAME_SETUP_VAR2			("setup_var2")
#define CMDCHECK_SETUP_VAR2			(9)

#define CMDCHECK_SETUP_DVARS_START_INDEX (10)
#define CMDNAME_SETUP_VAR3		("setup_var_3")
#define CMDCHECK_SETUP_VAR3		("3")

#define CMDNAME_SETUP_VAR_VS	("setup_var_vs")
#define CMDCHECK_SETUP_VAR_VS	("vs")

#define CMDNAME_SETUP_VAR_CV	("setup_var_cv")
#define CMDCHECK_SETUP_VAR_CV	("cv")

#define SETUP_VAR_SIZE_THRESHOLD (0x10)

GRUB_MOD_LICENSE("GPLv3+");

void print_varname(grub_efi_char16_t* str);

void print_varname(grub_efi_char16_t* str)
{
    while(*str != 0x0)
    {
        grub_printf("%c", (grub_uint8_t) *str);
        str++;
    }
}

grub_uint64_t pack_data(grub_uint8_t* data, grub_uint16_t offset, grub_uint16_t size);

grub_uint64_t pack_data(grub_uint8_t* data, grub_uint16_t offset, grub_uint16_t size)
{
    grub_uint64_t res = 0, buf = 0;
    for (int i = offset + size - 1; i >= offset; --i) {
	buf = data[i];
        res += buf << 8 * (i - offset);
    }
    return res;
}

void set_data(grub_uint8_t* data, grub_uint16_t offset, grub_uint16_t size, grub_uint64_t value);

void set_data(grub_uint8_t* data, grub_uint16_t offset, grub_uint16_t size, grub_uint64_t value)
{
    for (int i = offset; i < offset + size; ++i) {
        data[i] = (value >> 8 * (i - offset)) & 0xFF;
    }
}

static grub_err_t
grub_cmd_setup_var (grub_command_t cmd,
           int argc, char *argv[])
{
    grub_efi_status_t status;
    grub_efi_guid_t setup_var_guid = INSYDE_SETUP_VAR_GUID;
    grub_efi_guid_t guid;
    grub_uint8_t tmp_data[MAX_VAR_DATA_SIZE];
    grub_uint16_t offset = 0x1af;
    grub_efi_uintn_t setup_var_size = INSYDE_SETUP_VAR_SIZE;
    grub_uint8_t set_value = 0x0;
    grub_efi_uint32_t setup_var_attr = 0x7;
    char* endptr;

    grub_uint16_t var_size = 0;
    grub_efi_char16_t* custom_varname = NULL;
    grub_efi_uintn_t custom_varname_size = 0;

    grub_efi_char16_t name[MAX_VARIABLE_SIZE/2];
    grub_efi_uintn_t name_size;

    grub_uint16_t isMode2 = 0;
    grub_uint16_t isMode3 = 0;
    grub_uint16_t isModeVS = 0;
    grub_uint16_t isModeCV = 0;
    if (cmd->name[CMDCHECK_SETUP_VAR2] == '2') isMode2 = 1;
    if (grub_strlen(cmd->name) > CMDCHECK_SETUP_DVARS_START_INDEX && cmd->name[CMDCHECK_SETUP_DVARS_START_INDEX] != 0)
    {
        char name_suffix[11];
        grub_strncpy(name_suffix, cmd->name + CMDCHECK_SETUP_DVARS_START_INDEX, 10);
        if (!grub_strcmp(name_suffix, CMDCHECK_SETUP_VAR3))
            isMode3 = 1;
        else if (!grub_strcmp(name_suffix, CMDCHECK_SETUP_VAR_VS))
            isModeVS = 1;
        else if (!grub_strcmp(name_suffix, CMDCHECK_SETUP_VAR_CV))
            isModeCV = 1;
    }

    if (argc == 0)
    {
        grub_printf(
                "Hello!\n"
        );
        grub_printf(
                "You may brick your InsydeH2o based laptop and need to send it in\n"
        );
        grub_printf(
                "for repair.\n"
        );
        grub_printf(
                "Some vendors (like Sony) are rather slow, and it could get expensive if they\n"
        );
        grub_printf(
                "discover that you used this or similar tools.\nYou should be  *very*  sure what you do.\n"
        );
        grub_printf(
                "\n\nThis Setup variable modification tool may only work with current (July 2009)\n"
        );
        grub_printf(
                "Insyde H2o based firmware releases. Vendors may decide to lock the EFI\n"
        );
        grub_printf(
                "environment completely out or introduce other security measures.\n"
        );
        grub_printf(
                "Final warning: YOU MAY BRICK YOUR LAPTOP IF YOU USE THIS TOOL - I TAKE  N O \n"
        );
        grub_printf(
                "RESPONSIBILITY FOR  Y O U R  ACTIONS.\n"
        );
        if (isMode3 || isModeVS || isModeCV)
            grub_printf(
                    "\n\n(c) 2009 by Bernhard Froemel <bfroemel@gmail.com>\n(c) 2021 by datasone <datasone@datasone.moe>\n"
            );
        else
            grub_printf(
                    "\n\n(c) 2009 by Bernhard Froemel <bfroemel@gmail.com>\n"
            );
    }

    name[0] = 0x0;

    if (isModeCV)
    {
        int varname_size = grub_strlen(argv[0]) + 1;
        custom_varname = (grub_efi_char16_t*) grub_malloc(varname_size * sizeof(grub_efi_char16_t));
        for (int i = 0; i < varname_size; ++i)
            custom_varname[i] = argv[0][i];
        custom_varname_size = varname_size * 2;
        grub_printf("Looking for %s variable...\n", argv[0]);
    }
    else
    {
        /* scan for Setup variable */
        grub_printf("Looking for Setup variable...\n");
    }

    do
    {
        name_size = MAX_VARIABLE_SIZE;
        status = efi_call_3(grub_efi_system_table->runtime_services->get_next_variable_name,
        &name_size,
        name,
        &guid);

        if(status == GRUB_EFI_NOT_FOUND)
        { /* finished traversing VSS */
            break;
        }

        if(status)
        {
            grub_printf("status: 0x%02x\n", (grub_uint32_t) status);
        }

        if(! status && (
                (!isModeCV && name_size == INSYDE_SETUP_VAR_NSIZE && 0 == grub_memcmp(name, INSYDE_SETUP_VAR, name_size)) ||
                (isMode2 && name_size == INSYDE_CUSTOM_VAR_NSIZE && 0 == grub_memcmp(name, INSYDE_CUSTOM_VAR, name_size)) ||
                (isModeCV && name_size == custom_varname_size && 0 == grub_memcmp(name, custom_varname, name_size))
                ))
        {
            grub_printf("var name: ");
            print_varname(name);
            grub_printf(", var size: %u, var guid: %08x-%04x-%04x - %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n\n",
            (grub_uint32_t) name_size,
            guid.data1,
            guid.data2,
            guid.data3,
            guid.data4[0], guid.data4[1], guid.data4[2], guid.data4[3], guid.data4[4], guid.data4[5], guid.data4[6], guid.data4[7]
            );

            if(grub_memcmp(&guid, &setup_var_guid, sizeof(grub_efi_guid_t)) == 0)
            {
                grub_printf("--> GUID matches expected GUID\n");
            }
            else
            {
                grub_printf("--> GUID does not match expected GUID, taking it nevertheless...\n");
                grub_memcpy(&setup_var_guid, &guid, sizeof(grub_efi_guid_t));
            }

            /* obtain current contents of Setup variable */
            if(argc >= 1 && argc < 3 + (isModeVS) + (isModeCV * 2))
            {
                if (isModeCV)
                {
                    grub_errno = 0;
                    offset = grub_strtoul(argv[1], &endptr, 16);
                    if(endptr == argv[1] || grub_errno != 0)
                    {
                        return grub_error(GRUB_ERR_BAD_ARGUMENT, "can't decode your second argument. Please provide a hex value (e.g. 0x1af).");
                    }
                }
                else
                {
                    grub_errno = 0;
                    offset = grub_strtoul(argv[0], &endptr, 16);
                    if(endptr == argv[0] || grub_errno != 0)
                    {
                        return grub_error(GRUB_ERR_BAD_ARGUMENT, "can't decode your first argument. Please provide a hex value (e.g. 0x1af).");
                    }
                }
                status = efi_call_5(grub_efi_system_table->runtime_services->get_variable,
                    name,
                    &setup_var_guid,
                    &setup_var_attr,
                    &setup_var_size,
                    tmp_data);
                if(status == GRUB_EFI_BUFFER_TOO_SMALL)
                {
                    grub_printf("expected a different size of the Setup variable (got %d (0x%x) bytes). Continue with care...\n", (int)setup_var_size, (int)setup_var_size);
                    status = efi_call_5(grub_efi_system_table->runtime_services->get_variable,
                    name,
                    &setup_var_guid,
                    &setup_var_attr,
                    &setup_var_size,
                    tmp_data);
                }
                if(status)
                {
                    return grub_error(GRUB_ERR_INVALID_COMMAND, "can't get variable using efi (error: 0x%016x)", status);
                }
		if (isModeCV)
		{
                    grub_printf("successfully obtained \"%s\" variable from VSS (got %d (0x%x) bytes).\n", argv[0], (int)setup_var_size, (int)setup_var_size);
		}
		else
		{
		    grub_printf("successfully obtained \"Setup\" variable from VSS (got %d (0x%x) bytes).\n", (int)setup_var_size, (int)setup_var_size);
		}
                if(offset > setup_var_size)
                {
                    /* When in newly added modes and the Setup variable size is too small(smaller than threshold, 0x10 here), supress the error and continue to the next Setup variable */
                    if ((isMode3) && setup_var_size < SETUP_VAR_SIZE_THRESHOLD)
                    {
                        grub_printf("Too small variable detected, ignoring.\n\n");
                        continue;
                    }
                    return grub_error(GRUB_ERR_BAD_ARGUMENT, "offset is out of range.");
                }
                if (argc == 2 && isModeVS) // VS with only size param
                {
                    var_size = grub_strtoul(argv[1], &endptr, 16);
                    if(endptr == argv[1] || grub_errno != 0)
                    {
                        return grub_error(GRUB_ERR_BAD_ARGUMENT, "can't decode your second argument. Please provide a hex value (e.g. 0x01).");
                    }
                    grub_printf("offset 0x%02x is: 0x%02lx\n", offset, pack_data(tmp_data, offset, var_size));
                }
                else if (argc == 3 && isModeCV) // CV with only size param
                {
                    var_size = grub_strtoul(argv[2], &endptr, 16);
                    if(endptr == argv[2] || grub_errno != 0)
                    {
                        return grub_error(GRUB_ERR_BAD_ARGUMENT, "can't decode your third argument. Please provide a hex value (e.g. 0x01).");
                    }
                    grub_printf("offset 0x%02x is: 0x%02lx\n", offset, pack_data(tmp_data, offset, var_size));
                }
                else
                    grub_printf("offset 0x%02x is: 0x%02x\n", offset, tmp_data[offset]);
            }
            /* modify and write Setup variable, if user requests it (old commands) */
            if((argc == 2) && !(isModeVS || isModeCV))
            {
                set_value = grub_strtoul(argv[1], &endptr, 16);
                if(endptr == argv[1] || grub_errno != 0)
                {
                    return grub_error(GRUB_ERR_BAD_ARGUMENT, "can't decode your second argument. Please provide a hex value (e.g. 0x01).");
                }
                grub_printf("setting offset 0x%02x to 0x%02x\n", offset, set_value);
                tmp_data[offset] = set_value;
                status = efi_call_5(grub_efi_system_table->runtime_services->set_variable,
                    name,
                    &setup_var_guid,
                    setup_var_attr,
                    setup_var_size,
                    tmp_data);
                if(status)
                {
                    return grub_error(GRUB_ERR_INVALID_COMMAND, "can't set variable using efi (error: 0x%016x)", status);
                }
            }
            else if (isModeVS)
            {
                if (argc == 3) // VS with size and setval
                {
                    var_size = grub_strtoul(argv[1], &endptr, 16);
                    grub_uint64_t larger_set_value = grub_strtoull(argv[2], &endptr, 16);
                    if (endptr == argv[2] || grub_errno != 0)
                    {
                        return grub_error(GRUB_ERR_BAD_ARGUMENT, "can't decode your third argument. Please provide a hex value (e.g. 0x01).");
                    }
                    grub_printf("setting offset 0x%02x to 0x%02lx\n", offset, larger_set_value);
                    set_data(tmp_data, offset, var_size, larger_set_value);
                    status = efi_call_5(grub_efi_system_table->runtime_services->set_variable,
                                        name,
                                        &setup_var_guid,
                                        setup_var_attr,
                                        setup_var_size,
                                        tmp_data);
                    if(status)
                    {
                        return grub_error(GRUB_ERR_INVALID_COMMAND, "can't set variable using efi (error: 0x%016x)", status);
                    }
                }
            }
            else if (isModeCV)
            {
                if (argc == 4) // CV with size and setval
                {
                    var_size = grub_strtoul(argv[2], &endptr, 16);
                    grub_uint64_t larger_set_value = grub_strtoull(argv[3], &endptr, 16);
                    if (endptr == argv[3] || grub_errno != 0)
                    {
                        return grub_error(GRUB_ERR_BAD_ARGUMENT, "can't decode your fourth argument. Please provide a hex value (e.g. 0x01).");
                    }
                    grub_printf("setting offset 0x%02x to 0x%02lx\n", offset, larger_set_value);
                    set_data(tmp_data, offset, var_size, larger_set_value);
                    status = efi_call_5(grub_efi_system_table->runtime_services->set_variable,
                                        name,
                                        &setup_var_guid,
                                        setup_var_attr,
                                        setup_var_size,
                                        tmp_data);
                    if(status)
                    {
                        return grub_error(GRUB_ERR_INVALID_COMMAND, "can't set variable using efi (error: 0x%016x)", status);
                    }
                }
            }
        }
    } while (! status);

    if(argc == 0 || argc > 2 + (isModeVS) + (isModeCV * 2))
    {
        if (isModeVS)
            return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s offset [size] [setval]", cmd->name);
        if (isModeCV)
            return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s varstorename offset [size] [setval]", cmd->name);
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s offset [setval]", cmd->name);
    }

    grub_free(custom_varname);
    return grub_errno;
}

static grub_err_t
grub_cmd_lsefivar (grub_command_t cmd __attribute__ ((unused)),
           int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
    grub_efi_status_t status;
    grub_efi_guid_t guid;
    grub_uint8_t tmp_data[MAX_VAR_DATA_SIZE];
    grub_efi_uintn_t setup_var_size = INSYDE_SETUP_VAR_SIZE;
    grub_efi_uint32_t setup_var_attr = 0x7;

    grub_efi_char16_t name[MAX_VARIABLE_SIZE/2];
    grub_efi_uintn_t name_size;

    name[0] = 0x0;
    /* scan for Setup variable */
    grub_printf("Listing EFI variables...\n");
    do
    {
        name_size = MAX_VARIABLE_SIZE;
        status = efi_call_3(grub_efi_system_table->runtime_services->get_next_variable_name,
        &name_size,
        name,
        &guid);

        if(status == GRUB_EFI_NOT_FOUND)
        { /* finished traversing VSS */
            break;
        }

        if(status)
        {
            grub_printf("status: 0x%02x\n", (grub_uint32_t) status);
        }
        if(! status)
        {
            setup_var_size = 1;
            status = efi_call_5(grub_efi_system_table->runtime_services->get_variable,
            name,
            &guid,
            &setup_var_attr,
            &setup_var_size,
            tmp_data);
            if (status && status != GRUB_EFI_BUFFER_TOO_SMALL)
            {
                grub_printf("error (0x%x) getting var size:\n  ", (grub_uint32_t)status);
                setup_var_size = 0;
            }
            status = 0;

            grub_printf("name size: %02u, var size: %06u (0x%06x), var guid: %08x-%04x-%04x - %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x, name: ",
            (grub_uint32_t) name_size, (grub_uint32_t) setup_var_size, (grub_uint32_t) setup_var_size,
            guid.data1,
            guid.data2,
            guid.data3,
            guid.data4[0], guid.data4[1], guid.data4[2], guid.data4[3], guid.data4[4], guid.data4[5], guid.data4[6], guid.data4[7]
            );
            print_varname(name);
            grub_printf("\n");
        }
    } while (! status);

    return grub_errno;
}


static grub_command_t cmd_setup_var;
static grub_command_t cmd_setup_var2;
static grub_command_t cmd_setup_var_3;
static grub_command_t cmd_setup_var_vs;
static grub_command_t cmd_setup_var_cv;
static grub_command_t cmd_setup_lsvar;

GRUB_MOD_INIT(setup_var)
{
    cmd_setup_var = grub_register_command ("setup_var", grub_cmd_setup_var,
                    "setup_var offset [setval]",
                    "Read/Write specific (byte) offset of setup variable.");
    cmd_setup_var2 = grub_register_command ("setup_var2", grub_cmd_setup_var,
                    "setup_var2 offset [setval]",
                    "Read/Write specific (byte) offset of setup and custom variables.");
    cmd_setup_var_3 = grub_register_command ("setup_var_3", grub_cmd_setup_var,
                    "setup_var_3 offset [setval]",
                    "Read/Write specific (byte) offset of setup variables ignoring error, use with great caution!!!");
    cmd_setup_var_vs = grub_register_command ("setup_var_vs", grub_cmd_setup_var,
                    "setup_var_vs offset [size] [setval]",
                    "Read/write specific (byte) offset of setup variable, capable of longer value (specify with size).");
    cmd_setup_var_cv = grub_register_command ("setup_var_cv", grub_cmd_setup_var,
                    "setup_var_cv varstorename offset [size] [setval]",
                    "Read/write specific (byte) offset of specified variable, capable of longer value (specify with size).");
    cmd_setup_lsvar = grub_register_command ("lsefivar", grub_cmd_lsefivar,
                    "lsefivar",
                    "Lists all efi variables.");
}

GRUB_MOD_FINI(setup_var)
{
    grub_unregister_command (cmd_setup_var);
    grub_unregister_command (cmd_setup_var2);
    grub_unregister_command (cmd_setup_var_3);
    grub_unregister_command (cmd_setup_var_vs);
    grub_unregister_command (cmd_setup_var_cv);
    grub_unregister_command (cmd_setup_lsvar);
}
