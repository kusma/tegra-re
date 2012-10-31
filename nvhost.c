#include "nvhost.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <nvhost_ioctl.h>

static int gr3d_fd = -1, ctrl_fd = -1;

int nvhost_open(int nvmap_fd)
{
	struct nvhost_set_nvmap_fd_args fda;

	gr3d_fd = open("/dev/nvhost-gr3d", O_RDWR);
	if (gr3d_fd < 0)
		return -1;

	ctrl_fd = open("/dev/nvhost-ctrl", O_RDWR);
	if (ctrl_fd < 0) {
		close(gr3d_fd);
		gr3d_fd = -1;
		return -1;
	}

	fda.fd = nvmap_fd;
	if (ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_SET_NVMAP_FD, &fda) < 0) {
		close(gr3d_fd);
		gr3d_fd = -1;
		close(ctrl_fd);
		ctrl_fd = -1;
		return -1;
	}

	return 0;
}

int nvhost_get_gr3d_fd(void)
{
	return gr3d_fd;
}

int nvhost_read_3d_reg(int offset, uint32_t *val)
{
	int ret;
	struct nvhost_read_3d_reg_args ra;
	ra.offset = offset;
	ret = ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_READ_3D_REG, &ra);
	*val = ra.value;
	return ret;
}

int nvhost_flush(void)
{
	return ioctl(gr3d_fd, NVHOST_IOCTL_CHANNEL_FLUSH);
}

int nvhost_get_version(void)
{
	struct nvhost_get_param_args gpa;
	if (ioctl(ctrl_fd, NVHOST_IOCTL_CTRL_GET_VERSION, &gpa) < 0)
		return NVHOST_SUBMIT_VERSION_V0;
	return gpa.value;
}

int nvhost_syncpt_read(int id, unsigned int *syncpt)
{
	struct nvhost_ctrl_syncpt_read_args ra;
	ra.id = id;
	if (ioctl(ctrl_fd, NVHOST_IOCTL_CTRL_SYNCPT_READ, &ra) < 0)
		return -1;
	*syncpt = ra.value;
	return 0;
}

int nvhost_syncpt_wait(int id, int thresh, unsigned int timeout)
{
	struct nvhost_ctrl_syncpt_wait_args wa;
	wa.id = id;
	wa.thresh = thresh;
	wa.timeout = timeout;
	return ioctl(ctrl_fd, NVHOST_IOCTL_CTRL_SYNCPT_WAIT, &wa);
}
