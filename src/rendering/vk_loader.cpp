#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <rendering/vk_loader.hpp>

#include <vulkan/vulkan_context.hpp>
#include "stb_image.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtx/quaternion.hpp>

#include <iostream>

std::optional<std::vector<std::shared_ptr<MeshAsset>>>
loadGltfMeshes(VulkanContext* engine, std::filesystem::path filePath) {
    
    std::cout << "Loading GLTF: " << filePath << "\n";

    auto dataFile = fastgltf::GltfDataBuffer::FromPath(filePath);
    if (dataFile.error() != fastgltf::Error::None) {
        std::cerr << "File not found or allocation failed!\n";
        return std::nullopt;
    }

    auto& data = dataFile.get();

    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    auto load = parser.loadGltfBinary(data, filePath.parent_path(), gltfOptions);
    
    if (load)
        gltf = std::move(load.get());
    else {
        fprintf(stderr, "Failed to load GlTF: {%s} \n", fastgltf::to_underlying(load.error()));
        return {};
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes) {
        
        MeshAsset newmesh;
        newmesh.name = mesh.name;

        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            
            GeoSurface newSurface;
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

            size_t initial_vtx = vertices.size();
            {
                
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);
                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) { 
                        indices.push_back(idx + initial_vtx); 
                    });

            }
            {
                auto posAttr = p.findAttribute("POSITION");
                if (posAttr != p.attributes.end()) {
                    fastgltf::Accessor& posAccessor = gltf.accessors[posAttr->accessorIndex];
                    vertices.resize(vertices.size() + posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        gltf, posAccessor, [&](glm::vec3 v, size_t index) {
                            Vertex newvtx;
                            newvtx.position = v;
                            newvtx.normal = {1, 0, 0};
                            newvtx.color = glm::vec4{1.f};
                            newvtx.uv_x = 0;
                            newvtx.uv_y = 0;
                            vertices[initial_vtx + index] = newvtx;
                        });
                }
            }
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    gltf, gltf.accessors[(*normals).accessorIndex],
                    [&](glm::vec3 v, size_t index) { vertices[initial_vtx + index].normal = v; });
            }

            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(
                    gltf, gltf.accessors[(*uv).accessorIndex], [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(
                    gltf, gltf.accessors[(*colors).accessorIndex],
                    [&](glm::vec4 v, size_t index) { vertices[initial_vtx + index].color = v; });
            }
            newmesh.surfaces.push_back(newSurface);

        }
        constexpr bool OverrideColors = true;
        if (OverrideColors) {
            for (Vertex& vtx : vertices) {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }
        newmesh.meshBuffers = engine->uploadMesh(indices, vertices);

        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
    }
    return meshes;
}
