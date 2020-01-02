#pragma once

#include "constant_buffer.h"
#include "sparky.h"
#include "descriptor.h"
#include "d3dx12.h"

#include <array>
#include <cassert>

#define NOMINMAX
#include <d3d12.h>

namespace detail
{
	sp_constant_buffer_heap sp_constant_buffer_heap_create(const char* name, const sp_constant_buffer_heap_desc& desc)
	{
		sp_constant_buffer_heap constant_buffer_heap;

		constant_buffer_heap._name = name;

		// A constant buffer is expected to be 256 byte aligned so the heap should as well
		const int size_in_bytes_aligned = (desc.size_in_bytes + 255) & ~255;

		HRESULT hr = detail::_sp._device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size_in_bytes_aligned),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constant_buffer_heap._resource));
		assert(SUCCEEDED(hr));

		constant_buffer_heap._head = 0;
		constant_buffer_heap._size_in_bytes = size_in_bytes_aligned;

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
		constant_buffer_heap._resource->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

		return constant_buffer_heap;
	}

	void sp_constant_buffer_heap_reset(sp_constant_buffer_heap& constant_buffer_heap)
	{
		constant_buffer_heap._head = 0;
	}

	void sp_constant_buffer_heap_destroy(sp_constant_buffer_heap& constant_buffer_heap)
	{
		constant_buffer_heap._head = 0;
		constant_buffer_heap._size_in_bytes = 0;
		constant_buffer_heap._resource = nullptr;
	}
}

sp_constant_buffer sp_constant_buffer_create(int size_in_bytes)
{
	const int size_in_bytes_aligned = (size_in_bytes + 255) & ~255;

	assert(detail::_sp._constant_buffer_heap._head + size_in_bytes_aligned <= detail::_sp._constant_buffer_heap._size_in_bytes);

	sp_descriptor_handle constant_buffer_view = detail::sp_descriptor_alloc(detail::_sp._descriptor_heap_cbv_srv_uav_cpu);

	D3D12_CONSTANT_BUFFER_VIEW_DESC constant_buffer_view_desc = {
		detail::_sp._constant_buffer_heap._resource->GetGPUVirtualAddress() + detail::_sp._constant_buffer_heap._head,
		static_cast<UINT>(size_in_bytes_aligned)
	};

	detail::_sp._device->CreateConstantBufferView(&constant_buffer_view_desc, constant_buffer_view._handle_cpu_d3d12);

	sp_constant_buffer constant_buffer = {
		size_in_bytes,
		constant_buffer_view,
		detail::_sp._constant_buffer_heap._head
	};

	detail::_sp._constant_buffer_heap._head += size_in_bytes_aligned;

	return constant_buffer;
}

void sp_constant_buffer_update(sp_constant_buffer& constant_buffer, const void* data)
{
	void* data_gpu;
	CD3DX12_RANGE read_range(0, 0); // A range where end <= begin indicates we do not intend to read from this resource on the CPU.
	HRESULT hr = detail::_sp._constant_buffer_heap._resource->Map(0, &read_range, &data_gpu);
	assert(SUCCEEDED(hr));
	memcpy(static_cast<uint8_t*>(data_gpu) + constant_buffer._offset_in_heap, data, constant_buffer._size_in_bytes);
	detail::_sp._constant_buffer_heap._resource->Unmap(0, nullptr);
}