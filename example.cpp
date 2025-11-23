#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <iostream>

int main()
{
    rcompute c;
    if (!rcompute_init(&c, 4, 3))
    {
        std::cerr << "GL init failed\n";
        return 1;
    }

    // Load shader from file
    c.program = rcompute_compile_file("shader.comp");
    if (!c.program)
    {
        std::cerr << "Shader compilation failed\n";
        return 1;
    }

    int initial = 0;
    GLuint buf = rcompute_buffer(sizeof(int), &initial);
    rcompute_buffer_bind(buf, 0);

    rcompute_run(&c, 1, 1, 1);

    int out;
    rcompute_read(buf, &out, sizeof(out));
    std::cout << "GPU wrote: " << out << "\n";

    // Cleanup
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&c);

    return 0;
}
