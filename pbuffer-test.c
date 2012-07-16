#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_PROGRAM_BINARY_LENGTH_OES
#define GL_PROGRAM_BINARY_LENGTH_OES 0x8741
#endif

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

void render(void)
{
	GLenum err;
	GLint status, vbo, bin_len;
	GLint vs = glCreateShader(GL_VERTEX_SHADER),
	    fs = glCreateShader(GL_FRAGMENT_SHADER),
	    p = glCreateProgram();
	const char *vs_str =
	    "attribute vec3 pos;\n"
	    "uniform mat4 mvp;\n"
	    "void main()\n"
	    "{\n"
	    "\tgl_Position = mvp * vec4(pos, 1.0);\n"
	    "}\n";

	static const char *fs_str =
	    "precision mediump float;"
	    "void main()\n"
	    "{\n"
	    "\tgl_FragColor = vec4(1.0, 0.0, 1.0, 0.5);\n"
	    "}\n";

	static const GLfloat verts[] = { 0.0, 0.0, 0.0 };

	fprintf(stderr, "*** START COMPILING (%d %d)\n", strlen(vs_str), strlen(fs_str));
	glShaderSource(vs, 1, &vs_str, NULL);
	glShaderSource(fs, 1, &fs_str, NULL);
	
	fprintf(stderr, "*** START COMPILING 2\n");
	glCompileShader(fs);
	fprintf(stderr, "*** START COMPILING 3\n");
	glCompileShader(vs);
	fprintf(stderr, "*** STOP COMPILING\n");
	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glLinkProgram(p);
	fprintf(stderr, "*** STOP COMPILING\n");

	glGetProgramiv(p, GL_LINK_STATUS, &status);
	if (!status)
		die("failed to link");

#if 0
	bin_len = 0;	
	glGetProgramiv(p, GL_PROGRAM_BINARY_LENGTH_OES, &bin_len);
	printf("binary length: %d\n", bin_len);
	{
		char buf[10000];
		GLenum format = 0;
		int len = 0;
		glGetProgramBinaryOES(p, sizeof(buf), &len, &format, buf);
	}

#endif
/*	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW); */

	glUseProgram(p);
/*	glVertexAttribPointer(glGetAttribLocation(p, "pos"), 3, GL_FLOAT, GL_FALSE, 0, NULL); */
	fprintf(stderr, "*** DRAW 1 POINT\n");
	glDrawArrays(GL_POINTS, 0, 1);
	glFlush();
	fprintf(stderr, "*** DRAW 1 POINT\n");
	glDrawArrays(GL_POINTS, 0, 1);
	glFlush();
	fprintf(stderr, "*** DRAW 2 POINTS\n");
	glDrawArrays(GL_POINTS, 0, 2);
	glFlush();
/*
	err = glGetError();
	if (err)
		die("GL error: 0x%x", err);

	fprintf(stderr, "*** CLEAR 1 0 1\n");
	fflush(stderr);
	glClearColor(1.0, 0.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glFlush();

	fprintf(stderr, "*** CLEAR 0 1 1\n");
	fflush(stderr);
	glClearColor(1.0, 0.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_POINTS, 0, 1);
	glFlush();
*/
/*
	fprintf(stderr, "FLUSH\n");
	fflush(stderr); */

/*	glFinish(); */
/*	fprintf(stderr, "*** READPIXELS\n");
	fflush(stderr);
	GLubyte data[4];
	glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
	fprintf(stderr, "*** STOP RENDER\n");
	fflush(stderr);
	printf("%d %d %d %d\n", data[0], data[1], data[2], data[3]); */
}

int main(int argc, char *argv[])
{
	EGLConfig cfg;
	EGLint num_cfg;
	static const EGLint cfg_attrs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint major, minor;
	EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	EGLSurface surf;
	static const EGLint surf_attrs[] = {
		EGL_WIDTH, 128,
		EGL_HEIGHT, 128,
		EGL_NONE
	};

	EGLContext ctx;
	static const EGLint ctx_attrs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
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

	printf("%s\n", glGetString(GL_EXTENSIONS));

	render();

	eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(dpy, ctx);
	eglDestroySurface(dpy, surf);
	eglTerminate(dpy);
}

