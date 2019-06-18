#pragma once

#include "handle.h"
#include "descriptor.h"

#include <array>

#include <d3d12.h>

struct sp_constant_buffer_heap_desc
{
	int size_bytes = 0;
};

struct sp_constant_buffer_heap
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12Resource> _resource;
	int _size_bytes;
	int _head;
};

using sp_constant_buffer_heap_handle = sp_handle;

namespace detail
{
	void sp_constant_buffer_heap_pool_create();
	void sp_constant_buffer_heap_pool_destroy();
}

sp_constant_buffer_heap_handle sp_constant_buffer_heap_create(const char* name, const sp_constant_buffer_heap_desc& desc);

void sp_constant_buffer_heap_reset(sp_constant_buffer_heap_handle constant_buffer_heap_handle);

// TODO: Don't expose constant_buffe except by handle
const sp_constant_buffer_heap& sp_constant_buffer_heap_get_hack(const sp_constant_buffer_heap_handle& constant_buffer_heap_handle);

void sp_constant_buffer_heap_destroy(sp_constant_buffer_heap_handle constant_buffer_heap_handle);

sp_descriptor_handle sp_constant_buffer_alloc(sp_constant_buffer_heap_handle constant_buffer_heap_handle, int size_bytes, const void* initial_data);