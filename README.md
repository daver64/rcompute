# rcompute

A minimal, STB-style single-header library for OpenGL compute shaders.

## Features

- 🚀 Simple API - just a few functions to get started
- 📦 Single-header library - drop in and use
- 🔧 Zero dependencies (besides OpenGL, GLEW, GLFW)
- 💾 Flexible buffer management with usage hints
- 📝 Load shaders from files or strings
- ✅ Proper resource cleanup and error handling
- 🛡️ Comprehensive null pointer safety

## Quick Start

### Installation

1. Copy `include/rcompute.h` to your project
2. Define `RCOMPUTE_IMPLEMENTATION` in **one** C/C++ file before including:

```cpp
#define RCOMPUTE_IMPLEMENTATION
#include "rcompute.h"
```

3. Link with: `-lGLEW -lGL -lglfw`

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
void rcompute_set_program(rcompute *c, GLuint program);
```
Sets the active program for the compute context.

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
void rcompute_buffer_bind(GLuint buf, GLuint binding);
```
Binds a buffer to a shader storage binding point.

```cpp
void rcompute_buffer_write(GLuint buf, GLsizeiptr offset, GLsizeiptr size, const void *data);
```
Updates existing buffer data at the specified offset.

```cpp
void rcompute_buffer_destroy(GLuint buf);
```
Destroys a buffer and frees GPU memory.

### Execution

```cpp
void rcompute_run(rcompute *c, int nx, int ny, int nz);
```
Dispatches compute shader with work groups (nx, ny, nz).

```cpp
void rcompute_dispatch_1d(rcompute *c, int nx);
```
Convenience function for 1D dispatch (equivalent to `rcompute_run(c, nx, 1, 1)`).

### Data Transfer

```cpp
void rcompute_read(GLuint buf, void *out, GLsizeiptr size);
```
Reads data from buffer to CPU memory.

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

Public domain / Unlicense

## Examples

The project includes several examples demonstrating different use cases:

- **`example.cpp`** - Basic usage with file loading
- **`example_advanced.cpp`** - Buffer management and updates
- **`example_comprehensive.cpp`** - Advanced algorithms:
  - 1D smoothing filter (multi-buffer operations)
  - Parallel reduction with shared memory
  - Matrix multiplication with 2D work groups

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
