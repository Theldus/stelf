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

#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xed/xed-interface.h>

#include "elf.h"
#include "util.h"
#include "main.h"

/*
 * Uncomment to enable DOUBLE_CHECK:
 * With this macro enabled, each patched instruction is checked aginst
 * the original to make sure it is exactly the same decoded instruction.
 *
 * This adds some overhead so its disabled by default.
 * Enabled it if you're not sure if the patching is behaving
 * correctly.
 */
/* #define DOUBLE_CHECK. */

#define OPC_BITD_MASK 0x2

/* Flags. */
#define FLG_SCAN  1
#define FLG_WRITE 2
#define FLG_READ  4
static unsigned flags = FLG_READ;
static uint32_t amnt_should_read = 0; /* in bits. */

/* Some data. */
int machine_mode;
int machine_address;

static struct elf_file_info info;
static char  *out_file;
static char  *inp_file;

/**
 * D-bit (direction bit) instructions table.
 *
 * Unfortunately I had to create this by hand, and
 * this is the only instructions that have D-bit
 * _and_ use ModRM _and_ use 2-set of registers.
 *
 * If someone knows of a better way to do that,
 * I really appreciate.
 *
 * Based on GDB's i386-opc.tbl table file.
 */
static xed_iclass_enum_t bitD_list[] = {
	XED_ICLASS_MOV,
	XED_ICLASS_ADD,
	XED_ICLASS_SUB,
	XED_ICLASS_SBB,
	XED_ICLASS_CMP,
	XED_ICLASS_AND,
	XED_ICLASS_OR,
	XED_ICLASS_XOR,
	XED_ICLASS_ADC
};

/**
 * @brief For a given decoded instruction, checks if the
 * provided instruction have the 'direction-bit'.
 *
 * Since there is no pattern to identify whether a D-bit may
 * appear or not, this function just iterates over the
 * list above.
 *
 * @param inst Decoded instruction.
 *
 * @return Returns 1 if contains the 'D-bit' in the opcode,
 * 0 if not.
 */
static inline int inst_have_bitD(const xed_decoded_inst_t *inst)
{
	xed_iclass_enum_t iclass;
	size_t i;

	iclass = xed_decoded_inst_get_iclass(inst);
	for (i = 0; i < sizeof(bitD_list)/sizeof(bitD_list[0]); i++)
		if (iclass == bitD_list[i])
			return (1);

	return (0);
}

/**
 * @brief Check if the current instruction pointed by @p inst,
 * is eligible to patch:
 *
 * In order that an instruction be eligible, it should:
 * - Have D-bit/direction-bit in the opcode
 * - Have ModRM byte
 * - Be in the format: RegSrc/RegDst
 *
 * @param inst Decoded instruction.
 *
 * @return Returns 1 if eligible, 0 if not.
 *
 */
static int inst_is_eligible(const xed_decoded_inst_t *inst)
{
	uint8_t modrm;
	const xed_inst_t *xi;
	xed_operand_enum_t op_name1, op_name2;

	/* Have D-bit?. */
	if (!inst_have_bitD(inst))
	{
		INFO("Not bitD!\n");
		return (0);
	}

	/* Have ModRM? */
	if (!(modrm = xed_decoded_inst_get_modrm(inst)))
	{
		INFO("Not ModRM!\n");
		return (0);
	}

	/* Is Reg/Reg?. */
	xi       = xed_decoded_inst_inst(inst);
	op_name1 = xed_operand_name(xed_inst_operand(xi, 0));
	op_name2 = xed_operand_name(xed_inst_operand(xi, 1));
	if (!xed_operand_is_register(op_name1) ||
		!xed_operand_is_register(op_name2))
	{
		INFO("Not Reg/Reg!\n");
		return (0);
	}

	return (1);
}

/**
 * @brief Given a current decoded instruction pointed by @p inst,
 * patches (or not, if FLG_SCAN) the instruction encoding,
 * accordingly with the specified @p target bit.
 *
 * @param buff  Buffer pointing to the beginning of the instruction
 *              (must be RW if FLG_WRITE).
 * @param inst  Current decoded instruction.
 * @param isize Current instruction size.
 * @param target_bit Target bit to be set (or cleared) in the
 *                   the instruction.
 *
 * @return Returns 1 if the patch was succeeded, 0 if not.
 */
static int patch_inst(
	uint8_t *buff, const xed_decoded_inst_t *inst, unsigned isize,
	uint8_t target_bit)
{
	uint8_t reg1, reg2;
	uint8_t nbuff[16] = {0};
	xed_decoded_inst_t inst_new;
	unsigned off_modrm, off_opcode;
	uint8_t rex, bitD, modrm, opcode;

	((void)inst_new);
	memcpy(nbuff, buff, isize);

	/* Get offsets for ModRM and opcode. */
	off_modrm  = xed3_operand_get_pos_modrm(inst);
	off_opcode = xed3_operand_get_pos_nominal_opcode(inst);

	/*
	 * Check if the bitD is already equals to our target_bit,
	 * if so, nothing need to be done!.
	 */
	opcode = nbuff[off_opcode];
	modrm  = nbuff[off_modrm];
	bitD   = (opcode >> 1) & 1;

	if (!(flags & FLG_SCAN) && bitD == target_bit) {
		INFO("bitD is already equals to target (%d, opc: 0x%02X)!\n",
			target_bit, opcode);
		return (1); /* since this is not an error. */
	}

	/* Flit bitD. */
	opcode ^= 2;

	/* Just some sanity check. */
	if ((modrm >> 6) != 0x3) { /* Register addressing mode. */
		ERR("Not register addressing mode detected!!!\n");
		return (0);
	}

	reg1 = (modrm >> 3) & 0x7;
	reg2 = (modrm & 0x7);

	/* Clear modrm and set the register in inverted order. */
	modrm &= 0xC0;
	modrm |= (reg2 << 3) | reg1;

	/*
	 * Check if have a REX prefix:
	 * If so, we might need to invert the order of the extension
	 * bits too.
	 */
	if (xed3_operand_get_rex(inst) != 0)
	{
		rex = nbuff[off_opcode - 1];
		if (xed3_operand_get_rexr(inst) !=
			xed3_operand_get_rexb(inst))
		{
			rex ^= 0x5; /* xor by 101b to invert both R and B bit. */
			/*
			 * Write new REX to our instruction buffer.
			 * (assuming REX offset to be right before the opcode!)
			 */
			nbuff[off_opcode - 1] = rex;
		}
	}

	/* Write to the instruction buffer. */
	nbuff[off_opcode] = opcode;
	nbuff[off_modrm]  = modrm;

	/* Check if the new inst is equal to the original. */
#ifdef DOUBLE_CHECK
	if (!is_decoded_inst_equals_to_inst_buff(inst, nbuff, &inst_new))
	{
		ERR("Instructions do not match!:\n");
		ERR("Old inst:  "); print_inst_str(inst);
		ERR("New instr: "); print_inst_str(&inst_new);
		return (0);
	}
#endif

#if DBG_LVL == 1
	DEBUG("Old inst:  "); print_inst_str(inst);
	DEBUG("New instr: "); print_inst_str(&inst_new);
#endif

	/* Do not make the changes if in only-test mode. */
	if (!(flags & FLG_SCAN))
		memcpy(buff, nbuff, isize);

	return (1);
}

/**
 * @brief Reads from stdin and returns the next bit to be
 * patched into the ELF file.
 *
 * @return Returns the bit read, or -1 if EOF.
 */
static int read_next_bit(void)
{
	static int bits      = 0;
	static int bits_left = 0;
	int ret;

	ret = -1;

	/* If no bits left, read a new byte */
	if (!bits_left)
	{
		bits = getchar();
		if (bits == EOF)
			return (ret);
		bits_left = 8;
	}

	/* Extract the leftmost bit and shift bits */
	ret = bits & 1;
	bits >>= 1;
	bits_left--;

	return (ret);
}

/**
 * @brief Given a decoded instruction pointed by @p inst,
 * writes to stdout the read bit.
 *
 * @param inst Current decoded instruction.
 * @param buff Buffer pointing to the beginning of the current
 *             instruction.
 */
static void write_next_bit(const xed_decoded_inst_t *inst,
	const uint8_t *buff)
{
	static unsigned bits_amnt = 0;
	static unsigned curr_byte = 0;
	unsigned off_opcode;
	unsigned off_modrm;
	unsigned d_bit;

	off_modrm  = xed3_operand_get_pos_modrm(inst);
	off_opcode = xed3_operand_get_pos_nominal_opcode(inst);

	/* Just some sanity check. */
	if ((buff[off_modrm] >> 6) != 0x3) {
		ERR("Not register addressing mode detected!!!\n");
		return;
	}

	d_bit     = (buff[off_opcode] >> 1) & 1;
	curr_byte = (curr_byte >> 1) | (d_bit << 7);
	bits_amnt++;

	if (bits_amnt == 8) {
		putchar(curr_byte);
		bits_amnt = 0;
		curr_byte = 0;
	}
}

/**
 * @brief For an already parsed ELF file, read its entire .text
 * section and decodes all instruction. Its behavior depends
 * on the current mode.
 * If:
 *   FLG_SCAN:  Only decodes the instructions and checks for
 *              instructions candidates to patch.
 *   FLG_WRITE: Reads from stdin and write to the output ELF file.
 *   FLG_READ:  Reads from the input ELF file and write to stdout
 *              the (already saved) bits.
 */
static void decode_instructions(void)
{
	uint8_t *buff;
	int      next_bit;
	unsigned inst_len;
	size_t   rem_bytes;
	size_t   written_bits;
	unsigned amnt_bits_read;
	size_t   total_inst_count;
	size_t   patch_inst_count;
	xed_error_enum_t   xed_error;
	xed_decoded_inst_t decoded_inst;

	buff       = info.file_buff + info.elf_file_off;
	rem_bytes  = info.elf_text_size;
	next_bit   = 0;
	written_bits     = 0;
	total_inst_count = 0;
	patch_inst_count = 0;
	amnt_bits_read   = 0;

	while (rem_bytes)
	{
		xed_decoded_inst_zero(&decoded_inst);
		xed_decoded_inst_set_mode(&decoded_inst,
			machine_mode, machine_address);

		/* Decode instruction. */
		xed_error =
			xed_decode(&decoded_inst, buff, rem_bytes);

		if (xed_error != XED_ERROR_NONE)
			errx("Error decoding instruction at offset: %jd (%s)\n",
				(buff - (info.file_buff + info.elf_file_off)),
				xed_error_enum_t2str(xed_error));

		inst_len = xed_decoded_inst_get_length(&decoded_inst);
		total_inst_count++;

		/* Check if instruction is eligible to read and/or patch. */
		if (!inst_is_eligible(&decoded_inst))
			goto skip;

		patch_inst_count++;

		/* Read from stdin and write that bit into the file. */
		if (flags & FLG_WRITE) {
			next_bit = read_next_bit();
			if (next_bit < 0)
				break;
		}

		if (flags & (FLG_WRITE|FLG_SCAN))
			patch_inst(buff, &decoded_inst, inst_len, next_bit);

		else if (flags & FLG_READ) {
			write_next_bit(&decoded_inst, buff);
			if (++amnt_bits_read == amnt_should_read)
				break;
		}

		written_bits++;

	skip:
		/* Update pointers. */
		buff      += inst_len;
		rem_bytes -= inst_len;
	}

	if (flags & FLG_SCAN) {
		printf(
			"Scan summary:\n"
			"%zu bytes available "
			"(%zu inst patcheables, out of %zu (~%zu %%))\n",
			patch_inst_count/8, patch_inst_count, total_inst_count,
			(patch_inst_count*100)/total_inst_count);
	}

	if (flags & FLG_WRITE) {
		printf(
			"Write summary:\n"
			"Wrote %zu bits (%zu bytes)\n",
			written_bits, written_bits/8);

		if (next_bit >= 0)
			printf(
				"WARNING: Entire input was not written!\n"
				"Please check the max amnt of bytes available to write!\n");
	}
}

/**
 * @brief Initializes the input ELF file pointed by @p in,
 * and fill the auxiliary data structures with the relevant
 * info.
 *
 * @param in Path to the ELF file to read.
 *
 * @return Returns if success, 0 otherwise.
 */
static int init_elf(const char *in)
{
	int fd_in;
	int fd_out = 0;

	/* Initialize XED context. */
	xed_tables_init();

	/* Open bin file. */
	fd_in = open_and_load_elf_text(in, &info);
	if (fd_in < 0)
		return (0);

	/* Create output file (if required) to be processed. */
	if (out_file) {
		if ((fd_out = copy_file(fd_in, out_file)) < 0)
			errto(out_close_elf, "Unable to create a file copy, aborting...\n");
		info.elf_fd = fd_out;
		info.rdwr   = 1;
	}
	else {
		info.elf_fd = fd_in;
		info.rdwr   = 0;
	}

	if (!mmap_elf(&info))
		errto(out_close_fdin, "Unable to mmap ELF file!\n");

	/* Set machine type. */
	if (info.elf_machine_type == 64) {
		machine_mode    = XED_MACHINE_MODE_LONG_64;
		machine_address = XED_ADDRESS_WIDTH_64b;
	} else {
		machine_mode    = XED_MACHINE_MODE_LEGACY_32;
		machine_address = XED_ADDRESS_WIDTH_32b;
	}

	return (1);

out_close_fdin:
	if (info.rdwr)
		close(fd_out);
out_close_elf:
	unload_elf_text();

	return (0);
}

/**
 * @brief Show program usage.
 * @param prgname Program name.
 */
static void usage(const char *prgname)
{
	fprintf(stderr, "Usage: %s [options] elf_file\n", prgname);
	fprintf(stderr,
		"Options:\n"
		"  -s \n"
		"      Scan the elf_file and obtains the max amount of bytes available\n"
		"      to add.\n"
		"  -r <amnt>\n"
		"      Reads amnt of bytes on the elf_file and outputs to stdout.\n"
		"      If 0, read everything.\n"
		"  -w \n"
		"      Writes all the input (from stdin) into a copy of elf_file.\n"
		"      (default to: \"out\", change with: -o)\n"
		"  -o <output-file>\n"
		"      Changes the default output file to the one specified.\n"
		"  -h \n"
		"      This help\n\n"
		"Examples:\n"
		"  %s -r 123 my_elf > out_file\n"
		"      Reads 123 bytes from my_elf into \"out_file\".\n"
		"  %s -s my_elf\n"
		"      Scan my_elf and returns the amount of bytes available to add.\n"
		"  %s -w my_elf < input\n"
		"      Write the contents of 'input' into \"out\" (default output file).\n"
		"  %s -w my_elf -o my_new_elf < input\n"
		"      Write the contents of 'input' into \"my_new_elf\".\n",
		prgname, prgname, prgname, prgname);
	exit(EXIT_FAILURE);
}

/**
 * @brief Parse command-line arguments.
 *
 * @param argc Argument count.
 * @param argv Argument list.
 */
static void parse_args(int argc, char **argv)
{
	int c; /* Current arg. */
	while ((c = getopt(argc, argv, "swhr:o:")) != -1)
	{
		switch (c) {
		case 'h':
			usage(argv[0]);
			break;
		case 's':
			flags = FLG_SCAN;
			break;
		case 'w':
			flags    = FLG_WRITE;
			out_file = "out";
			break;
		case 'r':
			flags = FLG_READ;
			amnt_should_read = atoi(optarg) * 8;
			if (!amnt_should_read)
				amnt_should_read = 0xFFFFFFFF;
			break;
		case 'o':
			out_file = optarg;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	/* If not input file available. */
	if (optind >= argc) {
		fprintf(stderr, "Expected <elf_file> after options!\n");
		usage(argv[0]);
	}

	inp_file = argv[optind];
}

/* Main. */
int main(int argc, char **argv)
{
	parse_args(argc, argv);
	if (!init_elf(inp_file))
		errx("Unable to initialize ELF file!\n");

	/* Decode everything. */
	decode_instructions();

	/* Deallocate everything. */
	munmap_elf(&info);

	return (0);
}
