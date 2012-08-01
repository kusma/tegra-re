#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <asm/types.h>
#include <nvmap_ioctl.h>

#include "hook.h"

int enable_logging;

static int wrap_log(const char *format, ...)
{
	va_list args;
	int ret;

	if (!enable_logging)
		return 0;

	va_start(args, format);
	ret = vfprintf(stderr, format, args);
	va_end(args);
	return ret;
}

void do_hexdump(const void *data, int offset, int size)
{
	unsigned char *buf = (void *) data;
	char alpha[17];
	int i;
	for (i = 0; i < size; i++) {
		if (!(i % 16))
			wrap_log("\t\t%08X", (unsigned int) offset + i);
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

void do_hexdump_words(const void *data, int offset, int size)
{
	const unsigned char *buf8 = data;
	const unsigned int *buf32 = data;
	char alpha[17];
	int i;
	for (i = 0; i < size; i++) {
		if (!(i % 16))
			wrap_log("\t\t%08X", (unsigned int) offset + i);
		if (!(i % 4))
			wrap_log(" %08X", buf32[i / 4]);
		if (isprint(buf8[i]) && (buf8[i] < 0xA0))
			alpha[i % 16] = buf8[i];
		else
			alpha[i % 16] = '.';
		if ((i % 16) == 15) {
			alpha[16] = 0;
			wrap_log("\t|%s|\n", alpha);
		}
	}
	if (i % 16) {
		for (i %= 16; i < 16; i++) {
			if (!(i % 4))
				wrap_log("         ");
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

void hexdump(const void *data, int size)
{
	wrap_log("ptr %p\n", data);
	do_hexdump(data, 0, size);
}

int nvmap_fd = -1;

void hexdump_handle(long handle, int offset, int size)
{
	int err;
	void *buf;
	struct nvmap_rw_handle rwh = {
		.handle = handle,
		.offset = offset,
		.elem_size = size,
		.hmem_stride = size,
		.user_stride = size,
		.count = 1
	};
	static int (*ioctl)(int fd, int request, ...) = NULL;

	if (nvmap_fd < 0)
		return;

	if (!ioctl)
		ioctl = libc_dlsym("ioctl");

	buf = malloc(size);
	rwh.addr = (unsigned long)buf;
	if ((err = ioctl(nvmap_fd, NVMAP_IOC_READ, &rwh)) == 0) {
		wrap_log("handle %lx\n", handle);
		do_hexdump(buf, offset, size);
	} else {
		fprintf(stderr, "FAILED TO NVMAP_IOC_READ = %d!\n", err);
		exit(1);
	}
	free(buf);
}

void hexdump_handle_words(long handle, int offset, int size)
{
	int err;
	void *buf;
	struct nvmap_rw_handle rwh = {
		.handle = handle,
		.offset = offset,
		.elem_size = size,
		.hmem_stride = size,
		.user_stride = size,
		.count = 1
	};
	static int (*ioctl)(int fd, int request, ...) = NULL;

	if (nvmap_fd < 0)
		return;

	if (!ioctl)
		ioctl = libc_dlsym("ioctl");

	buf = malloc(size);
	rwh.addr = (unsigned long)buf;
	if ((err = ioctl(nvmap_fd, NVMAP_IOC_READ, &rwh)) == 0) {
		wrap_log("handle %lx\n", handle);
		do_hexdump_words(buf, offset, size);
	} else {
		fprintf(stderr, "FAILED TO NVMAP_IOC_READ = %d!\n", err);
		exit(1);
	}
	free(buf);
}

static struct nvmap_map_caller mappings[100];
static int num_mappings = 0;

static int nvmap_ioctl_pre(int fd, int request, ...)
{
	struct nvmap_alloc_handle *ah;
	struct nvmap_rw_handle *rwh;
	struct nvmap_map_caller *mc;

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
	case NVMAP_IOC_FREE:
	case NVMAP_IOC_PIN_MULT:
		break;

	case NVMAP_IOC_ALLOC:
		ah = ptr;
		wrap_log("Alloc(0x%x, 0x%x, 0x%x, %d)", ah->handle,
		    ah->heap_mask, ah->flags, ah->align);
		break;

	case NVMAP_IOC_MMAP:
		mc = ptr;
		mappings[num_mappings++] = *mc;
/*		wrap_log("MMap(0x%x, 0x%x, %d, 0x%x, %p)", mc->handle, mc->offset, mc->length, mc->flags, mc->addr); */
		wrap_log("struct nvmap_map_caller mc = {\n"
		    "\t.handle = 0x%x,\n"
		    "\t.offset = 0x%x,\n"
		    "\t.length = %d,\n"
		    "\t.flags = 0x%x,\n"
		    "\t.addr = %p\n"
		    "};\n"
		    "ioctl(%d, NVMAP_IOC_MMAP, &mc)", mc->handle, mc->offset, mc->length, mc->flags, mc->addr, fd);
		break;

	case NVMAP_IOC_WRITE:
		rwh = ptr;
		wrap_log("virtual address: %p:\n", rwh->addr);
		hexdump((void *)rwh->addr, rwh->elem_size);
		wrap_log("Write(%p, 0x%x, 0x%x, %d, %d, %d, %d)", rwh->addr,
		    rwh->handle, rwh->offset, rwh->elem_size, rwh->hmem_stride,
		    rwh->user_stride, rwh->count);
		break;

	default:
		wrap_log("ioctl(%d (/dev/nvmap), 0x%x (%d), ...)", fd, request, _IOC_NR(request));
	}
}

static int nvmap_ioctl_post(int ret, int fd, int request, ...)
{
	struct nvmap_create_handle *ch;
	struct nvmap_pin_handle *ph;
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
		ch = ptr;
		wrap_log("0x%x = CreateHandle(%d) = %d\n", ch->handle, ch->size, ret);
		break;

	case NVMAP_IOC_FREE:
		wrap_log("FreeHandle(0x%x) = %d\n", (int)ptr, ret);
		break;

	case NVMAP_IOC_PIN_MULT:
		ph = ptr;
		if (ph->count > 1) {
			hexdump((void *)ph->handles, ph->count * sizeof(unsigned long *));
			wrap_log("PinMultiple(0x%x, %d) = %p\n", ph->handles, ph->count, ph->addr);
			hexdump((void *)ph->addr, ph->count * sizeof(unsigned long *));
		} else
			wrap_log("PinSingle(0x%x) = %p\n", ph->handles, ph->addr);
		break;

	default:
		wrap_log(" = %d\n", ret);
	}
}

#include <nvhost_ioctl.h>
static int nvhost_gr3d_ioctl_pre(int fd, int request, ...)
{
	struct nvhost_set_nvmap_fd_args *fdargs;
	void *ptr = NULL;
	if (_IOC_SIZE(request)) {
		/* find pointer to payload */
		va_list va;
		va_start(va, request);
		ptr = va_arg(va, void *);
		va_end(va);
	}

	switch (request) {
	case NVHOST_IOCTL_CHANNEL_FLUSH:
		wrap_log("ioctl(%d (/dev/nvhost-gr3d), NVHOST_IOCTL_CHANNEL_FLUSH, ...)", fd);
		break;

	case NVHOST_IOCTL_CHANNEL_SET_NVMAP_FD:
		fdargs = ptr;
		nvmap_fd = fdargs->fd;
		wrap_log("ioctl(%d (/dev/nvhost-gr3d), NVHOST_IOCTL_CHANNEL_SET_NVMAP_FD, %d)", fd, fdargs->fd);
		break;

	default:
		wrap_log("ioctl(%d (/dev/nvhost-gr3d), 0x%x (%d), ...)", fd, request, _IOC_NR(request));
	}
}

static int nvhost_gr3d_ioctl_post(int ret, int fd, int request, ...)
{
	void *ptr = NULL;

	if (_IOC_SIZE(request)) {
		/* find pointer to payload */
		va_list va;
		va_start(va, request);
		ptr = va_arg(va, void *);
		va_end(va);
	}

	switch (request) {
	default:
		wrap_log(" = %d\n", ret);
	}
}

ssize_t nvhost_gr3d_write_pre(int fd, const void *ptr, size_t count)
{
	static struct nvhost_submit_hdr hdr;
	const unsigned char *curr = ptr;
	size_t remaining = count;
	int i;

	static int (*orig_ioctl)(int fd, int request, ...) = NULL;

	if (!orig_ioctl)
		orig_ioctl = libc_dlsym("ioctl");
#if 0
	hexdump(ptr, count);
	wrap_log("write(%d (/dev/nvhost-gr3d), %p, %d)\n", fd, ptr, count);
#endif

	while (1) {
		if (!hdr.num_cmdbufs && !hdr.num_relocs) {

			if (remaining < sizeof(hdr))
				break;

			memcpy(&hdr, curr, sizeof(hdr));
			curr += sizeof(hdr);
			remaining -= sizeof(hdr);

			wrap_log("hdr:\n");
			wrap_log("\thdr.syncpt_id = %d\n", hdr.syncpt_id);
			wrap_log("\thdr.syncpt_incrs = %d\n", hdr.syncpt_incrs);
			wrap_log("\thdr.num_cmdbufs = %d\n", hdr.num_cmdbufs);
			wrap_log("\thdr.num_relocs = %d\n", hdr.num_relocs);
		} else if (hdr.num_cmdbufs) {
			struct nvhost_cmdbuf cmdbuf;

			if (remaining < sizeof(cmdbuf))
				break;

			memcpy(&cmdbuf, curr, sizeof(cmdbuf));
			curr += sizeof(cmdbuf);
			remaining -= sizeof(cmdbuf);
			--hdr.num_cmdbufs;

			wrap_log("cmdbuf(%d left):\n", hdr.num_cmdbufs);
			wrap_log("\tcmdbuf.mem = %p\n", cmdbuf.mem);
			wrap_log("\tcmdbuf.offset = 0x%x\n", cmdbuf.offset);
			wrap_log("\tcmdbuf.words = 0x%x\n", cmdbuf.words);
#if 0
			for (i = 0; i < num_mappings; ++i) {
				unsigned char *ptr;
				if (mappings[i].handle != cmdbuf.mem)
					continue;
			/*	if (mappings[i].offset > cmdbuf.offset)
					continue; */
				ptr = (unsigned char *)mappings[i].addr;
				/* hexdump(ptr, mappings->length); */
				ptr -= mappings[i].offset;
				ptr += cmdbuf.offset;
				hexdump(ptr, cmdbuf.words * 4);
				break;
			}
			if (i == num_mappings)
#endif
			hexdump_handle_words(cmdbuf.mem, cmdbuf.offset, cmdbuf.words * 4);
		} else if (hdr.num_relocs) {
			struct nvhost_reloc reloc;

			if (remaining < sizeof(reloc))
				break;

			memcpy(&reloc, curr, sizeof(reloc));
			curr += sizeof(reloc);
			remaining -= sizeof(reloc);
			--hdr.num_relocs;
			
			wrap_log("reloc:\n");
			wrap_log("\tcmdbuf_mem = %p\n", reloc.cmdbuf_mem);
			wrap_log("\tcmdbuf_offset = 0x%x\n", reloc.cmdbuf_offset);
			wrap_log("\ttarget = %p\n", reloc.target);
			wrap_log("\ttarget_offset = 0x%x\n", reloc.target_offset);
		}
	}
}

ssize_t nvhost_gr3d_write_post(int ret, int fd, const void *ptr, size_t count)
{
	wrap_log("kernel-driver returned: = %d\n", ret);	
}

const struct open_hook open_hooks[] = {
	{ "/dev/nvmap", { nvmap_ioctl_pre, nvmap_ioctl_post } },
	{ "/dev/nvhost-gr3d", {
		nvhost_gr3d_ioctl_pre, nvhost_gr3d_ioctl_post,
		nvhost_gr3d_write_pre, nvhost_gr3d_write_post
	}}
};
const int num_open_hooks = sizeof(open_hooks) / sizeof(*open_hooks);


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
