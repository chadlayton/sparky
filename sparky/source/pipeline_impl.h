#pragma once

#include "sparky.h"
#include "pipeline.h"
#include "handle.h"
#include "texture.h"
#include "shader.h"
#include "d3dx12.h"

#include <array>

#define NOMINMAX
#include <d3d12.h>

#include <wrl.h>

namespace detail
{
	namespace resource_pools
	{
		std::array<sp_graphics_pipeline_state, 1024> graphics_pipelines;
		sp_handle_pool graphics_pipeline_handles;

		std::array<sp_compute_pipeline_state, 1024> compute_pipelines;
		sp_handle_pool compute_pipeline_handles;
	}

	void sp_graphics_pipeline_state_pool_create()
	{
		sp_handle_pool_create(&resource_pools::graphics_pipeline_handles, static_cast<int>(resource_pools::graphics_pipelines.size()));
	}

	sp_graphics_pipeline_state& sp_graphics_pipeline_state_pool_get(const sp_graphics_pipeline_state_handle& pipeline_handle)
	{
		return detail::resource_pools::graphics_pipelines[pipeline_handle.index];
	}

	void sp_graphics_pipeline_state_pool_destroy()
	{
		sp_handle_pool_destroy(&resource_pools::graphics_pipeline_handles);
	}

	void sp_compute_pipeline_state_pool_create()
	{
		sp_handle_pool_create(&resource_pools::compute_pipeline_handles, static_cast<int>(resource_pools::compute_pipelines.size()));
	}

	sp_compute_pipeline_state& sp_compute_pipeline_state_pool_get(const sp_compute_pipeline_state_handle& pipeline_handle)
	{
		return detail::resource_pools::compute_pipelines[pipeline_handle.index];
	}

	void sp_compute_pipeline_state_pool_destroy()
	{
		sp_handle_pool_destroy(&resource_pools::compute_pipeline_handles);
	}
}

namespace detail
{
	void sp_graphics_pipeline_state_init(const char* name, const sp_graphics_pipeline_state_desc& desc, sp_graphics_pipeline_state* pipeline_state)
	{
		// TODO: Deduce from vertex shader reflection data?
		D3D12_INPUT_ELEMENT_DESC input_element_desc[D3D12_STANDARD_VERTEX_ELEMENT_COUNT];
		memset(input_element_desc, 0, sizeof(input_element_desc));
		unsigned input_element_count = 0;
		for (; input_element_count < D3D12_STANDARD_VERTEX_ELEMENT_COUNT; ++input_element_count)
		{
			if (!desc.input_layout[input_element_count]._semantic_name)
			{
				break;
			}

			input_element_desc[input_element_count].SemanticName = desc.input_layout[input_element_count]._semantic_name;
			input_element_desc[input_element_count].SemanticIndex = desc.input_layout[input_element_count]._semantic_index;
			input_element_desc[input_element_count].Format = desc.input_layout[input_element_count]._format;
			input_element_desc[input_element_count].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		}

		const sp_vertex_shader& vertex_shader = detail::sp_vertex_shader_pool_get(desc.vertex_shader_handle);
		const sp_pixel_shader & pixel_shader = detail::sp_pixel_shader_pool_get(desc.pixel_shader_handle);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc_d3d12 = {};
		pipeline_state_desc_d3d12.pRootSignature = _sp._root_signature.Get();
		pipeline_state_desc_d3d12.VS = CD3DX12_SHADER_BYTECODE(vertex_shader._blob.Get());
		pipeline_state_desc_d3d12.PS = CD3DX12_SHADER_BYTECODE(pixel_shader._blob.Get());
		pipeline_state_desc_d3d12.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pipeline_state_desc_d3d12.SampleMask = UINT_MAX;

		// RasterizerState
		{
			pipeline_state_desc_d3d12.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			switch (desc.cull_face)
			{
			case sp_rasterizer_cull_face::front: pipeline_state_desc_d3d12.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; break;
			case sp_rasterizer_cull_face::back:  pipeline_state_desc_d3d12.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;  break;
			case sp_rasterizer_cull_face::none:  pipeline_state_desc_d3d12.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  break;
			default: assert(false);
			}
			switch (desc.fill_mode)
			{
			case sp_rasterizer_fill_mode::solid:     pipeline_state_desc_d3d12.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;     break;
			case sp_rasterizer_fill_mode::wireframe: pipeline_state_desc_d3d12.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; break;
			default: assert(false);
			}
			pipeline_state_desc_d3d12.RasterizerState.FrontCounterClockwise = TRUE;
		}

		// DepthStencilState
		{
			if (desc.depth_stencil_format == sp_texture_format::unknown)
			{
				pipeline_state_desc_d3d12.DepthStencilState.DepthEnable = FALSE;
			}
			else
			{
				pipeline_state_desc_d3d12.DepthStencilState.DepthEnable = TRUE;
				pipeline_state_desc_d3d12.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pipeline_state_desc_d3d12.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
				pipeline_state_desc_d3d12.DSVFormat = detail::sp_texture_format_get_dsv_format_d3d12(desc.depth_stencil_format);
			}
			pipeline_state_desc_d3d12.DepthStencilState.StencilEnable = FALSE;
		}

		pipeline_state_desc_d3d12.InputLayout = { input_element_desc, input_element_count };
		pipeline_state_desc_d3d12.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		unsigned render_target_count = 0;
		for (; render_target_count < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++render_target_count)
		{
			if (desc.render_target_formats[render_target_count] == sp_texture_format::unknown)
			{
				break;
			}

			pipeline_state_desc_d3d12.RTVFormats[render_target_count] = detail::sp_texture_format_get_srv_format_d3d12(desc.render_target_formats[render_target_count]);
		}
		pipeline_state_desc_d3d12.NumRenderTargets = render_target_count;
		pipeline_state_desc_d3d12.SampleDesc.Count = 1;

		HRESULT hr = _sp._device->CreateGraphicsPipelineState(&pipeline_state_desc_d3d12, IID_PPV_ARGS(&pipeline_state->_pipeline_d3d12));
		assert(SUCCEEDED(hr));

		switch (desc.primitive_topology)
		{
		case sp_primitive_topology::point_list:     pipeline_state->_primtive_topology_d3d = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;                 break;
		case sp_primitive_topology::line_list:      pipeline_state->_primtive_topology_d3d = D3D_PRIMITIVE_TOPOLOGY_LINELIST;                  break;
		case sp_primitive_topology::line_strip:     pipeline_state->_primtive_topology_d3d = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;                 break;
		case sp_primitive_topology::triange_list:   pipeline_state->_primtive_topology_d3d = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;              break;
		case sp_primitive_topology::triangle_strip: pipeline_state->_primtive_topology_d3d = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;             break;
		case sp_primitive_topology::patch:          pipeline_state->_primtive_topology_d3d = D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST; break;
		default: assert(false);
	}

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
		pipeline_state->_pipeline_d3d12->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

		pipeline_state->_name = name;
		pipeline_state->_desc = desc;
	}
}

sp_graphics_pipeline_state_handle sp_graphics_pipeline_state_create(const char* name, const sp_graphics_pipeline_state_desc& desc)
{
	sp_graphics_pipeline_state_handle pipeline_state_handle = sp_handle_alloc(&detail::resource_pools::graphics_pipeline_handles);
	sp_graphics_pipeline_state& pipeline_state = detail::resource_pools::graphics_pipelines[pipeline_state_handle.index];

	detail::sp_graphics_pipeline_state_init(name, desc, &pipeline_state);

#if SP_DEBUG_LIVE_SHADER_RELOADING
	sp_vertex_shader& vertex_shader = detail::sp_vertex_shader_pool_get(desc.vertex_shader_handle);
	sp_file_watch_create(vertex_shader._desc.filepath, [&vertex_shader, &pipeline_state](const char*) {
		sp_vertex_shader temp;
		if (detail::sp_vertex_shader_init(vertex_shader._desc, &temp))
		{
			vertex_shader = std::move(temp);
			detail::sp_graphics_pipeline_state_init(pipeline_state._name, pipeline_state._desc, &pipeline_state);
		}
	});

	sp_pixel_shader& pixel_shader = detail::sp_pixel_shader_pool_get(desc.pixel_shader_handle);
	sp_file_watch_create(pixel_shader._desc.filepath, [&pixel_shader, &pipeline_state](const char*) {
		sp_pixel_shader temp;
		if (detail::sp_pixel_shader_init(pixel_shader._desc, &temp))
		{
			pixel_shader = std::move(temp);
			detail::sp_graphics_pipeline_state_init(pipeline_state._name, pipeline_state._desc, &pipeline_state);
		}
	});
#endif

	return pipeline_state_handle;
}

void sp_graphics_pipeline_state_destroy(const sp_graphics_pipeline_state_handle& pipeline_state_handle)
{
	sp_graphics_pipeline_state& pipeline_state = detail::resource_pools::graphics_pipelines[pipeline_state_handle.index];

	pipeline_state._name = nullptr;
	pipeline_state._pipeline_d3d12.Reset();

	sp_handle_free(&detail::resource_pools::graphics_pipeline_handles, pipeline_state_handle);
}

sp_compute_pipeline_state_handle sp_compute_pipeline_state_create(const char* name, const sp_compute_pipeline_state_desc& desc)
{
	sp_compute_pipeline_state_handle pipeline_state_handle = sp_handle_alloc(&detail::resource_pools::compute_pipeline_handles);
	sp_compute_pipeline_state& pipeline_state = detail::resource_pools::compute_pipelines[pipeline_state_handle.index];

	D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_state_desc_d3d12 = {};
	pipeline_state_desc_d3d12.pRootSignature = detail::_sp._root_signature.Get();
	pipeline_state_desc_d3d12.CS = CD3DX12_SHADER_BYTECODE(detail::sp_compute_shader_pool_get(desc.compute_shader_handle)._blob.Get());
	HRESULT hr = detail::_sp._device->CreateComputePipelineState(&pipeline_state_desc_d3d12, IID_PPV_ARGS(&pipeline_state._impl));
	assert(SUCCEEDED(hr));

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
	pipeline_state._impl->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	pipeline_state._name = name;
	pipeline_state._desc = desc;

	return pipeline_state_handle;
}

void sp_compute_pipeline_state_destroy(const sp_graphics_pipeline_state_handle& pipeline_state_handle)
{
	sp_compute_pipeline_state& pipeline_state = detail::resource_pools::compute_pipelines[pipeline_state_handle.index];

	pipeline_state._name = nullptr;
	pipeline_state._impl.Reset();

	sp_handle_free(&detail::resource_pools::compute_pipeline_handles, pipeline_state_handle);
}