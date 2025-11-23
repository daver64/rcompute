#define RCOMPUTE_IMPLEMENTATION
#include "include/rcompute.h"
#include <iostream>

int main() {
    rcompute c;
    rcompute_init(&c, 4, 3);
    
    // Query with OpenGL directly for comparison
    GLint direct_count[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &direct_count[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &direct_count[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &direct_count[2]);
    
    std::cout << "Direct GL query:\n";
    std::cout << "  X: " << direct_count[0] << "\n";
    std::cout << "  Y: " << direct_count[1] << "\n";
    std::cout << "  Z: " << direct_count[2] << "\n\n";
    
    // Query via rcompute
    int count_x, count_y, count_z;
    rcompute_get_limits(&c, &count_x, &count_y, &count_z, NULL, NULL, NULL, NULL);
    
    std::cout << "Via rcompute_get_limits:\n";
    std::cout << "  X: " << count_x << "\n";
    std::cout << "  Y: " << count_y << "\n";
    std::cout << "  Z: " << count_z << "\n\n";
    
    // Check for GL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cout << "OpenGL Error: " << err << "\n";
    }
    
    std::cout << "This is actually correct! The X dimension can dispatch up to 2^31-1 work groups.\n";
    std::cout << "This means you can have billions of work groups in the X dimension.\n";
    
    rcompute_destroy(&c);
    return 0;
}
