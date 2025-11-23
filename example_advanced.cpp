#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <iostream>

int main()
{
    // Initialize compute context
    rcompute c;
    if (!rcompute_init(&c, 4, 3))
    {
        std::cerr << "GL init failed\n";
        if (const char *err = rcompute_get_last_error())
            std::cerr << "Error: " << err << "\n";
        return 1;
    }

    // Load shader from file
    GLuint program = rcompute_compile_file("shader.comp");
    if (!program)
    {
        std::cerr << "Shader compilation failed\n";
        if (const char *err = rcompute_get_last_error())
            std::cerr << "Error: " << err << "\n";
        return 1;
    }
    rcompute_set_program(&c, program);

    // Create buffer with dynamic usage
    int data[4] = {10, 20, 30, 40};
    GLuint buf = rcompute_buffer_ex(sizeof(data), data, RCOMPUTE_DYNAMIC);
    
    // Bind buffer to shader
    rcompute_buffer_bind(buf, 0);

    // Run compute shader using 1D dispatch convenience function
    rcompute_dispatch_1d(&c, 1);

    // Read results
    int result[4];
    rcompute_read(buf, result, sizeof(result));
    
    std::cout << "Results: ";
    for (int i = 0; i < 4; i++)
        std::cout << result[i] << " ";
    std::cout << "\n";

    // Update buffer data
    int new_data[4] = {100, 200, 300, 400};
    rcompute_buffer_write(buf, 0, sizeof(new_data), new_data);
    
    // Run again
    rcompute_dispatch_1d(&c, 1);
    
    // Read updated results
    rcompute_read(buf, result, sizeof(result));
    std::cout << "After update: ";
    for (int i = 0; i < 4; i++)
        std::cout << result[i] << " ";
    std::cout << "\n";

    // Proper cleanup
    rcompute_buffer_destroy(buf);
    rcompute_destroy(&c);

    std::cout << "Success!\n";
    return 0;
}
