# A patch to grub for writing UEFI varstores

This repo is based on [setup_var patch (invalid link now)](http://luna.vmars.tuwien.ac.at/~froemel/insydeh2o_efi/grub2-add-setup_var-cmd.patch) and [setup_var2 patch](https://habr.com/post/190354/), more new commands are added to handle different situations. It contains a patch to grub, which adds several commands to grub's rescue shell. This tool allows read/write access to the UEFI varstore, and is usually used to change BIOS settings that are hidden from the UI.

This tool's binary release is a single efi executable, which will fail to grub rescue shell due to no grub config files.

⚠ Use this tool with exetreme caution as accessing wrong varstore or variable may completely brick your computer!

## Legacy commands

The legacy commands are `setup_var`, `setup_var2` and `setup_var3`, with same usage and minor differences.

```
setup_var(2/_3) offsetInVarStore [optional value to write]
```

The offset value and the value to write should be in hexadecimal, e.g. `0x6C`. Without the optional value, the command read the value at the offset. The value to read and write is limited in one byte.


There are some guides like [this](https://dortania.github.io/OpenCore-Post-Install/misc/msr-lock.html) describing how to change UEFI BIOS hidden settings.

#### setup_var

`setup_var` is the original command, the main purpose is to access the EFI variables in the varstore with its offset. This command search for the varstore named "Setup", which most firmware stores Setup settings in, and access the value in it with the given offset.

#### setup_var2

`setup_var2` searches for varstore named "Custom" instead of "Setup". It may be used for some Insyde firmwares.

#### setup_var_3

In some particular firmwares, such as early version BIOS of Dell XPS 8930, multiple varstores with the same name "Setup" exists, while only one of them stores BIOS settings with others not related to settings. Those dummy varstores usually come with fairly small size (less than 0x10 bytes), which gives this command the chance to sort them out.

With those firmwares, these message will appear in the command output (notice the two var named "Setup" with different GUIDs):

```
var name: Setup, var size: 12, var guid: 80e1202e-2697-4264 - ...

...

var name: Setup, var size: 12, var guid: ec87d643-eba4-4bb5 - ...
```

`setup_var_3` searches for varstore with "Setup", but discards variables that are too small (currently the threshold is 0x10 bytes), which allows skipping dummy varstores.

## New commands

The following commands add new abilities to this command, to be cleared and avoid mistyping, they are not named in numbers and are not compatible with legacy commands.

⚠ Those commands are not fully tested, so please be more cautious when using them.

#### setup_var_vs

`setup_var_vs` allows accessing varying size variables, the legacy commands can only read/write the byte at the given offset, this command accesses bytes with given size at given offset.

Usage:

```
setup_var_vs offsetInVarStore [optional variable size] [optional value to write]
```

The additional variable size parameter specifies the size of the variable (how many bytes) in hexdecimal, and is required for writing value. The variable size can be found in the IFR dump. When the size parameter is 0x01, this command behaves the same as `setup_var`

⚠ This command HAS NOT been tested as I don't have firmware with larger values exposed to UI, so I can't decide the endianness of the varstore. Be sure to read the notes before using it!

Note: In the UEFI specification, all values are in little endian, which means the variables should be stored in little endian format as well, e.g. a 16-bit value `0x01C0` at offset `0x10` would be stored as byte `0xC0` at offset `0x10` and byte `0x01` at offset `0x11`. This tool assumes the variable is stored in little endian, but to prevent unnecessary loss brought by non-standard firmwares, manually checking the bytes of large values with size `0x01` is always recommended.

This command supports up to 64-bit (i.e. 8 bytes) variables as it covers all IFR data types except `EFI_IFR_TYPE_STRING` and `EFI_IFR_TYPE_BUFFER`, which are both arrays in variable length.

#### setup_var_cv

Recent firmwares often stores BIOS settings into multiple varstores, and sometimes most of them are not in default "Setup" name. `setup_var_cv` allows accessing varying size variables in varstore with the given name.

Usage:

```
setup_var_cv nameOfVarStore offsetInVarStore [optional variable size] [optional value to write]
```

The `setup_var_cv` command read the name of the desired varstore from the parameter and access the corresponding varstore. This command can also handle varying size variables, read the `setup_var_vs` section for the optional variable size parameter.

The IFR dump contains the info about which varstore the variable is in, e.g. for an option dumped like this:

```
One Of: Ipv6 PXE Support, VarStoreInfo (VarOffset/VarName): 0x2, VarStore: 0x24, QuestionId: 0x3DE, Size: 1
```

The variable should be found in the VarStore with ID 0x24, and the info of the varstore can be found in the heading lines of the IFR dump, with a line like this:

```
VarStore: VarStoreId: 0x24 [D1405D16-7AFC-4695-BB12-41459D3695A2], Size: 0x9, Name: NetworkStackVar
```

So, the variable controlling "Ipv6 PXE Support" is in the varstore with name "NetworkStackVar" at offset 0x2.

⚠ Only the custom varstore name part of this command is tested. The variable size part, as said above, is NOT tested. So be sure to read the whole `setup_var_vs` section before using it to access variables with size greater than 1!

## Build Notes

This repo only contains the patch files now: `setup_var.c` and `Makefile.core.def.patch`. So [grub](https://www.gnu.org/software/grub/grub-download.html) source is required.

To apply patch to grub source:

```shell
cp setup_var.c /path/to/grub/grub-core/commands/efi/
patch /path/to/grub/grub-core/Makefile.core.def Makefile.core.def.patch
```

To build grub:

```shell
cd /path/to/grub
./autogen.sh
./configure --with-platform=efi --prefix=<temporary install prefix>
make && make install
```

To generate modified GRUB shell:

```shell
cd <temporary install prefix>
./bin/grub-mkstandalone -O x86_64-efi -o modGRUBShell.efi
```
