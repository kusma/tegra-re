#include "CgDrv.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
	int ret;
	struct CgCtx *ctx = CgDrv_Create();

	const char *str =
/*	    "fjattribute vec2 a;" */
	    "void main()\n"
	    "{\n"
	    "\tgl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
	    "}\n";

	if (!ctx) {
		fprintf(stderr, "CgDrv_Create failed!\n");
		return -1;
	}

	ret = CgDrv_Compile(ctx, 1, VERTEX, str, strlen(str), 0);
	if (ret) {
		fprintf(stderr, "CgDrv_Compile returned %d\n", ret);
		if (ctx->error)
			fprintf(stderr, "%s", ctx->error);
		
		return -1;
	}

	printf("compiled successfully!\n");
}
