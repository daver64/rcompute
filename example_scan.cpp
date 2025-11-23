// Parallel prefix sum (scan) - fundamental GPU algorithm
// Demonstrates shared memory and synchronization

#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <stdio.h>

int main()
{
    printf("=== Parallel Prefix Sum (Scan) ===\n\n");
    
    const int N = 512;  // Must be power of 2 for this simple implementation
    
    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3)) {
        fprintf(stderr, "Init failed\n");
        return 1;
    }
    
    ctx.program = rcompute_compile_file("example_scan.comp");
    if (!ctx.program) {
        fprintf(stderr, "Compile failed: %s\n", rcompute_get_last_error());
        return 1;
    }
    
    // Create test data - simple sequence
    printf("Computing prefix sum of %d elements...\n", N);
    int *input = new int[N];
    int *output = new int[N];
    
    for (int i = 0; i < N; i++)
        input[i] = 1;  // Sum will be 0,1,2,3...
    
    GLuint buf_in = rcompute_buffer(N * sizeof(int), input);
    GLuint buf_out = rcompute_buffer(N * sizeof(int), NULL);
    
    rcompute_buffer_bind(buf_in, 0);
    rcompute_buffer_bind(buf_out, 1);
    rcompute_set_uniform_int(&ctx, "n", N);
    
    rcompute_timer_begin();
    rcompute_dispatch_1d(&ctx, 1);  // Single work group
    rcompute_barrier_all();
    double elapsed = rcompute_timer_end();
    
    rcompute_read(buf_out, output, N * sizeof(int));
    
    printf("GPU time: %.3f ms\n\n", elapsed);
    
    // Verify
    printf("First 20 elements:\n");
    printf("Input:  ");
    for (int i = 0; i < 20; i++)
        printf("%d ", input[i]);
    printf("\nOutput: ");
    for (int i = 0; i < 20; i++)
        printf("%d ", output[i]);
    printf("\n\n");
    
    // Check correctness
    int expected = 0;
    bool correct = true;
    for (int i = 0; i < N; i++) {
        if (output[i] != expected) {
            printf("ERROR at index %d: expected %d, got %d\n", i, expected, output[i]);
            correct = false;
            break;
        }
        expected += input[i];
    }
    
    if (correct)
        printf("✓ Scan result is correct!\n");
    
    delete[] input;
    delete[] output;
    rcompute_buffer_destroy(buf_in);
    rcompute_buffer_destroy(buf_out);
    rcompute_destroy(&ctx);
    
    return 0;
}
