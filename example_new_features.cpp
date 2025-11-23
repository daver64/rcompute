#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <iostream>
#include <iomanip>

void demo_uniform_helpers() {
    std::cout << "=== Uniform Helpers Demo ===\n";
    
    rcompute c;
    rcompute_init(&c, 4, 3);
    
    const char *shader = R"(
#version 430
layout(local_size_x = 1) in;
layout(std430, binding = 0) buffer Buf { float data[]; };

uniform int ivalue;
uniform float fvalue;
uniform vec3 vvalue;

void main() {
    data[0] = float(ivalue) + fvalue + vvalue.x + vvalue.y + vvalue.z;
}
)";
    
    c.program = rcompute_compile(shader);
    
    // Use uniform helpers
    rcompute_set_uniform_int(&c, "ivalue", 10);
    rcompute_set_uniform_float(&c, "fvalue", 5.5f);
    rcompute_set_uniform_vec3(&c, "vvalue", 1.0f, 2.0f, 3.0f);
    
    float result = 0.0f;
    GLuint buf = rcompute_buffer(sizeof(float), &result);
    rcompute_buffer_bind(buf, 0);
    
    rcompute_dispatch_1d(&c, 1);
    rcompute_read(buf, &result, sizeof(float));
    
    std::cout << "Result: " << result << " (expected: 21.5)\n\n";
    
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&c);
}

void demo_zero_buffer() {
    std::cout << "=== Zero-Initialized Buffer Demo ===\n";
    
    rcompute c;
    rcompute_init(&c, 4, 3);
    
    const char *shader = R"(
#version 430
layout(local_size_x = 256) in;
layout(std430, binding = 0) buffer Buf { int data[]; };

void main() {
    uint gid = gl_GlobalInvocationID.x;
    data[gid] = int(gid);
}
)";
    
    c.program = rcompute_compile(shader);
    
    // Create zero-initialized buffer
    GLuint buf = rcompute_buffer_zero(256 * sizeof(int));
    GLsizeiptr size = rcompute_buffer_size(buf);
    std::cout << "Buffer size: " << size << " bytes\n";
    
    rcompute_buffer_bind(buf, 0);
    rcompute_dispatch_1d(&c, 1);
    
    int results[10];
    rcompute_read(buf, results, sizeof(results));
    
    std::cout << "First 10 values: ";
    for (int i = 0; i < 10; i++) 
        std::cout << results[i] << " ";
    std::cout << "\n\n";
    
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&c);
}

void demo_shader_defines() {
    std::cout << "=== Shader Defines Demo ===\n";
    
    rcompute c;
    rcompute_init(&c, 4, 3);
    
    const char *shader = R"(
#version 430
layout(local_size_x = 1) in;
layout(std430, binding = 0) buffer Buf { int data; };

void main() {
#ifdef MULTIPLY
    data *= FACTOR;
#else
    data += FACTOR;
#endif
}
)";
    
    const char *defines[] = {"MULTIPLY", "FACTOR 10"};
    c.program = rcompute_compile_with_defines(shader, defines, 2);
    
    int value = 5;
    GLuint buf = rcompute_buffer(sizeof(int), &value);
    rcompute_buffer_bind(buf, 0);
    
    rcompute_dispatch_1d(&c, 1);
    rcompute_read(buf, &value, sizeof(int));
    
    std::cout << "5 * 10 = " << value << "\n\n";
    
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&c);
}

void demo_timing() {
    std::cout << "=== GPU Timing Demo ===\n";
    
    rcompute c;
    rcompute_init(&c, 4, 3);
    
    const char *shader = R"(
#version 430
layout(local_size_x = 256) in;
layout(std430, binding = 0) buffer Buf { float data[]; };

void main() {
    uint gid = gl_GlobalInvocationID.x;
    float sum = 0.0;
    for (int i = 0; i < 1000; i++) {
        sum += sin(float(gid + i));
    }
    data[gid] = sum;
}
)";
    
    c.program = rcompute_compile(shader);
    
    GLuint buf = rcompute_buffer_zero(1024 * 1024 * sizeof(float));
    rcompute_buffer_bind(buf, 0);
    
    rcompute_timer_begin();
    rcompute_dispatch_1d(&c, 1024 * 1024 / 256);
    double elapsed_ms = rcompute_timer_end();
    
    std::cout << "Computed 1M values in " << std::fixed << std::setprecision(3) 
              << elapsed_ms << " ms\n\n";
    
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&c);
}

void demo_limits() {
    std::cout << "=== Compute Limits Query Demo ===\n";
    
    rcompute c;
    rcompute_init(&c, 4, 3);
    
    int max_count_x, max_count_y, max_count_z;
    int max_size_x, max_size_y, max_size_z;
    int max_invocations;
    
    rcompute_get_limits(&c, &max_count_x, &max_count_y, &max_count_z,
                        &max_size_x, &max_size_y, &max_size_z, &max_invocations);
    
    std::cout << "Max work group count: (" << max_count_x << ", " << max_count_y 
              << ", " << max_count_z << ")\n";
    std::cout << "Max work group size: (" << max_size_x << ", " << max_size_y 
              << ", " << max_size_z << ")\n";
    std::cout << "Max invocations per work group: " << max_invocations << "\n\n";
    
    rcompute_destroy(&c);
}

void demo_barriers() {
    std::cout << "=== Custom Barriers Demo ===\n";
    
    rcompute c;
    rcompute_init(&c, 4, 3);
    
    const char *shader = R"(
#version 430
layout(local_size_x = 256) in;
layout(std430, binding = 0) buffer Buf { int data[]; };

void main() {
    uint gid = gl_GlobalInvocationID.x;
    data[gid] = int(gid * 2);
}
)";
    
    c.program = rcompute_compile(shader);
    
    GLuint buf = rcompute_buffer_zero(256 * sizeof(int));
    rcompute_buffer_bind(buf, 0);
    
    rcompute_dispatch_1d(&c, 1);
    
    // Use custom barrier instead of automatic one
    rcompute_barrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    int result[5];
    rcompute_read(buf, result, sizeof(result));
    
    std::cout << "First 5 values (doubled): ";
    for (int i = 0; i < 5; i++)
        std::cout << result[i] << " ";
    std::cout << "\n\n";
    
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&c);
}

int main() {
    std::cout << "\n=== RCompute New Features Demo ===\n\n";
    
    demo_uniform_helpers();
    demo_zero_buffer();
    demo_shader_defines();
    demo_timing();
    demo_limits();
    demo_barriers();
    
    std::cout << "=== All demos completed ===\n";
    return 0;
}
