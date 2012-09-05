#include <GLES/gl.h>
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *egl_strerror(EGLint err)
{
        switch (err) {
        case EGL_SUCCESS: return "success";
        case EGL_NOT_INITIALIZED: return "not initialized";
        case EGL_BAD_ACCESS: return "bad access";
        case EGL_BAD_ALLOC: return "bad alloc";
        case EGL_BAD_ATTRIBUTE: return "bad attribute";
        case EGL_BAD_CONTEXT: return "bad context";
        case EGL_BAD_CONFIG: return "bad config";
        case EGL_BAD_CURRENT_SURFACE: return "bad current surface";
        case EGL_BAD_DISPLAY: return "bad display";
        case EGL_BAD_SURFACE: return "bad surface";
        case EGL_BAD_MATCH: return "bad match";
        case EGL_BAD_PARAMETER: return "bad parameter";
        case EGL_BAD_NATIVE_PIXMAP: return "bad native pixmap";
        case EGL_BAD_NATIVE_WINDOW: return "bad native window";

        default:
                return "unknown error";
        }
}

void egl_perror(const char *str)
{
        fprintf(stderr, "%s: %s\n", str, egl_strerror(eglGetError()));
}

#include <stdarg.h>
void die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	fputs("fatal: ", stderr);
	vfprintf(stderr, fmt, va);
	fputc('\n', stderr);
	va_end(va);
	exit(1);
}

#define WIDTH 32
#define HEIGHT 32

void render(void)
{
	int x, y;
	GLenum err;
	static const GLfloat verts[] = { 0.0, 0.0, 0.0 };

/*	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glVertexAttribPointer(glGetAttribLocation(p, "pos"), 3, GL_FLOAT, GL_FALSE, 0, NULL);
*/

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(0, 0, WIDTH, HEIGHT);

	printf("# setup\n");
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDrawArrays(GL_POINTS, 0, 1);
	glFlush();

	printf("# draw\n");
	glPointSize(13.37f);
	glDrawArrays(GL_POINTS, 0, 1);
	glDisableClientState(GL_VERTEX_ARRAY);
	glFlush();

	{
		GLubyte data[WIDTH * HEIGHT * 4];
		printf("# readpixels\n");
		glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, data);
		fputs("# ", stdout);
		for (y = 0; y < HEIGHT; ++y, fputs("\n# ", stdout))
			for (x = 0; x < WIDTH; ++x)
				putchar(data[(y * WIDTH + x) * 4] ? '*' : '.');
		printf("%d %d %d %d\n", data[0], data[1], data[2], data[3]);
	}
}

int main(int argc, char *argv[])
{
	EGLConfig cfg;
	EGLint num_cfg;
	static const EGLint cfg_attrs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
		EGL_NONE
	};

	EGLint major, minor;
	EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	EGLSurface surf;
	static const EGLint surf_attrs[] = {
		EGL_WIDTH, WIDTH,
		EGL_HEIGHT, HEIGHT,
		EGL_NONE
	};

	EGLContext ctx;
	static const EGLint ctx_attrs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 1,
		EGL_NONE
	};

	if (!eglInitialize(dpy, &major, &minor)) {
		fprintf(stderr, "failed to init egl\n");
		exit(1);
	}

	printf("EGL version: %d.%d\n", major, minor);

	if (!eglChooseConfig(dpy, cfg_attrs, &cfg, 1, &num_cfg) || num_cfg != 1) {
		egl_perror("failed to choose config");
		exit(1);
	}

	if ((surf = eglCreatePbufferSurface(dpy, cfg, surf_attrs)) == EGL_NO_SURFACE) {
		egl_perror("failed to create surface");
		exit(1);
	}

	if ((ctx = eglCreateContext(dpy, cfg, NULL, ctx_attrs)) == EGL_NO_CONTEXT) {
		egl_perror("failed to create context");
		exit(1);
	}

	if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
		egl_perror("failed to make current");
		exit(1);
	}

	printf("# extensions: %s\n", glGetString(GL_EXTENSIONS));

	render();

	eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(dpy, ctx);
	eglDestroySurface(dpy, surf);
	eglTerminate(dpy);
}

