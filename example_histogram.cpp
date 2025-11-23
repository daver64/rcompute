// Image histogram using atomic operations
// Demonstrates atomic operations for concurrent updates

#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <stdio.h>
#include <math.h>

void generate_gradient(float *data, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            float v = (x + y) / (float)(width + height);
            data[idx + 0] = v;
            data[idx + 1] = v;
            data[idx + 2] = v;
            data[idx + 3] = 1.0f;
        }
    }
}

void print_histogram_bars(const unsigned int *bins, int total)
{
    const int BAR_WIDTH = 60;
    unsigned int max_count = 0;
    for (int i = 0; i < 256; i++)
        if (bins[i] > max_count)
            max_count = bins[i];
    
    printf("\nHistogram (bin : count : bar)\n");
    for (int i = 0; i < 256; i += 16) {
        printf("%3d: %6u ", i, bins[i]);
        int bar_len = (bins[i] * BAR_WIDTH) / max_count;
        for (int j = 0; j < bar_len; j++)
            printf("█");
        printf("\n");
    }
}

int main()
{
    printf("=== Image Histogram with Atomics ===\n\n");
    
    const int WIDTH = 1024;
    const int HEIGHT = 1024;
    
    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3)) {
        fprintf(stderr, "Init failed\n");
        return 1;
    }
    
    ctx.program = rcompute_compile_file("example_histogram.comp");
    if (!ctx.program) {
        fprintf(stderr, "Compile failed: %s\n", rcompute_get_last_error());
        return 1;
    }
    
    // Generate test image
    printf("Generating %dx%d gradient image...\n", WIDTH, HEIGHT);
    float *image_data = new float[WIDTH * HEIGHT * 4];
    generate_gradient(image_data, WIDTH, HEIGHT);
    
    GLuint tex = rcompute_texture_2d(WIDTH, HEIGHT, GL_RGBA32F, image_data);
    rcompute_texture_bind(tex, 0, GL_RGBA32F);
    
    // Create histogram buffer (256 bins, zero-initialized)
    GLuint hist_buf = rcompute_buffer_zero(256 * sizeof(unsigned int));
    rcompute_buffer_bind(hist_buf, 0);
    
    printf("Computing histogram...\n");
    rcompute_timer_begin();
    rcompute_dispatch_2d(&ctx, (WIDTH + 15) / 16, (HEIGHT + 15) / 16);
    rcompute_barrier_all();
    double elapsed = rcompute_timer_end();
    
    // Read results
    unsigned int *histogram = new unsigned int[256];
    rcompute_read(hist_buf, histogram, 256 * sizeof(unsigned int));
    
    printf("Computed in %.3f ms\n", elapsed);
    printf("Throughput: %.2f Mpixels/sec\n",
           (WIDTH * HEIGHT / 1e6) / (elapsed / 1000.0));
    
    // Verify total count
    unsigned int total = 0;
    for (int i = 0; i < 256; i++)
        total += histogram[i];
    
    printf("\nTotal pixels counted: %u (expected: %d)\n", total, WIDTH * HEIGHT);
    
    if (total == WIDTH * HEIGHT)
        printf("✓ Histogram is correct!\n");
    
    print_histogram_bars(histogram, total);
    
    delete[] image_data;
    delete[] histogram;
    rcompute_texture_destroy(tex);
    rcompute_buffer_destroy(hist_buf);
    rcompute_destroy(&ctx);
    
    return 0;
}
