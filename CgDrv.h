#include <stddef.h>

enum shader_type {
	VERTEX = 0,
	FRAGMENT = 1
};

struct CgCtx {
	unsigned int unknown00;
	const char *error;
	const char *log;
	unsigned int unknown12[17];
};

struct CgCtx *CgDrv_Create(void);
int CgDrv_Compile(struct CgCtx *ctx, int unknown0, enum shader_type type, const char *str, size_t len, int unknown1);
