#define SP_HEADER_ONLY 1
#define SP_DEBUG_RESOURCE_NAMING_ENABLED 1 
#define SP_DEBUG_RENDERDOC_HOOK_ENABLED 1
#define SP_DEBUG_API_VALIDATION_LEVEL 0
#define SP_DEBUG_SHUTDOWN_LEAK_REPORT_ENABLED 0
#define SP_DEBUG_LIVE_SHADER_RELOADING 1

// TODO: std::codecvt_utf8_utf16 deprecated in C++17 but no replacement offered...
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <sparky/sparky.h>

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

const void* sp_vertex_data_get_cube(int* size_in_bytes, int* stride_in_bytes)
{
	struct vertex
	{
		math::vec<3> position;
		math::vec<3> normal;
		math::vec<2> texcoord;
		math::vec<4> color;
	};

	static const vertex vertices[] =
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

	*size_in_bytes = sizeof(vertices);
	*stride_in_bytes = sizeof(vertex);

	return &vertices;
}

void* sp_vertex_data_get_plane(int* size_in_bytes, int* stride_in_bytes)
{
	struct vertex
	{
		math::vec<3> position;
	};

	static std::vector<vertex> vertices;


	for (int y = 1; y < 128 - 1; ++y)
	{
		for (int x = 1; x < 128 - 1; ++x)
		{
			vertex v00 = { { static_cast<float>(x), 0.0f, static_cast<float>(y) } };
			vertex v01 = { { static_cast<float>(x), 0.0f, static_cast<float>(y) + 1} };
			vertex v10 = { { static_cast<float>(x) + 1, 0.0f, static_cast<float>(y) } };
			vertex v11 = { { static_cast<float>(x) + 1, 0.0f, static_cast<float>(y) + 1} };

			if ((x & 1) ^ (y & 1))
			{
				vertices.push_back(v00);
				vertices.push_back(v01);
				vertices.push_back(v11);

				vertices.push_back(v00);
				vertices.push_back(v11);
				vertices.push_back(v10);
			}
			else
			{
				vertices.push_back(v00);
				vertices.push_back(v01);
				vertices.push_back(v10);

				vertices.push_back(v01);
				vertices.push_back(v11);
				vertices.push_back(v10);
			}
		}
	}

	*size_in_bytes = sizeof(vertex) * vertices.size();
	*stride_in_bytes = sizeof(vertex);

	return vertices.data();
}

struct sp_material_task
{
	sp_graphics_pipeline_state_handle pipeline_state;

	sp_descriptor_table descriptor_tables[12];
	sp_constant_buffer constant_buffers[12];
};

struct sp_material
{

};

struct sp_mesh
{
	sp_vertex_buffer vertex_buffer;
	sp_material material;
};

struct sp_model
{
	sp_mesh meshes[12];
	int mesh_count;
};

int main()
{
	const int window_width = 1280;
	const int window_height = 720;
	const float aspect_ratio = window_width / static_cast<float>(window_height);

	camera camera{ { 0, 16, 64 }, {0, 0, 0} };

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

	sp_window_event_set_char_callback(
		[](void* user_data, char key) {
		},
		nullptr);

	sp_window_event_set_resize_callback(
		[](void* user_data, int width, int height) {
		},
		nullptr);

	sp_window window = sp_window_create("terrain", { window_width, window_height });

	sp_init(window);

	sp_graphics_pipeline_state_handle terrain_pipeline_state = sp_graphics_pipeline_state_create("terrain", {
		sp_vertex_shader_create({ "shaders/terrain.hlsl" }),
		sp_pixel_shader_create({ "shaders/terrain.hlsl" }),
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
		sp_rasterizer_fill_mode::solid,
	});

	__declspec(align(16)) struct
	{
		math::mat<4> view_matrix;
		math::mat<4> projection_matrix;
		math::mat<4> view_projection_matrix;
		math::mat<4> inverse_view_matrix;
		math::mat<4> inverse_projection_matrix;
		math::mat<4> inverse_view_projection_matrix;
		math::vec<3> camera_position_ws;
		float dummy = 0;

	} constant_buffer_per_frame_data;

	sp_constant_buffer constant_buffer_per_frame = sp_constant_buffer_create(sizeof(constant_buffer_per_frame_data));

	sp_descriptor_handle constant_buffer_descriptors_cbv[] = {
		constant_buffer_per_frame._constant_buffer_view
	};
	sp_descriptor_table descriptor_table_per_frame_cbv = sp_descriptor_table_create(sp_descriptor_table_type::cbv, constant_buffer_descriptors_cbv);

	__declspec(align(16)) struct constant_buffer_per_draw_terrain_data
	{
		math::mat<4> world_matrix;
	};

	int cube_vertices_size_in_bytes = 0;
	int cube_vertices_stride_in_bytes = 0;
	const void* vertex_data_cube = sp_vertex_data_get_plane(&cube_vertices_size_in_bytes, &cube_vertices_stride_in_bytes);
	sp_vertex_buffer_handle vertex_bufffer_terrain = sp_vertex_buffer_create("terrain", { cube_vertices_size_in_bytes, cube_vertices_stride_in_bytes });
	sp_vertex_buffer_update(vertex_bufffer_terrain, vertex_data_cube, cube_vertices_size_in_bytes);

	sp_constant_buffer constant_buffer_per_draw_terrain = sp_constant_buffer_create(sizeof(constant_buffer_per_draw_terrain_data));

	sp_descriptor_handle constant_buffer_descriptors_per_draw_terrain_cbv[] = {
		constant_buffer_per_draw_terrain._constant_buffer_view
	};
	sp_descriptor_table descriptor_table_terrain_per_draw_cbv = sp_descriptor_table_create(sp_descriptor_table_type::cbv, constant_buffer_descriptors_cbv);

	sp_graphics_command_list graphics_command_list = sp_graphics_command_list_create("graphics_command_list", {});

	while (sp_window_poll())
	{
		detail::sp_debug_gui_begin_frame();

		camera_update(&camera, input);

		{
			const math::mat<4> camera_transform = camera_get_transform(camera);
			const math::mat<4> view_matrix = math::inverse(camera_transform);
			const math::mat<4> projection_matrix = math::create_perspective_fov_rh(math::pi / 3, aspect_ratio, 0.1f, 10000.0f);
			const math::mat<4> view_projection_matrix = math::multiply(view_matrix, projection_matrix);

			constant_buffer_per_frame_data.view_matrix = view_matrix;
			constant_buffer_per_frame_data.projection_matrix = projection_matrix;
			constant_buffer_per_frame_data.view_projection_matrix = view_projection_matrix;
			constant_buffer_per_frame_data.inverse_view_matrix = math::inverse(view_matrix);
			constant_buffer_per_frame_data.inverse_projection_matrix = math::inverse(projection_matrix);
			constant_buffer_per_frame_data.inverse_view_projection_matrix = math::inverse(view_projection_matrix);
			constant_buffer_per_frame_data.camera_position_ws = camera.position;

			sp_constant_buffer_update(constant_buffer_per_frame, &constant_buffer_per_frame_data);
		}

		{
			sp_graphics_command_list_begin(graphics_command_list);

			sp_graphics_command_list_set_descriptor_table(graphics_command_list, 1, descriptor_table_per_frame_cbv);

			{
				int width, height;
				sp_window_get_size(window, &width, &height);
				sp_graphics_command_list_set_viewport(graphics_command_list, { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) });
				sp_graphics_command_list_set_scissor_rect(graphics_command_list, { 0, 0, width, height });

				sp_graphics_command_list_clear_render_target(graphics_command_list, detail::_sp._back_buffer_texture_handles[detail::_sp._back_buffer_index]);
			}

			// terrain
			{
				graphics_command_list._command_list_d3d12->BeginEvent(1, "terrain", 8);

				sp_graphics_command_list_set_pipeline_state(graphics_command_list, terrain_pipeline_state);

				sp_texture_handle lighting_render_target_handles[] = {
					detail::_sp._back_buffer_texture_handles[detail::_sp._back_buffer_index]
				};
				sp_graphics_command_list_set_render_targets(graphics_command_list, lighting_render_target_handles, static_cast<int>(std::size(lighting_render_target_handles)), {});

				graphics_command_list._command_list_d3d12->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				constant_buffer_per_draw_terrain_data per_draw_data{
					math::create_identity<4>()
				};

				sp_constant_buffer_update(constant_buffer_per_draw_terrain, &per_draw_data);

				sp_graphics_command_list_set_descriptor_table(graphics_command_list, 3, descriptor_table_terrain_per_draw_cbv);

				sp_graphics_command_list_set_vertex_buffers(graphics_command_list, &vertex_bufffer_terrain, 1);
				sp_graphics_command_list_draw_instanced(graphics_command_list, cube_vertices_size_in_bytes / cube_vertices_stride_in_bytes, 1);

				graphics_command_list._command_list_d3d12->EndEvent();
			}

			{
				bool open = true;
				int window_flags = 0;
				ImGui::Begin("Demo", &open, window_flags);

				if (ImGui::CollapsingHeader("Terrain"))
				{
				}

				ImGui::End();
			}

			detail::sp_debug_gui_record_draw_commands(graphics_command_list);

			sp_graphics_command_list_end(graphics_command_list);
		}

		sp_graphics_queue_execute(graphics_command_list);

		sp_swap_chain_present();

		input_update(&input);

		sp_file_watch_tick();
	}

	sp_device_wait_for_idle();

	sp_shutdown();

	return 0;
}