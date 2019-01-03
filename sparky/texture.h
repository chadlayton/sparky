#pragma once

#include "handle.h"
#include "d3dx12.h"
#include "descriptor.h"

#include <vector>

#include <wrl.h>
#include <d3d12.h>

enum class sp_texture_format
{
	unknown,
	r8g8b8a8,
	d16,
	d32,
};

struct sp_texture_desc
{
	int width;
	int height;
	sp_texture_format format;
};

struct sp_texture
{
	const char* _name;
	int _width;
	int _height;
	Microsoft::WRL::ComPtr<ID3D12Resource> _resource;

	sp_descriptor_handle _render_target_view;
	sp_descriptor_handle _shader_resource_view;
	sp_descriptor_handle _depth_stencil_view;

	D3D12_RESOURCE_STATES _default_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
};

using sp_texture_handle = sp_handle;

namespace detail
{
	void sp_texture_pool_create();
	void sp_texture_pool_destroy();
	sp_texture_handle sp_texture_handle_alloc();
	void sp_texture_handle_free(sp_texture_handle texture_handle);
	sp_texture& sp_texture_pool_get(sp_texture_handle texture_handle);

	void sp_texture_defaults_create();
	void sp_texture_defaults_destroy();
}

namespace detail
{
	inline bool sp_texture_format_is_depth(sp_texture_format format)
	{
		switch (format)
		{
		case sp_texture_format::unknown:  assert(false); return false;
		case sp_texture_format::r8g8b8a8: return false;
		case sp_texture_format::d16:      return true;
		case sp_texture_format::d32:      return true;
		};

		assert(false);

		return false;
	}

	inline DXGI_FORMAT sp_texture_format_get_base_format_d3d12(sp_texture_format format)
	{
		switch (format)
		{
		case sp_texture_format::unknown:  assert(false); return DXGI_FORMAT_UNKNOWN;
		case sp_texture_format::r8g8b8a8: return DXGI_FORMAT_R8G8B8A8_TYPELESS;
		case sp_texture_format::d16:      return DXGI_FORMAT_R16_TYPELESS;
		case sp_texture_format::d32:      return DXGI_FORMAT_R32_TYPELESS;
		};

		assert(false);

		return DXGI_FORMAT_UNKNOWN;
	}

	inline DXGI_FORMAT sp_texture_format_get_srv_format_d3d12(sp_texture_format format)
	{
		switch (format)
		{
		case sp_texture_format::unknown:  assert(false); return DXGI_FORMAT_UNKNOWN;
		case sp_texture_format::r8g8b8a8: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case sp_texture_format::d16:      return DXGI_FORMAT_R16_FLOAT;
		case sp_texture_format::d32:      return DXGI_FORMAT_R32_FLOAT;
		};

		assert(false);

		return DXGI_FORMAT_UNKNOWN;
	}

	inline DXGI_FORMAT sp_texture_format_get_dsv_format_d3d12(sp_texture_format format)
	{
		switch (format)
		{
		case sp_texture_format::unknown:  assert(false); return DXGI_FORMAT_UNKNOWN;
		case sp_texture_format::r8g8b8a8: assert(false); return DXGI_FORMAT_UNKNOWN;
		case sp_texture_format::d16:      return DXGI_FORMAT_D16_UNORM;
		case sp_texture_format::d32:      return DXGI_FORMAT_D32_FLOAT;
		};

		assert(false);

		return DXGI_FORMAT_UNKNOWN;
	}
}

sp_texture_handle sp_texture_create(const char* name, const sp_texture_desc& desc);
void sp_texture_destroy(sp_texture_handle texture_handle);
void sp_texture_update(const sp_texture_handle& texture_handle, const void* data_cpu, int size_bytes);

sp_texture_handle sp_texture_defaults_white();
sp_texture_handle sp_texture_defaults_black();
sp_texture_handle sp_texture_defaults_checkerboard();