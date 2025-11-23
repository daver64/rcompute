// N-body gravitational simulation
// Demonstrates particle physics on GPU

#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

struct Particle {
    float pos[4];  // x, y, z, mass
    float vel[4];  // vx, vy, vz, unused
};

float randf() { return (float)rand() / RAND_MAX; }

int main()
{
    printf("=== N-Body Gravitational Simulation ===\n\n");
    
    const int N = 4096;
    const int STEPS = 1000;
    const float DT = 0.001f;
    const float SOFTENING = 0.001f;
    
    rcompute ctx;
    if (!rcompute_init(&ctx, 4, 3)) {
        fprintf(stderr, "Init failed\n");
        return 1;
    }
    
    ctx.program = rcompute_compile_file("example_nbody.comp");
    if (!ctx.program) {
        fprintf(stderr, "Compile failed: %s\n", rcompute_get_last_error());
        return 1;
    }
    
    // Initialize particles
    printf("Initializing %d particles...\n", N);
    Particle *particles = new Particle[N];
    
    for (int i = 0; i < N; i++) {
        // Random positions in sphere
        float theta = randf() * 2.0f * M_PI;
        float phi = acos(2.0f * randf() - 1.0f);
        float r = pow(randf(), 1.0f/3.0f) * 0.5f;
        
        particles[i].pos[0] = r * sin(phi) * cos(theta);
        particles[i].pos[1] = r * sin(phi) * sin(theta);
        particles[i].pos[2] = r * cos(phi);
        particles[i].pos[3] = 1.0f + randf() * 0.5f; // mass
        
        // Orbital velocity
        float v = sqrt(r) * 0.3f;
        particles[i].vel[0] = -v * sin(theta);
        particles[i].vel[1] = v * cos(theta);
        particles[i].vel[2] = (randf() - 0.5f) * 0.1f;
        particles[i].vel[3] = 0.0f;
    }
    
    GLuint buffer = rcompute_buffer(N * sizeof(Particle), particles);
    rcompute_buffer_bind(buffer, 0);
    
    // Set uniforms
    rcompute_set_uniform_float(&ctx, "dt", DT);
    rcompute_set_uniform_float(&ctx, "softening", SOFTENING);
    rcompute_set_uniform_int(&ctx, "numBodies", N);
    
    // Simulation loop
    printf("Running %d simulation steps...\n", STEPS);
    printf("Progress: ");
    fflush(stdout);
    
    double total_time = 0.0;
    for (int step = 0; step < STEPS; step++) {
        if (step % 100 == 0) {
            printf("%d ", step);
            fflush(stdout);
        }
        
        rcompute_timer_begin();
        rcompute_dispatch_1d(&ctx, (N + 255) / 256);
        rcompute_barrier_all();
        total_time += rcompute_timer_end();
    }
    
    printf("\n\nSimulation complete!\n");
    printf("Total GPU time: %.2f ms\n", total_time);
    printf("Average per step: %.3f ms\n", total_time / STEPS);
    printf("Interactions per second: %.2f million\n", 
           (N * N * STEPS / 1e6) / (total_time / 1000.0));
    
    // Read final state
    rcompute_read(buffer, particles, N * sizeof(Particle));
    
    // Calculate center of mass and bounds
    float cx = 0, cy = 0, cz = 0, total_mass = 0;
    float min_x = 1e6, max_x = -1e6;
    float min_y = 1e6, max_y = -1e6;
    float min_z = 1e6, max_z = -1e6;
    
    for (int i = 0; i < N; i++) {
        float m = particles[i].pos[3];
        cx += particles[i].pos[0] * m;
        cy += particles[i].pos[1] * m;
        cz += particles[i].pos[2] * m;
        total_mass += m;
        
        min_x = fmin(min_x, particles[i].pos[0]);
        max_x = fmax(max_x, particles[i].pos[0]);
        min_y = fmin(min_y, particles[i].pos[1]);
        max_y = fmax(max_y, particles[i].pos[1]);
        min_z = fmin(min_z, particles[i].pos[2]);
        max_z = fmax(max_z, particles[i].pos[2]);
    }
    
    cx /= total_mass;
    cy /= total_mass;
    cz /= total_mass;
    
    printf("\nFinal state:\n");
    printf("  Center of mass: (%.3f, %.3f, %.3f)\n", cx, cy, cz);
    printf("  Bounds X: [%.3f, %.3f]\n", min_x, max_x);
    printf("  Bounds Y: [%.3f, %.3f]\n", min_y, max_y);
    printf("  Bounds Z: [%.3f, %.3f]\n", min_z, max_z);
    
    delete[] particles;
    rcompute_buffer_destroy(buffer);
    rcompute_destroy(&ctx);
    
    return 0;
}
