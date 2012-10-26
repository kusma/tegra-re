#include <stddef.h>

enum shader_type {
	VERTEX = 1,
	FRAGMENT = 2
};

struct CgCtx {
	unsigned int unknown00;
	const char *error;       /* error string */
	const char *log;         /* compile log */

	/* size and binary? */
	size_t        binary_size;
	void         *binary;

	unsigned int  unknown20; /* small int (1) */

	/* size and binary? huh, is this duplicate data? */
	void         *unknown24;
	size_t        unknown28;

	void         *unknown32; /* pointer */
	unsigned int  unknown36; /* NULL / 0x0*/
	unsigned int  unknown40; /* huge value (0x40a273d0) */
	unsigned int *unknown44; /* poiner to unknown40 */
	unsigned int  unknown48; /* NULL / 0x0 */
	unsigned int  unknown52; /* NULL / 0x0 */
	void         *unknown56; /* pointer to 0x100 bytes buffer -- is this for uniforms? */
	unsigned int  unknown60; /* small int (seen 3 and 5) */
	unsigned int  unknown64; /* small int (0x40) */
	unsigned int  unknown68; /* NULL / 0x0 */
	unsigned int  unknown72; /* small int (1) */
	unsigned int  unknown76; /* NULL / 0x0 */
};

struct CgCtx *CgDrv_Create(void);
int CgDrv_Compile(struct CgCtx *ctx, int unknown0, enum shader_type type, const char *str, size_t len, int unknown1);
