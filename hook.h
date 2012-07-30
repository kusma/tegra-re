#ifndef HOOK_H
#define HOOK_H

struct funcs {
	int (*ioctl_pre_fn)(int, int, ...);
	int (*ioctl_post_fn)(int, int, int, ...);
	ssize_t (*write_pre_fn)(int, const void *, size_t);
	ssize_t (*write_post_fn)(int, int, const void *, size_t);
};

struct open_hook {
	const char *path;
	struct funcs hooks;
};

static void *libc_dlsym(const char *name);

#endif /* WRAP_H */
