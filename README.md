# 🕵️ stelf
[![License: MIT](https://img.shields.io/badge/License-MIT-blueviolet.svg)](https://opensource.org/licenses/MIT)

Hide binary data inside x86 ELF files by changing the instruction encoding

## How it works?
TL;DR: Stelf works by putting binary data inside the instruction itself, but
keeping the same instruction as before, without changing the execution flow
or anything.

<details><summary>Longer explanation</summary>

Stelf works by (ab)using of the ModR/M byte and the 'direction-bit' `d`: in
instructions that involve memory operands or registers, the 'ModR/M' byte
follows the instruction's opcode.

This byte plays a crucial role in indicating the addressing mode of the
instruction and specifying the appropriate source or destination registers to
be used.

Take the `ADD EAX, EBX (01 d8)` instruction for example:
```text
  opcode                       ModR/M byte
 000000001                  [11] [011] [000]
        ||                    |    |     |
        | > sign bit          |    |      > RM 
        |                     |     > REG
         > direction bit       > MOD
         
direction bit = 0
sign bit = 1
MOD =  11, REG = 011, RM  = 000
```
The `s` bit is used to indicate 32-bit operands, whereas the `d` bit determines
the destination or source register within the ModR/M byte.

Regarding the ModR/M byte, the first two bits (bits 7 and 6) represent the `MOD`
field, which indicates the addressing mode of the instruction. Furthermore, bits
5-3 correspond to the `REG` field, which identifies the destination or source
register. Finally, bits 2-0 represent the `RM` field, which specifies the
addressing mode or a register.

Stelf only cares when MOD is equal to '11': register-addressing mode. In this
mode, the registers used in the instruction are specified by the `REG` and `RM`
fields. The table below depicts all of the possible values:
| REG Value | Reg if data size is 8 bits | Reg if data size is 16 bits | Reg if data size is 32 bits |
|:---------:|:--------------------------:|:---------------------------:|:---------------------------:|
|    000    |             al             |              ax             |             eax             |
|    001    |             cl             |              cx             |             ecx             |
|    010    |             dl             |              dx             |             edx             |
|    011    |             bl             |              bx             |             ebx             |
|    100    |             ah             |              sp             |             esp             |
|    101    |             ch             |              bp             |             ebp             |
|    110    |             dh             |              si             |             esi             |
|    111    |             bh             |              di             |             edi             |

A careful reader might ask, "Okay, `REG` and `RM` define the registers, but what
about the direction bit?" Indeed, this bit determines whether `REG` will be the
source or destination register, and here comes a crucial point: note that, 
depending on the opcode's bitD, it is possible to reverse the order of the
registers in the ModR/M byte while keeping the same instruction.

Let's take a look:
```text
 opcode        ModR/M
0000 0001   [11] [011] [000] = add eax, ebx (01 d8), REG is source
0000 0011   [11] [000] [011] = add eax, ebx (03 c3), REG is destination
```
Note that by inverting the direction bit, and also the order of the registers,
the instruction is maintained, although its encoding changes. This is exactly
how Stelf works: for each eligible instruction, the `d` bit is used to store the
useful data and the registers in ModR/M are inverted (or not) to maintain
the semantics of the instruction. So for each eligible instruction, a single bit
is stored.

_\~ For 64-bit registers (like RAX-RDX, R8-R15...) Stelf also takes into account the `REX`
prefix, but for simplicity the explanation will be omitted here \~._
</details>
  
## Usage
Using stelf is quite simple, just a) first analyze how many bytes are available to be
added to the target file and b) add this data.

### a) Scan how many bytes are available (`-s`):
Use the `-s` option to scan a target binary:
```bash
$ ./stelf -s ~/clang-static/bin/clang-11
Scan summary:
380174 bytes available (3041399 inst patcheables, out of 16166560 (~18 %))
```

### b) Add arbitrary data into the ELF file (`-w`):
Use the `-w` option to add a given file from stdin to the specified target file:
```bash
# Omitting output file, a 'out' file will be created:
$ ./stelf -w ~/clang-static/bin/clang-11 < my_input_file
Write summary:
Wrote 357336 bits (44667 bytes)

# Specifying the output file
$ ./stelf -w ~/clang-static/bin/clang-11 -o my_out_file < my_input_file
Write summary:
Wrote 357336 bits (44667 bytes)
```

### c) Read the written data (`r`):
To read the written data, just use the `-r` flag. With parameter '0', all binary
data is read, any other value reads the specified amount:
```bash
# Read all the data
$ ./stelf -r 0 out > my_read_data
$ wc -c my_read_data
380174 my_read_data

# Read only a given amount
$ ./stelf -r 44667 out > my_read_data
$ wc -c my_read_data
44667 my_read_data
```

## How much data can I store?
Stelf's effectiveness is influenced by a number of variables. Stelf makes use of nine
different instruction: `MOV`,`ADD`,`SUB`,`SBB`,`CMP`,`AND`, `OR`,`XOR`, and `ADC`, all
of which must be in Reg/Reg format. Additionally, only a single bit is gained per
patched instruction.

Assuming an average instruction size of 4 bytes, roughly 16% of the total instructions
can be patched, or about 1/200th of the size of the entire `.text` section.

The following table includes some examples:
|       **ELF file**      | **ELF size / `.text` size** | **% of insn patcheable** | **bytes available** |
|:-----------------------:|:---------------------------:|:------------------------:|:-------------------:|
|        /bin/bash        |        1.2M / 716kB         |            15%           |         3496        |
|       /usr/bin/lua      |       175kB / 106kB         |            20%           |         764         |
|     /usr/bin/docker     |        57M / 22.25M         |            6%            |        40455        |
|         Bat 0.21        |       5.1M / 2.65M          |            15%           |        12404        |
|     Hyperfine 1.15.0    |        2.3M / 1.42M         |            15%           |         6678        |
|     clang-11-static     |       117M / 63.21M         |            18%           |        380174       |
|    firefox/libxul.so    |       149M / 93.33M         |            15%           |        445933       |
| libnvoptix.so.510.47.03 |       168M / 13.69M         |            19%           |        83014        |

## Is it really stealth?
It all depends. The data is inserted into the instruction itself, but the code
path remains unchanged, and no data is visible to the naked eye in a hex editor,
for example. A very careful eye, on the other hand, might notice that the
instructions have been changed: a compiler would not generate two identical
instructions in different ways, with different encoding, so this raises
suspicions.

That said, I wouldn't put sensitive data in without some form of encryption for
example, but Stelf still strikes me as quite useful for creating 'watermarks'
e.g. in case you want to restrict an ELF as 'internal use' and things like.

Also, I confess that I was quite surprised with the amount of bytes available
for each ELF analyzed, I expected much less!. Furthermore, the available space
won't consume more disk, since it was always there anyway =).

## Building

## Contributing
Stelf is always open to the community and willing to accept contributions,
whether with issues, documentation, testing, new features, bugfixes, typos, and
etc. Welcome aboard.

## License and Authors
Stelf is licensed under MIT License. Written by Davidson Francis and
(hopefully) other
[contributors](https://github.com/Theldus/stelf/graphs/contributors).
