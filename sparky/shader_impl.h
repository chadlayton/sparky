#pragma once

#include "shader.h"
#include "handle.h"
#include "d3dx12.h"

#include <codecvt>

#include <wrl.h>
#include <d3d12.h>
#include <d3dcompiler.h>

namespace detail
{
	namespace resource_pools
	{
		std::array<sp_vertex_shader, 1024> vertex_shaders;
		sp_handle_pool vertex_shader_handles;

		std::array<sp_pixel_shader, 1024> pixel_shaders;
		sp_handle_pool pixel_shader_handles;
	}

	void sp_vertex_shader_pool_create()
	{
		sp_handle_pool_create(&resource_pools::vertex_shader_handles, static_cast<int>(resource_pools::vertex_shaders.size()));
	}

	void sp_vertex_shader_pool_destroy()
	{
		sp_handle_pool_destroy(&resource_pools::vertex_shader_handles);
	}

	sp_vertex_shader& sp_vertex_shader_pool_get(sp_vertex_shader_handle handle)
	{
		return resource_pools::vertex_shaders[handle.index];
	}

	void sp_pixel_shader_pool_create()
	{
		sp_handle_pool_create(&resource_pools::pixel_shader_handles, static_cast<int>(resource_pools::pixel_shaders.size()));
	}

	void sp_pixel_shader_pool_destroy()
	{
		sp_handle_pool_destroy(&resource_pools::pixel_shader_handles);
	}

	sp_pixel_shader& sp_pixel_shader_pool_get(sp_pixel_shader_handle handle)
	{
		return resource_pools::pixel_shaders[handle.index];
	}
}

sp_vertex_shader_handle sp_vertex_shader_create(const sp_vertex_shader_desc& desc)
{
	sp_vertex_shader_handle shader_handle = sp_handle_alloc(&detail::resource_pools::vertex_shader_handles);
	sp_vertex_shader& shader = detail::resource_pools::vertex_shaders[shader_handle.index];

	UINT compile_flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR; // XXX: Would be nice for performance if matrices were row major already

#if defined(_DEBUG)
	compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG_NAME_FOR_SOURCE;
#endif

	ID3DBlob* error_blob = nullptr;
	HRESULT hr = D3DCompileFromFile(
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(desc._file_path).c_str(),
		nullptr,
		nullptr,
		"vs_main",
		"vs_5_0",
		compile_flags,
		0,
		&shader._blob,
		&error_blob);

	if (error_blob)
	{
		OutputDebugStringA(reinterpret_cast<const char*>(error_blob->GetBufferPointer()));
		error_blob->Release();
	}

	assert(SUCCEEDED(hr));

	return shader_handle;
}

sp_pixel_shader_handle sp_pixel_shader_create(const sp_pixel_shader_desc& desc)
{
	sp_pixel_shader_handle shader_handle = sp_handle_alloc(&detail::resource_pools::pixel_shader_handles);
	sp_pixel_shader& shader = detail::resource_pools::pixel_shaders[shader_handle.index];

	UINT compile_flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR; // XXX: Would be nice for performance if matrices were row major alread

#if defined(_DEBUG)
	compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG_NAME_FOR_SOURCE;
#endif

	ID3DBlob* error_blob = nullptr;
	HRESULT hr = D3DCompileFromFile(
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(desc._file_path).c_str(),
		nullptr,
		nullptr,
		"ps_main",
		"ps_5_0",
		compile_flags,
		0,
		&shader._blob,
		&error_blob);

	if (error_blob)
	{
		OutputDebugStringA(reinterpret_cast<const char*>(error_blob->GetBufferPointer()));
		error_blob->Release();
	}

	assert(SUCCEEDED(hr));

	return shader_handle;
}