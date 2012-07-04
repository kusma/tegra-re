#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <linux/ioctl.h>
#include <dlfcn.h>

/*
dlsym(..., "CgDrv_Create")
dlsym(..., "CgDrv_Compile")
dlsym(..., "CgDrv_CleanUp")
dlsym(..., "CgDrv_Delete")
*/


int wrap_log(const char *format, ...)
{
	va_list args;
	int ret;
	va_start(args, format);
	ret = vfprintf(stderr, format, args);
	va_end(args);
	return ret;
}

enum shader_type {
	VERTEX = 0,
	FRAGMENT = 1
};

struct CgCtx {
	/* should be 80 bytes */
	unsigned int unknown00;
	const char *error;       /* error string */
	const char *log;         /* compile log */
#if 0
	size_t        unknown12; /* size of... something! The driver mallocs a buffer of this size right on return. */
	void         *unknown16; /* pointer (0x248 bytes) */
	unsigned int  unknown20; /* small int (1) */
	void         *unknown24; /* pointer (0x220 bytes) */
	unsigned int  unknown28; /* size of unknown6 */
	void         *unknown32; /* pointer */
	unsigned int  unknown36; /* NULL / 0x0*/
	unsigned int  unknown40; /* huge value (0x40a273d0) */
	unsigned int *unknown44; /* poiner to unknown40 */
	unsigned int  unknown48; /* NULL / 0x0 */
	unsigned int  unknown52; /* NULL / 0x0 */
	void         *unknown56; /* pointer to 0x80 bytes buffer */
	unsigned int  unknown60; /* small int (3) */
	unsigned int  unknown64; /* small int (0x40) */
	unsigned int  unknown68; /* NULL / 0x0 */
	unsigned int  unknown72; /* small int (1) */
	unsigned int  unknown76; /* NULL / 0x0 */
#else
 	unsigned int unknown12[17];
#endif
};

static void dump_ctx(struct CgCtx *ctx)
{
	int i;
	fprintf(stderr, "ctx:\n---8<---\n");
	fprintf(stderr, "unknown0 = %08x\n", ctx->unknown00);
	fprintf(stderr, "error: %s\n", ctx->error ? ctx->error : "<no error>");
	fprintf(stderr, "log: %s\n", ctx->log ? ctx->log : "<no log>");
	for (i = 0; i < 17; ++i) {
		fprintf(stderr, "unknown%d = %08x\n", 12 + i * 4, ctx->unknown12[i]);
	} 
/*	fprintf(stderr, "HMMM: %08x\n", ctx->unknown1[9]);
	fprintf(stderr, "HMMM2: %08x\n", &ctx->unknown1[8]);
	fprintf(stderr, "HMMM2: %08x\n", ctx->unknown1[8]); */
	fprintf(stderr, "---8<---\n");
}

static struct CgCtx *(*CgDrv_Create_orig)(void) = NULL;
struct CgCtx *CgDrv_Create(void)
{
	void *ret;
	fprintf(stderr, "CgDrvCreate()");
	if (CgDrv_Create_orig)
		ret = CgDrv_Create_orig();
	fprintf(stderr, " = 0x%p\n", ret);
	dump_ctx(ret);
	return ret;
}

static int (*CgDrv_Compile_orig)(struct CgCtx *ctx, int b, enum shader_type type, const char *str, size_t len, int d) = NULL;
int CgDrv_Compile(struct CgCtx *ctx, int b, enum shader_type type, const char *str, size_t len, int d)
{
	int ret;
	fprintf(stderr, "shader:\n---8<---\n%s---8<---\n", str);
	dump_ctx(ctx);
	fprintf(stderr, "CgDrv_Compile(%p, %d, %d, %s, %d, %d)", ctx, b, type, "<str>", len, d);
	if (CgDrv_Compile_orig)
		ret = CgDrv_Compile_orig(ctx, b, type, str, len, d);
	fprintf(stderr, " = %d\n", ret);
	if (ctx->error)
		fprintf(stderr, "error:\n---8<---\n%s---8<---\n", ctx->error);
	dump_ctx(ctx);
	return ret;
}

extern void *__libc_dlsym(void *, const char *);

void *dlsym(void *handle, const char *name)
{
	if (!strcmp(name, "CgDrv_Create")) {
		CgDrv_Create_orig = __libc_dlsym(handle, name);
		return CgDrv_Create;
	}
	if (!strcmp(name, "CgDrv_Compile")) {
		CgDrv_Compile_orig = __libc_dlsym(handle, name);
		return CgDrv_Compile;
	}
	fprintf(stderr, "dlsym(%p, \"%s\")\n", handle, name);
	return __libc_dlsym(handle, name);
}

