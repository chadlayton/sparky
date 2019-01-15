#define SP_DEBUG_RESOURCE_NAMING_ENABLED 1 
#define SP_DEBUG_RENDERDOC_HOOK_ENABLED 1

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

model model_create_from_gltf(const char* path, sp_texture_handle base_color_texture_handle, sp_texture_handle metalness_roughness_texture_handle)
{
	model model;

	const fx::gltf::ReadQuotas read_quotas_fx = {
		fx::gltf::detail::DefaultMaxBufferCount,
		fx::gltf::detail::DefaultMaxMemoryAllocation * 2,
		fx::gltf::detail::DefaultMaxMemoryAllocation * 2,
	};
	fx::gltf::Document doc_fx = fx::gltf::LoadFromText(path, read_quotas_fx);

	model.textures.reserve(doc_fx.textures.size());

	for (const auto& texture_fx : doc_fx.textures)
	{
		std::vector<uint8_t> image_data;

		const fx::gltf::Image& image_fx = doc_fx.images[texture_fx.source];

		if (!image_fx.uri.empty() && !image_fx.IsEmbeddedResource())
		{
			std::string image_path = fx::gltf::detail::GetDocumentRootPath(path) + "/" + image_fx.uri;
			
			int image_width, image_height, image_channels;
			stbi_uc* image_data = stbi_load(image_path.c_str(), &image_width, &image_height, &image_channels, STBI_rgb_alpha);
			assert(image_data);
			
			sp_texture_handle texture_handle = sp_texture_create(image_fx.uri.c_str(), { image_width, image_height, sp_texture_format::r8g8b8a8 });
			sp_texture_update(texture_handle, image_data, image_width * image_height * STBI_rgb_alpha);
			
			stbi_image_free(image_data);

			model.textures.push_back(texture_handle);
		}
		else
		{
			assert(false);
		}
	}

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

				material.base_color_texture_index = material_fx.pbrMetallicRoughness.baseColorTexture.index;
				material.metalness_roughness_texture_index = material_fx.pbrMetallicRoughness.metallicRoughnessTexture.index;
				material.base_color_factor = material_fx.pbrMetallicRoughness.baseColorFactor;
				material.metalness_factor = material_fx.pbrMetallicRoughness.metallicFactor;
				material.roughness_factor = material_fx.pbrMetallicRoughness.roughnessFactor;
				material.double_sided = material_fx.doubleSided;
			}
			model.materials.push_back(material);

			model::mesh mesh;
			{
				mesh.vertex_buffer_handle = sp_vertex_buffer_create(mesh_fx.name.c_str(), { static_cast<int>(vertices.size() * sizeof(vertex)), static_cast<int>(sizeof(vertex)) });
				sp_vertex_buffer_update(mesh.vertex_buffer_handle, vertices.data(), static_cast<int>(vertices.size() * sizeof(vertex)));
				mesh.vertex_count = static_cast<int>(vertices.size());
			}

			model.meshes.push_back(mesh);
		}
	}

	// Fill in any missing materials with the defaults
	for (auto& material : model.materials)
	{
		if (material.base_color_texture_index < 0)
		{
			material.base_color_texture_index = static_cast<int>(model.textures.size());
		}
	}

	model.textures.push_back(base_color_texture_handle);

	for (auto& material : model.materials)
	{
		if (material.metalness_roughness_texture_index < 0)
		{
			material.metalness_roughness_texture_index = static_cast<int>(model.textures.size());
		}
	}

	model.textures.push_back(metalness_roughness_texture_handle);

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
		},
		&input.current);

	sp_window_event_set_mouse_move_callback(
		[](void* user_data, int x, int y) {
			static_cast<input::state*>(user_data)->mouse.is_in_client_area = true;
			static_cast<input::state*>(user_data)->mouse.x = x;
			static_cast<input::state*>(user_data)->mouse.y = y;
		},
		&input.current);

	sp_window window = sp_window_create("demo", { window_width, window_height });

	sp_init(window);

	int back_buffer_index = _sp._swap_chain->GetCurrentBackBufferIndex();
	int frame_num = 0;

	sp_vertex_shader_handle gbuffer_vertex_shader_handle = sp_vertex_shader_create({ "shaders/gbuffer.hlsl" });
	sp_pixel_shader_handle gbuffer_pixel_shader_handle = sp_pixel_shader_create({ "shaders/gbuffer.hlsl" });
	sp_compute_shader_handle low_freq_noise_shader_handle = sp_compute_shader_create({ "shaders/low_freq_noise.hlsl" });

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
			sp_texture_format::r8g8b8a8,
			sp_texture_format::r8g8b8a8,
			sp_texture_format::r8g8b8a8,
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
			sp_texture_format::r8g8b8a8,
			sp_texture_format::r8g8b8a8,
			sp_texture_format::r8g8b8a8,
		},
		sp_texture_format::d32,
		sp_rasterizer_cull_face::none,
	});

	sp_compute_pipeline_state_handle low_freq_noise_pipeline_state_handle = sp_compute_pipeline_state_create("low_freq_noise", { low_freq_noise_shader_handle });

	//model scene = model_create_from_gltf("models/littlest_tokyo/scene.gltf", sp_texture_defaults_white(), sp_texture_defaults_white());
	//model scene = model_create_from_gltf("models/smashy_craft_city/scene.gltf", sp_texture_defaults_white(), sp_texture_defaults_white());
	model scene = model_create_from_gltf("models/MetalRoughSpheres/MetalRoughSpheres.gltf", sp_texture_defaults_white(), sp_texture_defaults_white());
	//model scene = model_create_from_gltf("models/TextureCoordinateTest/TextureCoordinateTest.gltf", sp_texture_defaults_white(), sp_texture_defaults_white());

	model cube = model_create_cube(sp_texture_defaults_checkerboard(), sp_texture_defaults_white());

	sp_vertex_shader_handle lighting_vertex_shader_handle = sp_vertex_shader_create({ "shaders/lighting.hlsl" });
	sp_pixel_shader_handle lighting_pixel_shader_handle = sp_pixel_shader_create({ "shaders/lighting.hlsl" });

	sp_graphics_pipeline_state_handle lighting_pipeline_state_handle = sp_graphics_pipeline_state_create("lighting", {
		lighting_vertex_shader_handle,
		lighting_pixel_shader_handle,
		{},
		{
			sp_texture_format::r8g8b8a8,
		},
	});

	__declspec(align(16)) struct
	{
		math::mat<4> projection_matrix;
		math::mat<4> view_projection_matrix;
		math::mat<4> inverse_view_projection_matrix;
		math::vec<3> camera_position_ws;
		float dummy;
		// TODO: Not sure if I want this in scene constants or a seprate lighting constants
		math::vec<3> sun_direction_ws;

	} constant_buffer_per_frame_data;

	__declspec(align(16)) struct
	{
		math::mat<4> world_matrix;

	} constant_buffer_per_object_data;

	sp_constant_buffer_handle constant_buffer_per_frame_handle = sp_constant_buffer_create("per_frame", { sizeof(constant_buffer_per_frame_data) });
	sp_constant_buffer_handle constant_buffer_per_object_handle = sp_constant_buffer_create("per_object", { sizeof(constant_buffer_per_object_data) });

	sp_graphics_command_list graphics_command_list = sp_graphics_command_list_create("main", {});
	sp_compute_command_list compute_command_list = sp_compute_command_list_create("main", {});

	sp_texture_handle gbuffer_base_color_texture_handle = sp_texture_create("gbuffer_base_color", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_metalness_roughness_texture_handle = sp_texture_create("gbuffer_metalness_roughness", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_normals_texture_handle = sp_texture_create("gbuffer_normals", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_depth_texture_handle = sp_texture_create("gbuffer_depth", { window_width, window_height, sp_texture_format::d32 });

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
			const math::mat<4> inverse_view_projection_matrix = math::inverse(view_projection_matrix);

			constant_buffer_per_frame_data.projection_matrix = projection_matrix;
			constant_buffer_per_frame_data.view_projection_matrix = view_projection_matrix;
			constant_buffer_per_frame_data.inverse_view_projection_matrix = math::inverse(view_projection_matrix);
			constant_buffer_per_frame_data.camera_position_ws = camera.position;
			constant_buffer_per_frame_data.sun_direction_ws = sun_direction_ws;

			sp_constant_buffer_update(constant_buffer_per_frame_handle, &constant_buffer_per_frame_data, sizeof(constant_buffer_per_frame_data));
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
				compute_command_list._command_list_d3d12->SetPipelineState(sp_compute_pipeline_state_get_impl(low_freq_noise_pipeline_state_handle));
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

				for (int i = 0; i < scene.meshes.size(); ++i)
				{
					{
						const math::mat<4> world_matrix = math::create_identity<4>();

						constant_buffer_per_object_data.world_matrix = world_matrix;

						sp_constant_buffer_update(constant_buffer_per_object_handle, &constant_buffer_per_object_data, sizeof(constant_buffer_per_object_data));
					}

					// TODO: We should probably be sorting based on some concept of material / pipeline state so we're not setting this for every draw.
					if (scene.materials[i].double_sided)
					{
						graphics_command_list._command_list_d3d12->SetPipelineState(sp_graphics_pipeline_state_get_impl(gbuffer_double_sided_pipeline_state_handle));
					}
					else
					{
						graphics_command_list._command_list_d3d12->SetPipelineState(sp_graphics_pipeline_state_get_impl(gbuffer_single_sided_pipeline_state_handle));
					}

					// TODO: The descriptors could all be baked per-draw call (if not going to adopt bindless)
					// Copy SRV
					graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(scene.textures[scene.materials[i].base_color_texture_index])._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(scene.textures[scene.materials[i].metalness_roughness_texture_index])._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					// Copy CBV
					graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(1, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_frame_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_object_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					sp_graphics_command_list_set_vertex_buffers(graphics_command_list, &scene.meshes[i].vertex_buffer_handle, 1);
					sp_graphics_command_list_draw_instanced(graphics_command_list, scene.meshes[i].vertex_count, 1);
				}

				for (int i = 0; i < cube.meshes.size(); ++i)
				{
					{
						const math::mat<4> world_matrix = math::create_identity<4>();
						constant_buffer_per_object_data.world_matrix = world_matrix;

						sp_constant_buffer_update(constant_buffer_per_object_handle, &constant_buffer_per_object_data, sizeof(constant_buffer_per_object_data));
					}

					// TODO: We should probably be sorting based on some concept of material / pipeline state so we're not setting this for every draw.
					if (cube.materials[i].double_sided)
					{
						graphics_command_list._command_list_d3d12->SetPipelineState(sp_graphics_pipeline_state_get_impl(gbuffer_double_sided_pipeline_state_handle));
					}
					else
					{
						graphics_command_list._command_list_d3d12->SetPipelineState(sp_graphics_pipeline_state_get_impl(gbuffer_single_sided_pipeline_state_handle));
					}

					// TODO: The descriptors could all be baked per-draw call (if not going to adopt bindless)
					// Copy SRV
					graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(cube.textures[cube.materials[i].base_color_texture_index])._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(cube.textures[cube.materials[i].metalness_roughness_texture_index])._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					// Copy CBV
					graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(1, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_frame_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_object_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					sp_graphics_command_list_set_vertex_buffers(graphics_command_list, &cube.meshes[i].vertex_buffer_handle, 1);
					sp_graphics_command_list_draw_instanced(graphics_command_list, cube.meshes[i].vertex_count, 1);
				}
			}

			// lighting
			{
				graphics_command_list._command_list_d3d12->SetPipelineState(sp_graphics_pipeline_state_get_impl(lighting_pipeline_state_handle));

				sp_texture_handle lighting_render_target_handles[] = {
					_sp._back_buffer_texture_handles[back_buffer_index]
				};
				sp_graphics_command_list_set_render_targets(graphics_command_list, lighting_render_target_handles, static_cast<int>(std::size(lighting_render_target_handles)), {});

				// Copy SRV
				graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
				_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_base_color_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_metalness_roughness_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_normals_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_depth_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				// Copy CBV
				graphics_command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(1, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
				_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_frame_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				sp_graphics_command_list_draw_instanced(graphics_command_list, 3, 1);
			}

			//sp_debug_gui_show_demo_window();
			//bool open = true;
			//int window_flags = 0;
			//ImGui::Begin("Sparky", &open, window_flags);
			//ImGui::InputFloat3("Sun Direction", sun_direction_ws.data, 1);
			//ImGui::End();

			// TODO: This is dumb. The debug gui should probably share the same heap as the rest of our scene but for now we need a separate one
			// since the default imgui implementation expects to be the only one using it.
			ID3D12DescriptorHeap* descriptor_heaps2[] = { _sp._descriptor_heap_debug_gui_gpu._heap_d3d12.Get() };
			graphics_command_list._command_list_d3d12->SetDescriptorHeaps(static_cast<unsigned>(std::size(descriptor_heaps2)), descriptor_heaps2);

			detail::sp_debug_gui_record_draw_commands(graphics_command_list);

			sp_graphics_command_list_close(graphics_command_list);
			sp_compute_command_list_close(compute_command_list);

			sp_descriptor_heap_reset(&_sp._descriptor_heap_cbv_srv_uav_gpu);
		}

		sp_graphics_queue_execute(graphics_command_list);

		sp_swap_chain_present();

		sp_device_wait_for_idle();

		sp_graphics_command_list_reset(graphics_command_list);
		sp_compute_command_list_reset(compute_command_list);

		back_buffer_index = _sp._swap_chain->GetCurrentBackBufferIndex();

		++frame_num;

		input_update(&input);
	}

	sp_device_wait_for_idle();

	sp_shutdown();

	return 0;
}