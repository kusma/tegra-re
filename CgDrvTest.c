#include "CgDrv.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

static int wrap_log(const char *format, ...)
{
	va_list args;
	int ret;
	va_start(args, format);
	ret = vfprintf(stdout, format, args);
	va_end(args);
	return ret;
}

void hexdump(const void *data, int size)
{
	unsigned char *buf = (void *) data;
	char alpha[17];
	int i;
	for (i = 0; i < size; i++) {
/*		if (!(i % 16))
			wrap_log("\t\t%08X", (unsigned int) buf + i); */
		if (((void *) (buf + i)) < ((void *) data)) {
			wrap_log("   ");
			alpha[i % 16] = '.';
		} else {
			wrap_log(" %02X", buf[i]);
			if (isprint(buf[i]) && (buf[i] < 0xA0))
				alpha[i % 16] = buf[i];
			else
				alpha[i % 16] = '.';
		}
		if ((i % 16) == 15) {
			alpha[16] = 0;
			wrap_log("\t|%s|\n", alpha);
		}
	}
	if (i % 16) {
		for (i %= 16; i < 16; i++) {
			wrap_log("   ");
			alpha[i] = '.';
			if (i == 15) {
				alpha[16] = 0;
				wrap_log("\t|%s|\n", alpha);
			}
		}
	}
}

static void dump_ctx(struct CgCtx *ctx)
{
	int i;
	wrap_log("unknown00 = %08x\n", ctx->unknown00);
	wrap_log("error: %s\n", ctx->error ? ctx->error : "<no error>");
	wrap_log("log: %s\n", ctx->log ? ctx->log : "<no log>");

	wrap_log("binary (%d bytes) =\n", ctx->binary_size);
	hexdump(ctx->binary, ctx->binary_size);

	wrap_log("unknown20 = %08x\n", ctx->unknown20);
	wrap_log("unknown24 = %p\n", ctx->unknown24);
	wrap_log("unknown28 = %zu\n", ctx->unknown28);
	hexdump(ctx->unknown24, ctx->unknown28);
	wrap_log("unknown32 = %p\n", ctx->unknown32);
	wrap_log("unknown36 = %08x\n", ctx->unknown36);
	wrap_log("unknown40 = %08x\n", ctx->unknown40);
	wrap_log("unknown44 = &unknown%d (%p)\n",
	    (int)ctx->unknown44 - (int)ctx, ctx->unknown44);

	wrap_log("unknown48 = %08x\n", ctx->unknown48);
	wrap_log("unknown52 = %08x\n", ctx->unknown52);
//	wrap_log("unknown56 = %p\n", ctx->unknown56);
//	hexdump(ctx->unknown56, 0x100);
	wrap_log("unknown60 = %08x\n", ctx->unknown60);
	wrap_log("unknown64 = %08x\n", ctx->unknown64);
	wrap_log("unknown68 = %08x\n", ctx->unknown68);
	wrap_log("unknown72 = %08x\n", ctx->unknown72);
	wrap_log("unknown76 = %08x\n", ctx->unknown76);
}

int main(int argc, char *argv[])
{
	int i, ret;
	enum shader_type type = FRAGMENT;
	for (i = 1; i < argc; ++i) {
		struct CgCtx *ctx;
#ifndef WANT_STABLE_ADDRS
		char *src;
		size_t src_len, src_alloc;
#else
		static char src[0x10000];
		size_t src_len;
#endif
		FILE *fp;

		const char *arg = argv[i];

		if (!strcmp(arg, "--frag")) {
			type = FRAGMENT;
			continue;
		}
		if (!strcmp(arg, "--vert")) {
			type = VERTEX;
			continue;
		}

		fp = fopen(arg, "r");
#ifndef WANT_STABLE_ADDRS
		src_alloc = 100;
		src = malloc(src_alloc);
		src_len = 0;
		while (1) {
			size_t read = fread(src + src_len, 1,
			    src_alloc - src_len, fp);

			if (!read)
				break;

			src_len += read;

			src_alloc = src_alloc * 2;
			src = realloc(src, src_alloc);
			if (!src) {
				perror("realloc");
				exit(1);
			}
		}
		fclose(fp);

		src = realloc(src, src_len + 1);
		if (!src) {
			perror("realloc");
			exit(1);
		}
#else
		src_len = fread(src, 1, sizeof(src) - 1, fp);
#endif
		src[src_len] = '\0';

		ctx = CgDrv_Create();
		if (!ctx) {
			fprintf(stderr, "CgDrv_Create failed!\n");
			return -1;
		}

		ret = CgDrv_Compile(ctx, 1, type, src, src_len, 0);
		if (ret) {
			fprintf(stderr, "CgDrv_Compile returned %d\n", ret);
			if (ctx->error)
				fprintf(stderr, "%s", ctx->error);

			return -1;
		}

		dump_ctx(ctx);
		fp = fopen("out.nvfb", "wb");
		if (fp) {
			fwrite(ctx->binary, 1, ctx->binary_size, fp);
			fclose(fp);
		}
	}
	return 0;
}

#if 0
extern void *__libc_malloc(size_t size);
void *malloc(size_t size)
{
	void *ret;

	wrap_log("malloc(0x%x)", size);
	ret = __libc_malloc(size);
	wrap_log(" = %p\n", ret);

	return ret;
}

extern void __libc_free(void *ptr);
void free(void *ptr)
{
	wrap_log("free(%p)\n", ptr);
	__libc_free(ptr);
}

extern void *__libc_realloc(void *ptr, size_t size);
void *realloc(void *ptr, size_t size)
{
	void *ret;

	wrap_log("realloc(%p, 0x%x)", ptr, size);
	ret = __libc_realloc(ptr, size);
	wrap_log(" = %p\n", ret);

	return ret;
}
#endif
