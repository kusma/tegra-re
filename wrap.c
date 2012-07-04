#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "hook.h"

static int wrap_log(const char *format, ...)
{
	va_list args;
	int ret;
	va_start(args, format);
	ret = vfprintf(stderr, format, args);
	va_end(args);
	return ret;
}

static void hexdump(const void *data, int size)
{
	unsigned char *buf = (void *) data;
	char alpha[17];
	int i;
	for (i = 0; i < size; i++) {
		if (!(i % 16))
			wrap_log("\t\t%08X", (unsigned int) buf + i);
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

/* extern void *__libc_dlopen(const char *, int); */
extern void *__libc_dlsym(void *, const char *);

void *libc_dlsym(const char *name)
{
	void *ret, *libc = dlopen("libc.so", RTLD_LAZY);
	if (!libc)
		libc = dlopen("/lib/arm-linux-gnueabi/libc.so.6", RTLD_LAZY);
	ret = __libc_dlsym(libc, name);
	dlclose(libc);
	return ret;
}

#define NVMAP_IOC_CREATE 0xc0084e00
#if 0
#define NVMAP_IOC_CLAIM 0xc0084e01
    NVMAP_IOC_FROM_ID = 0xc0084e02

    NVMAP_IOC_ALLOC = 0x40104e03
    NVMAP_IOC_FREE = 0x4e04
    NVMAP_IOC_MMAP = 0xc0184e05
    NVMAP_IOC_WRITE = 0x40204e06
    NVMAP_IOC_READ = 0x40204e07
    NVMAP_IOC_PARAM = 0xc0104e08
    NVMAP_IOC_PIN_MULT = 0xc0184e0a
    NVMAP_IOC_UNPIN_MULT = 0x40184e0b
    NVMAP_IOC_CACHE = 0x40184e0c
    NVMAP_IOC_GET_ID = 0xc0084e0d
#endif

#include <asm/types.h>
struct nvmap_create_handle {
	union {
		__u32 key;      /* ClaimPreservedHandle */
		__u32 id;       /* FromId */
		__u32 size;     /* CreateHandle */
	};
	__u32 handle;
};

static void dump_create_handle(struct nvmap_create_handle *ch)
{
	wrap_log("%p = struct nvmap_create_handle {\n\t{ 0x%x },\n\t0x%x\n};\n",
	    ch, ch->key, ch->handle);
}

static int nvmap_ioctl_pre(int fd, int request, ...)
{
	struct nvmap_create_handle *ch;
	void *ptr = NULL;
	if (_IOC_SIZE(request)) {
		/* find pointer to payload */
		va_list va;
		va_start(va, request);
		ptr = va_arg(va, void *);
		va_end(va);
	}

	switch (request) {
	case NVMAP_IOC_CREATE:
		ch = (struct nvmap_create_handle *)ptr;
		dump_create_handle(ch);
		wrap_log("ioctl(%d, NVMAP_IOC_CREATE, %p)", fd, ptr);
		break;
	default:
		wrap_log("ioctl(%d, 0x%x, ...)", fd, request);
	}
}

static int nvmap_ioctl_post(int ret, int fd, int request, ...)
{
	struct nvmap_create_handle *ch;
	void *ptr = NULL;
	wrap_log(" = %d\n", ret);

	if (_IOC_SIZE(request)) {
		/* find pointer to payload */
		va_list va;
		va_start(va, request);
		ptr = va_arg(va, void *);
		va_end(va);
	}

	switch (request) {
	case NVMAP_IOC_CREATE:
		ch = (struct nvmap_create_handle *)ptr;
		wrap_log("returned handle: %p\n", fd, ch->handle);
		break;
	}
}

const struct open_hook open_hooks[] = {
	{ "/dev/nvmap", { nvmap_ioctl_pre, nvmap_ioctl_post } }
};
const int num_open_hooks = sizeof(open_hooks) / sizeof(*open_hooks);

#if 0
int nvmap_ioctl_pre(int fd, int request, ...)
{
	int ret, i;
	size_t ioc_size = _IOC_SIZE(request);
	if (ioc_size) {
		void *ptr;
		va_list va;

		/* find pointer to payload */
		va_start(va, request);
		ptr = va_arg(va, void *);
		va_end(va);

		/* dump payload */
		hexdump(ptr, ioc_size);

		if (dev_nvmap_fd >= 0 && dev_nvmap_fd == fd)
			wrap_log("ioctl(%d, 0x%x, %p) = ", fd, request, ptr);
		ret = orig_ioctl(fd, request, ptr);
		
	} else {
		/* no payload */
		if (dev_nvmap_fd >= 0 && dev_nvmap_fd == fd)
			wrap_log("ioctl(%d, 0x%x) = ", fd, request);
		ret = orig_ioctl(fd, request);
	}

	if (dev_nvmap_fd >= 0 && dev_nvmap_fd == fd)
		fprintf(stderr, "%d\n", ret);

	return ret;	
}
#endif

/*
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
*/
