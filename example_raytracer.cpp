// Simple raytracer - spheres and plane with animation
// Demonstrates ray-object intersection in compute shaders

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

int main()
{
    printf("=== Simple Raytracer ===\n\n");
    
    const int WIDTH = 1280;
    const int HEIGHT = 720;
    const int FRAMES = 120;
    
    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3)) {
        fprintf(stderr, "Init failed\n");
        return 1;
    }
    
    ctx.program = rcompute_compile_file("example_raytracer.comp");
    if (!ctx.program) {
        fprintf(stderr, "Compile failed: %s\n", rcompute_get_last_error());
        return 1;
    }
    
    GLuint output_tex = rcompute_texture_2d(WIDTH, HEIGHT, GL_RGBA32F, NULL);
    rcompute_texture_bind(output_tex, 0, GL_RGBA32F);
    
    float *output = new float[WIDTH * HEIGHT * 4];
    
    printf("Rendering %d frames at %dx%d...\n", FRAMES, WIDTH, HEIGHT);
    printf("Progress: ");
    fflush(stdout);
    
    double total_time = 0.0;
    
    for (int frame = 0; frame < FRAMES; frame++) {
        if (frame % 10 == 0) {
            printf("%d ", frame);
            fflush(stdout);
        }
        
        float t = frame / 30.0f; // 30 fps animation
        float cam_x = sin(t * 0.5f) * 3.0f;
        float cam_y = 1.0f + sin(t * 0.3f) * 0.5f;
        float cam_z = cos(t * 0.5f) * 3.0f;
        
        rcompute_set_uniform_vec3(&ctx, "camPos", cam_x, cam_y, cam_z);
        rcompute_set_uniform_float(&ctx, "time", t);
        
        rcompute_timer_begin();
        rcompute_dispatch_2d(&ctx, (WIDTH + 15) / 16, (HEIGHT + 15) / 16);
        rcompute_barrier_all();
        total_time += rcompute_timer_end();
        
        // Save a few keyframes
        if (frame == 0 || frame == 30 || frame == 60 || frame == 90) {
            glBindTexture(GL_TEXTURE_2D, output_tex);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, output);
            
            char filename[64];
            snprintf(filename, sizeof(filename), "raytrace_frame%03d.ppm", frame);
            write_ppm(filename, output, WIDTH, HEIGHT);
        }
    }
    
    printf("\n\nRendering complete!\n");
    printf("Total time: %.2f ms\n", total_time);
    printf("Average per frame: %.2f ms (%.1f FPS)\n", 
           total_time / FRAMES, 1000.0 * FRAMES / total_time);
    printf("Throughput: %.2f Mpixels/sec\n",
           (WIDTH * HEIGHT * FRAMES / 1e6) / (total_time / 1000.0));
    
    printf("\nSaved frames: raytrace_frame000.ppm, frame030.ppm, frame060.ppm, frame090.ppm\n");
    
    delete[] output;
    rcompute_texture_destroy(output_tex);
    rcompute_destroy(&ctx);
    
    return 0;
}
