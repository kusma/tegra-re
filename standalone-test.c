#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <asm/types.h>
#include <nvmap_ioctl.h>
#include <nvhost_ioctl.h>

static int nvmap_fd, gr3d_fd, ctrl_fd;

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

/* class ids */
enum {
    NV_HOST1X_CLASS_ID = 0x1,
    NV_VIDEO_ENCODE_MPEG_CLASS_ID = 0x20,
    NV_GRAPHICS_3D_CLASS_ID = 0x60
};

#include <stdint.h>
typedef uint32_t u32;

/* cdma opcodes */
static inline u32 nvhost_opcode_setclass(
    unsigned class_id, unsigned offset, unsigned mask)
{
	return (0 << 28) | (offset << 16) | (class_id << 6) | mask;
}

static inline u32 nvhost_opcode_incr(unsigned offset, unsigned count)
{
	return (1 << 28) | (offset << 16) | count;
}

static inline u32 nvhost_opcode_nonincr(unsigned offset, unsigned count)
{
	return (2 << 28) | (offset << 16) | count;
}

static inline u32 nvhost_opcode_mask(unsigned offset, unsigned mask)
{
	return (3 << 28) | (offset << 16) | mask;
}

static inline u32 nvhost_opcode_imm(unsigned offset, unsigned value)
{
	return (4 << 28) | (offset << 16) | value;
}

static inline u32 nvhost_opcode_restart(unsigned address)
{
	return (5 << 28) | (address >> 4);
}

static inline u32 nvhost_opcode_gather(unsigned offset, unsigned count)
{
	return (6 << 28) | (offset << 16) | count;
}

static inline u32 nvhost_opcode_gather_nonincr(unsigned offset, unsigned count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | count;
}

static inline u32 nvhost_opcode_gather_incr(unsigned offset, unsigned count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | BIT(14) | count;
}

int main(void)
{
	unsigned long handle;
	struct nvhost_set_nvmap_fd_args fda;
	struct nvhost_get_param_args pa;
	struct nvhost_ctrl_syncpt_read_args ra;
	struct nvhost_ctrl_syncpt_wait_args wa;

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
		perror("/dev/nvhost-gr3d");
		exit(1);
	}

	ctrl_fd = open("/dev/nvhost-ctrl", O_RDWR);
	if (ctrl_fd < 0) {
		perror("/dev/nvhost-ctrl");
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
		nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0),
		nvhost_opcode_incr(0x0, 1),
		0x1 << 8 | 0x16
	};

	if (nvmap_write(handle, 0, cmds, sizeof(cmds)) < 0) {
		perror("nvmap_write");
		exit(1);
	}

	/* get syncpt threshold */
	ra.id = 22;
	if (ioctl(ctrl_fd, NVHOST_IOCTL_CTRL_SYNCPT_READ, &ra) < 0) {
		perror("NVHOST_IOCTL_CTRL_SYNCPT_READ");
		exit(1);
	}
	printf("0x%x\n", ra.value);

	submit_cmd.hdr.syncpt_id = 22;
	submit_cmd.hdr.syncpt_incrs = 1;
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
		perror("NVHOST_IOCTL_CHANNEL_FLUSH");
		exit(1);
	}

	wa.id = 0x16;
	wa.thresh = ra.value + submit_cmd.hdr.syncpt_incrs;
	wa.timeout = 0xffffffff;
	if (ioctl(ctrl_fd, NVHOST_IOCTL_CTRL_SYNCPT_WAIT, &wa) < 0) {
		perror("NVHOST_IOCTL_CTRL_SYNCPT_WAIT");
		exit(1);
	}

	return 0;
}

