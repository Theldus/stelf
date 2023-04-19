/*
 * MIT License
 *
 * Copyright (c) 2023 Davidson Francis <davidsondfgl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef UTIL_H
#define UTIL_H

	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <xed/xed-interface.h>

	#include "main.h"

	extern char *get_inst_str(const xed_decoded_inst_t *inst);

	extern char *get_inst_str_from_buff(
		const uint8_t *buff, size_t len, xed_decoded_inst_t *inst2);

	extern void print_inst_str(const xed_decoded_inst_t *inst);
	extern void print_inst_detailed(const xed_decoded_inst_t *inst);

	extern int is_decoded_inst_equals_to_inst_buff(
		const xed_decoded_inst_t *inst1,
		const uint8_t *inst2,
		xed_decoded_inst_t *ret_decoded_inst2);

	extern int copy_file(int fd_in, const char *out_file);

	extern int mmap_elf(struct elf_file_info *info);
	extern void munmap_elf(struct elf_file_info *info);


#endif /* UTIL_H */
