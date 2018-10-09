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

	if (input.current.keys['A'])
	{
		camera->position -= math::get_right(camera_transform) * 0.1f;
	}
	if (input.current.keys['D'])
	{
		camera->position += math::get_right(camera_transform) * 0.1f;
	}
	if (input.current.keys['W'])
	{
		camera->position += math::get_forward(camera_transform) * 0.1f;
	}
	if (input.current.keys['S'])
	{
		camera->position -= math::get_forward(camera_transform) * 0.1f;
	}

	if (input.current.keys[VK_LBUTTON] && input.current.mouse.is_in_client_area && input.previous.mouse.is_in_client_area)
	{
		camera->rotation.y -= (input.current.mouse.x - input.previous.mouse.x) * 0.01f;
		camera->rotation.x += (input.current.mouse.y - input.previous.mouse.y) * 0.01f;
	}
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
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R8G8B8A8_UNORM,
		},
		sp_texture_format::d32
		});

	sp_vertex_shader_handle lighting_vertex_shader_handle = sp_vertex_shader_create({ "lighting.hlsl" });
	sp_pixel_shader_handle lighting_pixel_shader_handle = sp_pixel_shader_create({ "lighting.hlsl" });

	sp_graphics_pipeline_state_handle lighting_pipeline_state_handle = sp_graphics_pipeline_state_create("lighting", {
		lighting_vertex_shader_handle,
		lighting_pixel_shader_handle,
		{},
		{
			DXGI_FORMAT_R8G8B8A8_UNORM,
		},
		});

	__declspec(align(16)) struct
	{
		math::mat<4> projection_matrix;
		math::mat<4> view_projection_matrix;
		math::mat<4> inverse_view_projection_matrix;

	} constant_buffer_per_frame_data;

	sp_constant_buffer_handle constant_buffer_per_frame_handle = sp_constant_buffer_create("per_frame", { sizeof(constant_buffer_per_frame_data) });

	sp_graphics_command_list command_list = sp_graphics_command_list_create("main", { gbuffer_pipeline_state_handle });

	std::vector<uint8_t> checkerboard_big_image_data = sp_image_checkerboard_data_create(1024, 1024);
	sp_texture_handle checkerboard_big_texture_handle = sp_texture_create("checkerboard_big", { 1024, 1024, sp_texture_format::r8g8b8a8 });
	sp_texture_update(checkerboard_big_texture_handle, &checkerboard_big_image_data[0], static_cast<int>(checkerboard_big_image_data.size()));

	std::vector<uint8_t> checkerboard_small_image_data = sp_image_checkerboard_data_create(128, 128);
	sp_texture_handle checkerboard_small_texture_handle = sp_texture_create("checkerboard_small", { 128, 128, sp_texture_format::r8g8b8a8 });
	sp_texture_update(checkerboard_small_texture_handle, &checkerboard_small_image_data[0], static_cast<int>(checkerboard_small_image_data.size()));

	sp_texture_handle gbuffer_base_color_texture_handle = sp_texture_create("gbuffer_base_color", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_normals_texture_handle = sp_texture_create("gbuffer_normals", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_depth_texture_handle = sp_texture_create("gbuffer_depth", { window_width, window_height, sp_texture_format::d32 });

	sp_vertex_buffer_handle triangle_vertex_buffer_handle;
	{
		struct vertex
		{
			math::vec<3> position;
			math::vec<3> normal;
			math::vec<2> texcoord;
			math::vec<4> color;
		};

		// XXX: There's no model/world transform yet so vertices are in world space
		vertex triangle_vertices[] =
		{
			{ { 0.0f, 5.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 5.0f, -5.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -5.0f, -5.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		triangle_vertex_buffer_handle = sp_vertex_buffer_create("triangle", { sizeof(triangle_vertices), sizeof(vertex) });
		sp_vertex_buffer_update(triangle_vertex_buffer_handle, triangle_vertices, sizeof(triangle_vertices));
	}

	while (sp_window_poll())
	{
		camera_update(&camera, input);

		{
			const math::mat<4> camera_transform = camera_get_transform(camera);
			const math::mat<4> view_matrix = math::inverse(camera_transform);
			const math::mat<4> projection_matrix = math::create_perspective_fov_lh(math::pi / 2, aspect_ratio, 0.1f, 100.0f);
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

			sp_texture_handle gbuffer_render_target_handles[] = {
				gbuffer_base_color_texture_handle,
				gbuffer_normals_texture_handle
			};
			sp_graphics_command_list_set_render_targets(command_list, gbuffer_render_target_handles, static_cast<int>(std::size(gbuffer_render_target_handles)), gbuffer_depth_texture_handle);

			sp_graphics_command_list_clear_render_target(command_list, gbuffer_base_color_texture_handle, { 0.0f, 0.0f, 0.0f, 0.0f });
			sp_graphics_command_list_clear_depth(command_list, gbuffer_depth_texture_handle, 1.0f);

			// XXX: This could all be baked per-draw call
			command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
			// Copy SRV
			_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(checkerboard_small_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			// Increment to start of CBV range
			sp_descriptor_heap_advance(&_sp._descriptor_heap_cbv_srv_uav_gpu, 11);
			// Copy CBV
			_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_frame_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			command_list._command_list_d3d12->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			sp_graphics_command_list_set_vertex_buffers(command_list, &triangle_vertex_buffer_handle, 1);
			command_list._command_list_d3d12->DrawInstanced(3, 1, 0, 0);

			command_list._command_list_d3d12->SetPipelineState(sp_graphics_pipeline_state_get_impl(lighting_pipeline_state_handle));

			sp_texture_handle lighting_render_target_handles[] = {
				_sp._back_buffer_texture_handles[frame_index]
			};
			sp_graphics_command_list_set_render_targets(command_list, lighting_render_target_handles, static_cast<int>(std::size(lighting_render_target_handles)), {});

			command_list._command_list_d3d12->SetGraphicsRootDescriptorTable(0, sp_descriptor_heap_get_head(_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_gpu_d3d12);
			// Copy SRV
			_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_base_color_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_normals_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, detail::sp_texture_pool_get(gbuffer_depth_texture_handle)._shader_resource_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			// Increment to start of CBV range
			sp_descriptor_heap_advance(&_sp._descriptor_heap_cbv_srv_uav_gpu, 9);
			// Copy CBV
			_sp._device->CopyDescriptorsSimple(1, sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_gpu)._handle_cpu_d3d12, sp_constant_buffer_get_hack(constant_buffer_per_frame_handle)._constant_buffer_view._handle_cpu_d3d12, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			command_list._command_list_d3d12->DrawInstanced(3, 1, 0, 0);

			sp_graphics_command_list_close(command_list);

			sp_descriptor_heap_reset(&_sp._descriptor_heap_cbv_srv_uav_gpu);
		}

		sp_graphics_queue_execute(command_list);

		sp_swap_chain_present();

		sp_device_wait_for_idle();

		sp_graphics_command_list_reset(command_list, gbuffer_pipeline_state_handle);

		frame_index = _sp._swap_chain->GetCurrentBackBufferIndex();

		input_update(&input);
	}

	sp_device_wait_for_idle();

	sp_shutdown();

	return 0;
}