#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

uint32_t read32(FILE *fp)
{
	uint32_t ret;
	unsigned char buf[4];
	if (!fread(buf, 1, 4, fp)) {
		fprintf(stderr, "fread: %s", strerror(ferror(fp)));
		exit(1);
	}

	ret = buf[0];
	ret |= ((uint32_t)buf[1]) << 8;
	ret |= ((uint32_t)buf[2]) << 16;
	ret |= ((uint32_t)buf[3]) << 24;
	return ret;
}

uint64_t read64(FILE *fp)
{
	uint64_t ret;
	uint32_t buf[2];
	buf[0] = read32(fp);
	buf[1] = read32(fp);
	ret = buf[0];
	ret |= ((uint64_t)buf[1]) << 32;
	return ret;
}

float decode_fp32(uint32_t bits)
{
	union {
		uint32_t bits;
		float value;
	} u = { bits };
	return u.value;
}

float decode_fp20(uint32_t bits)
{
	/* FP20 is 1.6.13 - bias 31
	 * FP32 is 1.8.23 - bias 127
	 * The strategy is to expand FP20 to FP32, encode it as an int,
	 * then reinterpret it as a float
	 */

	union {
		float value;
		uint32_t bits;
	} u;

	int mantissa = bits & 0x1FFF;
	int exponent = (bits >> 13) & 0x3F;
	int sign = (bits >> 19) & 1;

	if (exponent == 63) /* inf/nan */
		exponent = 255;
	else
		exponent += 127 - 31; /* adjust bias */

	mantissa = mantissa << (23 - 13);
	/* TODO: round off ?
	mantissa |= mantissa >> 10; */

	u.bits = (sign << 31) | (exponent << 23) | mantissa;
	return u.value;
}

float decode_fix10(uint32_t bits)
{
	bits &= 0x3ff;
	if (bits & (1 << 9))
		return -(((bits - 1) ^ 0x3ff) / 256.0f);
	return bits / 256.0f;
}

uint64_t embedded_consts;
static int embedded_consts_used;

const char *decode_operand_base(uint64_t bits)
{
	static char buf[64];
	char *dst = buf;

	int x10 = (bits >> 3) & 1;
	/* bit4: unknown */
	/* bit5: last bit of offset */
	int reg = (bits >> 5) & 63; /* bit 5..10 */
	int uni = (bits >> 11) & 1;

	switch (bits & ~7) {
	case 0x7c8: return "#0";
	case 0x7e8: return "#1";

	/* these needs bit12: */
	case 0x200: return "vPos.x";
	case 0x240: return "vPos.y";
	case 0x2c8: return "vFace";
	}

	/* not seen */
	if (bits & (1 << 4))
		dst += sprintf(dst, "?4? ");

	/* not seen */
	if (!x10 && reg & 1)
		dst += sprintf(dst, "?wat? ");

	if (reg >= 56) {
		if (reg >= 62) {
			/* not seen */
			if (x10)
				dst += sprintf(dst, "?wut? ");

			/* r28-r30 = embedded constants, r31 = 0 or 1 */
			if (reg == 62)
				dst += sprintf(dst, "#0???");
			else if (reg == 63)
				dst += sprintf(dst, "#1???");
		} else {
			int offset = 4 + (reg & 7) * 10;
			dst += sprintf(dst, "#%f", x10 ?
			    decode_fix10(embedded_consts >> offset) :
			    decode_fp20(embedded_consts >> offset));
			embedded_consts_used = 1;
		}
	} else {
		/* normal registers */
		dst += sprintf(dst, "%c%d%s",
		    uni ? 'c' : 'r',
		    x10 ? reg : reg >> 1,
		    x10 ? "_half" : "");
	}

	return buf;
}

const char *decode_operand(uint64_t bits)
{
	static char buf[64];
	char *dst = buf;
	int s2x = (bits >> 0) & 1;
	int neg = (bits >> 1) & 1;
	int abs = (bits >> 2) & 1;
	dst += sprintf(dst, "(0x%llx) ", bits);

	if (neg) {
		*dst = '-';
		++dst;
	}

	if (abs) {
		strcpy(dst, "abs(");
		dst += 4;
	}


	dst += sprintf(dst, "%s", decode_operand_base(bits));

	if (s2x)
		dst += sprintf(dst, " * #2");

	if (abs) {
		*dst = ')';
		++dst;
	}

	return buf;
}

const char *decode_rd(uint64_t bits)
{
	static char buf[64];
	char *dst = buf;
	int offs = (bits >> 2) & 63; /* 14..19 */
	int x10 = ((bits >> 2) & 1) != ((bits >> 1) & 1);
	int sat = (bits >> 12) & 1;
	int pscale = (bits >> 13) & 3;

	const char *pscale_str[4] = {
		"", "_mul2", "_mul4", "_div2"
	};

	dst += sprintf(dst, "(0x%llx) ", bits);

	switch (bits) {
	case 0x262:
		dst += sprintf(dst, "rKill");
		return buf;

	case 0x266:
		dst += sprintf(dst, "rKill_nz");
		return buf;

	case 0x666:
		dst += sprintf(dst, "rKill_z");
		return buf;

	case 0xe66:
		dst += sprintf(dst, "rKill_ge");
		return buf;

	case 0xa66:
		dst += sprintf(dst, "rKill_g");
		return buf;
	}

	/* seen -- enable special registers? */
	if (bits & 1)
		printf("?12? ");

	if (x10)
		dst += sprintf(dst, "x%d", offs);
	else {
		/* seen, temp write ? */
		if (offs & 1)
			dst += sprintf(dst, "?14? ");

		dst += sprintf(dst, "r%d", offs >> 1);
	}

	dst += sprintf(dst, "%s", pscale_str[pscale]);

	if (sat)
		dst += sprintf(dst, "_sat");

	return buf;
}

void disasm_frag_alu_instr(uint64_t instr)
{
	int opcode;

	printf(" %016"PRIx64" ", instr);

	if (instr == 0x3e41f200000fe7e8ull) {
		printf("NOP\n");
		return;
	}
#if 0
	/* not seen */
	if ((instr >> 20) & 1)
		printf("?20? ");

	/* seen related to gl_FragCoord.z and discard and step? */
	if ((instr >> 21) & 1)
		printf("?21? ");

	/* seen, probably +/- 1ULP or something... ? */
	if ((instr >> 22) & 1)
		printf("?22? ");

	/* seen, related to gl_FragCoord.z and integer conversion? */
	if ((instr >> 23) & 1)
		printf("?23? ");

	/* not seen */
	if ((instr >> 27) & 1)
		printf("?27? ");

	/* seen, related to temp-read? */
	if ((instr >> 28) & 1)
		printf("?28? ");
#endif
	/* seen, temp-write? */
	if ((instr >> 29) & 1)
		printf("?29? ");

	opcode = (instr >> 30) & 0x3; /* 30..31 */

	/* not seen */
	if ((instr >> 32) & 1)
		printf("?32? ");
	if ((instr >> 33) & 1)
		printf("?33? ");

	/* seen:
	 * (rA + rC) * rB
	 * rA * rB + rC * rB
	 * rA * rB + rC * rC
	 */
	if ((instr >> 34) & 1)
		printf("?34? ");

	/* not seen */
	if ((instr >> 35) & 1)
		printf("?35? ");
	if ((instr >> 36) & 1)
		printf("?36? ");

	/* seen:
	 * rA * rB + rC * rC
	 */
	if ((instr >> 37) & 1)
		printf("?37? ");

	/* seen, var + float(gl_FrontFacing) */
	if ((instr >> 50) & 1)
		printf("?50? ");

	/* not seen */
	if ((instr >> 63) & 1)
		printf("?63? ");

	switch (opcode) {
	case 0x0: printf("FMA "); break;
	case 0x1: printf("MIN "); break;
	case 0x2: printf("MAX "); break;
	case 0x3: printf("CSEL "); break;
	default:
		printf("0x%x ", opcode);
	}

	/* 12..29 */
	printf("%s, ", decode_rd((instr >> 12) & ((1 << 16) - 1)));
	/* 0..11 */
	printf("%s, ", decode_operand((instr >>  0) & ((1 << 12) - 1)));
	/* 51..62 */
	printf("%s, ", decode_operand((instr >> 51) & ((1 << 12) - 1)));
	/* 38..49 */
	printf("%s\n", decode_operand((instr >> 38) & ((1 << 12) - 1)));
}

void disasm_frag_alu_instrs(const uint64_t instrs[4])
{
	int i;
	embedded_consts = instrs[3];
	embedded_consts_used = 0;
	for (i = 0; i < 4; ++i) {
		if (i == 3 && embedded_consts_used) {
#if 0
			int j;
			for (j = 0; j < 3; ++j) {
				int offset = 4 + j * 20;
				printf("fp20[%d] = %f\n", j, decode_fp20(embedded_consts >> offset));
			}
			for (j = 0; j < 6; ++j) {
				int offset = 4 + j * 10;
				printf("fix10[%d] = %f\n", j, decode_fix10(embedded_consts >> offset));
			}
#endif
			continue;
		}

		printf("%d: ", i);
		disasm_frag_alu_instr(instrs[i]);
	}
}


void disasm_frag_alu(FILE *fp, int count)
{
	int i;
	for (i = 0; i < count; ++i) {
		uint64_t instrs[4];
		instrs[0] = read64(fp);
		instrs[1] = read64(fp);
		instrs[2] = read64(fp);
		instrs[3] = read64(fp);
		disasm_frag_alu_instrs(instrs);
	}
}

void disasm_frag_lut(FILE *fp, int count)
{
	int i;
	for (i = 0; i < count; ++i) {
		uint64_t word = read64(fp);
		const char *opcodes[] = {
			"??0",  /* 0 */
			"RCP",  /* 1 */
			"RSQ",  /* 2 */
			"LOG",  /* 3 */
			"EXP",  /* 4 */
			"SQRT", /* 5 */
			"SIN",  /* 6 */
			"COS",  /* 7 */
			"FRC",  /* 8 */
			"PREEXP", /* 9 */
			"PRESIN", /* 10 */
			"PRECOS", /* 11 */
			"?12",  /* 12 */
			"?13",  /* 13 */
			"?14",  /* 14 */
			"?15",  /* 15 */
		};
		int opcode = (word >> 22) & 15;
		int reg = (word >> 26) & 0x1f;
		printf("%016"PRIx64" %s r%d\n", word, opcodes[opcode], reg);
	}
}

void disasm_frag_tex(FILE *fp, int count)
{
	int i;
	for (i = 0; i < count; ++i) {
		uint32_t word = read32(fp);
		int op = (word >> 8) & 15;
		int sampler = word & 15;
		int bias = (word >> 12) & 1;
		const char *opcodes[] = {
			"??0", /* 0 */
			"??1", /* 1 */
			"??2", /* 2 */
			"??3", /* 3 */
			"TEX", /* 4 */
			"??5", /* 5 */
			"??6", /* 6 */
			"??7", /* 7 */
			"??8", /* 8 */
			"??9", /* 9 */
			"?10", /* 10 */
			"?11", /* 11 */
			"?12", /* 12 */
			"?13", /* 13 */
			"?14", /* 14 */
			"?15", /* 15 */
		};
		printf("%08"PRIx32" ", word);
		if (!op) {
			printf("NOP\n");
			continue;
		}

		printf("%s%s s%d\n", opcodes[op], bias ? "_bias" : "", sampler);
	}
}

void disasm_vert(FILE *fp, int count)
{
	int i, j;
	for (i = 0; i < count; ++i) {
		uint32_t words[4] = {
			read32(fp),
			read32(fp),
			read32(fp),
			read32(fp)
		};

		for (j = 0; j < 4; ++j)
			printf("%08"PRIx32, words[i]);
		printf(": ");

		printf("??? ");

		printf("rD.%s%s%s%s, ",
		    ((words[3] >> 16) & 1) ? "x" : "",
		    ((words[3] >> 15) & 1) ? "y" : "",
		    ((words[3] >> 14) & 1) ? "z" : "",
		    ((words[3] >> 13) & 1) ? "w" : "");

		if ((words[1] >> 7) & 1)
			printf("-");
		printf("rA.%c%c%c%c, ",
		    "xyzw"[(words[1] >> 5) & 3],
		    "xyzw"[(words[1] >> 3) & 3],
		    "xyzw"[(words[1] >> 1) & 3],
		    "xyzw"[((words[1] & 1) << 1) | (words[2] >> 31)]);

		if ((words[2] >> 22) & 1)
			printf("-");
		printf("rB.%c%c%c%c, ",
		    "xyzw"[(words[2] >> 20) & 3],
		    "xyzw"[(words[2] >> 18) & 3],
		    "xyzw"[(words[2] >> 16) & 3],
		    "xyzw"[(words[2] >> 14) & 3]);

		printf("\n");
	}
}

void disasm_fp(FILE *fp)
{
	int i;
	uint32_t hash, magic;
	uint32_t consts_offset, consts_len;
	uint32_t bin_offset, bin_len;
	char buf[8];

	hash = read32(fp);
	magic = read32(fp);

	if (magic == 0x26836d2f)
		printf("Fragment shader\n");
        else if (magic == 0x26836d1f)
		printf("Vertex shader\n");
	else {
		fprintf(stderr, "Error: %x\n", magic);
		exit(1);
	}

	fseek(fp, 0xd8, SEEK_SET);
	/* strange, offset is stored in words, while len is stored in bytes */
	consts_offset = read32(fp) * 4;
	consts_len = read32(fp);
	printf("constants at %x..%x\n", consts_offset,
	    consts_offset + consts_len);

	fseek(fp, consts_offset, SEEK_SET);
	for (i = 0; i < consts_len; i += 4)
		printf("const[%d]: %f\n", i, decode_fp32(read32(fp)));


	fseek(fp, 0xe8, SEEK_SET);
	/* strange, offset is stored in words, while len is stored in bytes */
	bin_offset = read32(fp) * 4;
	bin_len = read32(fp);
	printf("code at %x..%x\n", bin_offset, bin_offset + bin_len);

	fseek(fp, bin_offset, SEEK_SET);
	if (magic == 0x26836d2f) {
		fread(buf, 1, 8, fp);
		if (strncmp(buf, "AR20-BIN", 8)) {
			fprintf(stderr, "wrong header: '%s'\n", buf);
			exit(1);
		}
		fseek(fp, bin_offset + 16, SEEK_SET);
	}

	for (i = 16; i < bin_len; i += 4) {
		uint32_t cmd = read32(fp);
		/* printf("cmd: %08x\n", cmd); */
		if ((cmd & 0xf0000000) == 0x20000000 ||
		    (cmd & 0xf0000000) == 0x10000000) {
			int j, count = cmd & 0xffff;
			if ((cmd & 0x0fff0000) == 0x08040000) {
				if (count % 8 != 0) {
					fprintf(stderr,
					    "count %d not dividable by 8\n",
					    count);
					exit(1);
				}
				printf("found ALU code at %x,"
				    " %d instruction words\n",
				    bin_offset + i + 4, count / 8);
				disasm_frag_alu(fp, count / 8);
			} else if ((cmd & 0x0fff0000) == 0x06040000) {
				if (count % 2) {
					fprintf(stderr,
					    "count %d not dividable by 2\n",
					    count);
					exit(1);
				}
				printf("found LUT code at %x,"
				    " %d instruction words\n",
				    bin_offset + i + 4, count / 2);
				disasm_frag_lut(fp, count / 2);
			} else if ((cmd & 0x0fff0000) == 0x07010000) {
				printf("found TEX code (0x701) at %x,"
				    " %d instruction words\n",
				    bin_offset + i + 4, count);
				disasm_frag_tex(fp, count);
			} else if ((cmd & 0x0fff0000) == 0x02060000) {
				if (count % 4) {
					fprintf(stderr,
					    "count %d not dividable by 4\n",
					    count);
					exit(1);
				}
				printf("found vertex shader (0x206) at %x,"
				    " %d instruction words\n",
				    bin_offset + i + 4, count / 4);
				disasm_vert(fp, count / 4);
			} else {
				printf("unknown upload of %d words to %x\n",
				    count, (cmd >> 16) & 0xfff);
				for (j = 0; j < count; ++j) {
					uint32_t word = read32(fp);
					printf("%08"PRIx32"\n", word);
				}
			}
			i += 4 * count;
		} else if ((cmd & 0xf0000000) == 0x40000000)
			printf("setting register %x to %x\n",
			    (cmd >> 16) & 0xfff, cmd & 0xffff);
		else
			printf("unknown cmd %08x\n", cmd);
	}
}

int main(int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc; ++i) {
		FILE *fp = fopen(argv[i], "rb");
		if (!fp) {
			perror("fopen");
			exit(1);
		}

		disasm_fp(fp);
		fclose(fp);
	}
}
