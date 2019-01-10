#pragma once

#include "pipeline.h"
#include "handle.h"
#include "texture.h"
#include "shader.h"
#include "d3dx12.h"

#include <array>

#include <wrl.h>
#include <d3d12.h>

namespace detail
{
	namespace resource_pools
	{
		std::array<sp_graphics_pipeline_state, 1024> graphics_pipelines;
		sp_handle_pool graphics_pipeline_handles;
	}

	void sp_graphics_pipeline_state_pool_create()
	{
		sp_handle_pool_create(&resource_pools::graphics_pipeline_handles, static_cast<int>(resource_pools::graphics_pipelines.size()));
	}

	void sp_graphics_pipeline_state_pool_destroy()
	{
		sp_handle_pool_destroy(&resource_pools::graphics_pipeline_handles);
	}
}

sp_graphics_pipeline_state_handle sp_graphics_pipeline_state_create(const char* name, const sp_graphics_pipeline_state_desc& desc)
{
	sp_graphics_pipeline_state_handle pipeline_state_handle = sp_handle_alloc(&detail::resource_pools::graphics_pipeline_handles);
	sp_graphics_pipeline_state& pipeline_state = detail::resource_pools::graphics_pipelines[pipeline_state_handle.index];

	pipeline_state._name = name;

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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc = {};
	pipeline_state_desc.pRootSignature = _sp._root_signature.Get();
	pipeline_state_desc.VS = CD3DX12_SHADER_BYTECODE(detail::sp_vertex_shader_pool_get(desc.vertex_shader_handle)._blob.Get());
	pipeline_state_desc.PS = CD3DX12_SHADER_BYTECODE(detail::sp_pixel_shader_pool_get(desc.pixel_shader_handle)._blob.Get());
	pipeline_state_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pipeline_state_desc.SampleMask = UINT_MAX;

	// RasterizerState
	{
		pipeline_state_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		switch (desc.cull_face)
		{
		case sp_rasterizer_cull_face::front: pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; break;
		case sp_rasterizer_cull_face::back:  pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;  break;
		case sp_rasterizer_cull_face::none:  pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  break;
		default: assert(false);
		}
		pipeline_state_desc.RasterizerState.FrontCounterClockwise = TRUE;
	}

	// DepthStencilState
	{
		if (desc.depth_stencil_format == sp_texture_format::unknown)
		{
			pipeline_state_desc.DepthStencilState.DepthEnable = FALSE;
		}
		else
		{
			pipeline_state_desc.DepthStencilState.DepthEnable = TRUE;
			pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			pipeline_state_desc.DSVFormat = detail::sp_texture_format_get_dsv_format_d3d12(desc.depth_stencil_format);
		}
		pipeline_state_desc.DepthStencilState.StencilEnable = FALSE;
	}

	pipeline_state_desc.InputLayout = { input_element_desc, input_element_count };
	pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	unsigned render_target_count = 0;
	for (; render_target_count < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++render_target_count)
	{
		if (desc.render_target_formats[render_target_count] == sp_texture_format::unknown)
		{
			break;
		}

		pipeline_state_desc.RTVFormats[render_target_count] = detail::sp_texture_format_get_srv_format_d3d12(desc.render_target_formats[render_target_count]);
	}
	pipeline_state_desc.NumRenderTargets = render_target_count;
	pipeline_state_desc.SampleDesc.Count = 1;

	HRESULT hr = _sp._device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&pipeline_state._impl));
	assert(SUCCEEDED(hr));

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
	pipeline_state._impl->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	return pipeline_state_handle;
}

void sp_graphics_pipeline_state_destroy(const sp_graphics_pipeline_state_handle& pipeline_handle)
{

}

ID3D12PipelineState* sp_graphics_pipeline_state_get_impl(const sp_graphics_pipeline_state_handle& pipeline_handle)
{
	return detail::resource_pools::graphics_pipelines[pipeline_handle.index]._impl.Get();
}