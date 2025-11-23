// Texture processing example - edge detection with tinting
// Demonstrates 2D texture operations in compute shaders

#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <stdio.h>
#include <math.h>

// Generate a test image with some geometric patterns
void generate_test_image(float *data, int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int idx = (y * width + x) * 4;
            
            // Create a pattern with circles and gradients
            float cx = width / 2.0f;
            float cy = height / 2.0f;
            float dist = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
            float radius = width / 4.0f;
            
            // Circular pattern
            float circle = (dist < radius) ? 1.0f : 0.0f;
            float ring = (dist > radius * 0.8f && dist < radius * 1.2f) ? 1.0f : 0.0f;
            
            // Gradient background
            float gradX = x / (float)width;
            float gradY = y / (float)height;
            
            // Combine patterns
            data[idx + 0] = circle * 0.9f + ring * 0.3f + gradX * 0.2f; // R
            data[idx + 1] = circle * 0.3f + ring * 0.8f + gradY * 0.3f; // G
            data[idx + 2] = circle * 0.2f + ring * 0.9f + (1.0f - gradX) * 0.2f; // B
            data[idx + 3] = 1.0f; // A
        }
    }
}

// Simple PPM image writer for visualization
void write_ppm(const char *filename, const float *data, int width, int height)
{
    FILE *f = fopen(filename, "wb");
    if (!f)
    {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
    }
    
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    
    for (int i = 0; i < width * height; i++)
    {
        unsigned char r = (unsigned char)(data[i * 4 + 0] * 255.0f);
        unsigned char g = (unsigned char)(data[i * 4 + 1] * 255.0f);
        unsigned char b = (unsigned char)(data[i * 4 + 2] * 255.0f);
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    
    fclose(f);
    printf("Wrote image to %s\n", filename);
}

int main()
{
    printf("=== RCompute Texture Processing Example ===\n\n");
    
    // Image dimensions
    const int WIDTH = 512;
    const int HEIGHT = 512;
    const int CHANNELS = 4; // RGBA
    
    // Initialize
    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3))
    {
        fprintf(stderr, "Failed to initialize rcompute\n");
        return 1;
    }
    
    // Load and compile shader
    printf("Compiling shader...\n");
    ctx.program = rcompute_compile_file("example_texture.comp");
    if (!ctx.program)
    {
        fprintf(stderr, "Shader compilation failed: %s\n", rcompute_get_last_error());
        rcompute_destroy(&ctx);
        return 1;
    }
    
    // Generate test image
    printf("Generating test image (%dx%d)...\n", WIDTH, HEIGHT);
    float *input_data = new float[WIDTH * HEIGHT * CHANNELS];
    generate_test_image(input_data, WIDTH, HEIGHT);
    
    // Save input image
    write_ppm("input.ppm", input_data, WIDTH, HEIGHT);
    
    // Create textures
    printf("Creating GPU textures...\n");
    GLuint input_tex = rcompute_texture_2d(WIDTH, HEIGHT, GL_RGBA32F, input_data);
    GLuint output_tex = rcompute_texture_2d(WIDTH, HEIGHT, GL_RGBA32F, NULL); // NULL = uninitialized
    
    // Bind textures to image units
    rcompute_texture_bind(input_tex, 0, GL_RGBA32F);
    rcompute_texture_bind(output_tex, 1, GL_RGBA32F);
    
    // Set uniforms
    rcompute_set_uniform_float(&ctx, "threshold", 0.15f);
    rcompute_set_uniform_vec3(&ctx, "tintColor", 1.0f, 0.5f, 0.0f); // Orange tint for edges
    
    // Dispatch compute shader
    // Work groups of 16x16, need to cover entire image
    int groups_x = (WIDTH + 15) / 16;
    int groups_y = (HEIGHT + 15) / 16;
    
    printf("Processing image (dispatching %dx%d work groups)...\n", groups_x, groups_y);
    rcompute_timer_begin();
    rcompute_dispatch_2d(&ctx, groups_x, groups_y);
    rcompute_barrier_all();
    double elapsed = rcompute_timer_end();
    
    printf("Processing completed in %.3f ms\n", elapsed);
    
    // Read back output texture
    printf("Reading back results...\n");
    float *output_data = new float[WIDTH * HEIGHT * CHANNELS];
    
    // Bind output texture as regular texture to read back
    glBindTexture(GL_TEXTURE_2D, output_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, output_data);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Save output image
    write_ppm("output.ppm", output_data, WIDTH, HEIGHT);
    
    // Compute some statistics
    float max_diff = 0.0f;
    int edges_detected = 0;
    for (int i = 0; i < WIDTH * HEIGHT; i++)
    {
        float in_r = input_data[i * 4 + 0];
        float out_r = output_data[i * 4 + 0];
        float diff = fabsf(out_r - in_r);
        if (diff > max_diff)
            max_diff = diff;
        if (diff > 0.2f) // Significant change = edge
            edges_detected++;
    }
    
    printf("\nStatistics:\n");
    printf("  Maximum pixel difference: %.3f\n", max_diff);
    printf("  Edge pixels detected: %d (%.1f%%)\n", edges_detected, 
           100.0f * edges_detected / (WIDTH * HEIGHT));
    
    // Cleanup
    delete[] input_data;
    delete[] output_data;
    rcompute_texture_destroy(input_tex);
    rcompute_texture_destroy(output_tex);
    rcompute_destroy(&ctx);
    
    printf("\n=== Processing complete! ===\n");
    printf("View results: input.ppm and output.ppm\n");
    
    return 0;
}
