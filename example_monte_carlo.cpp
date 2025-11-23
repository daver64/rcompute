// Monte Carlo π estimation using GPU
// Demonstrates massive parallel random sampling

#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main()
{
    printf("=== Monte Carlo π Estimation ===\n\n");
    
    const int THREADS = 65536;  // 256 work groups × 256 threads
    const int SAMPLES_PER_THREAD = 1000;
    const long long TOTAL_SAMPLES = (long long)THREADS * SAMPLES_PER_THREAD;
    
    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3)) {
        fprintf(stderr, "Init failed\n");
        return 1;
    }
    
    ctx.program = rcompute_compile_file("example_monte_carlo.comp");
    if (!ctx.program) {
        fprintf(stderr, "Compile failed: %s\n", rcompute_get_last_error());
        return 1;
    }
    
    printf("Sampling %lld random points...\n", TOTAL_SAMPLES);
    printf("Using %d GPU threads\n\n", THREADS);
    
    // Create result buffer: [hits, total]
    unsigned int results[2] = {0, 0};
    GLuint buf = rcompute_buffer(2 * sizeof(unsigned int), results);
    rcompute_buffer_bind(buf, 0);
    
    // Use random seed
    rcompute_set_uniform_uint(&ctx, "seed_base", (unsigned int)rand());
    
    rcompute_timer_begin();
    rcompute_dispatch_1d(&ctx, THREADS / 256);
    rcompute_barrier_all();
    double elapsed = rcompute_timer_end();
    
    // Read results
    rcompute_read(buf, results, 2 * sizeof(unsigned int));
    
    unsigned int hits = results[0];
    unsigned int total = results[1];
    
    double pi_estimate = 4.0 * hits / (double)total;
    double error = fabs(pi_estimate - M_PI);
    double error_percent = 100.0 * error / M_PI;
    
    printf("Results:\n");
    printf("  Hits inside circle: %u\n", hits);
    printf("  Total samples: %u\n", total);
    printf("  π estimate: %.10f\n", pi_estimate);
    printf("  Actual π:   %.10f\n", M_PI);
    printf("  Error: %.10f (%.4f%%)\n", error, error_percent);
    printf("\nGPU time: %.2f ms\n", elapsed);
    printf("Sampling rate: %.2f billion samples/sec\n",
           (TOTAL_SAMPLES / 1e9) / (elapsed / 1000.0));
    
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&ctx);
    
    return 0;
}
