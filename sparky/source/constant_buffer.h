#pragma once

#include <array>

#define NOMINMAX
#include <d3d12.h>

struct sp_descriptor_handle;

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

sp_constant_buffer_heap sp_constant_buffer_heap_create(const char* name, const sp_constant_buffer_heap_desc& desc);

void sp_constant_buffer_heap_reset(sp_constant_buffer_heap* constant_buffer_heap);

void sp_constant_buffer_heap_destroy(sp_constant_buffer_heap* constant_buffer_heap);

sp_descriptor_handle sp_constant_buffer_create(sp_constant_buffer_heap* constant_buffer_heap, int size_bytes, const void* initial_data);

void sp_constant_buffer_update(sp_descriptor_handle constant_buffer, const void* data, int size_in_bytes);