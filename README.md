# rcompute

A lightweight, STB-style single-header library for OpenGL compute shaders. Write GPU-accelerated compute programs with minimal boilerplate—just drop in the header and start dispatching work to the GPU.

## Features

- 🚀 Simple API - just a few functions to get started
- 📦 Single-header library - drop in and use
- 🔧 Zero dependencies (besides OpenGL, GLEW, GLFW)
- 💾 Flexible buffer management with usage hints
- 📝 Load shaders from files or strings
- ✅ Proper resource cleanup and error handling
- 🛡️ Comprehensive null pointer safety
- 🎨 2D and 3D texture support with multiple formats
- ⚡ Async buffer operations with fence sync
- 🗺️ Buffer mapping for direct GPU memory access
- 🔧 Debug mode with verbose logging
- 🔄 Shader hot-reload capability
- ⏱️ GPU timing and profiling
- 🛡️ Buffer bounds checking

## Quick Start

### Installation

1. Copy `include/rcompute.h` to your project
2. Define `RCOMPUTE_IMPLEMENTATION` in **one** C/C++ file before including:

```cpp
#define RCOMPUTE_IMPLEMENTATION
#include "rcompute.h"
```

3. Link with: `-lGLEW -lGL -lglfw`

## Examples

The project includes extensive examples demonstrating different use cases:

### Getting Started

| Example | Description | Source |
|---------|-------------|--------|
| **example** | Basic usage with file loading | [`example.cpp`](example.cpp) |
| **example_advanced** | Buffer management and updates | [`example_advanced.cpp`](example_advanced.cpp) |
| **example_new_features** | Uniforms, timing, limits, barriers, shader defines | [`example_new_features.cpp`](example_new_features.cpp) |
| **example_advanced_features** | Buffer mapping, async reads, shader hot-reload, debug mode | [`example_advanced_features.cpp`](example_advanced_features.cpp) |

### Image Processing

| Example | Description | Source | Shader |
|---------|-------------|--------|--------|
| **example_texture** | 2D texture processing with edge detection filter | [`example_texture.cpp`](example_texture.cpp) | [`example_texture.comp`](example_texture.comp) |
| **example_blur** | Separable Gaussian blur filter (two-pass) | [`example_blur.cpp`](example_blur.cpp) | [`example_blur.comp`](example_blur.comp) |
| **example_histogram** | Image histogram using atomic operations | [`example_histogram.cpp`](example_histogram.cpp) | [`example_histogram.comp`](example_histogram.comp) |

### Mathematical Visualization

| Example | Description | Source | Shader |
|---------|-------------|--------|--------|
| **example_mandelbrot** | Mandelbrot fractal generation with zoom levels | [`example_mandelbrot.cpp`](example_mandelbrot.cpp) | [`example_mandelbrot.comp`](example_mandelbrot.comp) |
| **example_raytracer** | Simple raytracer with animated spheres and plane | [`example_raytracer.cpp`](example_raytracer.cpp) | [`example_raytracer.comp`](example_raytracer.comp) |

### Physics & Simulation

| Example | Description | Source | Shader |
|---------|-------------|--------|--------|
| **example_nbody** | N-body gravitational particle simulation | [`example_nbody.cpp`](example_nbody.cpp) | [`example_nbody.comp`](example_nbody.comp) |
| **example_comprehensive** | Algorithms: smoothing, reduction, matrix multiplication | [`example_comprehensive.cpp`](example_comprehensive.cpp) | Multiple shaders |

### Computational Algorithms

| Example | Description | Source | Shader |
|---------|-------------|--------|--------|
| **example_scan** | Parallel prefix sum using shared memory | [`example_scan.cpp`](example_scan.cpp) | [`example_scan.comp`](example_scan.comp) |
| **example_monte_carlo** | Monte Carlo π estimation with 65M samples | [`example_monte_carlo.cpp`](example_monte_carlo.cpp) | [`example_monte_carlo.comp`](example_monte_carlo.comp) |

**Compile any example:**
```bash
g++ -o example_name example_name.cpp -lGLEW -lGL -lglfw
./example_name
```

### Basic Example

```cpp
#define RCOMPUTE_IMPLEMENTATION
#include "rcompute.h"

int main() {
    // Initialize compute context (OpenGL 4.3+)
    rcompute c;
    if (!rcompute_init(&c, 4, 3)) {
        fprintf(stderr, "Failed to initialize\n");
        return 1;
    }

    // Load and compile shader from file
    c.program = rcompute_compile_file("shader.comp");
    
    // Create buffer
    int data = 0;
    GLuint buf = rcompute_buffer(sizeof(int), &data);
    rcompute_buffer_bind(buf, 0);
    
    // Run compute shader
    rcompute_run(&c, 1, 1, 1);
    
    // Read results
    int result;
    rcompute_read(buf, &result, sizeof(result));
    
    // Cleanup
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&c);
    
    return 0;
}
```

### Example Compute Shader (`shader.comp`)

```glsl
#version 430
layout(local_size_x = 1) in;
layout(std430, binding = 0) buffer Buf { int data[]; };

void main() {
    data[0] = 1337;
}
```

## API Reference

### Initialization

```cpp
int rcompute_init(rcompute *c, int gl_major, int gl_minor);
```
Creates an OpenGL context with the specified version. Returns 1 on success, 0 on failure.

```cpp
void rcompute_destroy(rcompute *c);
```
Cleans up all resources including the GL program and window.

### Shader Compilation

```cpp
GLuint rcompute_compile(const char *src);
```
Compiles a compute shader from a string. Returns program handle or 0 on error.

```cpp
GLuint rcompute_compile_file(const char *filepath);
```
Loads and compiles a compute shader from a file (`.comp` or `.glsl`). Returns program handle or 0 on error.

```cpp
GLuint rcompute_compile_with_defines(const char *src, const char **defines, int count);
```
Compiles a shader with preprocessor defines. Inserts `#define` statements after the `#version` line.

```cpp
void rcompute_set_program(rcompute *c, GLuint program);
```
Sets the active program for the compute context.

### Uniform Helpers

```cpp
void rcompute_set_uniform_int(rcompute *c, const char *name, int value);
void rcompute_set_uniform_uint(rcompute *c, const char *name, unsigned int value);
void rcompute_set_uniform_float(rcompute *c, const char *name, float value);
void rcompute_set_uniform_vec2(rcompute *c, const char *name, float x, float y);
void rcompute_set_uniform_vec3(rcompute *c, const char *name, float x, float y, float z);
void rcompute_set_uniform_vec4(rcompute *c, const char *name, float x, float y, float z, float w);
void rcompute_set_uniform_mat4(rcompute *c, const char *name, const float *matrix);
```
Convenience functions to set shader uniforms without manual `glGetUniformLocation` calls.

### Buffer Management

```cpp
GLuint rcompute_buffer(GLsizeiptr size, const void *data);
```
Creates a shader storage buffer with `DYNAMIC` usage. Pass `NULL` for uninitialized data.

```cpp
GLuint rcompute_buffer_ex(GLsizeiptr size, const void *data, rcompute_usage usage);
```
Creates a buffer with specific usage hint:
- `RCOMPUTE_STATIC` - Data rarely changes
- `RCOMPUTE_DYNAMIC` - Data changes frequently (default)
- `RCOMPUTE_STREAM` - Data changes every frame

```cpp
GLuint rcompute_buffer_zero(GLsizeiptr size);
```
Creates a zero-initialized buffer (equivalent to passing NULL as data).

```cpp
void rcompute_buffer_bind(GLuint buf, GLuint binding);
```
Binds a buffer to a shader storage binding point.

```cpp
void rcompute_buffer_write(GLuint buf, GLsizeiptr offset, GLsizeiptr size, const void *data);
```
Updates existing buffer data at the specified offset.

```cpp
GLsizeiptr rcompute_buffer_size(GLuint buf);
```
Queries the size of an allocated buffer.

```cpp
void *rcompute_buffer_map(GLuint buf, GLenum access);
void rcompute_buffer_unmap(GLuint buf);
```
Maps a buffer for direct CPU access. Use `GL_READ_ONLY`, `GL_WRITE_ONLY`, or `GL_READ_WRITE` for the access parameter. Always call `unmap` when done.

```cpp
void rcompute_buffer_destroy(GLuint buf);
```
Destroys a buffer and frees GPU memory.

### Texture Support

```cpp
GLuint rcompute_texture_2d(int width, int height, GLenum format, const void *data);
```
Creates a 2D texture for compute shader access. Supports formats:
- Float: `GL_R32F`, `GL_RG32F`, `GL_RGBA32F`
- Integer: `GL_R32I`, `GL_RG32I`, `GL_RGBA32I`
- Unsigned: `GL_R32UI`, `GL_RG32UI`, `GL_RGBA32UI`

```cpp
GLuint rcompute_texture_3d(int width, int height, int depth, GLenum format, const void *data);
```
Creates a 3D texture for compute shader access. Supports the same formats as 2D textures.

```cpp
void rcompute_texture_bind(GLuint tex, GLuint unit, GLenum format);
```
Binds texture to an image unit for compute shader read/write access. The format must match the texture's internal format.

```cpp
void rcompute_texture_destroy(GLuint tex);
```
Destroys a texture.

### Execution

```cpp
void rcompute_run(rcompute *c, int nx, int ny, int nz);
```
Dispatches compute shader with work groups (nx, ny, nz).

```cpp
void rcompute_dispatch_1d(rcompute *c, int nx);
```
Convenience function for 1D dispatch (equivalent to `rcompute_run(c, nx, 1, 1)`).

```cpp
void rcompute_dispatch_2d(rcompute *c, int nx, int ny);
```
Convenience function for 2D dispatch (equivalent to `rcompute_run(c, nx, ny, 1)`).

### Memory Barriers

```cpp
void rcompute_barrier(GLenum barriers);
```
Executes a custom memory barrier. Use GL barrier bit flags like `GL_SHADER_STORAGE_BARRIER_BIT`.

```cpp
void rcompute_barrier_all(void);
```
Executes all memory barriers (`GL_ALL_BARRIER_BITS`).

### Performance Profiling

```cpp
void rcompute_timer_begin(void);
double rcompute_timer_end(void);
```
GPU timing queries. `timer_end()` returns elapsed time in milliseconds.

### Capability Queries

```cpp
void rcompute_get_limits(rcompute *c, int *max_work_group_count_x,
                         int *max_work_group_count_y, int *max_work_group_count_z,
                         int *max_work_group_size_x, int *max_work_group_size_y,
                         int *max_work_group_size_z, int *max_invocations);
```
Queries OpenGL compute shader limits and capabilities. Pass NULL for parameters you don't need.

### Data Transfer

```cpp
void rcompute_read(GLuint buf, void *out, GLsizeiptr size);
```
Reads data from buffer to CPU memory (blocking).

```cpp
void rcompute_read_async(GLuint buf, void *data, size_t size, size_t offset);
void rcompute_wait_async(void);
```
Async buffer read operations. `read_async` initiates a non-blocking read with a fence sync object. Call `wait_async` to ensure the operation completes before using the data.

### Shader Hot-Reload

```cpp
int rcompute_reload_shader(rcompute *c, const char *filepath);
```
Reloads a shader from file, replacing the current program. Returns 1 on success, 0 on failure. Useful for iterative development.

### Debug & Diagnostics

```cpp
void rcompute_set_debug(int enable);
```
Enables or disables debug logging. When enabled, the library prints detailed information about operations.

```cpp
int rcompute_check_version(int required_major, int required_minor);
```
Checks if the current OpenGL version meets the minimum requirement. Returns 1 if supported, 0 otherwise.

### Error Handling

```cpp
const char *rcompute_get_last_error(void);
```
Returns the last error message or `NULL` if no error occurred.

## Advanced Example

```cpp
#define RCOMPUTE_IMPLEMENTATION
#include "rcompute.h"
#include <stdio.h>

int main() {
    rcompute c;
    if (!rcompute_init(&c, 4, 3)) {
        fprintf(stderr, "Init failed: %s\n", rcompute_get_last_error());
        return 1;
    }

    // Load shader
    GLuint program = rcompute_compile_file("shader.comp");
    if (!program) {
        fprintf(stderr, "Compile failed: %s\n", rcompute_get_last_error());
        return 1;
    }
    rcompute_set_program(&c, program);

    // Create buffer with STATIC usage (data won't change)
    float data[1024] = {0};
    GLuint buf = rcompute_buffer_ex(sizeof(data), data, RCOMPUTE_STATIC);
    rcompute_buffer_bind(buf, 0);

    // Run shader
    rcompute_dispatch_1d(&c, 1024);

    // Read results
    float results[1024];
    rcompute_read(buf, results, sizeof(results));

    // Update buffer (if using DYNAMIC)
    float new_data[1024] = {1.0f};
    rcompute_buffer_write(buf, 0, sizeof(new_data), new_data);

    // Cleanup
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&c);

    return 0;
}
```

## Building

### Linux
```bash
g++ -o example example.cpp -lGLEW -lGL -lglfw
```

### macOS
```bash
clang++ -o example example.cpp -lGLEW -framework OpenGL -lglfw
```

### Windows (MinGW)
```bash
g++ -o example.exe example.cpp -lglew32 -lopengl32 -lglfw3
```

## Requirements

- OpenGL 4.3+ (for compute shaders)
- GLEW (OpenGL Extension Wrangler)
- GLFW 3.x (for context creation)
- C99 or C++11 compiler

## Thread Safety

The library is **not thread-safe**. Each thread should create its own `rcompute` context.

## Multiple Contexts

You can create multiple `rcompute` contexts sequentially. GLFW is initialized once on the first call to `rcompute_init()`. When using multiple contexts in one program, you don't need to manually terminate GLFW between them.

```cpp
rcompute c1, c2;
rcompute_init(&c1, 4, 3);
// Use c1...
rcompute_destroy(&c1);

// Create another context - works fine
rcompute_init(&c2, 4, 3);
// Use c2...
rcompute_destroy(&c2);
```

## License

MIT License - see [LICENSE](LICENSE) file for details.

Copyright (c) 2025 David Rowbotham

### Example Shaders

- **`shader.comp`** - Simple value write
- **`smoothing.comp`** - 1D neighbor averaging filter
- **`reduction.comp`** - Parallel sum reduction with shared memory
- **`matmul.comp`** - Matrix multiplication
- **`brightness.comp`** - 2D image brightness adjustment
- **`particles.comp`** - Physics simulation with multiple buffers

## Common Issues

### "GL init failed"
- Ensure your GPU supports OpenGL 4.3+
- Check that GLEW and GLFW are properly installed
- Try updating graphics drivers

### "Shader compilation failed"
- Check shader syntax with `rcompute_get_last_error()`
- Verify compute shader version (`#version 430` or higher)
- Ensure local work group size is valid

### Memory leaks
- Always call `rcompute_destroy()` when done with a context
- Call `rcompute_buffer_destroy()` for each created buffer
- The library handles internal cleanup automatically
- GLFW is not automatically terminated; call `glfwTerminate()` at program exit if needed

### Using multiple contexts
- Multiple `rcompute_init()` calls in the same program now work correctly
- No need to manually manage GLFW initialization between contexts
- Each context maintains its own window and program state
