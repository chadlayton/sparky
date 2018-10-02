#pragma once

#include <codecvt>

const UINT g_pixel_size_bytes = 4;

namespace detail
{
	namespace resource_pools
	{
		std::array<sp_texture, 1024> textures;
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
}

std::vector<uint8_t> sp_image_checkerboard_data_create(int width, int height)
{
	const UINT row_pitch = width * g_pixel_size_bytes;
	const UINT cell_pitch = row_pitch >> 3;		// The width of a cell in the checkboard texture.
	const UINT cell_height = width >> 3;	// The height of a cell in the checkerboard texture.
	const UINT texture_size_bytes = row_pitch * height;

	std::vector<UINT8> data(texture_size_bytes);
	UINT8* pData = &data[0];

	for (UINT n = 0; n < texture_size_bytes; n += g_pixel_size_bytes)
	{
		UINT x = n % row_pitch;
		UINT y = n / row_pitch;
		UINT i = x / cell_pitch;
		UINT j = y / cell_height;

		if (i % 2 == j % 2)
		{
			pData[n] = 0x00;		// R
			pData[n + 1] = 0x00;	// G
			pData[n + 2] = 0x00;	// B
			pData[n + 3] = 0xff;	// A
		}
		else
		{
			pData[n] = 0xff;		// R
			pData[n + 1] = 0xff;	// G
			pData[n + 2] = 0xff;	// B
			pData[n + 3] = 0xff;	// A
		}
	}

	return data;
}

sp_texture_handle sp_texture_create(const char* name, const sp_texture_desc& desc)
{
	sp_texture_handle texture_handle = sp_handle_alloc(&detail::resource_pools::texture_handles);
	sp_texture& texture = detail::resource_pools::textures[texture_handle.index];

	// XXX: Is there any performance penalty to creating every texture with either an SRV (and an RTV/DSV)?

	D3D12_RESOURCE_DESC resource_desc_d3d12 = {};
	resource_desc_d3d12.MipLevels = 1;
	resource_desc_d3d12.Format = detail::sp_texture_format_get_base_format_d3d12(desc.format);
	resource_desc_d3d12.Width = desc.width;
	resource_desc_d3d12.Height = desc.height;
	resource_desc_d3d12.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	if (detail::sp_texture_format_is_depth(desc.format))
	{
		resource_desc_d3d12.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	else
	{
		resource_desc_d3d12.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	resource_desc_d3d12.DepthOrArraySize = 1;
	resource_desc_d3d12.SampleDesc.Count = 1;
	resource_desc_d3d12.SampleDesc.Quality = 0;
	resource_desc_d3d12.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE optimized_clear_value = {};
	if (detail::sp_texture_format_is_depth(desc.format))
	{
		optimized_clear_value.Format = detail::sp_texture_format_get_dsv_format_d3d12(desc.format);
		optimized_clear_value.DepthStencil.Depth = 1.0f;
		optimized_clear_value.DepthStencil.Stencil = 0;
	}
	else
	{
		optimized_clear_value.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
		optimized_clear_value.Color[0] = 0.0f;
		optimized_clear_value.Color[1] = 0.0f;
		optimized_clear_value.Color[2] = 0.0f;
		optimized_clear_value.Color[3] = 0.0f;
	}

	HRESULT hr = _sp._device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resource_desc_d3d12,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optimized_clear_value,
		IID_PPV_ARGS(&texture._resource));
	assert(hr == S_OK);

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
	texture._resource->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	texture._name = name;
	texture._width = desc.width;
	texture._height = desc.height;

	// TODO: Including the SRV and RTV in the texture isn't ideal. Need to lookup texture by handle just to
	// get another handle. Maybe host should deal with these + organizing into descriptor tables.

	texture._shader_resource_view = sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_cpu);

	D3D12_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc_d3d12 = {};
	shader_resource_view_desc_d3d12.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shader_resource_view_desc_d3d12.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
	shader_resource_view_desc_d3d12.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	shader_resource_view_desc_d3d12.Texture2D.MipLevels = 1;

	_sp._device->CreateShaderResourceView(texture._resource.Get(), &shader_resource_view_desc_d3d12, texture._shader_resource_view._handle_cpu_d3d12);

	if (detail::sp_texture_format_is_depth(desc.format))
	{
		texture._depth_stencil_view = sp_descriptor_alloc(&_sp._descriptor_heap_dsv_cpu);

		D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc_d3d12 = {};
		depth_stencil_view_desc_d3d12.Format = detail::sp_texture_format_get_dsv_format_d3d12(desc.format);
		depth_stencil_view_desc_d3d12.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		_sp._device->CreateDepthStencilView(texture._resource.Get(), &depth_stencil_view_desc_d3d12, texture._depth_stencil_view._handle_cpu_d3d12);
	}
	else
	{
		texture._render_target_view = sp_descriptor_alloc(&_sp._descriptor_heap_rtv_cpu);

		D3D12_RENDER_TARGET_VIEW_DESC render_target_view_desc_d3d12 = {};
		render_target_view_desc_d3d12.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
		render_target_view_desc_d3d12.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		render_target_view_desc_d3d12.Texture2D.MipSlice = 0;

		_sp._device->CreateRenderTargetView(texture._resource.Get(), &render_target_view_desc_d3d12, texture._render_target_view._handle_cpu_d3d12);
	}

	return texture_handle;
}

void sp_texture_destroy(sp_texture_handle texture_handle)
{
	sp_texture& texture = detail::resource_pools::textures[texture_handle.index];

	sp_handle_free(&detail::resource_pools::texture_handles, texture_handle);
}

void sp_texture_update(const sp_texture_handle& texture_handle, void* data_cpu, int size_bytes)
{
	sp_texture& texture = detail::resource_pools::textures[texture_handle.index];

	// Create the GPU upload buffer.
	UINT64 upload_buffer_size_bytes = 0;

	ID3D12Device* device = nullptr;
	texture._resource->GetDevice(__uuidof(*device), reinterpret_cast<void**>(&device));
	device->GetCopyableFootprints(&texture._resource->GetDesc(), 0, 1, 0, nullptr, nullptr, nullptr, &upload_buffer_size_bytes);
	device->Release();

	Microsoft::WRL::ComPtr<ID3D12Resource> texture_upload_buffer;

	HRESULT hr = _sp._device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size_bytes),
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
	subresource_data.RowPitch = texture._width * g_pixel_size_bytes;
	subresource_data.SlicePitch = subresource_data.RowPitch * texture._height;

	assert(size_bytes == subresource_data.SlicePitch);

	// TODO: Should this use a copy command list/queue
	sp_graphics_command_list texture_update_command_list = sp_graphics_command_list_create(texture._name, {});

	texture_update_command_list._command_list_d3d12->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture._resource.Get(), texture._default_state, D3D12_RESOURCE_STATE_COPY_DEST));

	UpdateSubresources(texture_update_command_list._command_list_d3d12.Get(),
		texture._resource.Get(),
		texture_upload_buffer.Get(),
		0,
		0,
		1,
		&subresource_data);

	texture_update_command_list._command_list_d3d12->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture._resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, texture._default_state));

	sp_graphics_command_list_close(texture_update_command_list);

	sp_graphics_queue_execute(texture_update_command_list);

	// Need to wait for texture to be updated before allowing upload buffer to fall out of scope
	sp_graphics_queue_wait_for_idle();

	sp_graphics_command_list_destroy(texture_update_command_list);
}