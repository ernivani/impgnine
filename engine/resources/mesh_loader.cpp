#define TINYOBJLOADER_IMPLEMENTATION
#include "mesh_loader.hpp"
#include <tiny_obj_loader.h>
#include <stdexcept>
#include <unordered_map>

namespace impgine {

void MeshLoader::loadModel(const std::string& path, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, AABB* outBoundingBox) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        throw std::runtime_error(err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    AABB bbox;  // Local bounding box

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // Expand bounding box
            bbox.expand(vertex.pos);

            if (index.texcoord_index >= 0 && static_cast<size_t>(index.texcoord_index) < attrib.texcoords.size() / 2) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            } else {
                vertex.texCoord = {0.0f, 0.0f};
            }

            vertex.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }

    // Output bounding box if requested
    if (outBoundingBox) {
        *outBoundingBox = bbox;
    }
}

} // namespace impgine
