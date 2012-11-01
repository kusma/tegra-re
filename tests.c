#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_PROGRAM_BINARY_LENGTH_OES
#define GL_PROGRAM_BINARY_LENGTH_OES 0x8741
#endif

#include <stdarg.h>
static void die(const char *fmt, ...)
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

void present(void);

void run_tests(void)
{
	GLenum err;
	GLint status, vbo, bin_len;
	GLint vs = glCreateShader(GL_VERTEX_SHADER),
	    fs = glCreateShader(GL_FRAGMENT_SHADER),
	    p = glCreateProgram();
	const char *vs_str =
/*	    "attribute vec3 pos;\n"
	    "uniform mat4 mvp;\n" */
	    "void main()\n"
	    "{\n"
/*	    "\tgl_Position = mvp * vec4(pos, 1.0);\n" */
	    "\tgl_PointSize = 1.5;\n"
	    "\tgl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
	    "}\n";

	static const char *fs_str =
	    "precision mediump float;\n"
	    "void main()\n"
	    "{\n"
	    "\tgl_FragColor = vec4(1.0, 1.0, 1.0, 0.5);\n"
/*	    "\tgl_FragColor = vec4(gl_FragCoord.xy + gl_FragCoord.xy, gl_FragCoord.xy * gl_FragCoord.xy);\n" */
/*	    "\tgl_FragColor = vec4(gl_FragCoord.xyxy + gl_FragCoord.xyxy);\n" */
	    "}\n";

	static const GLfloat verts[] = { 0.0, 0.0, 0.0 };

	glShaderSource(vs, 1, &vs_str, NULL);
	glShaderSource(fs, 1, &fs_str, NULL);

	glCompileShader(fs);
	glCompileShader(vs);
	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glLinkProgram(p);

	glGetProgramiv(p, GL_LINK_STATUS, &status);
	if (!status)
		die("failed to link");

/*	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glVertexAttribPointer(glGetAttribLocation(p, "pos"), 3, GL_FLOAT, GL_FALSE, 0, NULL);
*/

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(p);
	glDrawArrays(GL_POINTS, 0, 1);
	glFlush();

	present();
}
