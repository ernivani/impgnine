#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "engine.hpp"

namespace impgine {
    int main() {
        Engine engine;

        try {
            engine.run();
        } catch (const std::exception & e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
} // namespace impgine

int main() {
    return impgine::main();
}