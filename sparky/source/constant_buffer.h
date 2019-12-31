#pragma once

#include <array>

#define NOMINMAX
#include <d3d12.h>

struct sp_descriptor_handle;

// TODO: move sp_constant_buffer_heap into private API
struct sp_constant_buffer_heap_desc
{
	int size_bytes = 0;
};

struct sp_constant_buffer_heap
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12Resource> _resource;
	int _size_in_bytes;
	int _head;
};

struct sp_constant_buffer
{
	const int _size_in_bytes;
	const sp_descriptor_handle _constant_buffer_view;
	const sp_constant_buffer_heap& _heap;
	const int _offset_in_heap;
};

sp_constant_buffer_heap sp_constant_buffer_heap_create(const char* name, const sp_constant_buffer_heap_desc& desc);

void sp_constant_buffer_heap_reset(sp_constant_buffer_heap* constant_buffer_heap);

void sp_constant_buffer_heap_destroy(sp_constant_buffer_heap* constant_buffer_heap);

sp_constant_buffer sp_constant_buffer_create(sp_constant_buffer_heap& constant_buffer_heap, int size_in_bytes);

void sp_constant_buffer_update(sp_constant_buffer& constant_buffer, const void* data);