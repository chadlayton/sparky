#pragma once

#include "constant_buffer.h"
#include "handle.h"
#include "d3dx12.h"

#include <array>

#include <d3d12.h>

namespace detail
{
	namespace resource_pools
	{
		std::array<sp_constant_buffer, 1024> constant_buffers;
		sp_handle_pool constant_buffer_handles;
	}

	void sp_constant_buffer_pool_create()
	{
		sp_handle_pool_create(&resource_pools::constant_buffer_handles, static_cast<int>(resource_pools::constant_buffers.size()));
	}

	void sp_constant_buffer_pool_destroy()
	{
		sp_handle_pool_destroy(&resource_pools::constant_buffer_handles);
	}
}

using sp_constant_buffer_handle = sp_handle;

sp_constant_buffer_handle sp_constant_buffer_create(const char* name, const sp_constant_buffer_desc& desc)
{
	sp_constant_buffer_handle buffer_handle = sp_handle_alloc(&detail::resource_pools::constant_buffer_handles);
	sp_constant_buffer& buffer = detail::resource_pools::constant_buffers[buffer_handle.index];

	buffer._name = name;

	// TODO: This is probably dumb. I should just have one big buffer per frame and allocate views from it.
	// TODO: I should have sp_buffer only accessed through handles and value type sp_constant_buffer_view. 
	// TODO: Ditto for sp_texture and shader_resource_view, render_target_view, and depth_stencil_view.
	HRESULT hr = _sp._device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((desc.size_bytes + 255) & ~255), // CB size is required to be 256-byte aligned.
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&buffer._resource));
	assert(SUCCEEDED(hr));

	buffer._constant_buffer_view = sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_cpu);

	D3D12_CONSTANT_BUFFER_VIEW_DESC constant_buffer_view_desc = {};
	constant_buffer_view_desc.BufferLocation = buffer._resource->GetGPUVirtualAddress();
	constant_buffer_view_desc.SizeInBytes = (desc.size_bytes + 255) & ~255;	// CB size is required to be 256-byte aligned.

	_sp._device->CreateConstantBufferView(&constant_buffer_view_desc, buffer._constant_buffer_view._handle_cpu_d3d12);

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
	buffer._resource->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	return buffer_handle;
}

void sp_constant_buffer_update(sp_constant_buffer_handle buffer_handle, void* data_cpu, int size_bytes)
{
	sp_constant_buffer& buffer = detail::resource_pools::constant_buffers[buffer_handle.index];

	void* data_gpu;
	CD3DX12_RANGE read_range(0, 0); // A range where end <= begin indicates we do not intend to read from this resource on the CPU.
	HRESULT hr = buffer._resource->Map(0, &read_range, &data_gpu);
	assert(SUCCEEDED(hr));
	memcpy(data_gpu, data_cpu, size_bytes);
	buffer._resource->Unmap(0, nullptr);
}

// TODO: Don't expose constant_buffe except by handle
const sp_constant_buffer& sp_constant_buffer_get_hack(const sp_constant_buffer_handle& buffer_handle)
{
	return detail::resource_pools::constant_buffers[buffer_handle.index];
}

void sp_constant_buffer_destroy(sp_constant_buffer_handle buffer_handle)
{
	sp_constant_buffer& buffer = detail::resource_pools::constant_buffers[buffer_handle.index];

	buffer._resource = nullptr;

	sp_handle_free(&detail::resource_pools::constant_buffer_handles, buffer_handle);
}