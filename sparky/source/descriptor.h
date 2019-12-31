#pragma once

#define NOMINMAX
#include <d3d12.h>

#include <wrl.h>

static constexpr int SP_DESCRIPTOR_TABLE_SIZE_IN_DESCRIPTORS_MAX = 32;

// TODO: sp_descriptor_heap_cpu and sp_descriptor_heap_gpu? Most heaps are cpu only and tracking gpu 
// handles has some overhead. And only cbv_srv_uav and samplers can be gpu (shader visible). Would
// also add type safety.
struct sp_descriptor_handle
{
	D3D12_CPU_DESCRIPTOR_HANDLE _handle_cpu_d3d12;
	D3D12_GPU_DESCRIPTOR_HANDLE _handle_gpu_d3d12;
};

enum class sp_descriptor_table_type
{
	cbv,
	srv,
	uav,
	sampler
};

// TODO: Hide descriptor heap from public API. Only need descriptors and tables.
enum class sp_descriptor_heap_type
{
	cbv_srv_uav,
	sampler,
	rtv,
	dsv,
};

enum class sp_descriptor_heap_visibility
{
	cpu_only,
	cpu_and_gpu
};

struct sp_descriptor_heap_desc
{
	int descriptor_capacity = 128;
	sp_descriptor_heap_visibility visibility = sp_descriptor_heap_visibility::cpu_only;
	sp_descriptor_heap_type type = sp_descriptor_heap_type::cbv_srv_uav;
};

struct sp_descriptor_heap
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _heap_d3d12;
	int _descriptor_capacity;
	int _descriptor_count;
	int _descriptor_size;
	sp_descriptor_handle _base;
	sp_descriptor_handle _head;
};

// A descriptor table is a range of descriptors in a gpu visible descriptor heap
struct sp_descriptor_table
{
	const sp_descriptor_handle _descriptor;
	const int _descriptor_count;
	const sp_descriptor_table_type _type;
};

namespace detail
{
	sp_descriptor_handle sp_descriptor_alloc(sp_descriptor_heap& descriptor_heap, int descriptor_count = 1);
	void sp_descriptor_free(sp_descriptor_heap& descriptor_heap, sp_descriptor_handle* descriptors, int descriptor_count);
}

sp_descriptor_table sp_descriptor_table_create(sp_descriptor_table_type type, int size_in_descriptors);

sp_descriptor_table sp_descriptor_table_create(sp_descriptor_table_type type, const sp_descriptor_handle* descriptors, int descriptor_count);

template <int N>
sp_descriptor_table sp_descriptor_table_create(sp_descriptor_table_type type, const sp_descriptor_handle(&descriptors)[N])
{
	return sp_descriptor_table_create(type, descriptors, N);
}

void sp_descriptor_copy_to_table(sp_descriptor_table& descriptor_table, const sp_descriptor_handle* descriptors, int descriptor_count);

template <int N>
void sp_descriptor_copy_to_table(sp_descriptor_heap& descriptor_table, const sp_descriptor_handle(&descriptors)[N])
{
	sp_descriptor_copy_to_table(descriptor_table, descriptors, N);
}

sp_descriptor_heap sp_descriptor_heap_create(const char* name, const sp_descriptor_heap_desc& desc);

void sp_descriptor_heap_reset(sp_descriptor_heap& descriptor_heap);

sp_descriptor_handle sp_descriptor_heap_get_head(const sp_descriptor_heap& descriptor_heap);

void sp_descriptor_heap_destroy(sp_descriptor_heap& descriptor_heap);

void sp_descriptor_copy_to_heap(sp_descriptor_heap& dest_dscriptor_heap, const sp_descriptor_handle* source_descriptors, int descriptor_count);

template <int N>
void sp_descriptor_copy_to_heap(sp_descriptor_heap& dest_dscriptor_heap, const sp_descriptor_handle (&source_descriptors)[N])
{
	sp_descriptor_copy_to_heap(dest_dscriptor_heap, source_descriptors, N);
}
