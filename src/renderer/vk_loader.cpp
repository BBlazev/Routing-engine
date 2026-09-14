#include <renderer/vk_loader.hpp>

#include <vulkan/vk_device.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include <glm/gtx/quaternion.hpp>

#include <iostream>

namespace {


std::string gltf_error_string(fastgltf::Error error) {
	return std::to_string(static_cast<int>(fastgltf::to_underlying(error)));
}

} // namespace

std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(VulkanDevice& device, const std::filesystem::path& filePath,
																		const GltfLoadOptions& options) {

	std::cout << "Loading glTF: " << filePath.string() << "\n";

	if (!std::filesystem::exists(filePath)) {
		std::cerr << "  file does not exist\n";
		return std::nullopt;
	}

	auto dataFile = fastgltf::GltfDataBuffer::FromPath(filePath);
	if (dataFile.error() != fastgltf::Error::None) {
		std::cerr << "  failed to open file, error " << gltf_error_string(dataFile.error())
			<< "\n";
		return std::nullopt;
	}

	auto& data = dataFile.get();

	constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

	fastgltf::Parser parser{};

	const bool isBinary = filePath.extension() == ".glb" ||	filePath.extension() == ".GLB";

	auto load = isBinary
		? parser.loadGltfBinary(data, filePath.parent_path(), gltfOptions)
		: parser.loadGltfJson(data, filePath.parent_path(), gltfOptions);

	if (!load) {
		std::cerr << "  failed to parse glTF, error " << gltf_error_string(load.error())
			<< "\n";
		return std::nullopt;
	}

	fastgltf::Asset gltf = std::move(load.get());

	std::vector<std::shared_ptr<MeshAsset>> meshes;
	meshes.reserve(gltf.meshes.size());

	std::vector<uint32_t> indices;
	std::vector<Vertex>   vertices;

	for (fastgltf::Mesh& mesh : gltf.meshes) {
		MeshAsset newMesh;
		newMesh.name = mesh.name;

		indices.clear();
		vertices.clear();

		for (auto&& primitive : mesh.primitives) {
			if (!primitive.indicesAccessor.has_value()) {
				std::cerr << "  skipping non-indexed primitive in mesh '" << newMesh.name
					<< "'\n";
				continue;
			}

			GeoSurface surface;
			surface.startIndex = static_cast<uint32_t>(indices.size());
			surface.count =	static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count);

			const size_t initialVertex = vertices.size();

			{
				fastgltf::Accessor& indexAccessor =	gltf.accessors[primitive.indicesAccessor.value()];
				indices.reserve(indices.size() + indexAccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(
					gltf, indexAccessor, [&](std::uint32_t idx) {
					indices.push_back(idx + static_cast<uint32_t>(initialVertex));
				});
			}

			{
				auto posAttr = primitive.findAttribute("POSITION");

				if (posAttr == primitive.attributes.end()) {
					std::cerr << "  primitive has no POSITION attribute, skipping\n";
					continue;
				}

				fastgltf::Accessor& posAccessor = gltf.accessors[posAttr->accessorIndex];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(
					gltf, posAccessor, [&](glm::vec3 v, size_t index) {
					Vertex newVertex;
					newVertex.position = v;
					newVertex.normal = { 1.0f, 0.0f, 0.0f };
					newVertex.color = glm::vec4{ 1.0f };
					newVertex.uv_x = 0.0f;
					newVertex.uv_y = 0.0f;
					vertices[initialVertex + index] = newVertex;
				});
			}

			if (auto normals = primitive.findAttribute("NORMAL");

				normals != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(
					gltf, gltf.accessors[normals->accessorIndex],
					[&](glm::vec3 v, size_t index) {
					vertices[initialVertex + index].normal = v;
				});
			}

			if (auto uv = primitive.findAttribute("TEXCOORD_0");
				uv != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(
					gltf, gltf.accessors[uv->accessorIndex],
					[&](glm::vec2 v, size_t index) {
					vertices[initialVertex + index].uv_x = v.x;
					vertices[initialVertex + index].uv_y = v.y;
				});
			}

			if (auto colors = primitive.findAttribute("COLOR_0");
				colors != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(
					gltf, gltf.accessors[colors->accessorIndex],
					[&](glm::vec4 v, size_t index) {
					vertices[initialVertex + index].color = v;
				});
			}

			newMesh.surfaces.push_back(surface);
		}

		if (newMesh.surfaces.empty()) continue;

		if (options.overrideColorsWithNormals) {
			for (Vertex& vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.0f);
			}
		}

		newMesh.meshBuffers = device.upload_mesh(indices, vertices);
		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
	}

	std::cout << "  loaded " << meshes.size() << " mesh(es)\n";
	return meshes;
}