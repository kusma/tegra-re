#ifndef NVHOST_H
#define NVHOST_H

#include <stdint.h>

int nvhost_open(int nvmap_fd);
int nvhost_get_gr3d_fd(void);
int nvhost_read_3d_reg(int offset, uint32_t *val);
int nvhost_flush(void);
int nvhost_get_version(void);
int nvhost_syncpt_read(int id, unsigned int *syncpt);
int nvhost_syncpt_wait(int id, int thresh, unsigned int timeout);

#endif /* NVHOST_H */
