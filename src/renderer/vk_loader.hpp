#ifndef VK_LOADER_HPP
#define VK_LOADER_HPP

#include <vulkan/vk_types.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class VulkanDevice;

struct GeoSurface {

	uint32_t startIndex = 0;
	uint32_t count = 0;
	
};

struct MeshAsset {

	std::string name;
	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};

struct GltfLoadOptions {
	bool overrideColorsWithNormals = false;
};


std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(VulkanDevice& device, const std::filesystem::path& filePath,
																		const GltfLoadOptions& options = {});

#endif // VK_LOADER_HPP