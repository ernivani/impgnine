#pragma once

#include "../backend/pipeline.hpp"
#include <vector>
#include <string>

namespace impgine {

class MeshLoader {
public:
    static void loadModel(const std::string& path, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
};

} // namespace impgine
