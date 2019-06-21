#pragma once

#include "constant_buffer.h"
#include "descriptor.h"
#include "d3dx12.h"

#include <array>

#include <d3d12.h>

sp_constant_buffer_heap sp_constant_buffer_heap_create(const char* name, const sp_constant_buffer_heap_desc& desc)
{
	sp_constant_buffer_heap constant_buffer_heap;

	constant_buffer_heap._name = name;

	// A constant buffer is expected to be 256 byte aligned so the heap should as well
	const int size_bytes_aligned = (desc.size_bytes + 255) & ~255;

	HRESULT hr = _sp._device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size_bytes_aligned),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constant_buffer_heap._resource));
	assert(SUCCEEDED(hr));

	constant_buffer_heap._head = 0;
	constant_buffer_heap._size_bytes = size_bytes_aligned;

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
	constant_buffer_heap._resource->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	return constant_buffer_heap;
}

void sp_constant_buffer_heap_reset(sp_constant_buffer_heap* constant_buffer_heap)
{
	constant_buffer_heap->_head = 0;
}

void sp_constant_buffer_heap_destroy(sp_constant_buffer_heap* constant_buffer_heap)
{
	constant_buffer_heap->_head = 0;
	constant_buffer_heap->_size_bytes = 0;
	constant_buffer_heap->_resource = nullptr;
}

sp_descriptor_handle sp_constant_buffer_alloc(sp_constant_buffer_heap* constant_buffer_heap, int size_bytes, const void* initial_data)
{
	const int size_bytes_aligned = (size_bytes + 255) & ~255;

	sp_descriptor_handle constant_buffer_view = detail::sp_descriptor_alloc(&_sp._descriptor_heap_cbv_srv_uav_cpu_transient);

	D3D12_CONSTANT_BUFFER_VIEW_DESC constant_buffer_view_desc = {};
	constant_buffer_view_desc.BufferLocation = constant_buffer_heap->_resource->GetGPUVirtualAddress() + constant_buffer_heap->_head;
	constant_buffer_view_desc.SizeInBytes = size_bytes_aligned;

	assert(constant_buffer_heap->_head + size_bytes_aligned <= constant_buffer_heap->_size_bytes);

	_sp._device->CreateConstantBufferView(&constant_buffer_view_desc, constant_buffer_view._handle_cpu_d3d12);

	// TODO: This is probably dumb. Could map the buffer once per frame.
	{
		void* data_gpu;
		CD3DX12_RANGE read_range(0, 0); // A range where end <= begin indicates we do not intend to read from this resource on the CPU.
		HRESULT hr = constant_buffer_heap->_resource->Map(0, &read_range, &data_gpu);
		assert(SUCCEEDED(hr));
		memcpy(static_cast<uint8_t*>(data_gpu) + constant_buffer_heap->_head, initial_data, size_bytes);
		constant_buffer_heap->_resource->Unmap(0, nullptr);
	}

	constant_buffer_heap->_head += size_bytes_aligned;

	return constant_buffer_view;
}