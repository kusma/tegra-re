#ifndef NVMAP_H
#define NVMAP_H

int nvmap_open(void);
void nvmap_close(void);
int nvmap_get_fd(void);

typedef unsigned long nvmap_handle_t;

nvmap_handle_t nvmap_create(size_t size);
int nvmap_alloc(nvmap_handle_t h);
int nvmap_write(nvmap_handle_t h, size_t offset, const void *src, size_t size);
int nvmap_read(void *dst, nvmap_handle_t h, size_t offset, size_t size);
void *nvmap_mmap(nvmap_handle_t h, size_t offset, size_t length, int flags);
int nvmap_cache(void *ptr, nvmap_handle_t handle, size_t len, int op);

#endif /* NVMAP_H */
