#pragma once

#include "constant_buffer.h"
#include "vertex_buffer.h"
#include "d3dx12.h"

#include <array>

namespace detail
{
	namespace resource_pools
	{
		std::array<sp_vertex_buffer, 1024> vertex_buffers;
		sp_handle_pool vertex_buffer_handles;
	}

	void sp_vertex_buffer_pool_create()
	{
		sp_handle_pool_create(&resource_pools::vertex_buffer_handles, static_cast<int>(resource_pools::vertex_buffers.size()));
	}

	void sp_vertex_buffer_pool_destroy()
	{
		sp_handle_pool_destroy(&resource_pools::vertex_buffer_handles);
	}

	sp_vertex_buffer& sp_vertex_buffer_pool_get(sp_vertex_buffer_handle vertex_buffer_handle)
	{
		return resource_pools::vertex_buffers[vertex_buffer_handle.index];
	}
}

sp_vertex_buffer_handle sp_vertex_buffer_create(const char* name, const sp_vertex_buffer_desc& desc)
{
	HRESULT hr = S_OK;

	sp_vertex_buffer_handle buffer_handle = sp_handle_alloc(&detail::resource_pools::vertex_buffer_handles);
	sp_vertex_buffer& buffer = detail::resource_pools::vertex_buffers[buffer_handle.index];

	// Note: using upload heaps to transfer static data is not 
	// recommended. Every time the GPU needs it, the upload heap will be marshalled 
	// over. Please read up on Default Heap usage. An upload heap is used here for 
	// code simplicity.
	hr = _sp._device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(desc._size_bytes),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&buffer._resource));
	assert(SUCCEEDED(hr));

	buffer._name = name;

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
	buffer._resource->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	buffer._vertex_buffer_view.BufferLocation = buffer._resource->GetGPUVirtualAddress();
	buffer._vertex_buffer_view.StrideInBytes = desc._stride_bytes;
	buffer._vertex_buffer_view.SizeInBytes = desc._size_bytes;

	return buffer_handle;
}

ID3D12Resource* sp_vertex_buffer_get_impl(const sp_vertex_buffer_handle& buffer_handle)
{
	return detail::resource_pools::vertex_buffers[buffer_handle.index]._resource.Get();
}

void sp_vertex_buffer_update(const sp_vertex_buffer_handle& buffer_handle, const void* data_cpu, int size_bytes)
{
	sp_vertex_buffer& buffer = detail::resource_pools::vertex_buffers[buffer_handle.index];

	void* data_gpu;
	CD3DX12_RANGE read_range(0, 0); // A range where end <= begin indicates we do not intend to read from this resource on the CPU.
	HRESULT hr = buffer._resource->Map(0, &read_range, &data_gpu);
	assert(SUCCEEDED(hr));
	memcpy(data_gpu, data_cpu, size_bytes);
	buffer._resource->Unmap(0, nullptr);
}

void sp_vertex_buffer_destroy(const sp_vertex_buffer_handle& buffer_handle)
{
	sp_vertex_buffer& buffer = detail::resource_pools::vertex_buffers[buffer_handle.index];

	buffer._resource = nullptr;

	sp_handle_free(&detail::resource_pools::vertex_buffer_handles, buffer_handle);
}