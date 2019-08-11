#pragma once

#include "shader.h"
#include "handle.h"
#include "d3dx12.h"

#include <codecvt>

#include <wrl.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3d12shader.h>

namespace detail
{
	namespace resource_pools
	{
		std::array<sp_vertex_shader, 1024> vertex_shaders;
		sp_handle_pool vertex_shader_handles;

		std::array<sp_pixel_shader, 1024> pixel_shaders;
		sp_handle_pool pixel_shader_handles;

		std::array<sp_compute_shader, 1024> compute_shaders;
		sp_handle_pool compute_shader_handles;
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

	void sp_compute_shader_pool_create()
	{
		sp_handle_pool_create(&resource_pools::compute_shader_handles, static_cast<int>(resource_pools::compute_shaders.size()));
	}

	void sp_compute_shader_pool_destroy()
	{
		sp_handle_pool_destroy(&resource_pools::compute_shader_handles);
	}

	sp_compute_shader& sp_compute_shader_pool_get(sp_compute_shader_handle handle)
	{
		return resource_pools::compute_shaders[handle.index];
	}

	void sp_pixel_shader_recreate(sp_pixel_shader_handle& handle)
	{
		sp_pixel_shader& old_shader = detail::resource_pools::pixel_shaders[handle.index];
		sp_pixel_shader_handle new_handle = sp_pixel_shader_create(old_shader._desc);
		std::swap(resource_pools::pixel_shaders[new_handle.index], resource_pools::pixel_shaders[handle.index]);
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
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(desc.filepath).c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"vs_main",
		"vs_5_1",
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

struct sp_shader_reflection
{
	struct constant_buffer
	{
		struct constant_variable
		{
			const char* name;
			int size_bytes;
			int offset_bytes;
		};

		std::vector<constant_variable> variables;
	};

	std::vector<constant_buffer> buffers;
};

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
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(desc.filepath).c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps_main",
		"ps_5_1",
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

	sp_shader_reflection reflection;
	{
		Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection_d3d12 = nullptr;
		D3DReflect(shader._blob->GetBufferPointer(), shader._blob->GetBufferSize(), __uuidof(ID3D12ShaderReflection), (void**)&reflection_d3d12);

		D3D12_SHADER_DESC shader_desc;
		reflection_d3d12->GetDesc(&shader_desc);

		for (int constant_buffer_index = 0; constant_buffer_index < static_cast<int>(shader_desc.ConstantBuffers); ++constant_buffer_index)
		{
			sp_shader_reflection::constant_buffer constant_buffer_reflection;

			ID3D12ShaderReflectionConstantBuffer* constant_buffer_reflection_d3d12 = reflection_d3d12->GetConstantBufferByIndex(constant_buffer_index);

			D3D12_SHADER_BUFFER_DESC buffer_desc;
			constant_buffer_reflection_d3d12->GetDesc(&buffer_desc);

			for (int constant_variable_index = 0; constant_variable_index < static_cast<int>(buffer_desc.Variables); ++constant_variable_index)
			{
				sp_shader_reflection::constant_buffer::constant_variable constant_variable_reflection;

				ID3D12ShaderReflectionVariable* constant_variable_reflection_d3d12 = constant_buffer_reflection_d3d12->GetVariableByIndex(constant_variable_index);

				D3D12_SHADER_VARIABLE_DESC variable_desc;
				constant_variable_reflection_d3d12->GetDesc(&variable_desc);

				constant_variable_reflection.name = variable_desc.Name;
				constant_variable_reflection.offset_bytes = variable_desc.StartOffset;
				constant_variable_reflection.size_bytes = variable_desc.Size;

				constant_buffer_reflection.variables.push_back(constant_variable_reflection);
			}

			reflection.buffers.push_back(constant_buffer_reflection);
		}
	}

	shader._desc = desc;

	return shader_handle;
}

sp_compute_shader_handle sp_compute_shader_create(const sp_compute_shader_desc& desc)
{
	sp_compute_shader_handle shader_handle = sp_handle_alloc(&detail::resource_pools::compute_shader_handles);
	sp_compute_shader& shader = detail::resource_pools::compute_shaders[shader_handle.index];

	UINT compile_flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR; // XXX: Would be nice for performance if matrices were row major alread

#if defined(_DEBUG)
	compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG_NAME_FOR_SOURCE;
#endif

	ID3DBlob* error_blob = nullptr;
	HRESULT hr = D3DCompileFromFile(
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(desc.filepath).c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"cs_main",
		"cs_5_1",
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

void sp_pixel_shader_destroy(sp_pixel_shader_handle handle)
{
	sp_compute_shader& shader = detail::resource_pools::compute_shaders[handle.index];

	shader._blob.Reset();

	sp_handle_free(&detail::resource_pools::pixel_shader_handles, handle);
}