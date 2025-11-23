// rcompute.h - Minimal STB-style compute shader wrapper
// MIT License - see LICENSE file
#ifndef RCOMPUTE_H
#define RCOMPUTE_H

// User must link with: -lGLEW -lGL -lglfw
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // --------------------------------------------------------------
    // API
    // --------------------------------------------------------------

    // Buffer usage flags
    typedef enum
    {
        RCOMPUTE_STATIC = 0,  // GL_STATIC_DRAW
        RCOMPUTE_DYNAMIC = 1, // GL_DYNAMIC_COPY
        RCOMPUTE_STREAM = 2   // GL_STREAM_COPY
    } rcompute_usage;

    typedef struct
    {
        GLFWwindow *window;
        GLuint program;
    } rcompute;

    // create OpenGL context + window (hidden)
    int rcompute_init(rcompute *c, int gl_major, int gl_minor);

    // compile a compute shader from a string
    GLuint rcompute_compile(const char *src);

    // compile a compute shader from a file (.comp or .glsl)
    GLuint rcompute_compile_file(const char *filepath);

    // create SSBO of N bytes
    GLuint rcompute_buffer(GLsizeiptr size, const void *data);

    // bind buffer to shader storage binding point
    void rcompute_buffer_bind(GLuint buf, GLuint binding);

    // destroy a buffer
    void rcompute_buffer_destroy(GLuint buf);

    // run the compute shader: dispatch nx,ny,nz
    void rcompute_run(rcompute *c, int nx, int ny, int nz);

    // convenience: dispatch 1D compute (nx, 1, 1)
    void rcompute_dispatch_1d(rcompute *c, int nx);

    // read back from SSBO
    void rcompute_read(GLuint buf, void *out, GLsizeiptr size);

    // free resources
    void rcompute_destroy(rcompute *c);

    // get last error message (returns NULL if no error)
    const char *rcompute_get_last_error(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// Implementation
// ============================================================================
#ifdef RCOMPUTE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global error state
static char rcompute__last_error[512] = {0};
static int rcompute__glfw_initialized = 0;

// error printing and tracking
static void rcompute__err(const char *txt)
{
    snprintf(rcompute__last_error, sizeof(rcompute__last_error), "%s", txt);
    fprintf(stderr, "rcompute error: %s\n", txt);
}

static void rcompute__err_ex(const char *fmt, const char *arg)
{
    snprintf(rcompute__last_error, sizeof(rcompute__last_error), fmt, arg);
    fprintf(stderr, "rcompute error: ");
    fprintf(stderr, fmt, arg);
    fprintf(stderr, "\n");
}

// ---------------------------------
// create invisible 1×1 GLFW window
// ---------------------------------
int rcompute_init(rcompute *c, int gl_major, int gl_minor)
{
    if (!c)
        return 0;

    if (!rcompute__glfw_initialized)
    {
        if (!glfwInit())
            return 0;
        rcompute__glfw_initialized = 1;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl_minor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    c->window = glfwCreateWindow(1, 1, "", NULL, NULL);
    if (!c->window)
    {
        glfwTerminate();
        return 0;
    }

    glfwMakeContextCurrent(c->window);
    if (glewInit() != GLEW_OK)
        return 0;

    return 1;
}

// ---------------------------------
// compile compute shader
// ---------------------------------
GLuint rcompute_compile(const char *src)
{
    if (!src)
    {
        rcompute__err("Shader source is NULL");
        return 0;
    }

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &src, 0);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        rcompute__err(log);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader);
    glLinkProgram(prog);
    glDeleteShader(shader);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        rcompute__err(log);
        return 0;
    }

    return prog;
}

// ---------------------------------
// compile compute shader from file
// ---------------------------------
GLuint rcompute_compile_file(const char *filepath)
{
    if (!filepath)
    {
        rcompute__err("Filepath is NULL");
        return 0;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f)
    {
        rcompute__err_ex("Failed to open shader file: %s", filepath);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *src = (char *)malloc(len + 1);
    if (!src)
    {
        fclose(f);
        rcompute__err("Failed to allocate memory for shader");
        return 0;
    }

    size_t bytes_read = fread(src, 1, len, f);
    if (bytes_read != (size_t)len)
    {
        free(src);
        fclose(f);
        rcompute__err("Failed to read complete shader file");
        return 0;
    }
    src[len] = '\0';
    fclose(f);

    GLuint prog = rcompute_compile(src);
    free(src);

    return prog;
}

// ---------------------------------
GLuint rcompute_buffer_ex(GLsizeiptr size, const void *data, rcompute_usage usage)
{
    if (size <= 0)
    {
        rcompute__err("Buffer size must be positive");
        return 0;
    }

    GLenum gl_usage;
    switch (usage)
    {
    case RCOMPUTE_STATIC:
        gl_usage = GL_STATIC_DRAW;
        break;
    case RCOMPUTE_STREAM:
        gl_usage = GL_STREAM_COPY;
        break;
    case RCOMPUTE_DYNAMIC:
    default:
        gl_usage = GL_DYNAMIC_COPY;
        break;
    }

    GLuint buf;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, gl_usage);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return buf;
}

// ---------------------------------
GLuint rcompute_buffer(GLsizeiptr size, const void *data)
{
    return rcompute_buffer_ex(size, data, RCOMPUTE_DYNAMIC);
}

// ---------------------------------
void rcompute_buffer_write(GLuint buf, GLsizeiptr offset, GLsizeiptr size, const void *data)
{
    if (buf == 0 || !data || size <= 0)
    {
        rcompute__err("Invalid buffer write parameters");
        return;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// ---------------------------------
void rcompute_buffer_bind(GLuint buf, GLuint binding)
{
    if (buf == 0)
    {
        rcompute__err("Invalid buffer handle");
        return;
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buf);
}

// ---------------------------------
void rcompute_buffer_destroy(GLuint buf)
{
    if (buf != 0)
        glDeleteBuffers(1, &buf);
}

// ---------------------------------
void rcompute_set_program(rcompute *c, GLuint program)
{
    if (!c)
    {
        rcompute__err("Invalid compute context");
        return;
    }
    c->program = program;
}

// ---------------------------------
void rcompute_run(rcompute *c, int nx, int ny, int nz)
{
    if (!c || c->program == 0)
    {
        rcompute__err("Invalid compute context or program");
        return;
    }

    glUseProgram(c->program);
    glDispatchCompute(nx, ny, nz);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

// ---------------------------------
void rcompute_dispatch_1d(rcompute *c, int nx)
{
    rcompute_run(c, nx, 1, 1);
}

// ---------------------------------
void rcompute_read(GLuint buf, void *out, GLsizeiptr size)
{
    if (buf == 0 || !out || size <= 0)
    {
        rcompute__err("Invalid buffer read parameters");
        return;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    void *ptr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    if (!ptr)
    {
        rcompute__err("Failed to map buffer");
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return;
    }
    memcpy(out, ptr, size);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// ---------------------------------
void rcompute_destroy(rcompute *c)
{
    if (!c)
        return;

    if (c->program != 0)
        glDeleteProgram(c->program);

    if (c->window)
        glfwDestroyWindow(c->window);

    // Don't terminate GLFW here - allow multiple contexts
    // User should call glfwTerminate() manually if needed
}

// ---------------------------------
const char *rcompute_get_last_error(void)
{
    return rcompute__last_error[0] ? rcompute__last_error : NULL;
}

#endif // RCOMPUTE_IMPLEMENTATION
#endif // RCOMPUTE_H
