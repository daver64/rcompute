// Nebulabrot fractal accumulation on GPU using compute shader atomics
// Generates a single PPM image showing Buddhabrot-style density

#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"

#include <cmath>
#include <cstdint>
#include <stdio.h>
#include <vector>

struct Vec3 { float r, g, b; };

static float clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void write_ppm(const char *filename, const float *data, int width, int height)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; i++) {
        float cr = clamp01(data[i * 4 + 0]);
        float cg = clamp01(data[i * 4 + 1]);
        float cb = clamp01(data[i * 4 + 2]);

        unsigned char r = (unsigned char)(cr * 255.0f);
        unsigned char g = (unsigned char)(cg * 255.0f);
        unsigned char b = (unsigned char)(cb * 255.0f);
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);
}

static Vec3 palette(float t)
{
    // Cooler Buddhabrot-inspired gradient: deep navy -> teal -> ice -> white
    float m1 = clamp01((t - 0.02f) / 0.35f);
    float m2 = clamp01((t - 0.35f) / 0.45f);

    Vec3 base   {0.02f, 0.05f, 0.12f};  // dark navy
    Vec3 mid    {0.05f, 0.35f, 0.50f};  // teal
    Vec3 upper  {0.70f, 0.85f, 0.95f};  // icy blue
    Vec3 white  {1.00f, 1.00f, 1.00f};  // hot core

    // Two-stage lerp to keep shadows cool and cores neutral
    Vec3 c1 {
        base.r + (mid.r - base.r) * m1,
        base.g + (mid.g - base.g) * m1,
        base.b + (mid.b - base.b) * m1
    };

    Vec3 c2 {
        upper.r + (white.r - upper.r) * m2,
        upper.g + (white.g - upper.g) * m2,
        upper.b + (white.b - upper.b) * m2
    };

    Vec3 result {
        c1.r + (c2.r - c1.r) * t,
        c1.g + (c2.g - c1.g) * t,
        c1.b + (c2.b - c1.b) * t
    };
    return result;
}

int main()
{
    printf("=== Nebulabrot Fractal Generator ===\n\n");

    const int WIDTH = 1920;
    const int HEIGHT = 1080;

    // Tunable parameters
    const int SAMPLES_PER_INVOCATION = 64;  // more samples = richer image, slower runtime
    const int MAX_ITERATIONS = 800;
    const int MIN_ITERATIONS = 20;          // filter out short-lived orbits
    const int WORKGROUPS_X = 256;
    const int WORKGROUPS_Y = 144;
    const unsigned int SEED = 1337u;

    const float VIEW_MIN_X = -2.2f;
    const float VIEW_MAX_X = 1.2f;
    const float VIEW_MIN_Y = -1.5f;
    const float VIEW_MAX_Y = 1.5f;

    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3)) {
        fprintf(stderr, "Init failed: %s\n", rcompute_get_last_error());
        return 1;
    }

    ctx.program = rcompute_compile_file("example_nebulabrot.comp");
    if (!ctx.program) {
        fprintf(stderr, "Compile failed: %s\n", rcompute_get_last_error());
        return 1;
    }

    // Accumulation texture holds visit counts per pixel (integer format for atomic adds)
    GLuint accum_tex = rcompute_texture_2d(WIDTH, HEIGHT, GL_R32UI, NULL);
    rcompute_texture_bind(accum_tex, 0, GL_R32UI);

    // Clear accumulation buffer
    std::vector<uint32_t> zero_data(static_cast<size_t>(WIDTH) * HEIGHT, 0);
    glBindTexture(GL_TEXTURE_2D, accum_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT, zero_data.data());

    rcompute_set_uniform_uint(&ctx, "seed", SEED);
    rcompute_set_uniform_int(&ctx, "samplesPerInvocation", SAMPLES_PER_INVOCATION);
    rcompute_set_uniform_int(&ctx, "maxIterations", MAX_ITERATIONS);
    rcompute_set_uniform_int(&ctx, "minIterations", MIN_ITERATIONS);
    rcompute_set_uniform_vec2(&ctx, "viewMin", VIEW_MIN_X, VIEW_MIN_Y);
    rcompute_set_uniform_vec2(&ctx, "viewMax", VIEW_MAX_X, VIEW_MAX_Y);

    const unsigned long long total_samples =
        static_cast<unsigned long long>(WORKGROUPS_X) * WORKGROUPS_Y * 64ull * SAMPLES_PER_INVOCATION;
    printf("Dispatching ~%llu orbits (each GPU invocation traces %d samples)\n",
           total_samples, SAMPLES_PER_INVOCATION);

    rcompute_timer_begin();
    rcompute_dispatch_2d(&ctx, WORKGROUPS_X, WORKGROUPS_Y);
    rcompute_barrier_all();
    double elapsed_ms = rcompute_timer_end();
    printf("Accumulation completed in %.2f ms\n", elapsed_ms);

    // Read back counts
    std::vector<uint32_t> counts(static_cast<size_t>(WIDTH) * HEIGHT, 0);
    glBindTexture(GL_TEXTURE_2D, accum_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, counts.data());

    uint32_t max_count = 0;
    for (uint32_t v : counts) {
        if (v > max_count) max_count = v;
    }

    if (max_count == 0) {
        fprintf(stderr, "No samples recorded; try increasing samplesPerInvocation or workgroups.\n");
    }

    // Tone-map counts to colors on CPU for simplicity
    std::vector<float> output(static_cast<size_t>(WIDTH) * HEIGHT * 4, 1.0f);
    const float inv_log = (max_count > 0) ? 1.0f / std::log(static_cast<float>(max_count) + 1.0f) : 0.0f;

    for (size_t i = 0; i < counts.size(); ++i) {
        float norm = (max_count > 0) ? std::log(static_cast<float>(counts[i]) + 1.0f) * inv_log : 0.0f;
        norm = clamp01(norm * 0.9f);           // lower exposure slightly
        norm = std::pow(norm, 0.8f);            // darker mid/highlights for less blowout
        Vec3 col = palette(norm);

        output[i * 4 + 0] = col.r;
        output[i * 4 + 1] = col.g;
        output[i * 4 + 2] = col.b;
        output[i * 4 + 3] = 1.0f;
    }

    const char *out_name = "nebulabrot.ppm";
    write_ppm(out_name, output.data(), WIDTH, HEIGHT);
    printf("Saved image to %s\n", out_name);

    rcompute_texture_destroy(accum_tex);
    rcompute_destroy(&ctx);

    printf("Done! View the PPM to see the Nebulabrot.\n");
    return 0;
}
