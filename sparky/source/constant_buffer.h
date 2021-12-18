#pragma once

#include <array>

#define NOMINMAX
#include <d3d12.h>

namespace detail
{
	struct sp_constant_buffer_heap_desc
	{
		int size_in_bytes = 0;
	};

	struct sp_constant_buffer_heap
	{
		const char* _name;
		Microsoft::WRL::ComPtr<ID3D12Resource> _resource;
		int _size_in_bytes = 0;
		int _head = 0;
	};
}

struct sp_constant_buffer
{
	const int _size_in_bytes;
	const sp_descriptor_handle _constant_buffer_view;
	const int _offset_in_heap;
};

namespace detail
{
	sp_constant_buffer_heap sp_constant_buffer_heap_create(const char* name, const sp_constant_buffer_heap_desc& desc);

	void sp_constant_buffer_heap_reset(sp_constant_buffer_heap& constant_buffer_heap);

	void sp_constant_buffer_heap_destroy(sp_constant_buffer_heap& constant_buffer_heap);
}

sp_constant_buffer sp_constant_buffer_create(int size_in_bytes);

void sp_constant_buffer_update(sp_constant_buffer& constant_buffer, const void* data);