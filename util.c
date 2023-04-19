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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sendfile.h>

#include "elf.h"
#include "util.h"

/**
 *
 */
char *get_inst_str(const xed_decoded_inst_t *inst)
{
	static char buff[128];
	memset(buff, 0, sizeof buff);
	if (!xed_format_context(XED_SYNTAX_INTEL,
		inst, buff, sizeof buff - 1, 0, NULL, 0))
	{
		return (NULL);
	}
	return (buff);
}

/**
 *
 */
char *get_inst_str_from_buff(
	const uint8_t *buff, size_t len, xed_decoded_inst_t *inst2)
{
	static xed_decoded_inst_t inst;
	xed_error_enum_t xed_error;

	xed_decoded_inst_zero(&inst);
	xed_decoded_inst_set_mode(&inst, machine_mode, machine_address);

	/* Decode instruction. */
	xed_error = xed_decode(&inst, buff, len);
	if (xed_error != XED_ERROR_NONE)
		return (NULL);

	if (inst2)
		*inst2 = inst;
	return (get_inst_str(&inst));
}

/**
 *
 */
void print_inst_str(const xed_decoded_inst_t *inst)
{
	char *buff;
	unsigned length;

	buff   = get_inst_str(inst);
	length = xed_decoded_inst_get_length(inst);

	if (!buff)
		return;

	fprintf(stderr, "%s (", buff);
	for (unsigned i = 0; i < length; ++i)
		fprintf(stderr, "%02x ", xed_decoded_inst_get_byte(inst, i));
	fprintf(stderr, ")\n");
}

/**
 *
 */
void print_inst_detailed(const xed_decoded_inst_t *inst)
{
	unsigned inst_len;
	uint8_t modrm;

	print_inst_str(inst);
	inst_len = xed_decoded_inst_get_length(inst);
	modrm    = xed_decoded_inst_get_modrm(inst);

	printf("(ModRM: %02x, pos: %u) - ", modrm,
		xed3_operand_get_pos_modrm(inst));
	printf("(Opcode: %02x, pos: %u, len: %d bytes) - ",
		xed3_operand_get_nominal_opcode(inst),
		xed3_operand_get_pos_nominal_opcode(inst),
		inst_len);
}

/**
 *
 */
int is_decoded_inst_equals_to_inst_buff(
	const xed_decoded_inst_t *inst1,
	const uint8_t *inst2,
	xed_decoded_inst_t *ret_decoded_inst2)
{
	char inst1_str[16] = {0};
	char inst2_str[16] = {0};
	char *tmp;

	/* Instruction 1 string. */
	tmp = get_inst_str(inst1);
	strncpy(inst1_str, tmp, 15);

	/* Instruction 2 string. */
	tmp = get_inst_str_from_buff(inst2, 15, ret_decoded_inst2);
	if (!tmp)
	{
		ERR("Invalid new instruction!!!\n");
		return (0);
	}
	strncpy(inst2_str, tmp, 15);

	return strcmp(inst1_str, inst2_str) == 0;
}

/**
 *
 *
 * @return Returns the file descriptor of the
 * already copied file.
 */
int copy_file(int fd_in, const char *out_file)
{
	struct stat st = {0};
	ssize_t ret;
	int fd_out;

	if ((fd_out = open(out_file, O_CREAT|O_RDWR|O_TRUNC, 0755)) < 0)
		return (-1);

	fstat(fd_in, &st);
	ret = sendfile(fd_out, fd_in, NULL, st.st_size);
	if (ret != st.st_size)
		goto out0;

	lseek(fd_out, 0, SEEK_SET);
	return (fd_out);

out0:
	close(fd_out);
	return (-1);
}

/**
 *
 */
int mmap_elf(struct elf_file_info *info)
{
	struct stat st = {0};
	int prot;
	int flag;

	prot = (info->rdwr) ? PROT_READ|PROT_WRITE : PROT_READ;
	flag = (info->rdwr) ? MAP_SHARED : MAP_PRIVATE;

	fstat(info->elf_fd, &st);
	info->file_buff = mmap(0, st.st_size, prot, flag, info->elf_fd, 0);
	info->file_size = st.st_size;

	if (info->file_buff == MAP_FAILED)
		return (0);

	return (1);
}

/**
 *
 */
void munmap_elf(struct elf_file_info *info)
{
	if (info->rdwr) {
		msync (info->file_buff, info->file_size, MS_SYNC);
		munmap(info->file_buff, info->file_size);
		close (info->elf_fd);
	}
	else
		munmap(info->file_buff, info->file_size);

	unload_elf_text();
}
