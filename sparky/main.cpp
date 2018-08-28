#include <windows.h>
#include <wrl.h>
#include <shellapi.h>

#include <string>
#include <cassert>
#include <codecvt>
#include <array>
#include <vector>

#include <d3d12.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>

#include <fx/gltf.h>

#include "d3dx12.h"
#include "window.h"
#include "handle.h"

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

struct sp_graphics_pipeline
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _impl;
};

struct sp_vertex_buffer
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12Resource> _resource;
};

struct sp_texture
{
	const char* _name;
	int _width;
	int _height;
	Microsoft::WRL::ComPtr<ID3D12Resource> _impl;
};

struct sp
{
	Microsoft::WRL::ComPtr<ID3D12Device> _device;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> _swap_chain;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _graphics_queue;

	Microsoft::WRL::ComPtr<ID3D12Resource> _back_buffers[k_back_buffer_count];

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _render_target_view_descriptor_heap;
	unsigned _render_target_view_descriptor_size;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _shader_resource_view_descriptor_heap;
	unsigned _shader_resource_view_descriptor_size;
	CD3DX12_CPU_DESCRIPTOR_HANDLE _shader_resource_view_descriptor_handle_cpu;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> _root_signature;

	std::array<sp_vertex_shader, 1024> vertex_shaders;
	sp_handle_pool vertex_shader_handles;

	std::array<sp_pixel_shader, 1024> pixel_shaders;
	sp_handle_pool pixel_shader_handles;

	std::array<sp_texture, 1024> textures;
	sp_handle_pool texture_handles;

	std::array<sp_vertex_buffer, 1024> vertex_buffers;
	sp_handle_pool vertex_buffer_handles;

	std::array<sp_graphics_pipeline, 1024> graphics_pipelines;
	sp_handle_pool graphics_pipeline_handles;

} _sp;

using sp_vertex_shader_handle = sp_handle;
using sp_pixel_shader_handle = sp_handle;
using sp_graphics_pipeline_handle = sp_handle;
using sp_vertex_buffer_handle = sp_handle;
using sp_texture_handle = sp_handle;

void sp_init(const sp_window& window)
{
	HRESULT hr = S_FALSE;

	UINT dxgi_factory_flags = 0;

#if _DEBUG
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debug_interface;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
		{
			debug_interface->EnableDebugLayer();

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

	Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;
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

		hr = swap_chain1.As(&swap_chain);
		assert(SUCCEEDED(hr));
	}

	// No support for fullscreen transitions.
	hr = dxgi_factory->MakeWindowAssociation(static_cast<HWND>(window._handle), DXGI_MWA_NO_ALT_ENTER);
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> render_target_view_descriptor_heap;
	unsigned render_target_view_descriptor_size;
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC render_target_view_descriptor_heap_desc_d3d12 = {};
		render_target_view_descriptor_heap_desc_d3d12.NumDescriptors = k_back_buffer_count + 16; // TODO: Hardcoded
		render_target_view_descriptor_heap_desc_d3d12.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		render_target_view_descriptor_heap_desc_d3d12.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		hr = device->CreateDescriptorHeap(&render_target_view_descriptor_heap_desc_d3d12, IID_PPV_ARGS(&render_target_view_descriptor_heap));
		assert(SUCCEEDED(hr));

		render_target_view_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> back_buffers[k_back_buffer_count];
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE render_target_view_descriptor_handle(render_target_view_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (int back_buffer_index = 0; back_buffer_index < k_back_buffer_count; back_buffer_index++)
		{
			hr = swap_chain->GetBuffer(back_buffer_index, IID_PPV_ARGS(&back_buffers[back_buffer_index]));
			assert(SUCCEEDED(hr));
			device->CreateRenderTargetView(back_buffers[back_buffer_index].Get(), nullptr, render_target_view_descriptor_handle);
			render_target_view_descriptor_handle.Offset(1, render_target_view_descriptor_size);
		}
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shader_resource_view_descriptor_heap;
	unsigned shader_resource_view_descriptor_size;
	{
		D3D12_DESCRIPTOR_HEAP_DESC shader_resource_view_descriptor_heap_desc_d3d12 = {};
		shader_resource_view_descriptor_heap_desc_d3d12.NumDescriptors = 32; // TODO: Hardcoded
		shader_resource_view_descriptor_heap_desc_d3d12.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		shader_resource_view_descriptor_heap_desc_d3d12.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		hr = device->CreateDescriptorHeap(&shader_resource_view_descriptor_heap_desc_d3d12, IID_PPV_ARGS(&shader_resource_view_descriptor_heap));
		assert(SUCCEEDED(hr));

		shader_resource_view_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	// Create an empty root signature.
	Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
	{
		// TODO: Hardcoded
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		// TODO: Hardcoded
		CD3DX12_ROOT_PARAMETER1 root_parameters[1];
		root_parameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

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
	_sp._swap_chain = swap_chain;
	_sp._graphics_queue = graphics_queue;
	for (int back_buffer_index = 0; back_buffer_index < k_back_buffer_count; back_buffer_index++)
	{
		_sp._back_buffers[back_buffer_index] = back_buffers[back_buffer_index];
	}
	_sp._render_target_view_descriptor_heap = render_target_view_descriptor_heap;
	_sp._render_target_view_descriptor_size = render_target_view_descriptor_size;
	_sp._shader_resource_view_descriptor_heap = shader_resource_view_descriptor_heap;
	_sp._shader_resource_view_descriptor_size = shader_resource_view_descriptor_size;
	_sp._shader_resource_view_descriptor_handle_cpu.InitOffsetted(shader_resource_view_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), 0);
	_sp._root_signature = root_signature;


	sp_handle_pool_init(&_sp.vertex_shader_handles, static_cast<int>(_sp.vertex_shaders.size()));
	sp_handle_pool_init(&_sp.pixel_shader_handles, static_cast<int>(_sp.pixel_shaders.size()));
	sp_handle_pool_init(&_sp.texture_handles, static_cast<int>(_sp.textures.size()));
	sp_handle_pool_init(&_sp.vertex_buffer_handles, static_cast<int>(_sp.vertex_buffers.size()));
	sp_handle_pool_init(&_sp.graphics_pipeline_handles, static_cast<int>(_sp.graphics_pipelines.size()));
}

void sp_shutdown()
{
}

struct sp_vertex_buffer_desc
{
	int _size_bytes = 0;
	void* _data = nullptr;
};

sp_vertex_buffer_handle sp_vertex_buffer_create(const char* name, const sp_vertex_buffer_desc& desc)
{
	HRESULT hr = S_OK;

	sp_vertex_buffer_handle buffer_handle = sp_handle_alloc(&_sp.vertex_buffer_handles);
	sp_vertex_buffer& buffer = _sp.vertex_buffers[buffer_handle];

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

	return buffer_handle;
}

ID3D12Resource* sp_vertex_buffer_get_impl(const sp_vertex_buffer_handle& buffer_handle)
{
	return _sp.vertex_buffers[buffer_handle]._resource.Get();
}

void sp_vertex_buffer_update(const sp_vertex_buffer_handle& buffer_handle, void* data_cpu, size_t size_bytes)
{
	HRESULT hr = S_OK;

	sp_vertex_buffer& buffer = _sp.vertex_buffers[buffer_handle];

	void* data_gpu;
	// A range where end <= begin indicates we do not intend to read from this resource on the CPU.
	CD3DX12_RANGE read_range(0, 0);
	hr = buffer._resource->Map(0, &read_range, &data_gpu);
	assert(SUCCEEDED(hr));
	memcpy(data_gpu, data_cpu, size_bytes);
	buffer._resource->Unmap(0, nullptr);
}

void sp_vertex_buffer_destroy(const sp_vertex_buffer_handle& buffer_handle)
{
	sp_vertex_buffer& buffer = _sp.vertex_buffers[buffer_handle];

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
	sp_vertex_shader& shader = _sp.vertex_shaders[shader_handle];

#if defined(_DEBUG)
	UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG_NAME_FOR_SOURCE;
#else
	UINT compile_flags = 0;
#endif

	ID3DBlob* error_blob = nullptr;
	HRESULT hr = D3DCompileFromFile(
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(desc._file_path).c_str(), 
		nullptr, 
		nullptr, 
		"VSMain", 
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
	sp_pixel_shader& shader = _sp.pixel_shaders[shader_handle];

#if defined(_DEBUG)
	UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compile_flags = 0;
#endif

	ID3DBlob* error_blob = nullptr;
	HRESULT hr = D3DCompileFromFile(
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(desc._file_path).c_str(), 
		nullptr, 
		nullptr, 
		"PSMain", 
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
	return _sp.vertex_shaders[shader_handle]._blob.Get();
}

ID3DBlob* sp_pixel_shader_get_impl(const sp_pixel_shader_handle& shader_handle)
{
	return _sp.pixel_shaders[shader_handle]._blob.Get();
}

struct sp_input_element_desc
{
	const char* _semantic_name;
	unsigned _semantic_index;
	DXGI_FORMAT _format;
};

struct sp_graphics_pipeline_desc
{
	sp_vertex_shader_handle _vertex_shader_handle;
	sp_pixel_shader_handle _pixel_shader_handle;
	sp_input_element_desc _input_layout[D3D12_STANDARD_VERTEX_ELEMENT_COUNT];
};

sp_graphics_pipeline_handle sp_graphics_pipeline_create(const char* name, const sp_graphics_pipeline_desc& desc)
{
	sp_graphics_pipeline_handle pipeline_handle = sp_handle_alloc(&_sp.graphics_pipeline_handles);
	sp_graphics_pipeline& pipeline = _sp.graphics_pipelines[pipeline_handle];

	pipeline._name = name;

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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {};
	pipeline_desc.InputLayout = { input_element_desc, input_element_count };
	pipeline_desc.pRootSignature = _sp._root_signature.Get();
	pipeline_desc.VS = CD3DX12_SHADER_BYTECODE(sp_vertex_shader_get_impl(desc._vertex_shader_handle));
	pipeline_desc.PS = CD3DX12_SHADER_BYTECODE(sp_pixel_shader_get_impl(desc._pixel_shader_handle));
	pipeline_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pipeline_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pipeline_desc.DepthStencilState.DepthEnable = FALSE;
	pipeline_desc.DepthStencilState.StencilEnable = FALSE;
	pipeline_desc.SampleMask = UINT_MAX;
	pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipeline_desc.NumRenderTargets = 1;
	pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipeline_desc.SampleDesc.Count = 1;

	HRESULT hr = _sp._device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline._impl));
	assert(SUCCEEDED(hr));

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	pipeline._impl->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	return pipeline_handle;
}

ID3D12PipelineState* sp_graphics_pipeline_get_impl(const sp_graphics_pipeline_handle& pipeline_handle)
{
	return _sp.graphics_pipelines[pipeline_handle]._impl.Get();
}

struct sp_graphics_command_list_desc
{
	sp_graphics_pipeline_handle pipeline_handle;
};

struct sp_graphics_command_list
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _command_list_d3d12;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _command_allocator_d3d12;
};

sp_graphics_command_list sp_graphics_command_list_create(const char* name, const sp_graphics_command_list_desc& desc)
{
	sp_graphics_command_list command_list;

	HRESULT hr = _sp._device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, 
		IID_PPV_ARGS(&command_list._command_allocator_d3d12));
	assert(SUCCEEDED(hr));

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	command_list._command_allocator_d3d12->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	hr = _sp._device->CreateCommandList(
		0, 
		D3D12_COMMAND_LIST_TYPE_DIRECT, 
		command_list._command_allocator_d3d12.Get(), 
		sp_graphics_pipeline_get_impl(desc.pipeline_handle),
		IID_PPV_ARGS(&command_list._command_list_d3d12));
	assert(SUCCEEDED(hr));

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	command_list._command_list_d3d12->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	command_list._name = name;

	// TODO: Starting out closed just to force a call to reset later so a pipeline can be 
	// supplied (or not - explicitly). Right now the interface doesn't allow optional handles.
	command_list._command_list_d3d12->Close();

	return command_list;
}

void sp_graphics_command_list_reset(sp_graphics_command_list& command_list)
{
	HRESULT hr = command_list._command_allocator_d3d12->Reset();
	assert(SUCCEEDED(hr));

	hr = command_list._command_list_d3d12->Reset(
		command_list._command_allocator_d3d12.Get(),
		nullptr);
	assert(SUCCEEDED(hr));
}

void sp_graphics_command_list_reset(sp_graphics_command_list& command_list, const sp_graphics_pipeline_handle& pipeline_handle)
{
	HRESULT hr = command_list._command_allocator_d3d12->Reset();
	assert(SUCCEEDED(hr));

	hr = command_list._command_list_d3d12->Reset(
		command_list._command_allocator_d3d12.Get(), 
		sp_graphics_pipeline_get_impl(pipeline_handle));
	assert(SUCCEEDED(hr));
}

void sp_graphics_command_list_destroy(sp_graphics_command_list& command_list)
{
	command_list._name = nullptr;
	command_list._command_list_d3d12.Reset();
	command_list._command_allocator_d3d12.Reset();
}

ID3D12GraphicsCommandList* sp_graphics_command_list_get_impl(const sp_graphics_command_list& command_list)
{
	return command_list._command_list_d3d12.Get();
}

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
	int _width;
	int _height;
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
	sp_texture& texture = _sp.textures[texture_handle];

	D3D12_RESOURCE_DESC resource_desc_d3d12 = {};
	resource_desc_d3d12.MipLevels = 1;
	resource_desc_d3d12.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resource_desc_d3d12.Width = desc._width;
	resource_desc_d3d12.Height = desc._height;
	resource_desc_d3d12.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource_desc_d3d12.Flags = D3D12_RESOURCE_FLAG_NONE;
	resource_desc_d3d12.DepthOrArraySize = 1;
	resource_desc_d3d12.SampleDesc.Count = 1;
	resource_desc_d3d12.SampleDesc.Quality = 0;
	resource_desc_d3d12.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	HRESULT hr = S_OK;

	hr = _sp._device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resource_desc_d3d12,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture._impl));
	assert(hr == S_OK);

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	texture._impl->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	texture._name = name;
	texture._width = desc._width;
	texture._height = desc._height;

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc_d3d12 = {};
	shader_resource_view_desc_d3d12.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shader_resource_view_desc_d3d12.Format = resource_desc_d3d12.Format;
	shader_resource_view_desc_d3d12.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	shader_resource_view_desc_d3d12.Texture2D.MipLevels = 1;
	_sp._device->CreateShaderResourceView(texture._impl.Get(), &shader_resource_view_desc_d3d12, _sp._shader_resource_view_descriptor_handle_cpu);
	_sp._shader_resource_view_descriptor_handle_cpu.Offset(1, _sp._shader_resource_view_descriptor_size);

	return texture_handle;
}

void sp_texture_update(const sp_texture_handle& texture_handle, void* data_cpu, size_t size_bytes)
{
	sp_texture& texture = _sp.textures[texture_handle];

	sp_graphics_command_list texture_update_command_list = sp_graphics_command_list_create(texture._name, {});
	sp_graphics_command_list_reset(texture_update_command_list);

	// TODO: Buffer size
	// Create the GPU upload buffer.
	const UINT64 upload_buffer_size_bytes = 1024 * 1024 * 128; // GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

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
	std::vector<UINT8> image_data = sp_image_create_checkerboard_data(texture._width, texture._height);

	D3D12_SUBRESOURCE_DATA subresource_data = {};
	subresource_data.pData = &image_data[0];
	subresource_data.RowPitch = texture._width * g_pixel_size_bytes;
	subresource_data.SlicePitch = subresource_data.RowPitch * texture._height;

	UpdateSubresources(texture_update_command_list._command_list_d3d12.Get(),
		texture._impl.Get(),
		texture_upload_buffer.Get(),
		0,
		0,
		1,
		&subresource_data);

	sp_graphics_command_list_get_impl(texture_update_command_list)->Close();

	sp_graphics_queue_execute(texture_update_command_list);

	// Need to wait for texture to be updated before allowing upload buffer to fall out of scope
	sp_graphics_queue_wait_for_idle();

	sp_graphics_command_list_destroy(texture_update_command_list);
}

void sp_swap_chain_present()
{
	HRESULT hr = _sp._swap_chain->Present(1, 0);
	assert(SUCCEEDED(hr));
}

/*
struct sp_render_pass_desc
{
	sp_texture_handle _color_targets[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	sp_texture_handle _depth_target;
};

struct sp_render_pass
{

};*/

int main()
{
	const int window_width = 1280;
	const int window_height = 720;
	const float aspect_ratio = window_width / static_cast<float>(window_height);

	sp_window window = sp_window_create(L"demo", window_width, window_height);

	sp_init(window);

	int frame_index = _sp._swap_chain->GetCurrentBackBufferIndex();

	sp_graphics_command_list command_list;
	sp_vertex_buffer_handle vertex_buffer_handle;
	sp_graphics_pipeline_handle pipeline_handle;
	sp_texture_handle texture0_handle;
	sp_texture_handle texture1_handle;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view_d3d12;

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		sp_vertex_shader_handle vertex_shader_handle = sp_vertex_shader_create({ "shaders.hlsl" });
		sp_pixel_shader_handle pixel_shader_handle = sp_pixel_shader_create({ "shaders.hlsl" });

		pipeline_handle = sp_graphics_pipeline_create("mypipeline", {
			vertex_shader_handle, 
			pixel_shader_handle, 
			{ 
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT } 
			}
		});
	}

	command_list = sp_graphics_command_list_create("main", {});

	// create textures
	{
		texture0_handle = sp_texture_create("mytexture0", { 1024, 1024 });
		sp_texture_update(texture0_handle, nullptr, 0);

		texture1_handle = sp_texture_create("mytexture1", { 512, 512 });
		sp_texture_update(texture1_handle, nullptr, 0);
	}

	// create the vertex buffer.
	{
		struct vertex
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT3 normal;
			DirectX::XMFLOAT2 texcoord;
			DirectX::XMFLOAT4 color;
		};

		vertex triangle_vertices[] =
		{
			{ { 0.0f, 0.25f * aspect_ratio, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f * aspect_ratio, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * aspect_ratio, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		vertex_buffer_handle = sp_vertex_buffer_create("triangle", { sizeof(triangle_vertices) });
		sp_vertex_buffer_update(vertex_buffer_handle, triangle_vertices, sizeof(triangle_vertices));

		// TODO: Move this into vertex_buffer
		// Initialize the vertex buffer view.
		vertex_buffer_view_d3d12.BufferLocation = sp_vertex_buffer_get_impl(vertex_buffer_handle)->GetGPUVirtualAddress();
		vertex_buffer_view_d3d12.StrideInBytes = sizeof(vertex);
		vertex_buffer_view_d3d12.SizeInBytes = sizeof(triangle_vertices);
	}

	while (sp_window_poll())
	{
		// Record all the commands we need to render the scene into the command list.
		{
			sp_graphics_command_list_reset(command_list, pipeline_handle);

			// Set necessary state.
			sp_graphics_command_list_get_impl(command_list)->SetGraphicsRootSignature(_sp._root_signature.Get());
			ID3D12DescriptorHeap* descriptor_heaps[] = { _sp._shader_resource_view_descriptor_heap.Get() };
			sp_graphics_command_list_get_impl(command_list)->SetDescriptorHeaps(static_cast<unsigned>(std::size(descriptor_heaps)), descriptor_heaps);
			sp_graphics_command_list_get_impl(command_list)->SetGraphicsRootDescriptorTable(0, _sp._shader_resource_view_descriptor_heap->GetGPUDescriptorHandleForHeapStart());
			CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(window_width), static_cast<float>(window_height));
			sp_graphics_command_list_get_impl(command_list)->RSSetViewports(1, &viewport);
			CD3DX12_RECT scissor_rect(0, 0, window_width, window_height);
			sp_graphics_command_list_get_impl(command_list)->RSSetScissorRects(1, &scissor_rect);

			// Indicate that the back buffer will be used as a render target.
			sp_graphics_command_list_get_impl(command_list)->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_sp._back_buffers[frame_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			CD3DX12_CPU_DESCRIPTOR_HANDLE render_target_view_handle_cpu(_sp._render_target_view_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), frame_index, _sp._render_target_view_descriptor_size);
			sp_graphics_command_list_get_impl(command_list)->OMSetRenderTargets(1, &render_target_view_handle_cpu, FALSE, nullptr);

			// Record commands.
			const float clear_color[] = { 0.0f, 0.2f, 0.4f, 1.0f };
			sp_graphics_command_list_get_impl(command_list)->ClearRenderTargetView(render_target_view_handle_cpu, clear_color, 0, nullptr);
			sp_graphics_command_list_get_impl(command_list)->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			sp_graphics_command_list_get_impl(command_list)->IASetVertexBuffers(0, 1, &vertex_buffer_view_d3d12);
			sp_graphics_command_list_get_impl(command_list)->DrawInstanced(3, 1, 0, 0);

			// Indicate that the back buffer will now be used to present.
			sp_graphics_command_list_get_impl(command_list)->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_sp._back_buffers[frame_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

			HRESULT hr = sp_graphics_command_list_get_impl(command_list)->Close();
			assert(SUCCEEDED(hr));
		}

		sp_graphics_queue_execute(command_list);

		sp_swap_chain_present();

		sp_device_wait_for_idle();

		frame_index = _sp._swap_chain->GetCurrentBackBufferIndex();
	}

	sp_device_wait_for_idle();

	sp_shutdown();

	return 0;
}