#pragma once

#include "descriptor.h"

#include <cassert>
#include <codecvt>

#define NOMINMAX
#include <d3d12.h>

#include <wrl.h>

namespace detail
{
	sp_descriptor_handle sp_descriptor_alloc(sp_descriptor_heap* descriptor_heap, int descriptor_count)
	{
		assert(descriptor_count >= 0);
		assert(descriptor_heap->_descriptor_count + descriptor_count < descriptor_heap->_descriptor_capacity);

		sp_descriptor_handle descriptor_handle = descriptor_heap->_head;

		const SIZE_T offset = static_cast<SIZE_T>(descriptor_heap->_descriptor_size) * descriptor_count;

		descriptor_heap->_head._handle_cpu_d3d12.ptr += offset;
		descriptor_heap->_head._handle_gpu_d3d12.ptr += offset;

		descriptor_heap->_descriptor_count += descriptor_count;

		return descriptor_handle;
	}
}

sp_descriptor_heap sp_descriptor_heap_create(const char* name, const sp_descriptor_heap_desc& desc)
{
	sp_descriptor_heap descriptor_heap;

	descriptor_heap._name = name;

	static_assert(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(sp_descriptor_heap_type::cbv_srv_uav) == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "sp_descriptor_heap_type::cbv_srv_uav != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV");
	static_assert(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(sp_descriptor_heap_type::sampler) == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "sp_descriptor_heap_type::sampler != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER");
	static_assert(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(sp_descriptor_heap_type::rtv) == D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "sp_descriptor_heap_type::rtv != D3D12_DESCRIPTOR_HEAP_TYPE_RTV");
	static_assert(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(sp_descriptor_heap_type::dsv) == D3D12_DESCRIPTOR_HEAP_TYPE_DSV, "sp_descriptor_heap_type::dsv != D3D12_DESCRIPTOR_HEAP_TYPE_DSV");

	D3D12_DESCRIPTOR_HEAP_DESC heap_desc_d3d12 = {};
	heap_desc_d3d12.NumDescriptors = desc.descriptor_capacity;
	heap_desc_d3d12.Type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(desc.type);
	heap_desc_d3d12.Flags = (desc.visibility == sp_descriptor_heap_visibility::cpu_and_gpu) ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = _sp._device->CreateDescriptorHeap(&heap_desc_d3d12, IID_PPV_ARGS(&descriptor_heap._heap_d3d12));
	assert(SUCCEEDED(hr));

#if SP_DEBUG_RESOURCE_NAMING_ENABLED
	descriptor_heap._heap_d3d12->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	descriptor_heap._descriptor_size = _sp._device->GetDescriptorHandleIncrementSize(heap_desc_d3d12.Type);
	descriptor_heap._base = {
		descriptor_heap._heap_d3d12->GetCPUDescriptorHandleForHeapStart(),
		descriptor_heap._heap_d3d12->GetGPUDescriptorHandleForHeapStart()
	};
	descriptor_heap._head = descriptor_heap._base;

	descriptor_heap._descriptor_capacity = heap_desc_d3d12.NumDescriptors;
	descriptor_heap._descriptor_count = 0;

	return descriptor_heap;
}

void sp_descriptor_heap_destroy(sp_descriptor_heap* descriptor_heap)
{
	descriptor_heap->_heap_d3d12.Reset();
}

void sp_descriptor_heap_reset(sp_descriptor_heap* descriptor_heap)
{
	descriptor_heap->_head = descriptor_heap->_base;
	descriptor_heap->_descriptor_count = 0;
}

sp_descriptor_handle sp_descriptor_heap_get_head(const sp_descriptor_heap& descriptor_heap)
{
	return descriptor_heap._head;
}

void sp_descriptor_copy_to_heap(sp_descriptor_heap* dest_dscriptor_heap, const sp_descriptor_handle* source_descriptors, int descriptor_count)
{
	static const int SP_DESCRIPTOR_COPY_COUNT_MAX = 32;

	assert(descriptor_count < SP_DESCRIPTOR_COPY_COUNT_MAX);

	// Our source descriptors are not expected to be congiguous in memory as required by 
	// CopyDescriptorsSimple. To get the desired behavior we use the full CopyDescriptors
	// function to copy from N ranges of 1 descriptor to 1 contiguous range of N descriptors

	D3D12_CPU_DESCRIPTOR_HANDLE source_descriptor_range_starts[SP_DESCRIPTOR_COPY_COUNT_MAX];
	std::transform(source_descriptors, source_descriptors + descriptor_count, source_descriptor_range_starts, [](const sp_descriptor_handle& handle) { return handle._handle_cpu_d3d12;  });
	UINT source_descriptor_range_sizes[SP_DESCRIPTOR_COPY_COUNT_MAX];
	std::fill_n(source_descriptor_range_sizes, descriptor_count, 1);

	D3D12_CPU_DESCRIPTOR_HANDLE dest_descriptor_range_starts[1] = { detail::sp_descriptor_alloc(dest_dscriptor_heap, descriptor_count)._handle_cpu_d3d12 };
	const UINT dest_descriptor_range_sizes[1] = { static_cast<UINT>(descriptor_count) };
	const UINT dest_descriptor_range_count = 1;

#if _DEBUG
	// TODO: Since we're filtering D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES due to a bug 
	// in the D3D12 debug layer, let's check for bad ranges ourselves.
	for (int i = 0; i < descriptor_count; ++i)
	{
		for (int j = 0; j < descriptor_count; ++j)
		{
			assert(source_descriptor_range_starts[i].ptr != dest_descriptor_range_starts[j].ptr);
		}
	}
#endif

	_sp._device->CopyDescriptors(
		dest_descriptor_range_count,
		dest_descriptor_range_starts,
		dest_descriptor_range_sizes,
		descriptor_count,
		source_descriptor_range_starts,
		source_descriptor_range_sizes,
		dest_dscriptor_heap->_heap_d3d12->GetDesc().Type);
}