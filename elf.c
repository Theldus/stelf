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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <libelf.h>
#include <gelf.h>

#include "elf.h"

/* Some data about the ELF file. */
static Elf *elf;
static int elf_fd;
static Elf_Data *strtab_data;

/**
 * @brief Given a file, open the ELF file and initialize
 * its data structure.
 *
 * @param file Path of the file to be opened, if any.
 *
 * @return Returns 0 if success, -1 otherwise.
 */
static int open_elf(const char *file)
{
	Elf_Kind ek;

	if ((elf_fd = open(file, O_RDONLY, 0)) < 0)
		errto(out1, "Unable to open %s!\n", file);

	if ((elf = elf_begin(elf_fd, ELF_C_READ, NULL)) == NULL)
		errto(out2, "elf_begin() failed: %s\n", elf_errmsg(-1));

	ek = elf_kind(elf);
	if (ek != ELF_K_ELF)
		errto(out2, "File \"%s\" (fd: %d) is not an ELF file!\n", file, elf_fd);

	return (0);
out2:
	close(elf_fd);
out1:
	elf_end(elf);
	return (-1);
}

/**
 * @brief Given an opened ELF file, closes all the
 * resources.
 */
static void close_elf(void)
{
	if (!elf)
		return;
	if (elf) {
		elf_end(elf);
		elf = NULL;
	}
	if (elf_fd > 0) {
		close(elf_fd);
		elf_fd = -1;
	}
}

/**
 * @Brief Given a opened ELF file, find a load its strtab content.
 *
 * @return Returns 1 if success, 0 otherwise.
 */
static int load_strtab(int *machine_type)
{
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	Elf_Scn *scn;

	if ((gelf_getehdr(elf, &ehdr)) == NULL)
		errto(out0, "Unable to load ELF header!\n");

	/* Validate machine. */
	if (ehdr.e_machine != EM_386 && ehdr.e_machine != EM_X86_64)
		errto(out0, "Unsupported machine type!!!\n");
	*machine_type = (ehdr.e_machine == EM_X86_64) ? 64 : 32;

	scn = elf_getscn(elf, ehdr.e_shstrndx);
	if (!scn)
		errto(out0, "Unable to get string index!\n");

	gelf_getshdr(scn, &shdr);
	if (shdr.sh_type != SHT_STRTAB)
		errto(out0, "Unable to get string table!\n");

	strtab_data = elf_getdata(scn, NULL);
	if (!strtab_data)
		errto(out0, "Unable to find strtab data!\n");

	return (1);
out0:
	return (0);
}

/**
 * @brief Find ...
 *
 * @param addr
 * @param f_off
 * @param s_size
 *
 * @return
 */
static int find_text_section(uint64_t *addr, uint64_t *f_off,
	uint64_t *s_size)
{
	GElf_Shdr shdr;
	Elf_Scn *scn;
	char *sname;

	scn = NULL;
	while ((scn = elf_nextscn(elf, scn)) != NULL)
	{
		/* Unable to get section header. */
		if (gelf_getshdr(scn, &shdr) == NULL)
			continue;

		if (shdr.sh_type != SHT_PROGBITS)
			continue;

		/* Check if we're at .text. */
		sname = strtab_data->d_buf + shdr.sh_name;
		if (strcmp(sname, ".text"))
			continue;

		*addr   = shdr.sh_addr;
		*f_off  = shdr.sh_offset;
		*s_size = shdr.sh_size;
		return (1);
	}
	return (0);
}

/**
 *
 */
int open_and_load_elf_text(const char *elf_file,
	struct elf_file_info *info)
{
	uint64_t f_off;

	if (!elf_file)
		return (-1);

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx("Unable to initialize libelf\n");

	if (open_elf(elf_file) < 0)
		return (-1);

	if (!load_strtab(&info->elf_machine_type))
		goto out0;

	if (!find_text_section(
			&info->elf_text_base_addr,
			&info->elf_file_off,
			&info->elf_text_size))
	{
		goto out0;
	}

	return (elf_fd);
out0:
	close_elf();
	return (0);
}

/**
 *
 */
void unload_elf_text(void) {
	close_elf();
}
