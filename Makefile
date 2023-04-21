# MIT License
#
# Copyright (c) 2023 Davidson Francis <davidsondfgl@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Paths
CFLAGS += -Wall -Wextra -pedantic -O2
XED_KIT_PATH ?= $(PWD)/xed-install-base-2023-04-07-lin-x86-64
INCLUDE_PATH  = $(XED_KIT_PATH)/include
LIBRARY_PATH  = $(XED_KIT_PATH)/lib/

CC ?= cc
CFLAGS  = -I$(INCLUDE_PATH)
LDFLAGS = -L$(LIBRARY_PATH)
LDLIBS  = -lxed -lelf
OBJ = main.o util.o elf.o
HDR = main.h util.h elf.h
BIN = stelf

.PHONY: all clean

all: $(BIN)

# C Files
main.o: main.c $(HDR) Makefile
	$(CC) $(CFLAGS) main.c -c
util.o: util.c $(HDR) Makefile
	$(CC) $(CFLAGS) util.c -c
elf.o: elf.c elf.h main.h Makefile
	$(CC) $(CFLAGS) elf.c -c

$(BIN): $(OBJ)
	$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

clean:
	$(RM) $(OBJ)
	$(RM) $(BIN)
