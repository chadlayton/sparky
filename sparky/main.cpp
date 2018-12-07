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

#include "command_list_impl.h"
#include "constant_buffer_impl.h"
#include "pipeline_impl.h"
#include "texture_impl.h"
#include "vertex_buffer_impl.h"
#include "shader_impl.h"
#include "descriptor_impl.h"

#include <string>
#include <cassert>
#include <codecvt>
#include <array>
#include <vector>
#include <utility>

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

	const float movement_speed_mod = input.current.keys[VK_SHIFT] ? 10.0f : 1.0f;

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
		camera->position += math::get_forward(camera_transform) * 0.1f * movement_speed_mod;
	}
	if (input.current.keys['S'])
	{
		camera->position -= math::get_forward(camera_transform) * 0.1f * movement_speed_mod;
	}

	if (input.current.keys[VK_LBUTTON] && input.current.mouse.is_in_client_area && input.previous.mouse.is_in_client_area)
	{
		camera->rotation.y -= (input.current.mouse.x - input.previous.mouse.x) * 0.01f;
		camera->rotation.x += (input.current.mouse.y - input.previous.mouse.y) * 0.01f;
	}
}

struct model
{
	struct mesh
	{
		int vertex_count;
		sp_vertex_buffer_handle vertex_buffer_handle;
		int base_color_texture_index;
		int metalness_roughness_texture_index;
	};

	std::vector<mesh> meshes;
	std::vector<sp_texture_handle> textures;
};

model model_create_cube(sp_texture_handle base_color_texture_handle)
{
	model model;

	model.textures.push_back(base_color_texture_handle);

	model::mesh mesh;

	mesh.base_color_texture_index = 0;
	mesh.metalness_roughness_texture_index = 0;

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
		{ { -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		{ { -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },

		// Back
		{ { -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		{ { 0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		{ { 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },

		// Left
		{ { -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },

		// Right
		{ { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { 0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		{ { 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { 0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },

		// Top
		{ { -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		{ { -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },

		// Bottom
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		{ { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
		{ { 0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	};

	mesh.vertex_buffer_handle = sp_vertex_buffer_create("cube", { sizeof(cube_vertices), sizeof(cube_vertex) });
	sp_vertex_buffer_update(mesh.vertex_buffer_handle, cube_vertices, sizeof(cube_vertices));

	mesh.vertex_count = static_cast<int>(std::size(cube_vertices));

	model.meshes.push_back(mesh);

	return model;
}

namespace detail
{
	int get_accessor_stride_bytes(const fx::gltf::Accessor& accessor)
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

	template <typename T>
	const std::vector<T> get_buffer_data(const fx::gltf::Document& doc, const fx::gltf::Accessor& accessor)
	{
		const auto& buffer_view = doc.bufferViews[accessor.bufferView];
		const auto& buffer = doc.buffers[buffer_view.buffer];

		const int stride_bytes = get_accessor_stride_bytes(accessor);

		assert(stride_bytes == sizeof(T));
		assert(buffer_view.byteStride == 0 || buffer_view.byteStride == sizeof(T));

		std::vector<T> data(accessor.count);

		std::memcpy(data.data(), &buffer.data[buffer_view.byteOffset + accessor.byteOffset], accessor.count * stride_bytes);

		return data;
	}
}

model model_create_from_gltf(const char* path)
{
	model model;

	fx::gltf::Document doc_fx = fx::gltf::LoadFromText(path);

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
			
			sp_texture_handle texture_handle = sp_texture_create(image_fx.name.c_str(), { image_width, image_height, sp_texture_format::r8g8b8a8 });
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
			std::vector<unsigned> indices = detail::get_buffer_data<unsigned>(doc_fx, doc_fx.accessors[primitive_gltf.indices]);

			std::vector<math::vec<3>> positions;
			std::vector<math::vec<3>> normals;
			std::vector<math::vec<4>> tangents;
			std::vector<math::vec<2>> texcoords;

			// TODO: How to handle models that are missing expected vertex attributes?
			for (const auto& attrib_fx : primitive_gltf.attributes)
			{
				if (attrib_fx.first == "POSITION")
				{
					positions = detail::get_buffer_data<math::vec<3>>(doc_fx, doc_fx.accessors[attrib_fx.second]);
				}
				else if (attrib_fx.first == "NORMAL")
				{
					normals = detail::get_buffer_data<math::vec<3>>(doc_fx, doc_fx.accessors[attrib_fx.second]);
				}
				else if (attrib_fx.first == "TANGENT")
				{
					tangents = detail::get_buffer_data<math::vec<4>>(doc_fx, doc_fx.accessors[attrib_fx.second]);
				}
				else if (attrib_fx.first == "TEXCOORD_0")
				{
					texcoords = detail::get_buffer_data<math::vec<2>>(doc_fx, doc_fx.accessors[attrib_fx.second]);
				}
			}

			if (texcoords.empty())
			{
				continue;
			}

			math::vec<4> color = { static_cast <float> (rand()) / static_cast <float> (RAND_MAX), static_cast <float> (rand()) / static_cast <float> (RAND_MAX), static_cast <float> (rand()) / static_cast <float> (RAND_MAX), 1.0f };

			struct vertex
			{
				math::vec<3> position;
				math::vec<3> normal;
				math::vec<2> texcoord;
				math::vec<4> color;
			};

			std::vector<vertex> vertices(indices.size());

			for (const auto& index : indices)
			{
				vertices.push_back({ positions[index], normals[index], texcoords[index], color });
			}

			model::mesh mesh;
			
			mesh.vertex_buffer_handle = sp_vertex_buffer_create(mesh_fx.name.c_str(), { static_cast<int>(vertices.size() * sizeof(vertex)), static_cast<int>(sizeof(vertex)) });
			sp_vertex_buffer_update(mesh.vertex_buffer_handle, vertices.data(), static_cast<int>(vertices.size() * sizeof(vertex)));
			mesh.vertex_count = static_cast<int>(vertices.size());
			mesh.base_color_texture_index = doc_fx.materials[primitive_gltf.material].pbrMetallicRoughness.baseColorTexture.index;
			//mesh.metalness_roughness_texture_index = doc_fx.materials[primitive_gltf.material].pbrMetallicRoughness.metallicRoughnessTexture.index;
			// TODO: Sketchfab exporter put metal/roughness into the occlusionTexture slot. Fix asset.
			mesh.metalness_roughness_texture_index = doc_fx.materials[primitive_gltf.material].occlusionTexture.index;

			// TODO: How to handle models that are missing expected textures?
			if (mesh.base_color_texture_index < 0 || mesh.metalness_roughness_texture_index < 0)
			{
				continue;
			}

			model.meshes.push_back(mesh);
		}
	}

	return model;
}

int main()
{
	const int window_width = 1280;
	const int window_height = 720;
	const float aspect_ratio = window_width / static_cast<float>(window_height);

	camera camera{ { 0, 0, -10 }, {0, 0, 0} };

	input input{ 0 };

	sp_window_event_set_key_down_callback(
		[](void* user_data, char key) {
			static_cast<input::state*>(user_data)->keys[key] = true;
		},
		&input.current);

	sp_window_event_set_key_up_callback(
		[](void* user_data, char key) {
			static_cast<input::state*>(user_data)->keys[key] = false;
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

	int frame_index = _sp._swap_chain->GetCurrentBackBufferIndex();

	sp_vertex_shader_handle gbuffer_vertex_shader_handle = sp_vertex_shader_create({ "gbuffer.hlsl" });
	sp_pixel_shader_handle gbuffer_pixel_shader_handle = sp_pixel_shader_create({ "gbuffer.hlsl" });

	sp_graphics_pipeline_state_handle gbuffer_pipeline_state_handle = sp_graphics_pipeline_state_create("gbuffer", {
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
		sp_texture_format::d32
	});

	//model scene = model_create_from_gltf("littlest_tokyo/scene.gltf");

	std::vector<uint8_t> checkerboard_big_image_data = sp_image_checkerboard_data_create(1024, 1024);
	sp_texture_handle checkerboard_big_texture_handle = sp_texture_create("checkerboard_big", { 1024, 1024, sp_texture_format::r8g8b8a8 });
	sp_texture_update(checkerboard_big_texture_handle, &checkerboard_big_image_data[0], static_cast<int>(checkerboard_big_image_data.size()));

	std::vector<uint8_t> checkerboard_small_image_data = sp_image_checkerboard_data_create(128, 128);
	sp_texture_handle checkerboard_small_texture_handle = sp_texture_create("checkerboard_small", { 128, 128, sp_texture_format::r8g8b8a8 });
	sp_texture_update(checkerboard_small_texture_handle, &checkerboard_small_image_data[0], static_cast<int>(checkerboard_small_image_data.size()));

	model cube = model_create_cube(checkerboard_small_texture_handle);

	sp_vertex_shader_handle lighting_vertex_shader_handle = sp_vertex_shader_create({ "lighting.hlsl" });
	sp_pixel_shader_handle lighting_pixel_shader_handle = sp_pixel_shader_create({ "lighting.hlsl" });

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

	} constant_buffer_per_frame_data;

	__declspec(align(16)) struct
	{
		math::mat<4> world_matrix;

	} constant_buffer_per_object_data;

	sp_constant_buffer_handle constant_buffer_per_frame_handle = sp_constant_buffer_create("per_frame", { sizeof(constant_buffer_per_frame_data) });
	sp_constant_buffer_handle constant_buffer_per_object_handle = sp_constant_buffer_create("per_object", { sizeof(constant_buffer_per_object_data) });

	sp_graphics_command_list command_list = sp_graphics_command_list_create("main", {});

	sp_texture_handle gbuffer_base_color_texture_handle = sp_texture_create("gbuffer_base_color", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_metalness_roughness_texture_handle = sp_texture_create("gbuffer_metalness_roughness", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_normals_texture_handle = sp_texture_create("gbuffer_normals", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_depth_texture_handle = sp_texture_create("gbuffer_depth", { window_width, window_height, sp_texture_format::d32 });

	while (sp_window_poll())
	{
		camera_update(&camera, input);

		{
			const math::mat<4> camera_transform = camera_get_transform(camera);
			const math::mat<4> view_matrix = math::inverse(camera_transform);
			const math::mat<4> projection_matrix = math::create_perspective_fov_lh(math::pi / 2, aspect_ratio, 0.1f, 10000.0f);
			const math::mat<4> view_projection_matrix = math::multiply(view_matrix, projection_matrix);
			const math::mat<4> inverse_view_projection_matrix = math::inverse(view_projection_matrix);

			constant_buffer_per_frame_data.projection_matrix = projection_matrix;
			constant_buffer_per_frame_data.view_projection_matrix = view_projection_matrix;
			constant_buffer_per_frame_data.inverse_view_projection_matrix = math::inverse(view_projection_matrix);

			sp_constant_buffer_update(constant_buffer_per_frame_handle, &constant_buffer_per_frame_data, sizeof(constant_buffer_per_frame_data));
		}

		// Record all the commands we need to render the scene into the command list.
		{
			command_list._command_list_d3d12->SetGraphicsRootSignature(_sp._root_signature.Get());

			// TODO: The call to SetDescriptorHeaps is expensive. Only want to do once per command list. Only do it automatically when starting new command list.
			ID3D12DescriptorHeap* descriptor_heaps[] = { _sp._descriptor_heap_cbv_srv_uav_gpu._heap_d3d12.Get() };
			command_list._command_list_d3d12->SetDescriptorHeaps(static_cast<unsigned>(std::size(descriptor_heaps)), descriptor_heaps);

			sp_graphics_command_list_set_viewport(command_list, { 0.0f, 0.0f, window_width, window_height });
			sp_graphics_command_list_set_scissor_rect(command_list, { 0, 0, window_width, window_height });

			// gbuffer
			{
				command_list._command_list_d3d12->SetPipelineState(sp_graphics_pipeline_state_get_impl(gbuffer_pipeline_state_handle));

				sp_texture_handle gbuffer_render_target_handles[] = {
					gbuffer_base_color_texture_handle,
					gbuffer_metalness_roughness_texture_handle,
					gbuffer_normals_texture_handle
				};
				sp_graphics_command_list_set_render_targets(command_list, gbuffer_render_target_handles, static_cast<int>(std::size(gbuffer_render_target_handles)), gbuffer_depth_texture_handle);

				sp_graphics_command_list_clear_render_target(command_list, gbuffer_base_color_texture_handle, { 1.0f, 0.0f, 0.0f, 0.0f });
				sp_graphics_command_list_clear_render_target(command_list, gbuffer_metalness_roughness_texture_handle, { 0.0f, 0.0f, 0.0f, 0.0f });
				sp_graphics_command_list_clear_render_target(command_list, gbuffer_normals_texture_handle, { 0.0f, 0.0f, 0.0f, 0.0f });
				sp_graphics_command_list_clear_depth(command_list, gbuffer_depth_texture_handle, 1.0f);

				command_list._command_list_d3d12->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				for (const auto& mesh : cube.meshes)
				{
					{
						const math::mat<4> world_matrix = math::create_identity<4>();
						constant_buffer_per_object_data.world_matrix = world_matrix;

						sp_constant_buffer_update(constant_buffer_per_object_handle, &constant_buffer_per_object_data, sizeof(constant_buffer_per_object_data));
					}

					// XXX: This could all be baked per-draw call
					// Copy SRV
					command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(cube.textures[mesh.base_color_texture_index])._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(cube.textures[mesh.metalness_roughness_texture_index])._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					// Copy CBV
					command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(1, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_frame_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_object_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					sp_graphics_command_list_set_vertex_buffers(command_list, &mesh.vertex_buffer_handle, 1);
					command_list._command_list_d3d12->DrawInstanced(mesh.vertex_count, 1, 0, 0);
				}
			}

			// lighting
			{
				command_list._command_list_d3d12->SetPipelineState(sp_graphics_pipeline_state_get_impl(lighting_pipeline_state_handle));

				sp_texture_handle lighting_render_target_handles[] = {
					_sp._back_buffer_texture_handles[frame_index]
				};
				sp_graphics_command_list_set_render_targets(command_list, lighting_render_target_handles, static_cast<int>(std::size(lighting_render_target_handles)), {});

				// Copy SRV
				command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
				_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_base_color_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_normals_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_depth_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				// Copy CBV
				command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(1, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
				_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_frame_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				command_list._command_list_d3d12->DrawInstanced(3, 1, 0, 0);
			}

			sp_graphics_command_list_close(command_list);

			sp_descriptor_heap_reset(&_sp._descriptor_heap_cbv_srv_uav_gpu);
		}

		sp_graphics_queue_execute(command_list);

		sp_swap_chain_present();

		sp_device_wait_for_idle();

		sp_graphics_command_list_reset(command_list);

		frame_index = _sp._swap_chain->GetCurrentBackBufferIndex();

		input_update(&input);
	}

	sp_device_wait_for_idle();

	sp_shutdown();

	return 0;
}