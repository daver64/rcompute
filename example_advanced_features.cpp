// Example demonstrating advanced rcompute features:
// - Buffer mapping
// - Async buffer reads
// - Shader hot-reload
// - Debug mode
// - Version checking

#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <stdio.h>
#include <string.h>

const char *shader_v1 = R"(
#version 430
layout(local_size_x = 256) in;
layout(std430, binding = 0) buffer Data {
    float values[];
};
uniform float multiplier;
void main() {
    uint idx = gl_GlobalInvocationID.x;
    values[idx] *= multiplier;
}
)";

const char *shader_v2 = R"(
#version 430
layout(local_size_x = 256) in;
layout(std430, binding = 0) buffer Data {
    float values[];
};
uniform float multiplier;
void main() {
    uint idx = gl_GlobalInvocationID.x;
    values[idx] = values[idx] * multiplier + 1.0; // Modified computation
}
)";

int main()
{
    printf("=== RCompute Advanced Features Demo ===\n\n");

    // Enable debug mode
    rcompute_set_debug(1);

    // Initialize with OpenGL 4.3
    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3))
    {
        fprintf(stderr, "Failed to initialize rcompute\n");
        return 1;
    }

    // Check OpenGL version
    if (rcompute_check_version(4, 3))
    {
        printf("OpenGL 4.3+ is supported\n");
    }

    // Compile initial shader
    printf("\n--- Compiling initial shader ---\n");
    ctx.program = rcompute_compile(shader_v1);
    if (!ctx.program)
    {
        fprintf(stderr, "Shader compilation failed\n");
        return 1;
    }

    // Create buffer and initialize with data
    const int N = 1024;
    float *data = new float[N];
    for (int i = 0; i < N; i++)
        data[i] = (float)i;

    GLuint buffer = rcompute_buffer(N * sizeof(float), data);
    rcompute_buffer_bind(buffer, 0);

    // Test 1: Buffer mapping
    printf("\n--- Test 1: Buffer Mapping ---\n");
    rcompute_set_uniform_float(&ctx, "multiplier", 2.0f);
    rcompute_dispatch_1d(&ctx, N);
    rcompute_barrier_all();

    float *mapped = (float *)rcompute_buffer_map(buffer, GL_READ_ONLY);
    if (mapped)
    {
        printf("First 5 values via mapping: %.0f, %.0f, %.0f, %.0f, %.0f\n",
               mapped[0], mapped[1], mapped[2], mapped[3], mapped[4]);
        rcompute_buffer_unmap(buffer);
    }

    // Test 2: Async read
    printf("\n--- Test 2: Async Buffer Read ---\n");
    rcompute_set_uniform_float(&ctx, "multiplier", 0.5f);
    rcompute_dispatch_1d(&ctx, N);
    
    float *async_data = new float[10];
    rcompute_read_async(buffer, async_data, 10 * sizeof(float), 0);
    
    printf("Doing some work while GPU computes...\n");
    
    rcompute_wait_async();
    printf("First 5 values via async read: %.0f, %.0f, %.0f, %.0f, %.0f\n",
           async_data[0], async_data[1], async_data[2], async_data[3], async_data[4]);

    // Test 3: Shader hot-reload
    printf("\n--- Test 3: Shader Hot-Reload ---\n");
    printf("Recompiling with modified shader...\n");
    
    GLuint old_program = ctx.program;
    ctx.program = rcompute_compile(shader_v2);
    if (ctx.program)
    {
        glDeleteProgram(old_program);
        printf("Shader reloaded successfully!\n");
        
        rcompute_set_uniform_float(&ctx, "multiplier", 2.0f);
        rcompute_dispatch_1d(&ctx, N);
        rcompute_barrier_all();
        
        rcompute_read_async(buffer, async_data, 10 * sizeof(float), 0);
        rcompute_wait_async();
        
        printf("First 5 values with new shader: %.1f, %.1f, %.1f, %.1f, %.1f\n",
               async_data[0], async_data[1], async_data[2], async_data[3], async_data[4]);
    }

    // Test 4: Buffer bounds checking
    printf("\n--- Test 4: Bounds Checking ---\n");
    printf("Attempting out-of-bounds write (should fail)...\n");
    float dummy = 999.0f;
    rcompute_buffer_write(buffer, N * sizeof(float), sizeof(float), &dummy);

    // Test 5: Query buffer size
    printf("\n--- Test 5: Buffer Size Query ---\n");
    GLsizeiptr size = rcompute_buffer_size(buffer);
    printf("Buffer size: %ld bytes (%d floats)\n", size, (int)(size / sizeof(float)));

    // Cleanup
    delete[] data;
    delete[] async_data;
    rcompute_buffer_destroy(buffer);
    rcompute_destroy(&ctx);

    printf("\n=== All tests completed successfully! ===\n");
    return 0;
}
