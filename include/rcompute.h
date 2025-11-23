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
        GLuint last_program; // Cache for optimization
    } rcompute;

    // create OpenGL context + window (hidden)
    int rcompute_init(rcompute *c, int gl_major, int gl_minor);

    // compile a compute shader from a string
    GLuint rcompute_compile(const char *src);

    // compile a compute shader from a file (.comp or .glsl)
    GLuint rcompute_compile_file(const char *filepath);

    // compile a compute shader with preprocessor defines
    GLuint rcompute_compile_with_defines(const char *src, const char **defines, int count);

    // reload shader from file (hot-reload support)
    int rcompute_reload_shader(rcompute *c, const char *filepath);

    // set active program for compute context
    void rcompute_set_program(rcompute *c, GLuint program);

    // Uniform helpers (must call after setting program)
    void rcompute_set_uniform_int(rcompute *c, const char *name, int value);
    void rcompute_set_uniform_uint(rcompute *c, const char *name, unsigned int value);
    void rcompute_set_uniform_float(rcompute *c, const char *name, float value);
    void rcompute_set_uniform_vec2(rcompute *c, const char *name, float x, float y);
    void rcompute_set_uniform_vec3(rcompute *c, const char *name, float x, float y, float z);
    void rcompute_set_uniform_vec4(rcompute *c, const char *name, float x, float y, float z, float w);
    void rcompute_set_uniform_mat4(rcompute *c, const char *name, const float *matrix);

    // create SSBO of N bytes
    GLuint rcompute_buffer(GLsizeiptr size, const void *data);

    // create SSBO with specific usage hint
    GLuint rcompute_buffer_ex(GLsizeiptr size, const void *data, rcompute_usage usage);

    // create zero-initialized SSBO
    GLuint rcompute_buffer_zero(GLsizeiptr size);

    // update existing buffer data
    void rcompute_buffer_write(GLuint buf, GLsizeiptr offset, GLsizeiptr size, const void *data);

    // bind buffer to shader storage binding point
    void rcompute_buffer_bind(GLuint buf, GLuint binding);

    // query buffer size
    GLsizeiptr rcompute_buffer_size(GLuint buf);

    // map buffer for direct access
    void *rcompute_buffer_map(GLuint buf, GLenum access);
    void rcompute_buffer_unmap(GLuint buf);

    // async buffer operations
    void rcompute_read_async(GLuint buf, void *data, size_t size, size_t offset);
    void rcompute_wait_async();

    // destroy a buffer
    void rcompute_buffer_destroy(GLuint buf);

    // Texture operations
    GLuint rcompute_texture_2d(int width, int height, GLenum format, const void *data);
    GLuint rcompute_texture_3d(int width, int height, int depth, GLenum format, const void *data);
    void rcompute_texture_bind(GLuint tex, GLuint unit, GLenum format);
    void rcompute_texture_destroy(GLuint tex);

    // run the compute shader: dispatch nx,ny,nz
    void rcompute_run(rcompute *c, int nx, int ny, int nz);

    // convenience: dispatch 1D compute (nx, 1, 1)
    void rcompute_dispatch_1d(rcompute *c, int nx);

    // convenience: dispatch 2D compute (nx, ny, 1)
    void rcompute_dispatch_2d(rcompute *c, int nx, int ny);

    // read back from SSBO
    void rcompute_read(GLuint buf, void *out, GLsizeiptr size);

    // buffer mapping for large transfers
    void *rcompute_buffer_map(GLuint buf, GLenum access);
    void rcompute_buffer_unmap(GLuint buf);

    // Memory barriers
    void rcompute_barrier(GLenum barriers);
    void rcompute_barrier_all(void);

    // GPU timing/profiling
    void rcompute_timer_begin(void);
    double rcompute_timer_end(void); // returns milliseconds
    void rcompute_timer_destroy(void);

    // Query compute limits
    void rcompute_get_limits(rcompute *c, int *max_work_group_count_x,
                             int *max_work_group_count_y, int *max_work_group_count_z,
                             int *max_work_group_size_x, int *max_work_group_size_y,
                             int *max_work_group_size_z, int *max_invocations);

    // free resources
    void rcompute_destroy(rcompute *c);

    // get last error message (returns NULL if no error)
    const char *rcompute_get_last_error(void);

    // check if OpenGL version is supported
    int rcompute_check_version(int required_major, int required_minor);

    // enable/disable debug logging
    void rcompute_set_debug(int enable);

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
#include <stdarg.h>

// Global error state
static char rcompute__last_error[512] = {0};
static int rcompute__glfw_initialized = 0;

// GPU timing state
static GLuint rcompute__query_id = 0;
static int rcompute__query_available = 0;

// Async read state
static GLsync rcompute__sync = NULL;

// Debug mode
static int rcompute__debug = 0;
static GLsync rcompute__async_sync = NULL;

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

static void rcompute__debug_log(const char *fmt, ...)
{
    if (!rcompute__debug) return;
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "[rcompute] ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

// ---------------------------------
// Debug mode
// ---------------------------------
void rcompute_set_debug(int enable)
{
    rcompute__debug = enable;
    rcompute__debug_log("Debug mode %s", enable ? "enabled" : "disabled");
}

// ---------------------------------
// Version check
// ---------------------------------
int rcompute_check_version(int required_major, int required_minor)
{
    const GLubyte *version = glGetString(GL_VERSION);
    if (!version) return 0;
    
    int major = 0, minor = 0;
    sscanf((const char *)version, "%d.%d", &major, &minor);
    
    if (major > required_major) return 1;
    if (major == required_major && minor >= required_minor) return 1;
    return 0;
}

// ---------------------------------
// create invisible 1×1 GLFW window
// ---------------------------------
int rcompute_init(rcompute *c, int gl_major, int gl_minor)
{
    if (!c)
        return 0;

    // Initialize to safe state
    c->window = NULL;
    c->program = 0;
    c->last_program = 0;

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

    rcompute__debug_log("Initialized OpenGL %d.%d context", gl_major, gl_minor);
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
// compile with preprocessor defines
// ---------------------------------
GLuint rcompute_compile_with_defines(const char *src, const char **defines, int count)
{
    if (!src)
    {
        rcompute__err("Shader source is NULL");
        return 0;
    }

    // Calculate total length needed
    size_t total_len = strlen(src) + 1;
    for (int i = 0; i < count; i++)
    {
        if (defines[i])
            total_len += strlen("#define ") + strlen(defines[i]) + 2; // +2 for newline
    }

    char *modified_src = (char *)malloc(total_len);
    if (!modified_src)
    {
        rcompute__err("Failed to allocate memory for shader");
        return 0;
    }

    // Find #version line and insert defines after it
    const char *version_start = strstr(src, "#version");
    const char *version_end = version_start ? strstr(version_start, "\n") : NULL;
    
    if (version_end)
    {
        size_t version_len = version_end - src + 1;
        memcpy(modified_src, src, version_len);
        char *insert_pos = modified_src + version_len;

        // Add defines
        for (int i = 0; i < count; i++)
        {
            if (defines[i])
            {
                int written = sprintf(insert_pos, "#define %s\n", defines[i]);
                insert_pos += written;
            }
        }

        // Add rest of source
        strcpy(insert_pos, version_end + 1);
    }
    else
    {
        // No version line, just prepend defines
        char *insert_pos = modified_src;
        for (int i = 0; i < count; i++)
        {
            if (defines[i])
            {
                int written = sprintf(insert_pos, "#define %s\n", defines[i]);
                insert_pos += written;
            }
        }
        strcpy(insert_pos, src);
    }

    GLuint prog = rcompute_compile(modified_src);
    free(modified_src);
    return prog;
}

// ---------------------------------
// set active program
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
// Uniform helpers
// ---------------------------------
void rcompute_set_uniform_int(rcompute *c, const char *name, int value)
{
    if (!c || !name) return;
    if (c->last_program != c->program) {
        glUseProgram(c->program);
        c->last_program = c->program;
    }
    GLint loc = glGetUniformLocation(c->program, name);
    if (loc != -1) glUniform1i(loc, value);
}

void rcompute_set_uniform_uint(rcompute *c, const char *name, unsigned int value)
{
    if (!c || !name) return;
    if (c->last_program != c->program) {
        glUseProgram(c->program);
        c->last_program = c->program;
    }
    GLint loc = glGetUniformLocation(c->program, name);
    if (loc != -1) glUniform1ui(loc, value);
}

void rcompute_set_uniform_float(rcompute *c, const char *name, float value)
{
    if (!c || !name) return;
    if (c->last_program != c->program) {
        glUseProgram(c->program);
        c->last_program = c->program;
    }
    GLint loc = glGetUniformLocation(c->program, name);
    if (loc != -1) glUniform1f(loc, value);
}

void rcompute_set_uniform_vec2(rcompute *c, const char *name, float x, float y)
{
    if (!c || !name) return;
    if (c->last_program != c->program) {
        glUseProgram(c->program);
        c->last_program = c->program;
    }
    GLint loc = glGetUniformLocation(c->program, name);
    if (loc != -1) glUniform2f(loc, x, y);
}

void rcompute_set_uniform_vec3(rcompute *c, const char *name, float x, float y, float z)
{
    if (!c || !name) return;
    if (c->last_program != c->program) {
        glUseProgram(c->program);
        c->last_program = c->program;
    }
    GLint loc = glGetUniformLocation(c->program, name);
    if (loc != -1) glUniform3f(loc, x, y, z);
}

void rcompute_set_uniform_vec4(rcompute *c, const char *name, float x, float y, float z, float w)
{
    if (!c || !name) return;
    if (c->last_program != c->program) {
        glUseProgram(c->program);
        c->last_program = c->program;
    }
    GLint loc = glGetUniformLocation(c->program, name);
    if (loc != -1) glUniform4f(loc, x, y, z, w);
}

void rcompute_set_uniform_mat4(rcompute *c, const char *name, const float *matrix)
{
    if (!c || !name || !matrix) return;
    if (c->last_program != c->program) {
        glUseProgram(c->program);
        c->last_program = c->program;
    }
    GLint loc = glGetUniformLocation(c->program, name);
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, matrix);
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
// Hot-reload shader from file
// ---------------------------------
int rcompute_reload_shader(rcompute *c, const char *filepath)
{
    if (!c || !filepath)
    {
        rcompute__err("Invalid parameters for reload");
        return 0;
    }

    GLuint old_program = c->program;
    GLuint new_program = rcompute_compile_file(filepath);
    
    if (!new_program)
    {
        rcompute__err("Failed to reload shader");
        return 0;
    }

    // Delete old program and use new one
    if (old_program != 0)
        glDeleteProgram(old_program);
    
    c->program = new_program;
    c->last_program = 0; // Reset cache
    
    rcompute__debug_log("Shader reloaded: %s", filepath);
    return 1;
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
GLuint rcompute_buffer_zero(GLsizeiptr size)
{
    return rcompute_buffer_ex(size, NULL, RCOMPUTE_DYNAMIC);
}

// ---------------------------------
void rcompute_buffer_write(GLuint buf, GLsizeiptr offset, GLsizeiptr size, const void *data)
{
    if (buf == 0 || !data || size <= 0)
    {
        rcompute__err("Invalid buffer write parameters");
        return;
    }

    // Bounds checking
    GLsizeiptr buf_size = rcompute_buffer_size(buf);
    if (offset + size > buf_size)
    {
        rcompute__err("Buffer write exceeds buffer bounds");
        return;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    rcompute__debug_log("Buffer write: %lld bytes at offset %lld", (long long)size, (long long)offset);
}

// ---------------------------------
// Async buffer operations
// ---------------------------------
void rcompute_read_async(GLuint buf, void *data, size_t size, size_t offset)
{
    if (buf == 0)
    {
        rcompute__err("Invalid buffer handle");
        return;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    
    // Create or reuse sync object
    if (rcompute__async_sync)
        glDeleteSync(rcompute__async_sync);
    
    rcompute__async_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    rcompute__debug_log("Async read initiated: %lld bytes at offset %lld", (long long)size, (long long)offset);
}

void rcompute_wait_async()
{
    if (!rcompute__async_sync)
    {
        rcompute__debug_log("No async operation to wait for");
        return;
    }

    GLenum result = glClientWaitSync(rcompute__async_sync, 0x00000001, 1000000000); // GL_SYNC_FLUSH_BIT, 1 second timeout
    if (result == GL_TIMEOUT_EXPIRED)
        rcompute__err("Async operation timeout");
    else if (result == GL_WAIT_FAILED)
        rcompute__err("Async wait failed");
    
    glDeleteSync(rcompute__async_sync);
    rcompute__async_sync = NULL;
    
    rcompute__debug_log("Async operation completed");
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
GLsizeiptr rcompute_buffer_size(GLuint buf)
{
    if (buf == 0)
    {
        rcompute__err("Invalid buffer handle");
        return 0;
    }

    GLint size = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &size);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return (GLsizeiptr)size;
}

// ---------------------------------
// Buffer mapping for large transfers
// ---------------------------------
void *rcompute_buffer_map(GLuint buf, GLenum access)
{
    if (buf == 0)
    {
        rcompute__err("Invalid buffer handle");
        return NULL;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    void *ptr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, access);
    if (!ptr)
    {
        rcompute__err("Failed to map buffer");
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return NULL;
    }
    
    rcompute__debug_log("Buffer mapped");
    return ptr;
}

void rcompute_buffer_unmap(GLuint buf)
{
    if (buf == 0)
    {
        rcompute__err("Invalid buffer handle");
        return;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    rcompute__debug_log("Buffer unmapped");
}

// ---------------------------------
// Texture operations
// ---------------------------------
GLuint rcompute_texture_2d(int width, int height, GLenum format, const void *data)
{
    if (width <= 0 || height <= 0)
    {
        rcompute__err("Invalid texture dimensions");
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Determine internal format and type
    GLenum internal_format = format;
    GLenum type = GL_UNSIGNED_BYTE;
    GLenum base_format = format;

    if (format == GL_R32F || format == GL_RG32F || format == GL_RGBA32F)
    {
        type = GL_FLOAT;
        if (format == GL_R32F) base_format = GL_RED;
        else if (format == GL_RG32F) base_format = GL_RG;
        else base_format = GL_RGBA;
    }
    else if (format == GL_R32I || format == GL_RG32I || format == GL_RGBA32I)
    {
        type = GL_INT;
        if (format == GL_R32I) base_format = GL_RED_INTEGER;
        else if (format == GL_RG32I) base_format = GL_RG_INTEGER;
        else base_format = GL_RGBA_INTEGER;
    }
    else if (format == GL_R32UI || format == GL_RG32UI || format == GL_RGBA32UI)
    {
        type = GL_UNSIGNED_INT;
        if (format == GL_R32UI) base_format = GL_RED_INTEGER;
        else if (format == GL_RG32UI) base_format = GL_RG_INTEGER;
        else base_format = GL_RGBA_INTEGER;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, base_format, type, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    rcompute__debug_log("2D texture created: %dx%d format=%d", width, height, format);
    return tex;
}

GLuint rcompute_texture_3d(int width, int height, int depth, GLenum format, const void *data)
{
    if (width <= 0 || height <= 0 || depth <= 0)
    {
        rcompute__err("Invalid texture dimensions");
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Determine internal format and type
    GLenum internal_format = format;
    GLenum type = GL_UNSIGNED_BYTE;
    GLenum base_format = format;

    if (format == GL_R32F || format == GL_RG32F || format == GL_RGBA32F)
    {
        type = GL_FLOAT;
        if (format == GL_R32F) base_format = GL_RED;
        else if (format == GL_RG32F) base_format = GL_RG;
        else base_format = GL_RGBA;
    }
    else if (format == GL_R32I || format == GL_RG32I || format == GL_RGBA32I)
    {
        type = GL_INT;
        if (format == GL_R32I) base_format = GL_RED_INTEGER;
        else if (format == GL_RG32I) base_format = GL_RG_INTEGER;
        else base_format = GL_RGBA_INTEGER;
    }
    else if (format == GL_R32UI || format == GL_RG32UI || format == GL_RGBA32UI)
    {
        type = GL_UNSIGNED_INT;
        if (format == GL_R32UI) base_format = GL_RED_INTEGER;
        else if (format == GL_RG32UI) base_format = GL_RG_INTEGER;
        else base_format = GL_RGBA_INTEGER;
    }

    glTexImage3D(GL_TEXTURE_3D, 0, internal_format, width, height, depth, 0, base_format, type, data);
    glBindTexture(GL_TEXTURE_3D, 0);

    rcompute__debug_log("3D texture created: %dx%dx%d format=%d", width, height, depth, format);
    return tex;
}

void rcompute_texture_bind(GLuint tex, GLuint unit, GLenum format)
{
    if (tex == 0)
    {
        rcompute__err("Invalid texture handle");
        return;
    }
    glBindImageTexture(unit, tex, 0, GL_FALSE, 0, GL_READ_WRITE, format);
    rcompute__debug_log("Texture bound to unit %u with format %d", unit, format);
}

void rcompute_texture_destroy(GLuint tex)
{
    if (tex != 0)
        glDeleteTextures(1, &tex);
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
void rcompute_dispatch_2d(rcompute *c, int nx, int ny)
{
    rcompute_run(c, nx, ny, 1);
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

// ---------------------------------
// Memory barriers
// ---------------------------------
void rcompute_barrier(GLenum barriers)
{
    glMemoryBarrier(barriers);
}

void rcompute_barrier_all(void)
{
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

// ---------------------------------
// GPU timing
// ---------------------------------
void rcompute_timer_begin(void)
{
    if (rcompute__query_id == 0)
    {
        glGenQueries(1, &rcompute__query_id);
    }
    glBeginQuery(GL_TIME_ELAPSED, rcompute__query_id);
    rcompute__query_available = 0;
}

double rcompute_timer_end(void)
{
    if (rcompute__query_id == 0)
    {
        rcompute__err("Timer not started");
        return 0.0;
    }

    glEndQuery(GL_TIME_ELAPSED);
    rcompute__query_available = 1;

    // Wait for query result
    GLuint64 elapsed_ns = 0;
    glGetQueryObjectui64v(rcompute__query_id, GL_QUERY_RESULT, &elapsed_ns);

    // Convert nanoseconds to milliseconds
    return (double)elapsed_ns / 1000000.0;
}

// ---------------------------------
// Query compute limits
// ---------------------------------
void rcompute_get_limits(rcompute *c, int *max_work_group_count_x,
                         int *max_work_group_count_y, int *max_work_group_count_z,
                         int *max_work_group_size_x, int *max_work_group_size_y,
                         int *max_work_group_size_z, int *max_invocations)
{
    if (!c)
    {
        rcompute__err("Invalid compute context");
        return;
    }

    if (max_work_group_count_x)
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, max_work_group_count_x);
    if (max_work_group_count_y)
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, max_work_group_count_y);
    if (max_work_group_count_z)
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, max_work_group_count_z);

    if (max_work_group_size_x)
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, max_work_group_size_x);
    if (max_work_group_size_y)
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, max_work_group_size_y);
    if (max_work_group_size_z)
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, max_work_group_size_z);

    if (max_invocations)
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, max_invocations);
}

#endif // RCOMPUTE_IMPLEMENTATION
#endif // RCOMPUTE_H
