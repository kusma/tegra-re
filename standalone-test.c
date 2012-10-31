#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <asm/types.h>
#include <nvmap_ioctl.h>
#include <nvhost_ioctl.h>

#include "nvmap.h"
#include "nvhost.h"

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

#define NVSYNCPT_3D                          (22)

int main(void)
{
	unsigned int syncpt;
	int reg;
	nvmap_handle_t handle;
	struct nvhost_submit_hdr_ext hdr;
	struct nvhost_cmdbuf cmdbuf;

	if (nvmap_open()) {
		perror("nvmap_open");
		exit(1);
	}

	if (nvhost_open(nvmap_get_fd())) {
		perror("nvhost_open");
		exit(1);
	}

	if (nvhost_syncpt_read(NVSYNCPT_3D, &syncpt)) {
		perror("nvhost_syncpt_read");
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
	if (!handle) {
		perror("nvmap_create");
		exit(1);
	}
	if (nvmap_alloc(handle) < 0) {
		perror("nvmap_alloc");
		exit(1);
	}

#if 1
	u32 *ptr = nvmap_mmap(handle, 0, 0x8000, 0);
	if (!ptr) {
		perror("nvmap_mmap");
		exit(1);
	}

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
	hdr.syncpt_id = NVSYNCPT_3D;
	hdr.syncpt_incrs = 1;
	hdr.num_cmdbufs = 1;
	hdr.num_relocs = 0;
	hdr.submit_version = NVHOST_SUBMIT_VERSION_V1;
	hdr.num_waitchks = 0;
	hdr.waitchk_mask = 0;

	if (ioctl(nvhost_get_gr3d_fd(), NVHOST_IOCTL_CHANNEL_SUBMIT_EXT, &hdr) < 0 &&
	    write(nvhost_get_gr3d_fd(), &hdr, sizeof(struct nvhost_submit_hdr)) < 0) {
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

	if (write(nvhost_get_gr3d_fd(), &cmdbuf, sizeof(cmdbuf)) < 0) {
		perror("write");
		exit(1);
	}

	if (nvhost_flush() < 0) {
		perror("nvhost_flush");
		exit(1);
	}

	if (nvhost_syncpt_wait(NVSYNCPT_3D, syncpt + hdr.syncpt_incrs, NVHOST_NO_TIMEOUT) < 0) {
		perror("nvhost_syncpt_wait");
		exit(1);
	}

	for (reg = 0x820; reg < 0x824; ++reg) {
		u32 val;
		if (!nvhost_read_3d_reg(reg, &val))
			printf("reg%03X = 0x%08X\n", reg, val);
	}
	return 0;
}

