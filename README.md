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

## Is it really stealth?

## Building

## Contributing
Stelf is always open to the community and willing to accept contributions,
whether with issues, documentation, testing, new features, bugfixes, typos, and
etc. Welcome aboard.

## License and Authors
Stelf is licensed under MIT License. Written by Davidson Francis and
(hopefully) other
[contributors](https://github.com/Theldus/stelf/graphs/contributors).
