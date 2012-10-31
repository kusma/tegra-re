#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <nvmap_ioctl.h>

#include "nvmap.h"

static int nvmap_fd = -1;

int nvmap_open(void)
{
	nvmap_fd = open("/dev/nvmap", O_DSYNC | O_RDWR);
	if (nvmap_fd < 0)
		return -1;
	return 0;
}

void nvmap_close(void)
{
	close(nvmap_fd);
	nvmap_fd = -1;
}

int nvmap_get_fd(void)
{
	return nvmap_fd;
}

nvmap_handle_t nvmap_create(unsigned int size)
{
	struct nvmap_create_handle ch;
	ch.size = size;
	if (ioctl(nvmap_fd, NVMAP_IOC_CREATE, &ch) < 0)
		return 0;
	return ch.handle;
}

int nvmap_alloc(nvmap_handle_t h)
{
	struct nvmap_alloc_handle ah;

	ah.handle = h;
	ah.heap_mask = 0x1;
	ah.flags = 0x1; /* NVMAP_HANDLE_WRITE_COMBINE */
	ah.align = 0x20;

	return ioctl(nvmap_fd, NVMAP_IOC_ALLOC, &ah);
}

int nvmap_write(nvmap_handle_t h, size_t offset, const void *src, size_t size)
{
	struct nvmap_rw_handle rwh;

	rwh.handle = h;
	rwh.offset = offset;
	rwh.addr = (unsigned long)src;
	rwh.elem_size = rwh.hmem_stride = rwh.user_stride  = size;
	rwh.count = 1;

	return ioctl(nvmap_fd, NVMAP_IOC_WRITE, &rwh);
}

int nvmap_read(void *dst, nvmap_handle_t h, size_t offset, size_t size)
{
	struct nvmap_rw_handle rwh;

	rwh.handle = h;
	rwh.offset = offset;
	rwh.addr = (unsigned long)dst;
	rwh.elem_size = rwh.hmem_stride = rwh.user_stride  = size;
	rwh.count = 1;

	return ioctl(nvmap_fd, NVMAP_IOC_READ, &rwh);
}

void *nvmap_mmap(nvmap_handle_t h, size_t offset, size_t length, int flags)
{
	void *ptr;
	struct nvmap_map_caller mc;
	mc.handle = h;
	mc.offset = offset;
	mc.length = length;
	mc.flags = flags;

	ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, nvmap_fd, 0);
	if (ptr == MAP_FAILED)
		return NULL;

	mc.addr = (long)ptr;
	if (ioctl(nvmap_fd, NVMAP_IOC_MMAP, &mc))
		return NULL;

	return ptr;
}

int nvmap_cache(void *ptr, nvmap_handle_t handle, size_t len, int op)
{
	struct nvmap_cache_op co;
	co.addr = (long)ptr;
	co.handle = handle;
	co.len = len;
	co.op = op;
	return ioctl(nvmap_fd, NVMAP_IOC_CACHE, &co);
}
