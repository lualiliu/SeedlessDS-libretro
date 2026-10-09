/* Soft-render path: GLES is never used for presentation. Provide linkable
 * no-ops so video_out_gl symbol references resolve. */

#include <stddef.h>

void glActiveTexture(unsigned int a) { (void)a; }
void glAttachShader(unsigned int a, unsigned int b) { (void)a; (void)b; }
void glBindFramebuffer(unsigned int a, unsigned int b) { (void)a; (void)b; }
void glBindTexture(unsigned int a, unsigned int b) { (void)a; (void)b; }
unsigned int glCheckFramebufferStatus(unsigned int a)
{
    (void)a;
    return 0x8CD5u; /* GL_FRAMEBUFFER_COMPLETE */
}
void glClear(unsigned int a) { (void)a; }
void glCompileShader(unsigned int a) { (void)a; }
unsigned int glCreateProgram(void) { return 1u; }
unsigned int glCreateShader(unsigned int a)
{
    (void)a;
    return 1u;
}
void glDeleteFramebuffers(int n, const unsigned int *a)
{
    (void)n;
    (void)a;
}
void glDeleteProgram(unsigned int a) { (void)a; }
void glDeleteShader(unsigned int a) { (void)a; }
void glDeleteTextures(int n, const unsigned int *a)
{
    (void)n;
    (void)a;
}
void glDetachShader(unsigned int a, unsigned int b) { (void)a; (void)b; }
void glDrawArrays(unsigned int a, int b, int c)
{
    (void)a;
    (void)b;
    (void)c;
}
void glEnableVertexAttribArray(unsigned int a) { (void)a; }
void glFinish(void) {}
void glFramebufferTexture2D(unsigned int a, unsigned int b, unsigned int c,
                            unsigned int d, int e)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
}
void glGenFramebuffers(int n, unsigned int *a)
{
    if (a && n > 0)
        a[0] = 1u;
}
void glGenTextures(int n, unsigned int *a)
{
    if (a && n > 0)
        a[0] = 1u;
}
int glGetAttribLocation(unsigned int a, const char *b)
{
    (void)a;
    (void)b;
    return 0;
}
void glGetShaderInfoLog(unsigned int a, int b, int *c, char *d)
{
    (void)a;
    (void)b;
    if (c)
        *c = 0;
    if (d)
        d[0] = 0;
}
void glGetShaderiv(unsigned int a, unsigned int b, int *c)
{
    (void)a;
    (void)b;
    if (c)
        *c = 1;
}
int glGetUniformLocation(unsigned int a, const char *b)
{
    (void)a;
    (void)b;
    return 0;
}
void glLinkProgram(unsigned int a) { (void)a; }
void glShaderSource(unsigned int a, int b, const char *const *c, const int *d)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}
void glTexImage2D(unsigned int a, int b, int c, int d, int e, int f,
                  unsigned int g, unsigned int h, const void *i)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)i;
}
void glTexParameteri(unsigned int a, unsigned int b, int c)
{
    (void)a;
    (void)b;
    (void)c;
}
void glTexSubImage2D(unsigned int a, int b, int c, int d, int e, int f,
                     unsigned int g, unsigned int h, const void *i)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)i;
}
void glUniform1f(int a, float b)
{
    (void)a;
    (void)b;
}
void glUniform1i(int a, int b)
{
    (void)a;
    (void)b;
}
void glUniform2f(int a, float b, float c)
{
    (void)a;
    (void)b;
    (void)c;
}
void glUniform4f(int a, float b, float c, float d, float e)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
}
void glUseProgram(unsigned int a) { (void)a; }
void glVertexAttribPointer(unsigned int a, int b, unsigned int c, unsigned char d,
                           int e, const void *f)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
}
void glViewport(int a, int b, int c, int d)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}
