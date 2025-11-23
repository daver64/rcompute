// Separable Gaussian blur filter
// Demonstrates two-pass image processing

#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <stdio.h>
#include <math.h>

void write_ppm(const char *filename, const float *data, int width, int height)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; i++) {
        unsigned char rgb[3] = {
            (unsigned char)(data[i * 4 + 0] * 255.0f),
            (unsigned char)(data[i * 4 + 1] * 255.0f),
            (unsigned char)(data[i * 4 + 2] * 255.0f)
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

void generate_test_pattern(float *data, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            // Checkerboard + gradient
            float check = ((x / 32) % 2) ^ ((y / 32) % 2) ? 1.0f : 0.3f;
            data[idx + 0] = check * (x / (float)width);
            data[idx + 1] = check * (y / (float)height);
            data[idx + 2] = check * (1.0f - x / (float)width);
            data[idx + 3] = 1.0f;
        }
    }
}

int main()
{
    printf("=== Separable Gaussian Blur ===\n\n");
    
    const int WIDTH = 1024;
    const int HEIGHT = 1024;
    
    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3)) {
        fprintf(stderr, "Init failed\n");
        return 1;
    }
    
    ctx.program = rcompute_compile_file("example_blur.comp");
    if (!ctx.program) {
        fprintf(stderr, "Compile failed: %s\n", rcompute_get_last_error());
        return 1;
    }
    
    // Generate test image
    printf("Generating %dx%d test image...\n", WIDTH, HEIGHT);
    float *input_data = new float[WIDTH * HEIGHT * 4];
    generate_test_pattern(input_data, WIDTH, HEIGHT);
    write_ppm("blur_input.ppm", input_data, WIDTH, HEIGHT);
    
    // Create textures (need 3: input, temp, output)
    GLuint tex_input = rcompute_texture_2d(WIDTH, HEIGHT, GL_RGBA32F, input_data);
    GLuint tex_temp = rcompute_texture_2d(WIDTH, HEIGHT, GL_RGBA32F, NULL);
    GLuint tex_output = rcompute_texture_2d(WIDTH, HEIGHT, GL_RGBA32F, NULL);
    
    // Gaussian weights for sigma=2.0
    float weights[5] = {0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f};
    
    // Pass 1: Horizontal blur (input -> temp)
    printf("Pass 1: Horizontal blur...\n");
    rcompute_texture_bind(tex_input, 0, GL_RGBA32F);
    rcompute_texture_bind(tex_temp, 1, GL_RGBA32F);
    rcompute_set_uniform_int(&ctx, "horizontal", 1);
    
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "weights[%d]", i);
        rcompute_set_uniform_float(&ctx, name, weights[i]);
    }
    
    rcompute_timer_begin();
    rcompute_dispatch_2d(&ctx, (WIDTH + 15) / 16, (HEIGHT + 15) / 16);
    rcompute_barrier_all();
    double time1 = rcompute_timer_end();
    printf("  Completed in %.3f ms\n", time1);
    
    // Pass 2: Vertical blur (temp -> output)
    printf("Pass 2: Vertical blur...\n");
    rcompute_texture_bind(tex_temp, 0, GL_RGBA32F);
    rcompute_texture_bind(tex_output, 1, GL_RGBA32F);
    rcompute_set_uniform_int(&ctx, "horizontal", 0);
    
    rcompute_timer_begin();
    rcompute_dispatch_2d(&ctx, (WIDTH + 15) / 16, (HEIGHT + 15) / 16);
    rcompute_barrier_all();
    double time2 = rcompute_timer_end();
    printf("  Completed in %.3f ms\n", time2);
    
    printf("\nTotal blur time: %.3f ms\n", time1 + time2);
    printf("Throughput: %.2f Mpixels/sec\n", 
           (WIDTH * HEIGHT * 2 / 1e6) / ((time1 + time2) / 1000.0));
    
    // Read back result
    float *output_data = new float[WIDTH * HEIGHT * 4];
    glBindTexture(GL_TEXTURE_2D, tex_output);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, output_data);
    write_ppm("blur_output.ppm", output_data, WIDTH, HEIGHT);
    printf("\nSaved: blur_input.ppm and blur_output.ppm\n");
    
    delete[] input_data;
    delete[] output_data;
    rcompute_texture_destroy(tex_input);
    rcompute_texture_destroy(tex_temp);
    rcompute_texture_destroy(tex_output);
    rcompute_destroy(&ctx);
    
    return 0;
}
