#pragma once

#include "d3dx12.h"

#include <array>
#include <codecvt>

const UINT g_pixel_size_bytes = 4;

namespace detail
{
	namespace resource_pools
	{
		std::array<sp_texture, 4096> textures;
		sp_handle_pool texture_handles;
	}

	void sp_texture_pool_create()
	{
		sp_handle_pool_create(&resource_pools::texture_handles, static_cast<int>(resource_pools::textures.size()));
	}

	void sp_texture_pool_destroy()
	{
		sp_handle_pool_destroy(&resource_pools::texture_handles);
	}

	sp_texture_handle sp_texture_handle_alloc()
	{
		return sp_handle_alloc(&resource_pools::texture_handles);
	}

	void sp_texture_handle_free(sp_texture_handle texture_handle)
	{
		sp_handle_free(&resource_pools::texture_handles, texture_handle);
	}

	sp_texture& sp_texture_pool_get(sp_texture_handle texture_handle)
	{
		return resource_pools::textures[texture_handle.index];
	}

	namespace defaults
	{
		sp_texture_handle white;
		sp_texture_handle black;
		sp_texture_handle checkerboard;
	}

	void sp_texture_defaults_create()
	{
		constexpr uint8_t image_data_white[] = {
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		};

		defaults::white = sp_texture_create("default_white", { 2, 2, 1, sp_texture_format::r8g8b8a8 });
		sp_texture_update(defaults::white, image_data_white, 2 * 2 * 4, 4);

		constexpr uint8_t image_data_black[] = {
			0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF,
			0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF,
		};

		defaults::black = sp_texture_create("default_black", { 2, 2, 1, sp_texture_format::r8g8b8a8 });
		sp_texture_update(defaults::black, image_data_black, 2 * 2 * 4, 4);

		constexpr uint8_t image_data_checkerboard[] = {
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF,
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF,
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF,
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF,
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
			0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		};

		defaults::checkerboard = sp_texture_create("default_checkerboard", { 8, 8, 1, sp_texture_format::r8g8b8a8 });
		sp_texture_update(defaults::checkerboard, image_data_checkerboard, 8 * 8 * 4, 4);
	}

	void sp_texture_defaults_destroy()
	{
		sp_texture_destroy(defaults::white);
		sp_texture_destroy(defaults::black);
		sp_texture_destroy(defaults::checkerboard);
	}
}

namespace detail
{
	int sp_texture_calculate_num_mip_levels(int height, int width)
	{
		return std::min(
			sp_texture_mip_level_max, 
			static_cast<int>(std::floor(std::log2(std::max(height, width)))) + 1);
	}
}

sp_texture_handle sp_texture_create(const char* name, const sp_texture_desc& desc)
{
	assert(desc.depth > 0);

	sp_texture_handle texture_handle = sp_handle_alloc(&detail::resource_pools::texture_handles);
	sp_texture& texture = detail::resource_pools::textures[texture_handle.index];

	D3D12_RESOURCE_DESC resource_desc_d3d12 = {};
	resource_desc_d3d12.Format = detail::sp_texture_format_get_base_format_d3d12(desc.format);
	resource_desc_d3d12.Width = desc.width;
	resource_desc_d3d12.Height = desc.height;
	resource_desc_d3d12.DepthOrArraySize = desc.depth;
	resource_desc_d3d12.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource_desc_d3d12.SampleDesc.Count = 1;
	resource_desc_d3d12.SampleDesc.Quality = 0;

	D3D12_CLEAR_VALUE* optimized_clear_value = nullptr;

	if (desc.depth == 1)
	{
		resource_desc_d3d12.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		if (detail::sp_texture_format_is_depth(desc.format))
		{
			resource_desc_d3d12.MipLevels = 1;
			resource_desc_d3d12.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			texture._optimized_clear_value.Format = detail::sp_texture_format_get_dsv_format_d3d12(desc.format);
			texture._optimized_clear_value.DepthStencil.Depth = 1.0f;
			texture._optimized_clear_value.DepthStencil.Stencil = 0;

			optimized_clear_value = &texture._optimized_clear_value;
		}
		else
		{
			if ((desc.flags & sp_texture_flags::render_target) != sp_texture_flags::none)
			{
				resource_desc_d3d12.MipLevels = 1;
				resource_desc_d3d12.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

				texture._optimized_clear_value.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
				texture._optimized_clear_value.Color[0] = 0.0f;
				texture._optimized_clear_value.Color[1] = 0.0f;
				texture._optimized_clear_value.Color[2] = 0.0f;
				texture._optimized_clear_value.Color[3] = 0.0f;

				optimized_clear_value = &texture._optimized_clear_value;
			}
			else
			{
				resource_desc_d3d12.MipLevels = detail::sp_texture_calculate_num_mip_levels(desc.width, desc.height);
				resource_desc_d3d12.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}
		}
	}
	else
	{
		resource_desc_d3d12.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		resource_desc_d3d12.MipLevels = 1;
	}

	const auto heap_properties_d3dx12 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	HRESULT hr = detail::_sp._device->CreateCommittedResource(
		&heap_properties_d3dx12,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc_d3d12,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		optimized_clear_value,
		IID_PPV_ARGS(&texture._resource));
	assert(hr == S_OK);

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
	texture._resource->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	texture._name = name;
	texture._width = desc.width;
	texture._height = desc.height;
	texture._depth = desc.depth;
	texture._num_mip_levels = resource_desc_d3d12.MipLevels;
	texture._format = desc.format;

	// TODO: Including the SRV and RTV in the texture isn't ideal. Need to lookup texture by handle just to
	// get another handle. The views should be first class types.

	texture._shader_resource_view = detail::sp_descriptor_alloc(detail::_sp._descriptor_heap_cbv_srv_uav_cpu);

	D3D12_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc_d3d12 = {};
	shader_resource_view_desc_d3d12.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shader_resource_view_desc_d3d12.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
	shader_resource_view_desc_d3d12.Texture2D.MipLevels = -1;

	if (desc.depth == 1)
	{
		shader_resource_view_desc_d3d12.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	}
	else
	{
		shader_resource_view_desc_d3d12.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	}

	detail::_sp._device->CreateShaderResourceView(texture._resource.Get(), &shader_resource_view_desc_d3d12, texture._shader_resource_view._handle_cpu_d3d12);

	if (desc.depth == 1)
	{
		if (detail::sp_texture_format_is_depth(desc.format))
		{
			texture._depth_stencil_view = detail::sp_descriptor_alloc(detail::_sp._descriptor_heap_dsv_cpu);

			D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc_d3d12 = {};
			depth_stencil_view_desc_d3d12.Format = detail::sp_texture_format_get_dsv_format_d3d12(desc.format);
			depth_stencil_view_desc_d3d12.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

			detail::_sp._device->CreateDepthStencilView(texture._resource.Get(), &depth_stencil_view_desc_d3d12, texture._depth_stencil_view._handle_cpu_d3d12);
		}
		else
		{
			if ((desc.flags & sp_texture_flags::render_target) != sp_texture_flags::none)
			{
				texture._render_target_view = detail::sp_descriptor_alloc(detail::_sp._descriptor_heap_rtv_cpu);

				D3D12_RENDER_TARGET_VIEW_DESC render_target_view_desc_d3d12 = {};
				render_target_view_desc_d3d12.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
				render_target_view_desc_d3d12.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				render_target_view_desc_d3d12.Texture2D.MipSlice = 0;

				detail::_sp._device->CreateRenderTargetView(texture._resource.Get(), &render_target_view_desc_d3d12, texture._render_target_view._handle_cpu_d3d12);
			}
			else
			{
				texture._unordered_access_view = detail::sp_descriptor_alloc(detail::_sp._descriptor_heap_cbv_srv_uav_cpu);

				D3D12_UNORDERED_ACCESS_VIEW_DESC unordered_access_view_desc_d3d12 = {};
				unordered_access_view_desc_d3d12.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
				unordered_access_view_desc_d3d12.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				unordered_access_view_desc_d3d12.Texture2D.MipSlice = 0;

				detail::_sp._device->CreateUnorderedAccessView(texture._resource.Get(), nullptr, &unordered_access_view_desc_d3d12, texture._unordered_access_view._handle_cpu_d3d12);
			}
		}
	}

	return texture_handle;
}

void sp_texture_destroy(sp_texture_handle texture_handle)
{
	sp_texture& texture = detail::resource_pools::textures[texture_handle.index];

	sp_handle_free(&detail::resource_pools::texture_handles, texture_handle);
}

void sp_texture_update(const sp_texture_handle& texture_handle, const void* data_cpu, int size_bytes, int pixel_size_bytes)
{
	sp_texture& texture = detail::resource_pools::textures[texture_handle.index];

	// Create the GPU upload buffer.
	UINT64 upload_buffer_size_bytes = 0;

	{
		ID3D12Device* device = nullptr;
		texture._resource->GetDevice(__uuidof(*device), reinterpret_cast<void**>(&device));
		const auto resource_desc_d3d12 = texture._resource->GetDesc();
		device->GetCopyableFootprints(&resource_desc_d3d12, 0, 1, 0, nullptr, nullptr, nullptr, &upload_buffer_size_bytes);
		device->Release();
	}

	// assert(size_bytes == upload_buffer_size_bytes);

	Microsoft::WRL::ComPtr<ID3D12Resource> texture_upload_buffer;

	const auto heap_properties_d3dx12 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto resource_desc_d3dx12 = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size_bytes);
	HRESULT hr = detail::_sp._device->CreateCommittedResource(
		&heap_properties_d3dx12,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc_d3dx12,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texture_upload_buffer));
	assert(hr == S_OK);

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
	texture_upload_buffer->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(texture._name).c_str());
#endif

	// Copy data to the intermediate upload heap and then schedule a copy 
	// from the upload heap to the Texture2D.
	D3D12_SUBRESOURCE_DATA subresource_data = {};
	subresource_data.pData = data_cpu;
	subresource_data.RowPitch = texture._width * pixel_size_bytes;
	subresource_data.SlicePitch = subresource_data.RowPitch * texture._height;

	assert(size_bytes == subresource_data.SlicePitch * texture._depth);

	// TODO: Should this use a copy command list/queue
	sp_graphics_command_list texture_update_command_list = sp_graphics_command_list_create(texture._name, {});

	sp_graphics_command_list_begin(texture_update_command_list);

	auto barrier_d3dx12 = CD3DX12_RESOURCE_BARRIER::Transition(texture._resource.Get(), texture._default_state, D3D12_RESOURCE_STATE_COPY_DEST);
	texture_update_command_list._command_list_d3d12->ResourceBarrier(1, &barrier_d3dx12);

	UpdateSubresources(texture_update_command_list._command_list_d3d12.Get(),
		texture._resource.Get(),
		texture_upload_buffer.Get(),
		0,
		0,
		1,
		&subresource_data);

	barrier_d3dx12 = CD3DX12_RESOURCE_BARRIER::Transition(texture._resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, texture._default_state);
	texture_update_command_list._command_list_d3d12->ResourceBarrier(1, &barrier_d3dx12);

	sp_graphics_command_list_end(texture_update_command_list);

	sp_graphics_queue_execute(texture_update_command_list);

	// Need to wait for texture to be updated before allowing upload buffer to fall out of scope
	sp_graphics_queue_wait_for_idle();

	sp_graphics_command_list_destroy(texture_update_command_list);
}

sp_texture_handle sp_texture_defaults_white()
{
	return detail::defaults::white;
}

sp_texture_handle sp_texture_defaults_black()
{
	return detail::defaults::black;
}

sp_texture_handle sp_texture_defaults_checkerboard()
{
	return detail::defaults::checkerboard;
}