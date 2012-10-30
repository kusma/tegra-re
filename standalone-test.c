#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

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

void *nvmap_mmap(long handle, int offset, int length, int flags)
{
	void *ptr;
	struct nvmap_map_caller mc;
	mc.handle = handle;
	mc.offset = offset;
	mc.length = length;
	mc.flags = flags;

	ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, nvmap_fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	mc.addr = (long)ptr;
	if (ioctl(nvmap_fd, NVMAP_IOC_MMAP, &mc)) {
		perror("ioctl");
		exit(1);
	}

	return ptr;
}

int nvmap_cache(void *ptr, long handle, int len, int op)
{
	struct nvmap_cache_op co;
	co.addr = (long)ptr;
	co.handle = handle;
	co.len = len;
	co.op = op;
	return ioctl(nvmap_fd, NVMAP_IOC_CACHE, &co);
}
/* class ids */
enum {
    NV_HOST1X_CLASS_ID = 0x1,
    NV_VIDEO_ENCODE_MPEG_CLASS_ID = 0x20,
    NV_GRAPHICS_3D_CLASS_ID = 0x60
};

enum {
    NV_HOST_MODULE_HOST1X = 0,
    NV_HOST_MODULE_MPE = 1,
    NV_HOST_MODULE_GR3D = 6
};

/* host class */
enum {
    NV_CLASS_HOST_INCR_SYNCPT = 0x0,
    NV_CLASS_HOST_WAIT_SYNCPT = 0x8,
    NV_CLASS_HOST_WAIT_SYNCPT_BASE = 0x9,
    NV_CLASS_HOST_INCR_SYNCPT_BASE = 0xc,
    NV_CLASS_HOST_INDOFF = 0x2d,
    NV_CLASS_HOST_INDDATA = 0x2e
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

static inline u32 nvhost_class_host_incr_syncpt_base(
    unsigned base_indx, unsigned offset)
{
	return (base_indx << 24) | offset;
}

static u32 nvhost_read_3d_reg(int offset)
{
	struct nvhost_read_3d_reg_args ra;
	ra.offset = offset;
	if (ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_READ_3D_REG, &ra)) {
		perror("ioctl");
		exit(1);
	}
	return ra.value;
}

#define NVSYNCPT_3D                          (22)

int main(void)
{
	int reg;
	unsigned long handle;
	struct nvhost_set_nvmap_fd_args fda;
	struct nvhost_get_param_args pa;
	struct nvhost_ctrl_syncpt_read_args ra;
	struct nvhost_ctrl_syncpt_wait_args wa;
	struct nvhost_submit_hdr_ext hdr;
	struct nvhost_cmdbuf cmdbuf;

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


#if 1
	u32 *ptr = nvmap_mmap(handle, 0, 0x8000, 0);
	/* invalidate cache */
	if (nvmap_cache(ptr, handle, 0x8000, NVMAP_CACHE_OP_INV) < 0) {
		perror("nvmap_cache");
		exit(1);
	}
	u32 *curr = ptr;
	*curr++ = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID, NV_CLASS_HOST_INCR_SYNCPT_BASE, 1);
	*curr++ = nvhost_class_host_incr_syncpt_base(3, 1);
	*curr++ = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0);
	*curr++ = nvhost_opcode_incr(0x820, 4);
	*curr++ = 0xF0F0F0F0;
	*curr++ = 0xFF00FF00;
	*curr++ = 0xFFFF0000;
	*curr++ = 0x0000FFFF;
	*curr++ = nvhost_opcode_imm(0x0, (0x1 << 8) | 0x16);

	/* flush and invalidate cache */
	if (nvmap_cache(ptr, handle, 0x8000, NVMAP_CACHE_OP_WB_INV) < 0) {
		perror("nvmap_cache");
		exit(1);
	}

#else
	unsigned int cmds[] = {
		nvhost_opcode_setclass(NV_HOST1X_CLASS_ID, NV_CLASS_HOST_INCR_SYNCPT_BASE, 1),
		nvhost_class_host_incr_syncpt_base(3, 1),
		nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0),
#if 0
		nvhost_opcode_incr(0x820, 4),
		0xF0F0F0F0,
		0xFF00FF00,
		0xFFFF0000,
		0x0000FFFF,
#endif
		nvhost_opcode_imm(0x0, (0x1 << 8) | 0x16)
	};

	if (nvmap_write(handle, 0, cmds, sizeof(cmds)) < 0) {
		perror("nvmap_write");
		exit(1);
	}
#endif

	/* get syncpt threshold */
	ra.id = NVSYNCPT_3D;
	if (ioctl(ctrl_fd, NVHOST_IOCTL_CTRL_SYNCPT_READ, &ra) < 0) {
		perror("NVHOST_IOCTL_CTRL_SYNCPT_READ");
		exit(1);
	}
	printf("0x%x\n", ra.value);

	hdr.syncpt_id = NVSYNCPT_3D;
	hdr.syncpt_incrs = 1;
	hdr.num_cmdbufs = 1;
	hdr.num_relocs = 0;
	hdr.submit_version = NVHOST_SUBMIT_VERSION_V1;
	hdr.num_waitchks = 0;
	hdr.waitchk_mask = 0;

	if (ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_SUBMIT_EXT, &hdr) < 0 &&
	    write(gr3d_fd, &hdr, sizeof(struct nvhost_submit_hdr)) < 0) {
		perror("write");
		exit(1);
	}

	cmdbuf.mem = handle;
	cmdbuf.offset = 0;
#if 0
	cmdbuf.words = sizeof(cmds) / sizeof(cmds[0]);
#else
	cmdbuf.words = curr - ptr;
#endif

	if (write(gr3d_fd, &cmdbuf, sizeof(cmdbuf)) < 0) {
		perror("write");
		exit(1);
	}

	if (ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_FLUSH) < 0) {
		perror("NVHOST_IOCTL_CHANNEL_FLUSH");
		exit(1);
	}

	wa.id = NVSYNCPT_3D;
	wa.thresh = ra.value + hdr.syncpt_incrs;
	wa.timeout = 0xffffffff;
	if (ioctl(ctrl_fd, NVHOST_IOCTL_CTRL_SYNCPT_WAIT, &wa) < 0) {
		perror("NVHOST_IOCTL_CTRL_SYNCPT_WAIT");
		exit(1);
	}

	for (reg = 0x820; reg < 0x824; ++reg)
		printf("reg%03X = 0x%08X\n", reg, nvhost_read_3d_reg(reg));
	return 0;
}

