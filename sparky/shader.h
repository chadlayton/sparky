#pragma once

#include "handle.h"

#include <wrl.h>
#include <d3d12.h>

struct sp_vertex_shader_desc
{
	const char* _file_path;
};

struct sp_pixel_shader_desc
{
	const char* _file_path;
};

struct sp_compute_shader_desc
{
	const char* _file_path;
};

struct sp_vertex_shader
{
	Microsoft::WRL::ComPtr<ID3DBlob> _blob;
};

struct sp_pixel_shader
{
	Microsoft::WRL::ComPtr<ID3DBlob> _blob;
};

struct sp_compute_shader
{
	Microsoft::WRL::ComPtr<ID3DBlob> _blob;
};

using sp_vertex_shader_handle = sp_handle;
using sp_pixel_shader_handle = sp_handle;
using sp_compute_shader_handle = sp_handle;

namespace detail
{
	void sp_vertex_shader_pool_create();
	void sp_vertex_shader_pool_destroy();
	sp_vertex_shader& sp_vertex_shader_pool_get(sp_vertex_shader_handle handle);

	void sp_pixel_shader_pool_create();
	void sp_pixel_shader_pool_destroy();
	sp_pixel_shader& sp_pixel_shader_pool_get(sp_pixel_shader_handle handle);

	void sp_compute_shader_pool_create();
	void sp_compute_shader_pool_destroy();
	sp_compute_shader& sp_compute_shader_pool_get(sp_compute_shader_handle handle);
}

sp_vertex_shader_handle sp_vertex_shader_create(const sp_vertex_shader_desc& desc);
sp_pixel_shader_handle sp_pixel_shader_create(const sp_pixel_shader_desc& desc);
sp_compute_shader_handle sp_compute_shader_create(const sp_compute_shader_desc& desc);