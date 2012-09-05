#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <asm/types.h>
#include <nvmap_ioctl.h>
#include <nvhost_ioctl.h>

static int nvmap_fd, gr3d_fd;

unsigned long nvmap_create(unsigned int size)
{
	struct nvmap_create_handle ch;
	struct nvmap_alloc_handle ah;

	ch.size = size;
	if (ioctl(nvmap_fd, NVMAP_IOC_CREATE, &ch) < 0) {
		perror("ioctl");
		exit(1);
	}

	return ch.handle;
}

int nvmap_alloc(unsigned long handle)
{
	struct nvmap_alloc_handle ah;

	ah.handle = handle;
	ah.heap_mask = 0x1;
	ah.flags = 0x1; /* NVMAP_HANDLE_WRITE_COMBINE */
	ah.align = 0x20;

	return ioctl(nvmap_fd, NVMAP_IOC_ALLOC, &ah);
}

int nvmap_write(long handle, int offset, const void *src, int size)
{
	struct nvmap_rw_handle rwh;

	rwh.handle = handle;
	rwh.offset = offset;
	rwh.addr = (unsigned long)src;
	rwh.elem_size = rwh.hmem_stride = rwh.user_stride  = size;
	rwh.count = 1;

	return ioctl(nvmap_fd, NVMAP_IOC_WRITE, &rwh);
}

int nvmap_read(void *dst, long handle, int offset, int size)
{
	struct nvmap_rw_handle rwh;

	rwh.handle = handle;
	rwh.offset = offset;
	rwh.addr = (unsigned long)dst;
	rwh.elem_size = rwh.hmem_stride = rwh.user_stride  = size;
	rwh.count = 1;

	return ioctl(nvmap_fd, NVMAP_IOC_READ, &rwh);
}


int main(void)
{
	unsigned long handle;
	struct nvhost_set_nvmap_fd_args fda;
	struct nvhost_get_param_args pa;

	struct {
		struct nvhost_submit_hdr hdr;
		struct nvhost_cmdbuf cmdbuf;
	} __attribute__ ((packed)) submit_cmd;

	nvmap_fd = open("/dev/nvmap", O_DSYNC | O_RDWR);
	if (nvmap_fd < 0) {
		perror("/dev/nvmap");
		exit(1);
	}

	gr3d_fd = open("/dev/nvhost-gr3d", O_RDWR);
	if (gr3d_fd < 0) {
		perror("/dev/gr3d");
		exit(1);
	}

	fda.fd = nvmap_fd;
	if (ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_SET_NVMAP_FD, &fda) < 0) {
		perror("ioctl");
		exit(1);
	}

#if 0
	if (ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_GET_SYNCPOINTS, &pa) < 0) {
		perror("ioctl");
		exit(1);
	}
	printf("syncpoints: 0x%x\n", pa.value);

	if (ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_GET_WAITBASES, &pa) < 0) {
		perror("ioctl");
		exit(1);
	}
	printf("waitbases: 0x%x\n", pa.value);
#endif

	handle = nvmap_create(0x8000);
	if (nvmap_alloc(handle) < 0) {
		perror("nvmap_alloc");
		exit(1);
	}
	printf("handle: 0x%lx\n", handle);

	unsigned int cmds[] = {
		0x0
	};

	if (nvmap_write(handle, 0, cmds, sizeof(cmds)) < 0) {
		perror("nvmap_write");
		exit(1);
	}

	submit_cmd.hdr.syncpt_incrs = 0;
	submit_cmd.hdr.num_cmdbufs = 1;
	submit_cmd.hdr.num_relocs = 0;

	submit_cmd.cmdbuf.mem = handle;
	submit_cmd.cmdbuf.offset = 0;
	submit_cmd.cmdbuf.words = sizeof(cmds) / sizeof(cmds[0]);

	if (write(gr3d_fd, &submit_cmd, sizeof(submit_cmd)) < 0) {
		perror("write");
		exit(1);
	}

	if (ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_FLUSH) < 0) {
		perror("flush");
		exit(1);
	}

	return 0;
}

