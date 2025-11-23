// Mandelbrot fractal generation on GPU
// Demonstrates compute shader for mathematical visualization

#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <stdio.h>

void write_ppm(const char *filename, const float *data, int width, int height)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; i++) {
        unsigned char r = (unsigned char)(data[i * 4 + 0] * 255.0f);
        unsigned char g = (unsigned char)(data[i * 4 + 1] * 255.0f);
        unsigned char b = (unsigned char)(data[i * 4 + 2] * 255.0f);
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);
}

int main()
{
    printf("=== Mandelbrot Fractal Generator ===\n\n");
    
    const int WIDTH = 1920;
    const int HEIGHT = 1080;
    
    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3)) {
        fprintf(stderr, "Init failed\n");
        return 1;
    }
    
    ctx.program = rcompute_compile_file("example_mandelbrot.comp");
    if (!ctx.program) {
        fprintf(stderr, "Compile failed: %s\n", rcompute_get_last_error());
        return 1;
    }
    
    // Create output texture
    GLuint output_tex = rcompute_texture_2d(WIDTH, HEIGHT, GL_RGBA32F, NULL);
    rcompute_texture_bind(output_tex, 0, GL_RGBA32F);
    
    // Render multiple zoom levels
    struct Scene {
        float cx, cy, zoom;
        int iterations;
        const char *name;
    } scenes[] = {
        {-0.5f, 0.0f, 4.0f, 256, "mandelbrot_full.ppm"},
        {-0.7f, 0.0f, 1.0f, 512, "mandelbrot_zoom1.ppm"},
        {-0.743643f, 0.131825f, 0.01f, 1024, "mandelbrot_zoom2.ppm"},
        {-0.743643f, 0.131825f, 0.001f, 2048, "mandelbrot_zoom3.ppm"}
    };
    
    float *output = new float[WIDTH * HEIGHT * 4];
    
    for (int i = 0; i < 4; i++) {
        printf("Rendering: %s (zoom=%.6f, iter=%d)\n", 
               scenes[i].name, scenes[i].zoom, scenes[i].iterations);
        
        rcompute_set_uniform_vec2(&ctx, "center", scenes[i].cx, scenes[i].cy);
        rcompute_set_uniform_float(&ctx, "zoom", scenes[i].zoom);
        rcompute_set_uniform_int(&ctx, "maxIterations", scenes[i].iterations);
        
        rcompute_timer_begin();
        rcompute_dispatch_2d(&ctx, (WIDTH + 15) / 16, (HEIGHT + 15) / 16);
        rcompute_barrier_all();
        double elapsed = rcompute_timer_end();
        
        printf("  Rendered in %.2f ms\n", elapsed);
        
        // Read back
        glBindTexture(GL_TEXTURE_2D, output_tex);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, output);
        
        write_ppm(scenes[i].name, output, WIDTH, HEIGHT);
        printf("  Saved to %s\n\n", scenes[i].name);
    }
    
    delete[] output;
    rcompute_texture_destroy(output_tex);
    rcompute_destroy(&ctx);
    
    printf("Done! View the .ppm files to see the fractals.\n");
    return 0;
}
