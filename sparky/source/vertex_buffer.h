#pragma once

#include "handle.h"

#define NOMINMAX
#include <d3d12.h>

#include <wrl.h>

struct sp_vertex_buffer_desc
{
	int _size_in_bytes = -1;
	int _stride_in_bytes = -1;
};

struct sp_vertex_buffer
{
	const char* _name = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _resource;
	D3D12_VERTEX_BUFFER_VIEW _vertex_buffer_view;
};

using sp_vertex_buffer_handle = sp_handle;

namespace detail
{
	void sp_vertex_buffer_pool_create();
	void sp_vertex_buffer_pool_destroy();
	sp_vertex_buffer& sp_vertex_buffer_pool_get(sp_vertex_buffer_handle vertex_buffer_handle);
}

sp_vertex_buffer_handle sp_vertex_buffer_create(const char* name, const sp_vertex_buffer_desc& desc);
void sp_vertex_buffer_update(const sp_vertex_buffer_handle& buffer_handle, const void* data_cpu, int size_bytes);
void sp_vertex_buffer_destroy(const sp_vertex_buffer_handle& buffer_handle);