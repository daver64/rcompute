#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

void demo_smoothing()
{
    std::cout << "\n=== 1D Smoothing Filter Demo ===\n";

    rcompute c;
    if (!rcompute_init(&c, 4, 3))
    {
        std::cerr << "Init failed\n";
        return;
    }

    c.program = rcompute_compile_file("smoothing.comp");
    if (!c.program)
    {
        std::cerr << "Shader compile failed: " << rcompute_get_last_error() << "\n";
        rcompute_destroy(&c);
        return;
    }

    const int N = 16;
    std::vector<float> input(N);
    std::vector<float> output(N);

    // Create noisy data
    for (int i = 0; i < N; i++)
    {
        input[i] = (i % 3 == 0) ? 10.0f : 1.0f;
    }

    GLuint buf_in = rcompute_buffer(N * sizeof(float), input.data());
    GLuint buf_out = rcompute_buffer(N * sizeof(float), nullptr);

    rcompute_buffer_bind(buf_in, 0);
    rcompute_buffer_bind(buf_out, 1);

    // Set uniform using helper
    rcompute_set_uniform_uint(&c, "array_size", N);

    rcompute_dispatch_1d(&c, (N + 255) / 256);

    rcompute_read(buf_out, output.data(), N * sizeof(float));

    std::cout << "Input:  ";
    for (int i = 0; i < N; i++)
        std::cout << std::fixed << std::setprecision(1) << input[i] << " ";
    std::cout << "\nOutput: ";
    for (int i = 0; i < N; i++)
        std::cout << std::fixed << std::setprecision(1) << output[i] << " ";
    std::cout << "\n";

    rcompute_buffer_destroy(buf_in);
    rcompute_buffer_destroy(buf_out);
    rcompute_destroy(&c);
}

void demo_reduction()
{
    std::cout << "\n=== Parallel Reduction (Sum) Demo ===\n";

    rcompute c;
    if (!rcompute_init(&c, 4, 3))
    {
        std::cerr << "Init failed\n";
        return;
    }

    c.program = rcompute_compile_file("reduction.comp");
    if (!c.program)
    {
        std::cerr << "Shader compile failed: " << rcompute_get_last_error() << "\n";
        rcompute_destroy(&c);
        return;
    }

    const int N = 1024;
    std::vector<float> values(N, 1.0f); // All ones

    int num_workgroups = (N + 255) / 256;
    std::vector<float> partial_sums(num_workgroups);

    GLuint buf_in = rcompute_buffer(N * sizeof(float), values.data());
    GLuint buf_out = rcompute_buffer(num_workgroups * sizeof(float), nullptr);

    rcompute_buffer_bind(buf_in, 0);
    rcompute_buffer_bind(buf_out, 1);

    rcompute_set_uniform_uint(&c, "array_size", N);

    rcompute_run(&c, num_workgroups, 1, 1);

    rcompute_read(buf_out, partial_sums.data(), num_workgroups * sizeof(float));

    // Final reduction on CPU
    float total = 0.0f;
    for (float val : partial_sums)
        total += val;

    std::cout << "Sum of " << N << " values: " << total << " (expected: " << N << ")\n";

    rcompute_buffer_destroy(buf_in);
    rcompute_buffer_destroy(buf_out);
    rcompute_destroy(&c);
}

void demo_matrix_multiply()
{
    std::cout << "\n=== Matrix Multiplication Demo ===\n";

    rcompute c;
    if (!rcompute_init(&c, 4, 3))
    {
        std::cerr << "Init failed\n";
        return;
    }

    c.program = rcompute_compile_file("matmul.comp");
    if (!c.program)
    {
        std::cerr << "Shader compile failed: " << rcompute_get_last_error() << "\n";
        rcompute_destroy(&c);
        return;
    }

    const int M = 4, N = 4, P = 4;

    // Create identity matrix A
    std::vector<float> A(M * N, 0.0f);
    for (int i = 0; i < M; i++)
        A[i * N + i] = 1.0f;

    // Create matrix B with values
    std::vector<float> B(N * P);
    for (int i = 0; i < N * P; i++)
        B[i] = i + 1.0f;

    std::vector<float> C(M * P);

    GLuint buf_a = rcompute_buffer(M * N * sizeof(float), A.data());
    GLuint buf_b = rcompute_buffer(N * P * sizeof(float), B.data());
    GLuint buf_c = rcompute_buffer(M * P * sizeof(float), nullptr);

    rcompute_buffer_bind(buf_a, 0);
    rcompute_buffer_bind(buf_b, 1);
    rcompute_buffer_bind(buf_c, 2);

    rcompute_set_uniform_uint(&c, "M", M);
    rcompute_set_uniform_uint(&c, "N", N);
    rcompute_set_uniform_uint(&c, "P", P);

    rcompute_run(&c, (P + 7) / 8, (M + 7) / 8, 1);

    rcompute_read(buf_c, C.data(), M * P * sizeof(float));

    std::cout << "Result matrix (Identity * B = B):\n";
    for (int i = 0; i < M; i++)
    {
        for (int j = 0; j < P; j++)
        {
            std::cout << std::setw(6) << std::fixed << std::setprecision(1) << C[i * P + j] << " ";
        }
        std::cout << "\n";
    }

    rcompute_buffer_destroy(buf_a);
    rcompute_buffer_destroy(buf_b);
    rcompute_buffer_destroy(buf_c);
    rcompute_destroy(&c);
}

int main()
{
    std::cout << "=== RCompute Comprehensive Examples ===\n";

    demo_smoothing();
    demo_reduction();
    demo_matrix_multiply();

    std::cout << "\n=== All demos completed ===\n";
    return 0;
}
