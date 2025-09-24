#include "engine.hpp"
#include <iostream>
#include <stdexcept>
#include <cstdlib>

namespace impgine {
    int main() {
        Engine engine;

        try {
            engine.run();
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
}

int main() {
    return impgine::main();
}