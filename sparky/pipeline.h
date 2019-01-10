#pragma once

#include "handle.h"
#include "texture.h"
#include "shader.h"

#include <wrl.h>

#include <d3d12.h>

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

struct sp_graphics_pipeline_state_desc
{
	sp_vertex_shader_handle vertex_shader_handle;
	sp_pixel_shader_handle pixel_shader_handle;
	sp_input_element_desc input_layout[D3D12_STANDARD_VERTEX_ELEMENT_COUNT];
	sp_texture_format render_target_formats[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	sp_texture_format depth_stencil_format = sp_texture_format::unknown;
	sp_rasterizer_cull_face cull_face = sp_rasterizer_cull_face::back;
};

struct sp_graphics_pipeline_state
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _impl;
};

using sp_graphics_pipeline_state_handle = sp_handle;

namespace detail
{
	void sp_graphics_pipeline_state_pool_create();
	void sp_graphics_pipeline_state_pool_destroy();
}

sp_graphics_pipeline_state_handle sp_graphics_pipeline_state_create(const char* name, const sp_graphics_pipeline_state_desc& desc);
void sp_graphics_pipeline_state_destroy(const sp_graphics_pipeline_state_handle& pipeline_handle);
ID3D12PipelineState* sp_graphics_pipeline_state_get_impl(const sp_graphics_pipeline_state_handle& pipeline_handle);