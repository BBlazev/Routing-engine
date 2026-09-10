#ifndef VK_LOADER_HPP
#define VK_LOADER_HPP

#include <vulkan/vk_types.hpp>
#include <unordered_map>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <optional>

class VulkanContext;

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>>
loadGltfMeshes(VulkanContext* engine, std::filesystem::path filePath);



#endif // !VK_LOADER_HPP
