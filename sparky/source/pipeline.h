#pragma once

#include "handle.h"
#include "texture.h"
#include "shader.h"

#define NOMINMAX
#include <d3d12.h>

#include <wrl.h>

struct sp_input_element_desc
{
	const char* _semantic_name;
	unsigned _semantic_index;
	DXGI_FORMAT _format;
};

enum class sp_rasterizer_cull_face
{
	front,
	back,
	none,
};

enum class sp_rasterizer_fill_mode
{
	solid,
	wireframe
};

struct sp_graphics_pipeline_state_desc
{
	sp_vertex_shader_handle vertex_shader_handle;
	sp_pixel_shader_handle pixel_shader_handle;
	sp_input_element_desc input_layout[D3D12_STANDARD_VERTEX_ELEMENT_COUNT];
	sp_texture_format render_target_formats[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	sp_texture_format depth_stencil_format = sp_texture_format::unknown;
	sp_rasterizer_cull_face cull_face = sp_rasterizer_cull_face::back;
	sp_rasterizer_fill_mode fill_mode = sp_rasterizer_fill_mode::solid;
};

struct sp_compute_pipeline_state_desc
{
	sp_compute_shader_handle compute_shader_handle;
};

struct sp_graphics_pipeline_state
{
	const char* _name;
	sp_graphics_pipeline_state_desc _desc;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _impl;
};

struct sp_compute_pipeline_state
{
	const char* _name;
	sp_compute_pipeline_state_desc _desc;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _impl;
};

using sp_graphics_pipeline_state_handle = sp_handle;
using sp_compute_pipeline_state_handle = sp_handle;

namespace detail
{
	void sp_graphics_pipeline_state_pool_create();
	sp_graphics_pipeline_state& sp_graphics_pipeline_state_pool_get(const sp_graphics_pipeline_state_handle& pipeline_handle);
	void sp_graphics_pipeline_state_pool_destroy();

	void sp_compute_pipeline_state_pool_destroy();
	sp_compute_pipeline_state& sp_compute_pipeline_state_pool_get(const sp_compute_pipeline_state_handle& pipeline_handle);
	void sp_compute_pipeline_state_pool_create();
}

sp_graphics_pipeline_state_handle sp_graphics_pipeline_state_create(const char* name, const sp_graphics_pipeline_state_desc& desc);
void sp_graphics_pipeline_state_destroy(const sp_graphics_pipeline_state_handle& pipeline_state_handle);

sp_compute_pipeline_state_handle sp_compute_pipeline_state_create(const char* name, const sp_compute_pipeline_state_desc& desc);
void sp_compute_pipeline_state_destroy(const sp_graphics_pipeline_state_handle& pipeline_state_handle);