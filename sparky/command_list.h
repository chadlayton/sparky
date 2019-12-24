#pragma once

#include "handle.h"
#include "pipeline.h"
#include "texture.h"

#include <array>

#include <d3d12.h>
#include <wrl.h>

struct sp_graphics_command_list_desc
{
	sp_graphics_pipeline_state_handle pipeline_state_handle;
};

struct sp_compute_command_list_desc
{
	sp_compute_pipeline_state_handle pipeline_state_handle;
};

struct sp_graphics_command_list
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _command_list_d3d12;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _command_allocator_d3d12;

	D3D12_RESOURCE_BARRIER _resource_transition_records[64];
	int _resource_transition_records_count = 0;
};

struct sp_compute_command_list
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _command_list_d3d12;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _command_allocator_d3d12;
};

struct sp_viewport
{
	float x, y, width, height;
	float depth_min = 0.0f;
	float depth_max = 1.0f;
};

struct sp_scissor_rect
{
	int x, y, width, height;
};

sp_graphics_command_list sp_graphics_command_list_create(const char* name, const sp_graphics_command_list_desc& desc);
void sp_graphics_command_list_begin(sp_graphics_command_list& command_list);
void sp_graphics_command_list_set_vertex_buffers(sp_graphics_command_list& command_list, const sp_vertex_buffer_handle* vertex_buffer_handles, int vertex_buffer_count);
void sp_graphics_command_list_set_render_targets(sp_graphics_command_list& command_list, sp_texture_handle* render_target_handles, int render_target_count, sp_texture_handle depth_stencil_handle);
void sp_graphics_command_list_set_viewport(sp_graphics_command_list& command_list, const sp_viewport& viewport);
void sp_graphics_command_list_set_scissor_rect(sp_graphics_command_list& command_list, const sp_scissor_rect& scissor);
void sp_graphics_command_list_clear_render_target(sp_graphics_command_list& command_list, sp_texture_handle render_target_handle);
void sp_graphics_command_list_clear_depth_stencil(sp_graphics_command_list& command_list, sp_texture_handle depth_stencil_handle);
void sp_graphics_command_list_clear_depth(sp_graphics_command_list& command_list, sp_texture_handle depth_stencil_handle);
void sp_graphics_command_list_clear_stencil(sp_graphics_command_list& command_list, sp_texture_handle depth_stencil_handle);
void sp_graphics_command_list_draw_instanced(sp_graphics_command_list& command_list, int vertex_count, int instance_count);
void sp_graphics_command_list_set_pipeline_state(sp_graphics_command_list& command_list, const sp_graphics_pipeline_state_handle& pipeline_state_handle);
void sp_graphics_command_list_set_descriptor_table(sp_graphics_command_list& command_list, int slot, sp_descriptor_heap table); // TODO: sp_descriptor_table? Includes slot?
void sp_graphics_command_list_end(sp_graphics_command_list& command_list);
void sp_graphics_command_list_destroy(sp_graphics_command_list& command_list);

sp_compute_command_list sp_compute_command_list_create(const char* name, const sp_compute_command_list_desc& desc);
void sp_compute_command_list_begin(sp_compute_command_list& command_list);
void sp_compute_command_list_set_pipeline_state(sp_compute_command_list& command_list, const sp_compute_pipeline_state_handle& pipeline_state_handle);
void sp_compute_command_list_dispatch(sp_compute_command_list& command_list, int thread_group_count_x, int thread_group_count_y, int thread_group_count_z);
void sp_compute_command_list_end(sp_compute_command_list& command_list);