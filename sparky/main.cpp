#define SP_DEBUG_RESOURCE_NAMING_ENABLED 1 
#define SP_DEBUG_RENDERDOC_HOOK_ENABLED 1
#define SP_DEBUG_API_VALIDATION_LEVEL 1
#define SP_DEBUG_SHUTDOWN_LEAK_REPORT_ENABLED 0

#include "handle.h"
#include "window.h"
#include "vertex_buffer.h"
#include "command_list.h"
#include "constant_buffer.h"
#include "texture.h"
#include "shader.h"
#include "pipeline.h"
#include "sparky.h"
#include "math.h"
#include "debug_gui.h"

#include "command_list_impl.h"
#include "constant_buffer_impl.h"
#include "pipeline_impl.h"
#include "texture_impl.h"
#include "vertex_buffer_impl.h"
#include "shader_impl.h"
#include "descriptor_impl.h"
#include "debug_gui_impl.h"

#include "shaders/clouds.cbuffer.hlsli"

#include <string>
#include <cassert>
#include <codecvt>
#include <array>
#include <vector>
#include <utility>
#include <iostream>

#include <windows.h>
#include <wrl.h>
#include <shellapi.h>

#include <fx/gltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct input
{
	struct state
	{
		bool keys[256];

		struct
		{
			bool is_in_client_area;
			int x;
			int y;
		} mouse;
	};

	state current;
	state previous;
};

void input_update(input* input)
{
	memcpy(&input->previous, &input->current, sizeof(input::state));
}

struct camera
{
	math::vec<3> position;
	math::vec<3> rotation;
};

math::mat<4> camera_get_transform(const camera& camera)
{
	math::mat<4> transform = math::create_identity<4>();
	transform = math::multiply(transform, math::create_rotation_x(camera.rotation.x));
	transform = math::multiply(transform, math::create_rotation_y(camera.rotation.y));
	transform = math::multiply(transform, math::create_rotation_z(camera.rotation.z));
	math::set_translation(transform, camera.position);
	return transform;
}

void camera_update(camera* camera, const input& input)
{
	if (ImGui::GetIO().WantCaptureMouse)
	{
		return;
	}

	const math::mat<4> camera_transform = camera_get_transform(*camera);

	const float movement_speed_mod = input.current.keys[VK_SHIFT] ? 100.0f : 1.0f;

	if (input.current.keys['A'])
	{
		camera->position -= math::get_right(camera_transform) * 0.1f * movement_speed_mod;
	}
	if (input.current.keys['D'])
	{
		camera->position += math::get_right(camera_transform) * 0.1f * movement_speed_mod;
	}
	if (input.current.keys['W'])
	{
		camera->position -= math::get_forward(camera_transform) * 0.1f * movement_speed_mod;
	}
	if (input.current.keys['S'])
	{
		camera->position += math::get_forward(camera_transform) * 0.1f * movement_speed_mod;
	}

	if (input.current.keys[VK_LBUTTON] && input.current.mouse.is_in_client_area && input.previous.mouse.is_in_client_area)
	{
		camera->rotation.y += (input.current.mouse.x - input.previous.mouse.x) * 0.01f;
		camera->rotation.x -= (input.current.mouse.y - input.previous.mouse.y) * 0.01f;
	}
}

struct model
{
	struct material
	{
		char name[128];
		int base_color_texture_index = -1;
		int metalness_roughness_texture_index = -1;
		std::array<float, 4> base_color_factor = { 1.0f, 1.0f, 1.0f, 1.0f };
		float metalness_factor = 1.0f;
		float roughness_factor = 1.0f;
		bool double_sided = false;
	};

	struct mesh
	{
		int vertex_count;
		sp_vertex_buffer_handle vertex_buffer_handle;
	};

	std::vector<mesh> meshes;
	std::vector<material> materials;
	std::vector<sp_texture_handle> textures;
};

model model_create_cube(sp_texture_handle base_color_texture_handle, sp_texture_handle metalness_roughness_texture_handle)
{
	model model;

	model.textures.push_back(base_color_texture_handle);
	model.textures.push_back(metalness_roughness_texture_handle);

	model::material material;

	material.base_color_texture_index = 0;
	material.metalness_roughness_texture_index = 1;
	material.double_sided = true;

	model.materials.push_back(material);

	model::mesh mesh;
	{
		struct cube_vertex
		{
			math::vec<3> position;
			math::vec<3> normal;
			math::vec<2> texcoord;
			math::vec<4> color;
		};

		cube_vertex cube_vertices[] =
		{
			// Front
			{ { -0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ {  0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ {  0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },

			// Back
			{ { -0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ { -0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ {  0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },

			// Left
			{ { -0.5f, -0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ { -0.5f,  0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.5f, -0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { -0.5f,  0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ { -0.5f,  0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.5f, -0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },

			// Right
			{ {  0.5f, -0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.5f,  0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ {  0.5f, -0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ {  0.5f, -0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.5f,  0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ {  0.5f,  0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },

			// Top
			{ { -0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ {  0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ {  0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },

			// Bottom
			{ { -0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
			{ { -0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ {  0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		};

		mesh.vertex_buffer_handle = sp_vertex_buffer_create("cube", { sizeof(cube_vertices), sizeof(cube_vertex) });
		sp_vertex_buffer_update(mesh.vertex_buffer_handle, cube_vertices, sizeof(cube_vertices));

		mesh.vertex_count = static_cast<int>(std::size(cube_vertices));
	}

	model.meshes.push_back(mesh);

	return model;
}

namespace detail
{
	int get_stride_bytes(const fx::gltf::Accessor& accessor)
	{
		int element_size_bytes = 0;
		switch (accessor.componentType)
		{
		case fx::gltf::Accessor::ComponentType::Byte:
		case fx::gltf::Accessor::ComponentType::UnsignedByte:
			element_size_bytes = 1;
			break;
		case fx::gltf::Accessor::ComponentType::Short:
		case fx::gltf::Accessor::ComponentType::UnsignedShort:
			element_size_bytes = 2;
			break;
		case fx::gltf::Accessor::ComponentType::Float:
		case fx::gltf::Accessor::ComponentType::UnsignedInt:
			element_size_bytes = 4;
			break;
		}

		int element_count = 0;
		switch (accessor.type)
		{
		case fx::gltf::Accessor::Type::Mat2:
			element_count = 4;
			break;
		case fx::gltf::Accessor::Type::Mat3:
			element_count = 9;
			break;
		case fx::gltf::Accessor::Type::Mat4:
			element_count = 16;
			break;
		case fx::gltf::Accessor::Type::Scalar:
			element_count = 1;
			break;
		case fx::gltf::Accessor::Type::Vec2:
			element_count = 2;
			break;
		case fx::gltf::Accessor::Type::Vec3:
			element_count = 3;
			break;
		case fx::gltf::Accessor::Type::Vec4:
			element_count = 4;
			break;
		}

		return element_size_bytes * element_count;
	}
}

model model_create_from_gltf(const char* path, std::function<void(const char*, model::material*, std::vector<const char*>*)> on_material_created)
{
	const fx::gltf::ReadQuotas read_quotas_fx = {
		fx::gltf::detail::DefaultMaxBufferCount,
		fx::gltf::detail::DefaultMaxMemoryAllocation * 2,
		fx::gltf::detail::DefaultMaxMemoryAllocation * 2,
	};
	fx::gltf::Document doc_fx = fx::gltf::LoadFromText(path, read_quotas_fx);

	std::vector<const char*> image_paths;

	for (const auto& texture_fx : doc_fx.textures)
	{
		const fx::gltf::Image& image_fx = doc_fx.images[texture_fx.source];

		if (!image_fx.uri.empty() && !image_fx.IsEmbeddedResource())
		{
			image_paths.push_back(image_fx.uri.c_str());
		}
		else
		{
			assert(false);
		}
	}

	std::vector<model::material> materials;
	std::vector<model::mesh> meshes;

	for (const auto& mesh_fx : doc_fx.meshes)
	{
		for (const auto& primitive_gltf : mesh_fx.primitives)
		{
			std::vector<unsigned> indices;
			{
				const auto& index_buffer_accessor_fx = doc_fx.accessors[primitive_gltf.indices];
				const auto& index_buffer_view_fx = doc_fx.bufferViews[index_buffer_accessor_fx.bufferView];
				const auto& index_buffer_fx = doc_fx.buffers[index_buffer_view_fx.buffer];

				indices.resize(index_buffer_accessor_fx.count);

				if (index_buffer_accessor_fx.componentType == fx::gltf::Accessor::ComponentType::UnsignedByte)
				{
					const uint8_t* index = reinterpret_cast<const uint8_t*>(&index_buffer_fx.data[index_buffer_view_fx.byteOffset + index_buffer_accessor_fx.byteOffset]);
					for (auto i = 0; i < indices.size(); ++i)
					{
						indices[i] = *index++;
					}
				}
				else if (index_buffer_accessor_fx.componentType == fx::gltf::Accessor::ComponentType::UnsignedShort)
				{
					const uint16_t* index = reinterpret_cast<const uint16_t*>(&index_buffer_fx.data[index_buffer_view_fx.byteOffset + index_buffer_accessor_fx.byteOffset]);
					for (auto i = 0; i < indices.size(); ++i)
					{
						indices[i] = *index++;
					}
				}
				else if (index_buffer_accessor_fx.componentType == fx::gltf::Accessor::ComponentType::UnsignedInt)
				{
					memcpy(indices.data(), &index_buffer_fx.data[index_buffer_view_fx.byteOffset + index_buffer_accessor_fx.byteOffset], indices.size() * sizeof(unsigned));
				}
				else
				{
					assert(false);
				}
			}

			std::vector<math::vec<3>> positions;
			std::vector<math::vec<3>> normals;
			std::vector<math::vec<4>> tangents;
			std::vector<math::vec<2>> texcoords;

			// TODO: How to handle models that are missing expected vertex attributes?
			for (const auto& attrib_fx : primitive_gltf.attributes)
			{
				if (attrib_fx.first == "POSITION")
				{
					const auto& position_buffer_accessor_fx = doc_fx.accessors[attrib_fx.second];
					const auto& position_buffer_view_fx = doc_fx.bufferViews[position_buffer_accessor_fx.bufferView];
					const auto& position_buffer_fx = doc_fx.buffers[position_buffer_view_fx.buffer];

					positions.resize(position_buffer_accessor_fx.count);

					memcpy(
						positions.data(), 
						&position_buffer_fx.data[position_buffer_view_fx.byteOffset + position_buffer_accessor_fx.byteOffset], 
						detail::get_stride_bytes(position_buffer_accessor_fx) * position_buffer_accessor_fx.count);
				}
				else if (attrib_fx.first == "NORMAL")
				{
					const auto& normal_buffer_accessor_fx = doc_fx.accessors[attrib_fx.second];
					const auto& normal_buffer_view_fx = doc_fx.bufferViews[normal_buffer_accessor_fx.bufferView];
					const auto& normal_buffer_fx = doc_fx.buffers[normal_buffer_view_fx.buffer];

					normals.resize(normal_buffer_accessor_fx.count);

					memcpy(
						normals.data(), 
						&normal_buffer_fx.data[normal_buffer_view_fx.byteOffset + normal_buffer_accessor_fx.byteOffset],
						detail::get_stride_bytes(normal_buffer_accessor_fx) * normal_buffer_accessor_fx.count);
				}
				else if (attrib_fx.first == "TANGENT")
				{
					const auto& tangent_buffer_accessor_fx = doc_fx.accessors[attrib_fx.second];
					const auto& tangent_buffer_view_fx = doc_fx.bufferViews[tangent_buffer_accessor_fx.bufferView];
					const auto& tangent_buffer_fx = doc_fx.buffers[tangent_buffer_view_fx.buffer];

					tangents.resize(tangent_buffer_accessor_fx.count);

					memcpy(
						tangents.data(), 
						&tangent_buffer_fx.data[tangent_buffer_view_fx.byteOffset + tangent_buffer_accessor_fx.byteOffset],
						detail::get_stride_bytes(tangent_buffer_accessor_fx) * tangent_buffer_accessor_fx.count);
				}
				else if (attrib_fx.first == "TEXCOORD_0")
				{
					const auto& texcoord_buffer_accessor_fx = doc_fx.accessors[attrib_fx.second];
					const auto& texcoord_buffer_view_fx = doc_fx.bufferViews[texcoord_buffer_accessor_fx.bufferView];
					const auto& texcoord_buffer_fx = doc_fx.buffers[texcoord_buffer_view_fx.buffer];

					texcoords.resize(texcoord_buffer_accessor_fx.count);

					memcpy(
						texcoords.data(), 
						&texcoord_buffer_fx.data[texcoord_buffer_view_fx.byteOffset + texcoord_buffer_accessor_fx.byteOffset],
						detail::get_stride_bytes(texcoord_buffer_accessor_fx) * texcoord_buffer_accessor_fx.count);
				}
			}

			// TODO: Handle missing required required vertex attributes
			if (positions.empty() || texcoords.empty() || normals.empty())
			{
				continue;
			}

			struct vertex
			{
				math::vec<3> position;
				math::vec<3> normal;
				math::vec<2> texcoord;
				math::vec<4> color;
			};

			std::vector<vertex> vertices;
			vertices.reserve(indices.size());

			for (const auto& index : indices)
			{
				vertices.push_back({ positions[index], normals[index], texcoords[index], { 1.0f, 1.0f, 1.0f, 1.0f } });
			}

			model::material material;
			{
				const fx::gltf::Material& material_fx = doc_fx.materials[primitive_gltf.material];

				strcpy(material.name, material_fx.name.c_str());
				material.base_color_texture_index = material_fx.pbrMetallicRoughness.baseColorTexture.index;
				material.metalness_roughness_texture_index = material_fx.pbrMetallicRoughness.metallicRoughnessTexture.index;
				material.base_color_factor = material_fx.pbrMetallicRoughness.baseColorFactor;
				material.metalness_factor = material_fx.pbrMetallicRoughness.metallicFactor;
				material.roughness_factor = material_fx.pbrMetallicRoughness.roughnessFactor;
				material.double_sided = material_fx.doubleSided;

				if (on_material_created) on_material_created(material_fx.name.c_str(), &material, &image_paths);
			}
			materials.push_back(material);

			model::mesh mesh;
			{
				mesh.vertex_buffer_handle = sp_vertex_buffer_create(mesh_fx.name.c_str(), { static_cast<int>(vertices.size() * sizeof(vertex)), static_cast<int>(sizeof(vertex)) });
				sp_vertex_buffer_update(mesh.vertex_buffer_handle, vertices.data(), static_cast<int>(vertices.size() * sizeof(vertex)));
				mesh.vertex_count = static_cast<int>(vertices.size());
			}
			meshes.push_back(mesh);
		}
	}

	std::vector<sp_texture_handle> textures;
	textures.reserve(image_paths.size() + 1 /* Plus one because we always add default below */);

	// Load all references
	for (const auto& image_path : image_paths)
	{
		std::string image_path_with_root = fx::gltf::detail::GetDocumentRootPath(path) + "/" + std::string(image_path);

		int image_width, image_height, image_channels;
		stbi_uc* image_data = stbi_load(image_path_with_root.c_str(), &image_width, &image_height, &image_channels, STBI_rgb_alpha);
		assert(image_data);

		sp_texture_handle texture_handle = sp_texture_create(image_path, { image_width, image_height, 1, sp_texture_format::r8g8b8a8 });
		sp_texture_update(texture_handle, image_data, image_width * image_height * STBI_rgb_alpha, 4);

		stbi_image_free(image_data);

		textures.push_back(texture_handle);
	}

	// Fill in any still missing textures with a default
	for (auto& material : materials)
	{
		if (material.base_color_texture_index < 0)
		{
			material.base_color_texture_index = static_cast<int>(textures.size());
		}

		if (material.metalness_roughness_texture_index < 0)
		{
			material.metalness_roughness_texture_index = static_cast<int>(textures.size());
		}
	}

	textures.push_back(sp_texture_defaults_white());

	model model;

	model.meshes = std::move(meshes);
	model.materials = std::move(materials);
	model.textures = std::move(textures);

	return model;
}

// TODO: Not sure how this should be handled. A render pass includes render targets which are part of the PSO.
// So it seems like we would need to create PSO for each material in this render pass. On the fly? Or do we just
// allow a finite number of combinations and build them all up front.
struct sp_render_pass_desc
{
	sp_texture_handle render_target_handles[4];
	sp_texture_handle depth_stencil_buffer_handle;
};

/*

	sp_render_pass render_pass;
	sp_material material;
	
	// ...
	sp_graphics_pipeline_handle handle = sp_graphics_pipeline_lookup(render_pass.state, material.state);
	if (!handle)
	{
		handle = sp_graphics_pipeline_create( {render_pass.state, material.state } );
	}

	sp_graphics_command_list_set_pipeline_state(handle);
*/

void* sp_image_create_from_file(const char* filename)
{
	int image_width, image_height, image_channels;
	stbi_uc* image_data = stbi_load(filename, &image_width, &image_height, &image_channels, STBI_rgb_alpha);
	assert(image_data);

	return image_data;
}

void* sp_image_create_from_file_hdr(const char* filename)
{
	int image_width, image_height, image_channels;
	float* image_data = stbi_loadf(filename, &image_width, &image_height, &image_channels, STBI_rgb_alpha);
	assert(image_data);

	return image_data;
}

void* sp_image_volume_create_from_directory(const char* dirname, const char* format, int size)
{
	const int volume_width = size;
	const int volume_height = size;
	const int volume_depth = size;

	const int image_slice_size_bytes = volume_width * volume_height * 4; // TODO: Assuming 4bpp
	const int image_volume_size_bytes = image_slice_size_bytes * volume_depth;

	uint8_t* image_volume_data = static_cast<uint8_t*>(malloc(image_volume_size_bytes));

	for (int i = 0; i < volume_depth; ++i) 
	{
		char filename[512];
		int len = sprintf(filename, "%s/", dirname);
		sprintf(filename + len, format, i);

		int image_slice_width, image_slice_height, image_slice_channels;
		stbi_uc* image_slice_data = stbi_load(filename, &image_slice_width, &image_slice_height, &image_slice_channels, STBI_rgb_alpha);
		assert(image_slice_data);
		assert(image_slice_width == volume_width);
		assert(image_slice_height == volume_height);

		memcpy(image_volume_data + (image_slice_size_bytes * i), image_slice_data, image_slice_size_bytes);

		stbi_image_free(image_slice_data);
	}

	return image_volume_data;
}

int main()
{
	const int window_width = 1280;
	const int window_height = 720;
	const float aspect_ratio = window_width / static_cast<float>(window_height);

	camera camera{ { 0, 0, 10 }, {0, 0, 0} };

	input input{ 0 };

	sp_window_event_set_key_down_callback(
		[](void* user_data, char key) {
			static_cast<input::state*>(user_data)->keys[key] = true;

			// TODO: Ugh. Would like debug_gui integration to be automatic.
			int button = 0;
			if (key == WM_LBUTTONDOWN) button = 0;
			if (key == WM_RBUTTONDOWN) button = 1;
			if (key == WM_MBUTTONDOWN) button = 2;
			ImGui::GetIO().MouseDown[button] = true;
			ImGui::GetIO().KeysDown[key] = true;
		},
		&input.current);

	sp_window_event_set_key_up_callback(
		[](void* user_data, char key) {
			static_cast<input::state*>(user_data)->keys[key] = false;

			// TODO: Ugh. Would like debug_gui integration to be automatic.
			int button = 0;
			if (key == WM_LBUTTONUP) button = 0;
			if (key == WM_RBUTTONUP) button = 1;
			if (key == WM_MBUTTONUP) button = 2;
			ImGui::GetIO().MouseDown[button] = false;
			ImGui::GetIO().KeysDown[key] = false;
		},
		&input.current);

	sp_window_event_set_mouse_move_callback(
		[](void* user_data, int x, int y) {
			static_cast<input::state*>(user_data)->mouse.is_in_client_area = true;
			static_cast<input::state*>(user_data)->mouse.x = x;
			static_cast<input::state*>(user_data)->mouse.y = y;
		},
		&input.current);

	sp_window_event_set_char_callback(
		[](void* user_data, char key) {
			if (ImGui::GetCurrentContext())
			{
				ImGui::GetIO().AddInputCharacter(key);
			}
		},
		nullptr);

	sp_window_event_set_resize_callback(
		[](void* user_data, int width, int height) {
			if (ImGui::GetCurrentContext())
			{
				ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
			}
		},
		nullptr
	);

	sp_window window = sp_window_create("demo", { window_width, window_height });

	sp_init(window);

	int back_buffer_index = _sp._swap_chain->GetCurrentBackBufferIndex();
	int frame_num = 0;

	const void* cloud_shape_image_data = sp_image_volume_create_from_directory("textures/cloud_shape", "cloud_shape.%d.tga", 128);
	sp_texture_handle cloud_shape_texture_handle = sp_texture_create("cloud_shape", { 128, 128, 128, sp_texture_format::r8g8b8a8 });
	sp_texture_update(cloud_shape_texture_handle, cloud_shape_image_data, 128 * 128 * 128 * 4, 4);

	const void* cloud_detail_image_data = sp_image_volume_create_from_directory("textures/cloud_detail", "cloud_detail.%d.tga", 32);
	sp_texture_handle cloud_detail_texture_handle = sp_texture_create("cloud_detail", { 32, 32, 32, sp_texture_format::r8g8b8a8 });
	sp_texture_update(cloud_detail_texture_handle, cloud_detail_image_data, 32 * 32 * 32 * 4, 4);

	const void* cloud_weather_image_data = sp_image_create_from_file("textures/cloud_weather.tga");
	sp_texture_handle cloud_weather_texture_handle = sp_texture_create("cloud_weather", { 512, 512, 1, sp_texture_format::r8g8b8a8 });
	sp_texture_update(cloud_weather_texture_handle, cloud_weather_image_data, 512 * 512 * 4, 4);

	const void* environment_diffuse_image_data = sp_image_create_from_file_hdr("environments/Factory_Catwalk/Factory_Catwalk_Env.hdr");
	sp_texture_handle environment_diffuse_texture = sp_texture_create("environment_diffuse", { 512, 256, 1, sp_texture_format::r32g32b32a32 });

	const void* environment_specular_image_data = sp_image_create_from_file_hdr("environments/Factory_Catwalk/Factory_Catwalk_2k.hdr");
	sp_texture_handle environment_specular_texture = sp_texture_create("environment_specular", { 2048, 1024, 1, sp_texture_format::r32g32b32a32 });
	sp_texture_update(environment_specular_texture, environment_specular_image_data, 2048 * 1024 * 4 * 4, 4 * 4);

	sp_vertex_shader_handle gbuffer_vertex_shader_handle = sp_vertex_shader_create({ "shaders/gbuffer.hlsl" });
	sp_pixel_shader_handle gbuffer_pixel_shader_handle = sp_pixel_shader_create({ "shaders/gbuffer.hlsl" });

	// TODO: The pipeline state should be part of the material. Not sure how we'll make the association though.
	sp_graphics_pipeline_state_handle gbuffer_single_sided_pipeline_state_handle = sp_graphics_pipeline_state_create("gbuffer_single_sided", {
		gbuffer_vertex_shader_handle,
		gbuffer_pixel_shader_handle,
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT }
		},
		{
			sp_texture_format::r10g10b10a2,
			sp_texture_format::r10g10b10a2,
			sp_texture_format::r10g10b10a2,
		},
		sp_texture_format::d32,
		sp_rasterizer_cull_face::back,
		});

	sp_graphics_pipeline_state_handle gbuffer_double_sided_pipeline_state_handle = sp_graphics_pipeline_state_create("gbuffer_double_sided", {
		gbuffer_vertex_shader_handle,
		gbuffer_pixel_shader_handle,
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT }
		},
		{
			sp_texture_format::r10g10b10a2,
			sp_texture_format::r10g10b10a2,
			sp_texture_format::r10g10b10a2,
		},
		sp_texture_format::d32,
		sp_rasterizer_cull_face::none,
		});

	sp_compute_shader_handle low_freq_noise_shader_handle = sp_compute_shader_create({ "shaders/low_freq_noise.hlsl" });

	sp_compute_pipeline_state_handle low_freq_noise_pipeline_state_handle = sp_compute_pipeline_state_create("low_freq_noise", { low_freq_noise_shader_handle });

	sp_vertex_shader_handle clouds_vertex_shader_handle = sp_vertex_shader_create({ "shaders/clouds.hlsl" });
	sp_pixel_shader_handle clouds_pixel_shader_handle = sp_pixel_shader_create({ "shaders/clouds.hlsl" });

	sp_graphics_pipeline_state_handle clouds_pipeline_state_handle = sp_graphics_pipeline_state_create("clouds", {
		clouds_vertex_shader_handle,
		clouds_pixel_shader_handle,
		{},
		{
			sp_texture_format::r10g10b10a2,
		},
		});

	sp_vertex_shader_handle lighting_vertex_shader_handle = sp_vertex_shader_create({ "shaders/lighting.hlsl" });
	sp_pixel_shader_handle lighting_pixel_shader_handle = sp_pixel_shader_create({ "shaders/lighting.hlsl" });

	sp_graphics_pipeline_state_handle lighting_pipeline_state_handle = sp_graphics_pipeline_state_create("lighting", {
		lighting_vertex_shader_handle,
		lighting_pixel_shader_handle,
		{},
		{
			sp_texture_format::r10g10b10a2,
		},
		});

	// TODO: Be nicer if I could assign handles to the material. Better if I could not write completely custom callbacks. Maybe some rules like:
	// material_override.create("MyMaterial").base_color_factor("Name", [](color* value) { value->r = 0; }).metalness(1.0).roughness(mul(3));
	auto shader_ball_material_loaded_callback = [](const char* material_name, model::material* material, std::vector<const char*>* image_paths)  {
		if (strcmp(material_name, "Inside") == 0)
		{
			material->metalness_factor = 1.0;
			material->roughness_factor = 1.0f;
			material->base_color_factor = { 1.0f, 0.0f, 0.0f, 1.0f };
		}
		else if (strcmp(material_name, "Outside") == 0)
		{
			material->metalness_factor = 1.0;
			material->roughness_factor = 0.0f;
			material->base_color_factor = { 0.0f, 1.0f, 0.0f, 1.0f };
		}
		else if (strcmp(material_name, "Base") == 0)
		{
			material->metalness_factor = 0.0;
			material->roughness_factor = 1.0f;
			material->base_color_factor = { 0.0f, 0.0f, 1.0f, 1.0f };
		}

		material->metalness_factor = 1.0;
		material->roughness_factor = 0.0f;
		material->base_color_factor = { 1.0f, 0.0f, 0.0f, 1.0f };
	};

	std::vector<std::pair<model, math::mat<4>>> entities{
		//{ model_create_from_gltf("models/littlest_tokyo/scene.gltf", nullptr), math::create_rotation_x(-math::pi_div_2) },
		//{ model_create_from_gltf("models/smashy_craft_city/scene.gltf", nullptr), math::create_rotation_x(0) },
		//model_create_from_gltf("models/MetalRoughSpheres/MetalRoughSpheres.gltf", sp_texture_defaults_white(), sp_texture_defaults_white()),
		//model_create_from_gltf("models/TextureCoordinateTest/TextureCoordinateTest.gltf", sp_texture_defaults_white(), sp_texture_defaults_white()),
		//model_create_cube(sp_texture_defaults_checkerboard(), sp_texture_defaults_white()),
		{ model_create_from_gltf("models/shader_ball/shader_ball.gltf", shader_ball_material_loaded_callback), math::create_rotation_x(math::pi_div_2) },
	};

	__declspec(align(16)) struct
	{
		math::mat<4> view_matrix;
		math::mat<4> projection_matrix;
		math::mat<4> view_projection_matrix;
		math::mat<4> inverse_view_matrix;
		math::mat<4> inverse_projection_matrix;
		math::mat<4> inverse_view_projection_matrix;
		math::vec<3> camera_position_ws;
		float dummy;
		// TODO: Not sure if I want this in scene constants or a seprate lighting constants
		math::vec<3> sun_direction_ws;
		float dummy2;

	} constant_buffer_per_frame_data;

	constant_buffer_clouds_per_frame_data clouds_per_frame_data;

	sp_constant_buffer_heap constant_buffer_heap = sp_constant_buffer_heap_create("constant_buffer_heap", { 32 * 1024 });

	sp_graphics_command_list graphics_command_list = sp_graphics_command_list_create("graphics_command_list", {});
	sp_compute_command_list compute_command_list = sp_compute_command_list_create("compute_command_list", {});

	sp_texture_handle gbuffer_base_color_texture_handle = sp_texture_create("gbuffer_base_color", { window_width, window_height, 1, sp_texture_format::r10g10b10a2 });
	sp_texture_handle gbuffer_metalness_roughness_texture_handle = sp_texture_create("gbuffer_metalness_roughness", { window_width, window_height, 1, sp_texture_format::r10g10b10a2 });
	sp_texture_handle gbuffer_normals_texture_handle = sp_texture_create("gbuffer_normals", { window_width, window_height, 1, sp_texture_format::r10g10b10a2 });
	sp_texture_handle gbuffer_depth_texture_handle = sp_texture_create("gbuffer_depth", { window_width, window_height, 1, sp_texture_format::d32 });

	sp_render_pass_desc gbuffer_render_pass_desc = {
		{
			gbuffer_base_color_texture_handle,
			gbuffer_metalness_roughness_texture_handle,
			gbuffer_normals_texture_handle
		},
		gbuffer_depth_texture_handle
	};

	math::vec<3> sun_direction_ws = math::normalize<3>({ 0.25f, -1.0f, -0.5f });

	while (sp_window_poll())
	{
		detail::sp_debug_gui_begin_frame();

		camera_update(&camera, input);

		if (input.current.keys[VK_LEFT])
		{
			sun_direction_ws = math::transform_vector(math::create_rotation_y(0.1f), sun_direction_ws);
		}
		if (input.current.keys[VK_RIGHT])
		{
			sun_direction_ws = math::transform_vector(math::create_rotation_y(-0.1f), sun_direction_ws);
		}
		if (input.current.keys[VK_UP])
		{
			sun_direction_ws = math::transform_vector(math::create_rotation_x(0.1f), sun_direction_ws);
		}
		if (input.current.keys[VK_DOWN])
		{
			sun_direction_ws = math::transform_vector(math::create_rotation_x(-0.1f), sun_direction_ws);
		}

		sp_descriptor_handle constant_buffer_per_frame;
		{
#ifdef JITTER_MATRIX
			const math::vec<2> jitter_sample_pattern[] = {
				{ -1.0f, 1.0f },
				{ 1.0f, 1.0f },
				{ -1.0f, -1.0f },
				{ 1.0f, -1.0f },
			};
			const int jitter_sample_pattern_index = (frame_num / 100) % std::size(jitter_sample_pattern);
			const float jitter_offset = 0.01f;
			const math::mat<4> jitter_matrix = math::create_translation({ jitter_sample_pattern[jitter_sample_pattern_index] * jitter_offset, 0.0f });
#else
			const math::mat<4> jitter_matrix = math::create_identity<4>();
#endif

			const math::mat<4> camera_transform = camera_get_transform(camera);
			const math::mat<4> view_matrix = math::inverse(camera_transform);
			const math::mat<4> projection_matrix = math::create_perspective_fov_rh(math::pi / 3, aspect_ratio, 0.1f, 10000.0f);
			const math::mat<4> view_projection_matrix = math::multiply(view_matrix, projection_matrix) * jitter_matrix;

			constant_buffer_per_frame_data.view_matrix = view_matrix;
			constant_buffer_per_frame_data.projection_matrix = projection_matrix;
			constant_buffer_per_frame_data.view_projection_matrix = view_projection_matrix;
			constant_buffer_per_frame_data.inverse_view_matrix = math::inverse(view_matrix);
			constant_buffer_per_frame_data.inverse_projection_matrix = math::inverse(projection_matrix);
			constant_buffer_per_frame_data.inverse_view_projection_matrix = math::inverse(view_projection_matrix);
			constant_buffer_per_frame_data.camera_position_ws = camera.position;
			constant_buffer_per_frame_data.sun_direction_ws = sun_direction_ws;

			constant_buffer_per_frame = sp_constant_buffer_alloc(&constant_buffer_heap, sizeof(constant_buffer_per_frame_data), &constant_buffer_per_frame_data);
		}

		sp_descriptor_handle constant_buffer_per_frame_clouds;
		{
			constant_buffer_per_frame_clouds = sp_constant_buffer_alloc(&constant_buffer_heap, sizeof(constant_buffer_clouds_per_frame_data), &clouds_per_frame_data);
		}

		// Record all the commands we need to render the scene into the command list.
		{
			// TODO: This could probably be done automatically somewhere.
			graphics_command_list._command_list_d3d12->SetGraphicsRootSignature(_sp._root_signature.Get());
			compute_command_list._command_list_d3d12->SetComputeRootSignature(_sp._root_signature.Get());

			// TODO: The call to SetDescriptorHeaps is expensive. Only want to do once per command list. Only do it automatically when starting new command list.
			ID3D12DescriptorHeap* descriptor_heaps[] = { _sp._descriptor_heap_cbv_srv_uav_gpu._heap_d3d12.Get() };
			graphics_command_list._command_list_d3d12->SetDescriptorHeaps(static_cast<unsigned>(std::size(descriptor_heaps)), descriptor_heaps);

			sp_graphics_command_list_set_viewport(graphics_command_list, { 0.0f, 0.0f, window_width, window_height });
			sp_graphics_command_list_set_scissor_rect(graphics_command_list, { 0, 0, window_width, window_height });

			{
				sp_compute_command_list_set_pipeline_state(compute_command_list, low_freq_noise_pipeline_state_handle);
				sp_compute_command_list_dispatch(compute_command_list, 8, 8, 8);
			}

			// gbuffer
			{
				sp_texture_handle gbuffer_render_target_handles[] = {
					gbuffer_base_color_texture_handle,
					gbuffer_metalness_roughness_texture_handle,
					gbuffer_normals_texture_handle
				};
				sp_graphics_command_list_set_render_targets(graphics_command_list, gbuffer_render_target_handles, static_cast<int>(std::size(gbuffer_render_target_handles)), gbuffer_depth_texture_handle);

				sp_graphics_command_list_clear_render_target(graphics_command_list, gbuffer_base_color_texture_handle);
				sp_graphics_command_list_clear_render_target(graphics_command_list, gbuffer_metalness_roughness_texture_handle);
				sp_graphics_command_list_clear_render_target(graphics_command_list, gbuffer_normals_texture_handle);
				sp_graphics_command_list_clear_depth(graphics_command_list, gbuffer_depth_texture_handle);

				graphics_command_list._command_list_d3d12->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				for (const std::pair<model, math::mat<4>>& entity : entities)
				{
					// C++17
					// const auto& [model, transform] = entity;

					const model& model = entity.first;
					const math::mat<4>& transform = entity.second;

					for (int i = 0; i < static_cast<int>(model.meshes.size()); ++i)
					{
						sp_descriptor_handle constant_buffer_per_object;
						{
							__declspec(align(16)) struct constant_buffer_per_object_data
							{
								math::mat<4> world_matrix;
								math::vec<4> base_color_factor;
								math::vec<4> metalness_roughness_factor;
							};

							constant_buffer_per_object_data per_object_data{
								transform,
								{ model.materials[i].base_color_factor[0], model.materials[i].base_color_factor[1], model.materials[i].base_color_factor[2], model.materials[i].base_color_factor[3] },
								{ model.materials[i].metalness_factor, model.materials[i].roughness_factor, 0.0f, 0.0f }
							};

							constant_buffer_per_object = sp_constant_buffer_alloc(&constant_buffer_heap, sizeof(constant_buffer_per_object_data), &per_object_data);
						}

						// TODO: We should probably be sorting based on some concept of material / pipeline state so we're not setting this for every draw.
						if (model.materials[i].double_sided)
						{
							sp_graphics_command_list_set_pipeline_state(graphics_command_list, gbuffer_double_sided_pipeline_state_handle);
						}
						else
						{
							sp_graphics_command_list_set_pipeline_state(graphics_command_list, gbuffer_single_sided_pipeline_state_handle);
						}

						// TODO: The descriptors could all be baked per-draw call (if not going to adopt bindless)
						// Copy SRV
						graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
						sp_descriptor_copy_to_heap(
							&_sp._descriptor_heap_cbv_srv_uav_gpu,
							{
								detail::sp_texture_pool_get(model.textures[model.materials[i].base_color_texture_index])._shader_resource_view,
								detail::sp_texture_pool_get(model.textures[model.materials[i].metalness_roughness_texture_index])._shader_resource_view,
							});
						// Copy CBV
						graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(1, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
						sp_descriptor_copy_to_heap(
							&_sp._descriptor_heap_cbv_srv_uav_gpu,
							{
								constant_buffer_per_frame,
								constant_buffer_per_object
							});

						sp_graphics_command_list_set_vertex_buffers(graphics_command_list, &model.meshes[i].vertex_buffer_handle, 1);
						sp_graphics_command_list_draw_instanced(graphics_command_list, model.meshes[i].vertex_count, 1);
					}
				}
			}

			/*
			// clouds
			{
				sp_graphics_command_list_set_pipeline_state(graphics_command_list, clouds_pipeline_state_handle);

				sp_texture_handle clouds_render_target_handles[] = {
					_sp._back_buffer_texture_handles[back_buffer_index]
				};
				sp_graphics_command_list_set_render_targets(graphics_command_list, clouds_render_target_handles, static_cast<int>(std::size(clouds_render_target_handles)), {});

				// Copy SRV
				graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head( _sp._descriptor_heap_cbv_srv_uav_gpu )._handle_gpu_d3d12);
				sp_descriptor_copy_to_heap(
					&_sp._descriptor_heap_cbv_srv_uav_gpu,
					{
						detail::sp_texture_pool_get(gbuffer_depth_texture_handle)._shader_resource_view,
						detail::sp_texture_pool_get(cloud_shape_texture_handle)._shader_resource_view,
						detail::sp_texture_pool_get(cloud_detail_texture_handle)._shader_resource_view,
						detail::sp_texture_pool_get(cloud_weather_texture_handle)._shader_resource_view,
					});
				// Copy CBV
				graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(1, sp_descriptor_heap_get_head( _sp._descriptor_heap_cbv_srv_uav_gpu )._handle_gpu_d3d12);
				sp_descriptor_copy_to_heap(
					&_sp._descriptor_heap_cbv_srv_uav_gpu,
					{
						constant_buffer_per_frame,
						constant_buffer_per_frame_clouds,
					});
				sp_graphics_command_list_draw_instanced(graphics_command_list, 3, 1);
			}*/
			
			// lighting
			{
				sp_graphics_command_list_set_pipeline_state(graphics_command_list, lighting_pipeline_state_handle);

				sp_texture_handle lighting_render_target_handles[] = {
					_sp._back_buffer_texture_handles[back_buffer_index]
				};
				sp_graphics_command_list_set_render_targets(graphics_command_list, lighting_render_target_handles, static_cast<int>(std::size(lighting_render_target_handles)), {});

				// Copy SRV
				graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
				sp_descriptor_copy_to_heap(
					&_sp._descriptor_heap_cbv_srv_uav_gpu,
					{
						detail::sp_texture_pool_get(gbuffer_base_color_texture_handle)._shader_resource_view,
						detail::sp_texture_pool_get(gbuffer_metalness_roughness_texture_handle)._shader_resource_view,
						detail::sp_texture_pool_get(gbuffer_normals_texture_handle)._shader_resource_view,
						detail::sp_texture_pool_get(gbuffer_depth_texture_handle)._shader_resource_view,
						detail::sp_texture_pool_get(environment_specular_texture)._shader_resource_view,
					});

				// Copy CBV
				graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(1, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
				sp_descriptor_copy_to_heap(
					&_sp._descriptor_heap_cbv_srv_uav_gpu,
					{
						constant_buffer_per_frame,
					});

				sp_graphics_command_list_draw_instanced(graphics_command_list, 3, 1);
			}

			//sp_debug_gui_show_demo_window();
			bool open = true;
			int window_flags = 0;
			ImGui::Begin("Materials", &open, window_flags);
			for (auto& entity : entities)
			{
				ImGui::PushID(&entity);

				for (int i = 0; i < entity.first.materials.size(); ++i)
				{
					model::material& material = entity.first.materials[i];

					ImGui::PushID(&material);
					ImGui::CollapsingHeader(material.name);
					ImGui::ColorEdit3("Base Color", material.base_color_factor.data());
					ImGui::DragFloat("Metalness", &material.metalness_factor, 0.01f, 0.0f, 1.0f);
					ImGui::DragFloat("Roughness", &material.roughness_factor, 0.01f, 0.0f, 1.0f);
					ImGui::PopID();
				}
				ImGui::PopID();
			}
			ImGui::End();
			
			ImGui::Begin("Camera", &open, window_flags);
			ImGui::Text("Position: %.1f, %.1f, %.1f", camera.position.x, camera.position.y, camera.position.z);
			ImGui::Text("Rotation: %.1f, %.1f, %.1f", camera.rotation.x, camera.rotation.y, camera.rotation.z);
			// TODO: Something is not right here e.g. <0,-1,0> is up
			ImGui::Text("Forward:  %.3f, %.3f, %.3f", math::get_forward(camera_get_transform(camera)).x, math::get_forward(camera_get_transform(camera)).y, math::get_forward(camera_get_transform(camera)).z);

			ImGui::End();
			ImGui::Begin("Clouds", &open, window_flags);
			ImGui::DragFloat("Sampling Scale Bias (Weather)", &clouds_per_frame_data.weather_sample_scale_bias, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Sampling Scale Bias (Density)", &clouds_per_frame_data.shape_sample_scale_bias, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Sampling Scale Bias (Detail)", &clouds_per_frame_data.detail_sample_scale_bias, 0.01f, -1.0f, 1.0f);
			// ImGui::DragFloat("Density", &clouds_per_frame_data.density_bias, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Coverage Bias", &clouds_per_frame_data.coverage_bias, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Type Bias", &clouds_per_frame_data.type_bias, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Shape Bias (Base)", &clouds_per_frame_data.shape_base_bias, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Shape Bias (Erosion)", &clouds_per_frame_data.shape_base_erosion_bias, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Detail Bias (Base)", &clouds_per_frame_data.shape_detail_bias, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Detail Bias (Erosion)", &clouds_per_frame_data.shape_detail_erosion_bias, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Absorption", &clouds_per_frame_data.absorption_factor, 0.001f, 0.0f, 1.0f);
			ImGui::DragFloat("Scattering", &clouds_per_frame_data.scattering_factor, 0.001f, 0.0f, 1.0f);
			ImGui::DragFloat("Cloud Layer (Begin)", &clouds_per_frame_data.cloud_layer_height_begin);
			ImGui::DragFloat("Cloud Layer (End)", &clouds_per_frame_data.cloud_layer_height_end);
			ImGui::DragFloat("Step Size (Cloud)", &clouds_per_frame_data.step_size_ws);
			ImGui::DragFloat("Step Size (Shadow)", &clouds_per_frame_data.shadow_step_size_ws);
			ImGui::DragFloat("Light (Ambient)", &clouds_per_frame_data.ambient_light);
			ImGui::DragFloat("Light (Sun)", &clouds_per_frame_data.sun_light);
			ImGui::DragInt("Debug Volume Raymarch", &clouds_per_frame_data.debug_raymarch_termination, 1.0f, 0, 1);
			ImGui::DragInt("Debug Shadow Raymarch", &clouds_per_frame_data.debug_shadow_raymarch_termination, 1.0f, 0, 1);
			ImGui::DragInt("Debug Shadow Raymarch (Step)", &clouds_per_frame_data.debug_shadow_raymarch_termination_step, 1.0f, 0, 128);
			ImGui::DragInt("Debug Toggle Frostbite Scattering", &clouds_per_frame_data.debug_toggle_frostbite_scattering, 1.0f, 0, 1);
			ImGui::DragFloat("Debug0", &clouds_per_frame_data.debug0, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Debug1", &clouds_per_frame_data.debug1, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat("Debug2", &clouds_per_frame_data.debug2, 100.01f,  0.0f, 10000.0f);
			ImGui::DragFloat("Debug3", &clouds_per_frame_data.debug3, 100.01f,  0.0f, 10000.0f);
			ImGui::End();

			// TODO: This is dumb. The debug gui should probably share the same heap as the rest of our scene but for now we need a separate one
			// since the default imgui implementation expects to be the only one using it.
			ID3D12DescriptorHeap* descriptor_heaps2[] = { _sp._descriptor_heap_debug_gui_gpu._heap_d3d12.Get() };
			graphics_command_list._command_list_d3d12->SetDescriptorHeaps(static_cast<unsigned>(std::size(descriptor_heaps2)), descriptor_heaps2);

			detail::sp_debug_gui_record_draw_commands(graphics_command_list);

			sp_graphics_command_list_close(graphics_command_list);
			sp_compute_command_list_close(compute_command_list);

			sp_descriptor_heap_reset(&_sp._descriptor_heap_cbv_srv_uav_gpu);
			sp_descriptor_heap_reset(&_sp._descriptor_heap_cbv_srv_uav_cpu_transient);
		}

		sp_graphics_queue_execute(graphics_command_list);

		sp_swap_chain_present();

		// TODO: Need to double buffer command lists/heaps/etc so we can start recording next frame immediately.
		sp_device_wait_for_idle();

		sp_graphics_command_list_reset(graphics_command_list);
		sp_compute_command_list_reset(compute_command_list);

		sp_constant_buffer_heap_reset(&constant_buffer_heap);

		back_buffer_index = _sp._swap_chain->GetCurrentBackBufferIndex();

		++frame_num;

		input_update(&input);
	}

	sp_device_wait_for_idle();

	sp_shutdown();

	return 0;
}