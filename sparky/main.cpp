#include <windows.h>
#include <wrl.h>
#include <shellapi.h>

#include <string>
#include <cassert>
#include <codecvt>
#include <array>
#include <vector>
#include <utility>

#include <RenderDoc\renderdoc_app.h>

#include <d3d12.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
//#include <pix3.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>

#include "d3dx12.h"
#include "window.h"
#include "handle.h"
#include "math.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED 1 

const int k_back_buffer_count = 2;

struct sp_vertex_shader
{
	Microsoft::WRL::ComPtr<ID3DBlob> _blob;
};

struct sp_pixel_shader
{
	Microsoft::WRL::ComPtr<ID3DBlob> _blob;
};

struct sp_graphics_pipeline_state
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _impl;
};

struct sp_vertex_buffer
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12Resource> _resource;
	D3D12_VERTEX_BUFFER_VIEW _vertex_buffer_view;
};

enum class sp_texture_format
{
	unknown,
	r8g8b8a8,
	d16,
	d32,
};

namespace detail
{
	bool sp_texture_format_is_depth(sp_texture_format format)
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

	DXGI_FORMAT sp_texture_format_get_base_format_d3d12(sp_texture_format format)
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

	DXGI_FORMAT sp_texture_format_get_srv_format_d3d12(sp_texture_format format)
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

	DXGI_FORMAT sp_texture_format_get_dsv_format_d3d12(sp_texture_format format)
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

struct sp_texture
{
	const char* _name;
	int _width;
	int _height;
	Microsoft::WRL::ComPtr<ID3D12Resource> _resource;

	D3D12_CPU_DESCRIPTOR_HANDLE _render_target_view;
	D3D12_CPU_DESCRIPTOR_HANDLE _shader_resource_view;
	D3D12_CPU_DESCRIPTOR_HANDLE _depth_stencil_view;

	D3D12_RESOURCE_STATES _default_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
};

using sp_vertex_shader_handle = sp_handle;
using sp_pixel_shader_handle = sp_handle;
using sp_graphics_pipeline_state_handle = sp_handle;
using sp_vertex_buffer_handle = sp_handle;
using sp_texture_handle = sp_handle;

struct sp
{
	Microsoft::WRL::ComPtr<ID3D12Device> _device;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> _swap_chain;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _graphics_queue;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _render_target_view_descriptor_heap;
	unsigned _render_target_view_descriptor_size;
	CD3DX12_CPU_DESCRIPTOR_HANDLE _render_target_view_cpu_descriptor_handle;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _depth_stencil_view_descriptor_heap;
	unsigned _depth_stencil_view_descriptor_size;
	CD3DX12_CPU_DESCRIPTOR_HANDLE _depth_stencil_view_cpu_descriptor_handle;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _shader_resource_view_shader_visible_descriptor_heap;
	unsigned _shader_resource_view_shader_visible_descriptor_size;
	CD3DX12_CPU_DESCRIPTOR_HANDLE _shader_resource_view_cpu_shader_visible_descriptor_handle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE _shader_resource_view_gpu_shader_visible_descriptor_handle;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _shader_resource_view_descriptor_heap;
	unsigned _shader_resource_view_descriptor_size;
	CD3DX12_CPU_DESCRIPTOR_HANDLE _shader_resource_view_cpu_descriptor_handle;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> _root_signature;

	std::array<sp_vertex_shader, 1024> vertex_shaders;
	sp_handle_pool vertex_shader_handles;

	std::array<sp_pixel_shader, 1024> pixel_shaders;
	sp_handle_pool pixel_shader_handles;

	std::array<sp_texture, 1024> textures;
	sp_handle_pool texture_handles;

	std::array<sp_vertex_buffer, 1024> vertex_buffers;
	sp_handle_pool vertex_buffer_handles;

	std::array<sp_graphics_pipeline_state, 1024> graphics_pipelines;
	sp_handle_pool graphics_pipeline_handles;

	sp_texture_handle _back_buffer_texture_handles[k_back_buffer_count];

} _sp;

#include "constant_buffer.h"

void sp_init(const sp_window& window)
{
	HRESULT hr = S_FALSE;

	UINT dxgi_factory_flags = 0;

#if _DEBUG
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debug_interface;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
		{
			Microsoft::WRL::ComPtr<ID3D12Debug1> debug_interface1;
			debug_interface.As(&debug_interface1);

			debug_interface1->EnableDebugLayer();
			debug_interface1->SetEnableGPUBasedValidation(true);

			dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
		}
		else
		{
			assert(false);
		}
	}
#endif

	// Obtain the DXGI factory
	Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory;
	hr = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&dxgi_factory));
	assert(SUCCEEDED(hr));

	// Look for first hardware adapter that supports D3D12
	IDXGIAdapter1* adapter = nullptr;
	for (unsigned adapter_index = 0; DXGI_ERROR_NOT_FOUND != dxgi_factory->EnumAdapters1(adapter_index, &adapter); ++adapter_index)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	Microsoft::WRL::ComPtr<ID3D12Device> device;
	hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
	assert(SUCCEEDED(hr));

	// Describe and create the command queue for graphics command lists
	D3D12_COMMAND_QUEUE_DESC graphics_queue_desc = {};
	graphics_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	graphics_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> graphics_queue;
	hr = device->CreateCommandQueue(&graphics_queue_desc, IID_PPV_ARGS(&graphics_queue));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3;
	{
		RECT window_client_rect;
		GetClientRect(static_cast<HWND>(window._handle), &window_client_rect);

		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
		swap_chain_desc.BufferCount = k_back_buffer_count;
		swap_chain_desc.Width = window_client_rect.right;
		swap_chain_desc.Height = window_client_rect.bottom;
		swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.SampleDesc.Count = 1;

		Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain1;
		hr = dxgi_factory->CreateSwapChainForHwnd(
			graphics_queue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
			(HWND)window._handle,
			&swap_chain_desc,
			nullptr,
			nullptr,
			&swap_chain1
		);
		assert(SUCCEEDED(hr));

		hr = swap_chain1.As(&swap_chain3);
		assert(SUCCEEDED(hr));
	}

	// No support for fullscreen transitions.
	hr = dxgi_factory->MakeWindowAssociation(static_cast<HWND>(window._handle), DXGI_MWA_NO_ALT_ENTER);
	assert(SUCCEEDED(hr));

	// TODO: Wrap heaps
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> render_target_view_descriptor_heap;
	unsigned render_target_view_descriptor_size;
	{
		D3D12_DESCRIPTOR_HEAP_DESC render_target_view_descriptor_heap_desc_d3d12 = {};
		render_target_view_descriptor_heap_desc_d3d12.NumDescriptors = k_back_buffer_count + 16; // TODO: Hardcoded
		render_target_view_descriptor_heap_desc_d3d12.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		render_target_view_descriptor_heap_desc_d3d12.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		hr = device->CreateDescriptorHeap(&render_target_view_descriptor_heap_desc_d3d12, IID_PPV_ARGS(&render_target_view_descriptor_heap));
		assert(SUCCEEDED(hr));

		render_target_view_descriptor_heap->SetName(L"RTV");

		render_target_view_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depth_stencil_view_descriptor_heap;
	unsigned depth_stencil_view_descriptor_size;
	{
		D3D12_DESCRIPTOR_HEAP_DESC depth_stencil_view_descriptor_heap_desc_d3d12 = {};
		depth_stencil_view_descriptor_heap_desc_d3d12.NumDescriptors = k_back_buffer_count + 16; // TODO: Hardcoded
		depth_stencil_view_descriptor_heap_desc_d3d12.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		depth_stencil_view_descriptor_heap_desc_d3d12.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		hr = device->CreateDescriptorHeap(&depth_stencil_view_descriptor_heap_desc_d3d12, IID_PPV_ARGS(&depth_stencil_view_descriptor_heap));
		assert(SUCCEEDED(hr));

		depth_stencil_view_descriptor_heap->SetName(L"DSV");

		depth_stencil_view_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shader_resource_view_descriptor_heap;
	unsigned shader_resource_view_descriptor_size;
	{
		D3D12_DESCRIPTOR_HEAP_DESC shader_resource_view_descriptor_heap_desc_d3d12 = {};
		shader_resource_view_descriptor_heap_desc_d3d12.NumDescriptors = 1024; // TODO: Hardcoded
		shader_resource_view_descriptor_heap_desc_d3d12.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		shader_resource_view_descriptor_heap_desc_d3d12.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		hr = device->CreateDescriptorHeap(&shader_resource_view_descriptor_heap_desc_d3d12, IID_PPV_ARGS(&shader_resource_view_descriptor_heap));
		assert(SUCCEEDED(hr));

		depth_stencil_view_descriptor_heap->SetName(L"CBV_SRV_UAV");

		shader_resource_view_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shader_resource_view_shader_visible_descriptor_heap;
	unsigned shader_resource_view_shader_visible_descriptor_size;
	{
		D3D12_DESCRIPTOR_HEAP_DESC shader_resource_view_descriptor_heap_desc_d3d12 = {};
		shader_resource_view_descriptor_heap_desc_d3d12.NumDescriptors = 32; // TODO: Hardcoded
		shader_resource_view_descriptor_heap_desc_d3d12.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		shader_resource_view_descriptor_heap_desc_d3d12.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		hr = device->CreateDescriptorHeap(&shader_resource_view_descriptor_heap_desc_d3d12, IID_PPV_ARGS(&shader_resource_view_shader_visible_descriptor_heap));
		assert(SUCCEEDED(hr));

		depth_stencil_view_descriptor_heap->SetName(L"CBV_SRV_UAV (SHADER_VISIBLE)");

		shader_resource_view_shader_visible_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	/*
	Root Signature defines the number of arguments and their types :
		Constants
		Descriptors
		Descriptor Tables
	Performance improves with fewer DWORDs used
		Keep argument list short
	Try not to change this signature too often
		A few times per frame

	Analog of function signature and function call arguments
	The main( int argc, char *argv[] ) {}; for your GPU code
	*/
	Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
	{
		// TODO: Hardcoded
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 12, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 6, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		// TODO: Hardcoded
		CD3DX12_ROOT_PARAMETER1 root_parameters[1];
		root_parameters[0].InitAsDescriptorTable(static_cast<unsigned>(std::size(ranges)), &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC  root_signature_desc;
		root_signature_desc.Init_1_1(static_cast<unsigned>(std::size(root_parameters)), root_parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
		Microsoft::WRL::ComPtr<ID3DBlob> root_signature_blob;
		hr = D3DX12SerializeVersionedRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &root_signature_blob, &error_blob);
		assert(SUCCEEDED(hr));
		hr = device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(), root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));
		assert(SUCCEEDED(hr));
	}

	_sp._device = device;
	_sp._swap_chain = swap_chain3;
	_sp._graphics_queue = graphics_queue;
	_sp._render_target_view_descriptor_heap = render_target_view_descriptor_heap;
	_sp._render_target_view_descriptor_size = render_target_view_descriptor_size;
	_sp._render_target_view_cpu_descriptor_handle.InitOffsetted(render_target_view_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), 0);
	_sp._depth_stencil_view_descriptor_heap = depth_stencil_view_descriptor_heap;
	_sp._depth_stencil_view_descriptor_size = depth_stencil_view_descriptor_size;
	_sp._depth_stencil_view_cpu_descriptor_handle.InitOffsetted(depth_stencil_view_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), 0);
	_sp._shader_resource_view_descriptor_heap = shader_resource_view_descriptor_heap;
	_sp._shader_resource_view_descriptor_size = shader_resource_view_descriptor_size;
	_sp._shader_resource_view_cpu_descriptor_handle.InitOffsetted(shader_resource_view_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), 0);
	_sp._shader_resource_view_shader_visible_descriptor_heap = shader_resource_view_shader_visible_descriptor_heap;
	_sp._shader_resource_view_shader_visible_descriptor_size = shader_resource_view_shader_visible_descriptor_size;
	_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.InitOffsetted(shader_resource_view_shader_visible_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), 0);
	_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.InitOffsetted(shader_resource_view_shader_visible_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), 0);
	_sp._root_signature = root_signature;

	sp_handle_pool_init(&_sp.vertex_shader_handles, static_cast<int>(_sp.vertex_shaders.size()));
	sp_handle_pool_init(&_sp.pixel_shader_handles, static_cast<int>(_sp.pixel_shaders.size()));
	sp_handle_pool_init(&_sp.texture_handles, static_cast<int>(_sp.textures.size()));
	sp_handle_pool_init(&_sp.vertex_buffer_handles, static_cast<int>(_sp.vertex_buffers.size()));
	sp_handle_pool_init(&_sp.graphics_pipeline_handles, static_cast<int>(_sp.graphics_pipelines.size()));
	sp_handle_pool_init(&detail::resource_pools::constant_buffer_handles, static_cast<int>(detail::resource_pools::constant_buffers.size()));

	for (int back_buffer_index = 0; back_buffer_index < k_back_buffer_count; ++back_buffer_index)
	{
		// TODO: sp_texture_create_from_swap_chain?

		_sp._back_buffer_texture_handles[back_buffer_index] = sp_handle_alloc(&_sp.texture_handles);
		sp_texture& texture = _sp.textures[_sp._back_buffer_texture_handles[back_buffer_index].index];

		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc;
		hr = swap_chain3->GetDesc1(&swap_chain_desc);
		assert(SUCCEEDED(hr));

		texture._name = "swap_chain_buffer";
		texture._width = swap_chain_desc.Width;
		texture._height = swap_chain_desc.Height;

		texture._default_state = D3D12_RESOURCE_STATE_PRESENT;

		hr = swap_chain3->GetBuffer(back_buffer_index, IID_PPV_ARGS(&texture._resource));
		assert(SUCCEEDED(hr));

		device->CreateRenderTargetView(texture._resource.Get(), nullptr, _sp._render_target_view_cpu_descriptor_handle);
		texture._render_target_view = _sp._render_target_view_cpu_descriptor_handle;
		_sp._render_target_view_cpu_descriptor_handle.Offset(1, _sp._render_target_view_descriptor_size);
	}
}

void sp_shutdown()
{
}

struct sp_vertex_buffer_desc
{
	int _size_bytes = -1;
	int _stride_bytes = -1;
};

sp_vertex_buffer_handle sp_vertex_buffer_create(const char* name, const sp_vertex_buffer_desc& desc)
{
	HRESULT hr = S_OK;

	sp_vertex_buffer_handle buffer_handle = sp_handle_alloc(&_sp.vertex_buffer_handles);
	sp_vertex_buffer& buffer = _sp.vertex_buffers[buffer_handle.index];

	// Note: using upload heaps to transfer static data is not 
	// recommended. Every time the GPU needs it, the upload heap will be marshalled 
	// over. Please read up on Default Heap usage. An upload heap is used here for 
	// code simplicity.
	hr = _sp._device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(desc._size_bytes),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&buffer._resource));
	assert(SUCCEEDED(hr));

	buffer._name = name;

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	buffer._resource->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	buffer._vertex_buffer_view.BufferLocation = buffer._resource->GetGPUVirtualAddress();
	buffer._vertex_buffer_view.StrideInBytes = desc._stride_bytes;
	buffer._vertex_buffer_view.SizeInBytes = desc._size_bytes;

	return buffer_handle;
}

ID3D12Resource* sp_vertex_buffer_get_impl(const sp_vertex_buffer_handle& buffer_handle)
{
	return _sp.vertex_buffers[buffer_handle.index]._resource.Get();
}

void sp_vertex_buffer_update(const sp_vertex_buffer_handle& buffer_handle, void* data_cpu, size_t size_bytes)
{
	sp_vertex_buffer& buffer = _sp.vertex_buffers[buffer_handle.index];

	void* data_gpu;
	CD3DX12_RANGE read_range(0, 0); // A range where end <= begin indicates we do not intend to read from this resource on the CPU.
	HRESULT hr = buffer._resource->Map(0, &read_range, &data_gpu);
	assert(SUCCEEDED(hr));
	memcpy(data_gpu, data_cpu, size_bytes);
	buffer._resource->Unmap(0, nullptr);
}

void sp_vertex_buffer_destroy(const sp_vertex_buffer_handle& buffer_handle)
{
	sp_vertex_buffer& buffer = _sp.vertex_buffers[buffer_handle.index];

	buffer._resource = nullptr;

	sp_handle_free(&_sp.vertex_buffer_handles, buffer_handle);
}

struct sp_vertex_shader_desc
{
	const char* _file_path;
};

struct sp_pixel_shader_desc
{
	const char* _file_path;
};

sp_vertex_shader_handle sp_vertex_shader_create(const sp_vertex_shader_desc& desc)
{
	sp_vertex_shader_handle shader_handle = sp_handle_alloc(&_sp.vertex_shader_handles);
	sp_vertex_shader& shader = _sp.vertex_shaders[shader_handle.index];

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
	sp_pixel_shader_handle shader_handle = sp_handle_alloc(&_sp.pixel_shader_handles);
	sp_pixel_shader& shader = _sp.pixel_shaders[shader_handle.index];

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

ID3DBlob* sp_vertex_shader_get_impl(const sp_vertex_shader_handle& shader_handle)
{
	return _sp.vertex_shaders[shader_handle.index]._blob.Get();
}

ID3DBlob* sp_pixel_shader_get_impl(const sp_pixel_shader_handle& shader_handle)
{
	return _sp.pixel_shaders[shader_handle.index]._blob.Get();
}

struct sp_input_element_desc
{
	const char* _semantic_name;
	unsigned _semantic_index;
	DXGI_FORMAT _format;
};

struct sp_graphics_pipeline_state_desc
{
	sp_vertex_shader_handle _vertex_shader_handle;
	sp_pixel_shader_handle _pixel_shader_handle;
	sp_input_element_desc _input_layout[D3D12_STANDARD_VERTEX_ELEMENT_COUNT];
	DXGI_FORMAT _render_target_formats[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	sp_texture_format depth_stencil_format = sp_texture_format::unknown;
};

sp_graphics_pipeline_state_handle sp_graphics_pipeline_state_create(const char* name, const sp_graphics_pipeline_state_desc& desc)
{
	sp_graphics_pipeline_state_handle pipeline_state_handle = sp_handle_alloc(&_sp.graphics_pipeline_handles);
	sp_graphics_pipeline_state& pipeline_state = _sp.graphics_pipelines[pipeline_state_handle.index];

	pipeline_state._name = name;

	// TODO: Deduce from vertex shader reflection data?
	D3D12_INPUT_ELEMENT_DESC input_element_desc[D3D12_STANDARD_VERTEX_ELEMENT_COUNT];
	memset(input_element_desc, 0, sizeof(input_element_desc));
	unsigned input_element_count = 0;
	for (; input_element_count < D3D12_STANDARD_VERTEX_ELEMENT_COUNT; ++input_element_count)
	{
		if (!desc._input_layout[input_element_count]._semantic_name)
		{
			break;
		}

		input_element_desc[input_element_count].SemanticName = desc._input_layout[input_element_count]._semantic_name;
		input_element_desc[input_element_count].SemanticIndex = desc._input_layout[input_element_count]._semantic_index;
		input_element_desc[input_element_count].Format = desc._input_layout[input_element_count]._format;
		input_element_desc[input_element_count].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc = {};
	pipeline_state_desc.pRootSignature = _sp._root_signature.Get();
	pipeline_state_desc.VS = CD3DX12_SHADER_BYTECODE(sp_vertex_shader_get_impl(desc._vertex_shader_handle));
	pipeline_state_desc.PS = CD3DX12_SHADER_BYTECODE(sp_pixel_shader_get_impl(desc._pixel_shader_handle));
	pipeline_state_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pipeline_state_desc.SampleMask = UINT_MAX;
	pipeline_state_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	if (desc.depth_stencil_format == sp_texture_format::unknown)
	{
		pipeline_state_desc.DepthStencilState.DepthEnable = FALSE;
	}
	else
	{
		pipeline_state_desc.DepthStencilState.DepthEnable = TRUE;
		pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		pipeline_state_desc.DSVFormat = detail::sp_texture_format_get_dsv_format_d3d12(desc.depth_stencil_format);
	}
	pipeline_state_desc.DepthStencilState.StencilEnable = FALSE;
	pipeline_state_desc.InputLayout = { input_element_desc, input_element_count };
	pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	unsigned render_target_count = 0;
	for (; render_target_count < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++render_target_count)
	{
		if (desc._render_target_formats[render_target_count] == DXGI_FORMAT_UNKNOWN)
		{
			break;
		}

		pipeline_state_desc.RTVFormats[render_target_count] = desc._render_target_formats[render_target_count];
	}
	pipeline_state_desc.NumRenderTargets = render_target_count;
	pipeline_state_desc.SampleDesc.Count = 1;

	HRESULT hr = _sp._device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&pipeline_state._impl));
	assert(SUCCEEDED(hr));

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	pipeline_state._impl->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	return pipeline_state_handle;
}

ID3D12PipelineState* sp_graphics_pipeline_state_get_impl(const sp_graphics_pipeline_state_handle& pipeline_handle)
{
	return _sp.graphics_pipelines[pipeline_handle.index]._impl.Get();
}

#include "command_list.h"

void sp_graphics_queue_execute(sp_graphics_command_list command_list)
{
	ID3D12CommandList* command_lists_d3d12[] = { sp_graphics_command_list_get_impl(command_list) };
	_sp._graphics_queue->ExecuteCommandLists(static_cast<unsigned>(std::size(command_lists_d3d12)), command_lists_d3d12);
}

void sp_graphics_queue_wait_for_idle()
{
	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	HRESULT hr = _sp._device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	UINT64 next_fence_value = 1;

	// Create an event handle to use for frame synchronization.
	HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fence_event);

	// Signal and increment the fence value.
	const UINT64 fence_value = next_fence_value;
	hr = _sp._graphics_queue->Signal(fence.Get(), fence_value);
	assert(SUCCEEDED(hr));
	next_fence_value++;

	// Wait until the previous frame is finished.
	if (fence->GetCompletedValue() < fence_value)
	{
		hr = fence->SetEventOnCompletion(fence_value, fence_event);
		assert(SUCCEEDED(hr));
		WaitForSingleObject(fence_event, INFINITE);
	}

	CloseHandle(fence_event);
}

// sp_compute_command_queue_execute(sp_compute_command_list command_list)

void sp_device_wait_for_idle()
{
	sp_graphics_queue_wait_for_idle();
	// sp_compute_queue_wait_for_idle();
}

struct sp_texture_desc
{
	int width;
	int height;
	sp_texture_format format;
	void* data_cpu = nullptr;
	int size_bytes = 0;
};

const UINT g_pixel_size_bytes = 4;

std::vector<uint8_t> sp_image_create_checkerboard_data(int width, int height)
{
	const UINT row_pitch = width * g_pixel_size_bytes;
	const UINT cell_pitch = row_pitch >> 3;		// The width of a cell in the checkboard texture.
	const UINT cell_height = width >> 3;	// The height of a cell in the checkerboard texture.
	const UINT texture_size_bytes = row_pitch * height;

	std::vector<UINT8> data(texture_size_bytes);
	UINT8* pData = &data[0];

	for (UINT n = 0; n < texture_size_bytes; n += g_pixel_size_bytes)
	{
		UINT x = n % row_pitch;
		UINT y = n / row_pitch;
		UINT i = x / cell_pitch;
		UINT j = y / cell_height;

		if (i % 2 == j % 2)
		{
			pData[n] = 0x00;		// R
			pData[n + 1] = 0x00;	// G
			pData[n + 2] = 0x00;	// B
			pData[n + 3] = 0xff;	// A
		}
		else
		{
			pData[n] = 0xff;		// R
			pData[n + 1] = 0xff;	// G
			pData[n + 2] = 0xff;	// B
			pData[n + 3] = 0xff;	// A
		}
	}

	return data;
}

sp_texture_handle sp_texture_create(const char* name, const sp_texture_desc& desc)
{
	sp_texture_handle texture_handle = sp_handle_alloc(&_sp.texture_handles);
	sp_texture& texture = _sp.textures[texture_handle.index];

	// XXX: Is there any performance penalty to creating every texture with either an SRV (and an RTV/DSV)?

	D3D12_RESOURCE_DESC resource_desc_d3d12 = {};
	resource_desc_d3d12.MipLevels = 1;
	resource_desc_d3d12.Format = detail::sp_texture_format_get_base_format_d3d12(desc.format);
	resource_desc_d3d12.Width = desc.width;
	resource_desc_d3d12.Height = desc.height;
	resource_desc_d3d12.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	if (detail::sp_texture_format_is_depth(desc.format))
	{
		resource_desc_d3d12.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	else
	{
		resource_desc_d3d12.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	resource_desc_d3d12.DepthOrArraySize = 1;
	resource_desc_d3d12.SampleDesc.Count = 1;
	resource_desc_d3d12.SampleDesc.Quality = 0;
	resource_desc_d3d12.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE optimized_clear_value = {};
	if (detail::sp_texture_format_is_depth(desc.format))
	{
		optimized_clear_value.Format = detail::sp_texture_format_get_dsv_format_d3d12(desc.format);
		optimized_clear_value.DepthStencil.Depth = 1.0f;
		optimized_clear_value.DepthStencil.Stencil = 0;
	}
	else
	{
		optimized_clear_value.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
		optimized_clear_value.Color[0] = 0.0f;
		optimized_clear_value.Color[1] = 0.0f;
		optimized_clear_value.Color[2] = 0.0f;
		optimized_clear_value.Color[3] = 0.0f;
	}

	HRESULT hr = _sp._device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resource_desc_d3d12,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optimized_clear_value,
		IID_PPV_ARGS(&texture._resource));
	assert(hr == S_OK);

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	texture._resource->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	texture._name = name;
	texture._width = desc.width;
	texture._height = desc.height;

	// TODO: Including the SRV and RTV in the texture isn't ideal. Need to lookup texture by handle just to
	// get another handle. Maybe host should deal with these + organizing into descriptor tables.

	D3D12_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc_d3d12 = {};
	shader_resource_view_desc_d3d12.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shader_resource_view_desc_d3d12.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
	shader_resource_view_desc_d3d12.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	shader_resource_view_desc_d3d12.Texture2D.MipLevels = 1;
	_sp._device->CreateShaderResourceView(texture._resource.Get(), &shader_resource_view_desc_d3d12, _sp._shader_resource_view_cpu_descriptor_handle);
	texture._shader_resource_view = _sp._shader_resource_view_cpu_descriptor_handle;
	_sp._shader_resource_view_cpu_descriptor_handle.Offset(1, _sp._shader_resource_view_descriptor_size);

	if (detail::sp_texture_format_is_depth(desc.format))
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc_d3d12 = {};
		depth_stencil_view_desc_d3d12.Format = detail::sp_texture_format_get_dsv_format_d3d12(desc.format);
		depth_stencil_view_desc_d3d12.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		_sp._device->CreateDepthStencilView(texture._resource.Get(), &depth_stencil_view_desc_d3d12, _sp._depth_stencil_view_cpu_descriptor_handle);
		texture._depth_stencil_view = _sp._depth_stencil_view_cpu_descriptor_handle;
		_sp._depth_stencil_view_cpu_descriptor_handle.Offset(1, _sp._depth_stencil_view_descriptor_size);
	}
	else
	{
		D3D12_RENDER_TARGET_VIEW_DESC render_target_view_desc_d3d12 = {};
		render_target_view_desc_d3d12.Format = detail::sp_texture_format_get_srv_format_d3d12(desc.format);
		render_target_view_desc_d3d12.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		render_target_view_desc_d3d12.Texture2D.MipSlice = 0;
		_sp._device->CreateRenderTargetView(texture._resource.Get(), &render_target_view_desc_d3d12, _sp._render_target_view_cpu_descriptor_handle);
		texture._render_target_view = _sp._render_target_view_cpu_descriptor_handle;
		_sp._render_target_view_cpu_descriptor_handle.Offset(1, _sp._render_target_view_descriptor_size);
	}
	
	return texture_handle;
}

void sp_texture_update(const sp_texture_handle& texture_handle, void* data_cpu, int size_bytes)
{
	sp_texture& texture = _sp.textures[texture_handle.index];

	// TODO: Should this use a copy command list/queue
	sp_graphics_command_list texture_update_command_list = sp_graphics_command_list_create(texture._name, {});

	// TODO: Buffer size
	// Create the GPU upload buffer.
	const int upload_buffer_size_bytes = 1024 * 1024 * 128; // GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

	Microsoft::WRL::ComPtr<ID3D12Resource> texture_upload_buffer;

	HRESULT hr = _sp._device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size_bytes),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texture_upload_buffer));
	assert(hr == S_OK);

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	texture_upload_buffer->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(texture._name).c_str());
#endif

	// Copy data to the intermediate upload heap and then schedule a copy 
	// from the upload heap to the Texture2D.
	std::vector<uint8_t> image_data = sp_image_create_checkerboard_data(texture._width, texture._height);

	D3D12_SUBRESOURCE_DATA subresource_data = {};
	subresource_data.pData = &image_data[0];
	subresource_data.RowPitch = texture._width * g_pixel_size_bytes;
	subresource_data.SlicePitch = subresource_data.RowPitch * texture._height;

	texture_update_command_list._command_list_d3d12->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture._resource.Get(), texture._default_state, D3D12_RESOURCE_STATE_COPY_DEST));

	UpdateSubresources(texture_update_command_list._command_list_d3d12.Get(),
		texture._resource.Get(),
		texture_upload_buffer.Get(),
		0,
		0,
		1,
		&subresource_data);

	texture_update_command_list._command_list_d3d12->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture._resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, texture._default_state));

	sp_graphics_command_list_close(texture_update_command_list);

	sp_graphics_queue_execute(texture_update_command_list);

	// Need to wait for texture to be updated before allowing upload buffer to fall out of scope
	sp_graphics_queue_wait_for_idle();

	sp_graphics_command_list_destroy(texture_update_command_list);
}

// TODO: Don't expose sp_texture except by handle
const sp_texture& sp_texture_get_hack(const sp_texture_handle& texture_handle)
{
	return _sp.textures[texture_handle.index];
}

void sp_swap_chain_present()
{
	HRESULT hr = _sp._swap_chain->Present(1, 0);
	assert(SUCCEEDED(hr));
}

struct sx_graphics_render_pass_desc
{

};

struct sx_graphics_render_pass
{
	const char* _name;
	sp_texture_handle _render_targets[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	sp_texture_handle _depth_stencil_target;
};

sx_graphics_render_pass sx_graphics_render_pass_create(const char* name, sx_graphics_render_pass_desc& desc)
{
	return {};
}

void sx_graphics_render_pass_record(const sx_graphics_render_pass& pass, sp_graphics_command_list& command_list, void* draw_calls)
{
	// events
	// transitions from D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE to D3D12_RESOURCE_STATE_RENDER_TARGET
	// bind render targets
	// draw calls...
	// unbind render targets
	// transitions from D3D12_RESOURCE_STATE_RENDER_TARGET back to D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
}

// sx_graphics_render_pass gbuffer_render_pass = sx_graphics_render_pass_create("gbuffer", desc);
// sx_graphics_render_pass_record(gbuffer_render_pass, command_list, draw_calls);

struct camera
{
	math::vec<3> position;
	math::vec<3> rotation;
};

math::mat<4> camera_get_transform(const camera& camera)
{
	math::mat<4> transform = math::create_identity<4>();
	transform = math::multiply(transform, math::create_rotation_x(camera.rotation.x));
	transform = math::multiply(transform, math::create_rotation_x(camera.rotation.y));
	transform = math::multiply(transform, math::create_rotation_x(camera.rotation.z));
	math::set_translation(transform, camera.position);
	return transform;
}

int main()
{
	//if (HMODULE mod = LoadLibraryA("renderdoc.dll"))
	//{
	//	RENDERDOC_API_1_1_2* RENDERDOC_API = nullptr;
	//	pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
	//	int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&RENDERDOC_API);
	//	assert(ret == 1);
	//}

	const int window_width = 1280;
	const int window_height = 720;
	const float aspect_ratio = window_width / static_cast<float>(window_height);

	sp_window window = sp_window_create(L"demo", window_width, window_height);

	sp_init(window);

	camera camera{ { 0, 0, -10 }, {0, 0, 0} };

	math::vec<3> point_ws = { 0, 5, 0 };

	math::mat<4> view_matrix = math::inverse(camera_get_transform(camera));

	math::vec<3> point_vs = math::transform_point(view_matrix, point_ws);

	math::mat<4> projection_matrix = math::create_perspective_fov_lh(math::pi / 2, aspect_ratio, 0.1f, 100.0f);

	math::mat<4> view_projection_matrix = math::multiply(view_matrix, projection_matrix);

	math::mat<4> inverse_view_projection_matrix = math::inverse(view_projection_matrix);

	int frame_index = _sp._swap_chain->GetCurrentBackBufferIndex();

	sp_vertex_shader_handle gbuffer_vertex_shader_handle = sp_vertex_shader_create({ "gbuffer.hlsl" });
	sp_pixel_shader_handle gbuffer_pixel_shader_handle = sp_pixel_shader_create({ "gbuffer.hlsl" });

	sp_graphics_pipeline_state_handle gbuffer_pipeline_state_handle = sp_graphics_pipeline_state_create("gbuffer", {
		gbuffer_vertex_shader_handle, 
		gbuffer_pixel_shader_handle, 
		{ 
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT } 
		},
		{
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R8G8B8A8_UNORM,
		},
		sp_texture_format::d32
	});

	sp_vertex_shader_handle lighting_vertex_shader_handle = sp_vertex_shader_create({ "lighting.hlsl" });
	sp_pixel_shader_handle lighting_pixel_shader_handle = sp_pixel_shader_create({ "lighting.hlsl" });

	sp_graphics_pipeline_state_handle lighting_pipeline_state_handle = sp_graphics_pipeline_state_create("lighting", {
		lighting_vertex_shader_handle,
		lighting_pixel_shader_handle,
		{},
		{
			DXGI_FORMAT_R8G8B8A8_UNORM,
		},
	});

	__declspec(align(16)) struct
	{
		math::mat<4> projection_matrix;
		math::mat<4> view_projection_matrix;
		math::mat<4> inverse_view_projection_matrix;

	} constant_buffer_per_frame_data;

	constant_buffer_per_frame_data.projection_matrix = projection_matrix;
	constant_buffer_per_frame_data.view_projection_matrix = view_projection_matrix;
	constant_buffer_per_frame_data.inverse_view_projection_matrix = math::inverse(view_projection_matrix);

	sp_constant_buffer_handle constant_buffer_per_frame_handle = sp_constant_buffer_create("per_frame", { sizeof(constant_buffer_per_frame_data) } );
	sp_constant_buffer_update(constant_buffer_per_frame_handle, &constant_buffer_per_frame_data, sizeof(constant_buffer_per_frame_data));

	sp_graphics_command_list command_list = sp_graphics_command_list_create("main", { gbuffer_pipeline_state_handle });

	sp_texture_handle checkerboard_big_texture_handle = sp_texture_create("checkerboard_big", { 1024, 1024, sp_texture_format::r8g8b8a8 });
	sp_texture_update(checkerboard_big_texture_handle, nullptr, 0);

	sp_texture_handle checkerboard_small_texture_handle = sp_texture_create("checkerboard_small", { 128, 128, sp_texture_format::r8g8b8a8 });
	sp_texture_update(checkerboard_small_texture_handle, nullptr, 0);

	sp_texture_handle gbuffer_base_color_texture_handle = sp_texture_create("gbuffer_base_color", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_normals_texture_handle = sp_texture_create("gbuffer_normals", { window_width, window_height, sp_texture_format::r8g8b8a8 });
	sp_texture_handle gbuffer_depth_texture_handle = sp_texture_create("gbuffer_depth", { window_width, window_height, sp_texture_format::d32 });

	sp_vertex_buffer_handle triangle_vertex_buffer_handle;
	{
		struct vertex
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT3 normal;
			DirectX::XMFLOAT2 texcoord;
			DirectX::XMFLOAT4 color;
		};

		// XXX: There's no model/world transform yet so vertices are in world space
		vertex triangle_vertices[] =
		{
			{ { 0.0f, 5.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 5.0f, -5.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -5.0f, -5.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		triangle_vertex_buffer_handle = sp_vertex_buffer_create("triangle", { sizeof(triangle_vertices), sizeof(vertex) });
		sp_vertex_buffer_update(triangle_vertex_buffer_handle, triangle_vertices, sizeof(triangle_vertices));
	}

	while (sp_window_poll())
	{
		// Record all the commands we need to render the scene into the command list.
		{
			sp_graphics_command_list_get_impl(command_list)->SetGraphicsRootSignature(_sp._root_signature.Get());

			// The call to SetDescriptorHeaps is expensive. Only want to do once per command list.
			ID3D12DescriptorHeap* descriptor_heaps[] = { _sp._shader_resource_view_shader_visible_descriptor_heap.Get() };
			sp_graphics_command_list_get_impl(command_list)->SetDescriptorHeaps(static_cast<unsigned>(std::size(descriptor_heaps)), descriptor_heaps);

			CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(window_width), static_cast<float>(window_height));
			sp_graphics_command_list_get_impl(command_list)->RSSetViewports(1, &viewport);
			CD3DX12_RECT scissor_rect(0, 0, window_width, window_height);
			sp_graphics_command_list_get_impl(command_list)->RSSetScissorRects(1, &scissor_rect);

			sp_texture_handle gbuffer_render_target_handles[] = {
				gbuffer_base_color_texture_handle,
				gbuffer_normals_texture_handle
			};
			sp_graphics_command_list_set_render_targets(command_list, gbuffer_render_target_handles, static_cast<int>(std::size(gbuffer_render_target_handles)), gbuffer_depth_texture_handle);

			sp_graphics_command_list_get_impl(command_list)->ClearDepthStencilView(sp_texture_get_hack(gbuffer_depth_texture_handle)._depth_stencil_view, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);

			sp_graphics_command_list_get_impl(command_list)->SetGraphicsRootDescriptorTable(0, _sp._shader_resource_view_gpu_shader_visible_descriptor_handle);
			// Copy SRV
			_sp._device->CopyDescriptorsSimple(1, _sp._shader_resource_view_cpu_shader_visible_descriptor_handle, sp_texture_get_hack(checkerboard_small_texture_handle)._shader_resource_view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			// Increment to start of CBV range
			_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.Offset(11, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.Offset(11, _sp._shader_resource_view_shader_visible_descriptor_size);
			// Copy CBV
			_sp._device->CopyDescriptorsSimple(1, _sp._shader_resource_view_cpu_shader_visible_descriptor_handle, sp_constant_buffer_get_hack(constant_buffer_per_frame_handle)._constant_buffer_view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);

			sp_graphics_command_list_get_impl(command_list)->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			sp_graphics_command_list_set_vertex_buffers(command_list, &triangle_vertex_buffer_handle, 1);
			sp_graphics_command_list_get_impl(command_list)->DrawInstanced(3, 1, 0, 0);

			sp_graphics_command_list_get_impl(command_list)->SetPipelineState(sp_graphics_pipeline_state_get_impl(lighting_pipeline_state_handle));

			sp_texture_handle lighting_render_target_handles[] = {
				_sp._back_buffer_texture_handles[frame_index]
			};
			sp_graphics_command_list_set_render_targets(command_list, lighting_render_target_handles, static_cast<int>(std::size(lighting_render_target_handles)), {});

			sp_graphics_command_list_get_impl(command_list)->SetGraphicsRootDescriptorTable(0, _sp._shader_resource_view_gpu_shader_visible_descriptor_handle);
			// Copy SRV
			_sp._device->CopyDescriptorsSimple(1, _sp._shader_resource_view_cpu_shader_visible_descriptor_handle, sp_texture_get_hack(gbuffer_base_color_texture_handle)._shader_resource_view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._device->CopyDescriptorsSimple(1, _sp._shader_resource_view_cpu_shader_visible_descriptor_handle, sp_texture_get_hack(gbuffer_normals_texture_handle)._shader_resource_view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._device->CopyDescriptorsSimple(1, _sp._shader_resource_view_cpu_shader_visible_descriptor_handle, sp_texture_get_hack(gbuffer_depth_texture_handle)._shader_resource_view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			// Increment to start of CBV range
			_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.Offset(9, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.Offset(9, _sp._shader_resource_view_shader_visible_descriptor_size);
			// Copy CBV
			_sp._device->CopyDescriptorsSimple(1, _sp._shader_resource_view_cpu_shader_visible_descriptor_handle, sp_constant_buffer_get_hack(constant_buffer_per_frame_handle)._constant_buffer_view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);
			_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.Offset(1, _sp._shader_resource_view_shader_visible_descriptor_size);

			sp_graphics_command_list_get_impl(command_list)->DrawInstanced(3, 1, 0, 0);

			sp_graphics_command_list_close(command_list);

			// Reset
			_sp._shader_resource_view_cpu_shader_visible_descriptor_handle.InitOffsetted(_sp._shader_resource_view_shader_visible_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), 0);
			_sp._shader_resource_view_gpu_shader_visible_descriptor_handle.InitOffsetted(_sp._shader_resource_view_shader_visible_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), 0);
		}

		sp_graphics_queue_execute(command_list);

		sp_swap_chain_present();

		sp_device_wait_for_idle();

		sp_graphics_command_list_reset(command_list, gbuffer_pipeline_state_handle);

		frame_index = _sp._swap_chain->GetCurrentBackBufferIndex();
	}

	sp_device_wait_for_idle();

	sp_shutdown();

	return 0;
}