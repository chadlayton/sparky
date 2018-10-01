#pragma once

#include "handle.h"

#include <array>

#include <d3d12.h>

struct sp_constant_buffer_desc
{
	int size_bytes = 0;
};

struct sp_constant_buffer
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12Resource> _resource;
	CD3DX12_CPU_DESCRIPTOR_HANDLE _constant_buffer_view;
};

using sp_constant_buffer_handle = sp_handle;

namespace detail
{
	void sp_constant_buffer_pool_create();
	void sp_constant_buffer_pool_destroy();
}

sp_constant_buffer_handle sp_constant_buffer_create(const char* name, const sp_constant_buffer_desc& desc);

void sp_constant_buffer_update(sp_constant_buffer_handle buffer_handle, void* data_cpu, int size_bytes);

// TODO: Don't expose constant_buffe except by handle
const sp_constant_buffer& sp_constant_buffer_get_hack(const sp_constant_buffer_handle& buffer_handle);

void sp_constant_buffer_destroy(sp_constant_buffer_handle buffer_handle);