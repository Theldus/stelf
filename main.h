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

#ifndef MAIN_H
#define MAIN_H

	#include <stdint.h>

	#define DBG_LVL 3

	#if DBG_LVL == 1
	#define DEBUG(...) fprintf(stderr, __VA_ARGS__);
	#else
	#define DEBUG
	#endif

	#define ERR(...)  fprintf(stderr, __VA_ARGS__);

	#if DBG_LVL == 0
	#define INFO(...) fprintf(stderr, __VA_ARGS__);
	#else
	#define INFO(...)
	#endif

	/* Error and exit */
	#define errx(...) \
		do { \
			fprintf(stderr, __VA_ARGS__); \
			exit(1); \
		} while (0)

	#define errto(lbl, ...) \
		do { \
			fprintf(stderr, __VA_ARGS__); \
			goto lbl; \
		} while (0)

	struct elf_file_info
	{
		/* ELF info. */
		uint64_t elf_text_base_addr;
		uint64_t elf_text_size;
		uint64_t elf_file_off;
		int      elf_machine_type;

		/* File info. */
		size_t   file_size;
		uint8_t *file_buff;
		int elf_fd;        /* Input/output. */

		/* Status. */
		int rdwr; /* Is file opened as rd/wr or ro?. */
	};

	extern int machine_mode;
	extern int machine_address;

#endif /* MAIN_H. */
