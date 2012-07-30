#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "hook.h"

extern const struct open_hook open_hooks[];
extern const int num_open_hooks;
extern int enable_logging;

static struct funcs hooks[1024];

extern void *__libc_dlsym(void *, const char *);

static void *libc_dlsym(const char *name)
{
	void *ret, *libc = dlopen("libc.so", RTLD_LAZY);
	if (!libc)
		libc = dlopen("/lib/arm-linux-gnueabi/libc.so.6", RTLD_LAZY);
	ret = __libc_dlsym(libc, name);
	dlclose(libc);
	return ret;
}

int open(const char* path, int flags, ...)
{
	mode_t mode = 0;
	int fd;
	static int (*orig_open)(const char* path, int flags, ...) = NULL;

	if (!orig_open)
		orig_open = libc_dlsym("open");

	if (flags & O_CREAT) {
		va_list args;
		va_start(args, flags);
		mode = (mode_t)va_arg(args, int);
		va_end(args);

		/* we don't hook file-creation */
		fd = orig_open(path, flags, mode);
	} else {
		int i;

		fd = orig_open(path, flags);
		fprintf(stderr, "open(\"%s\", %d) = %d\n", path, flags, fd);

		if (fd < 0 || fd >= 1024)
			return fd;

		/* install ioctl-hooks */
		for (i = 0; i < num_open_hooks; ++i)
			if (!strcmp(path, open_hooks[i].path)) {
				hooks[fd] = open_hooks[i].hooks;
				enable_logging = 1;
				break;
			}
	}

	return fd;
}

int ioctl(int fd, int request, ...)
{
	int ret, i;
	size_t ioc_size;
	static int (*orig_ioctl)(int fd, int request, ...) = NULL;

	if (!orig_ioctl)
		orig_ioctl = libc_dlsym("ioctl");

	ioc_size = _IOC_SIZE(request);
	if (ioc_size) {
		void *ptr;
		va_list va;

		/* find pointer to payload */
		va_start(va, request);
		ptr = va_arg(va, void *);
		va_end(va);

		if (fd >= 0 && fd < 1024 && hooks[fd].ioctl_pre_fn)
			hooks[fd].ioctl_pre_fn(fd, request, ptr);
		ret = orig_ioctl(fd, request, ptr);
		if (fd >= 0 && fd < 1024 && hooks[fd].ioctl_post_fn)
			hooks[fd].ioctl_post_fn(ret, fd, request, ptr);
	} else {
		/* no payload */
		if (fd >= 0 && fd < 1024 && hooks[fd].ioctl_pre_fn)
			hooks[fd].ioctl_pre_fn(fd, request);
		ret = orig_ioctl(fd, request);
		if (fd >= 0 && fd < 1024 && hooks[fd].ioctl_post_fn)
			hooks[fd].ioctl_post_fn(ret, fd, request);
	}

	return ret;	
}

ssize_t write(int fd, const void *buf, size_t count)
{
	ssize_t ret;
	static ssize_t (*orig_write)(int fd, const void *buf, size_t count) = NULL;

	if (!orig_write)
		orig_write = libc_dlsym("write");

	if (fd >= 0 && fd < 1024 && hooks[fd].write_pre_fn)
		hooks[fd].write_pre_fn(fd, buf, count);

	ret = orig_write(fd, buf, count);

	if (fd >= 0 && fd < 1024 && hooks[fd].write_post_fn)
		hooks[fd].write_post_fn(ret, fd, buf, count);

	return ret;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	void *ret;
	static void *(*orig_mmap)(void *addr, size_t length, int prot, int flags, int fd, off_t offset) = NULL;

	if (!orig_mmap)
		orig_mmap = libc_dlsym("mmap");

	ret = orig_mmap(addr, length, prot, flags, fd, offset);

	fprintf(stderr, "mmap(%p, %d, 0x%x, 0x%x, %d, %d) = %p\n", addr, length, prot, flags, fd, (int)offset, ret);

	return ret;
}

