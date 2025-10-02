#pragma once

#include "../backend/pipeline.hpp"
#include "../ECS/components.hpp"
#include <vector>
#include <string>

namespace impgine {

class MeshLoader {
public:
    static void loadModel(const std::string& path, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, AABB* outBoundingBox = nullptr);
};

} // namespace impgine
