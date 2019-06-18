#pragma once

#include "command_list.h"
#include "constant_buffer.h"
#include "pipeline.h"
#include "descriptor.h"
#include "debug_gui.h"
#include "d3dx12.h"

#if SP_DEBUG_RENDERDOC_HOOK_ENABLED
#include <RenderDoc\renderdoc_app.h>
#endif

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>

#include <array>

const int k_back_buffer_count = 2;

struct sp
{
	Microsoft::WRL::ComPtr<ID3D12Device> _device;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> _swap_chain;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _graphics_queue;

	sp_descriptor_heap _descriptor_heap_rtv_cpu;
	sp_descriptor_heap _descriptor_heap_dsv_cpu;
	sp_descriptor_heap _descriptor_heap_cbv_srv_uav_cpu;
	sp_descriptor_heap _descriptor_heap_cbv_srv_uav_cpu_transient;
	sp_descriptor_heap _descriptor_heap_cbv_srv_uav_gpu;

	sp_descriptor_heap _descriptor_heap_debug_gui_gpu;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> _root_signature;

	sp_texture_handle _back_buffer_texture_handles[k_back_buffer_count];
} _sp;

void sp_init(const sp_window& window);
void sp_shutdown();

void sp_graphics_queue_execute(sp_graphics_command_list command_list);
void sp_graphics_queue_wait_for_idle();

// sp_compute_queue_execute(sp_compute_command_list command_list);
// sp_compute_queue_wait_for_idle();

void sp_device_wait_for_idle();
void sp_swap_chain_present();

namespace detail
{
#if SP_DEBUG_RENDERDOC_HOOK_ENABLED
	void sp_renderdoc_init()
	{
		if (HMODULE mod = LoadLibraryA("renderdoc.dll"))
		{
			RENDERDOC_API_1_1_2* renderdoc = nullptr;
			pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
			int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&renderdoc);
			assert(ret == 1);
#if defined(_DEBUG)
			renderdoc->SetCaptureOptionU32(eRENDERDOC_Option_APIValidation, 1);
#endif
			renderdoc->SetCaptureOptionU32(eRENDERDOC_Option_DebugOutputMute, 0);
	}
	}
#endif
}

void sp_init(const sp_window& window)
{

#if SP_DEBUG_RENDERDOC_HOOK_ENABLED
	detail::sp_renderdoc_init();
#endif

	HRESULT hr = S_FALSE;

	UINT dxgi_factory_flags = 0;

#if _DEBUG
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debug_interface;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
		{
			dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;

			Microsoft::WRL::ComPtr<ID3D12Debug1> debug_interface1;
			if (SUCCEEDED(debug_interface.As(&debug_interface1)))
			{
				debug_interface1->EnableDebugLayer();
				debug_interface1->SetEnableGPUBasedValidation(true);
			}
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

	// TODO: Support for fullscreen transitions.
	hr = dxgi_factory->MakeWindowAssociation(static_cast<HWND>(window._handle), DXGI_MWA_NO_ALT_ENTER);
	assert(SUCCEEDED(hr));

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
		// https://www.slideshare.net/GaelHofemeier/efficient-rendering-with-directx-12-on-intel-graphics
		// TODO: Hardcoded
		CD3DX12_DESCRIPTOR_RANGE1 range_srv;
		range_srv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 12, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		CD3DX12_DESCRIPTOR_RANGE1 range_cbv;
		range_cbv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 6, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		CD3DX12_DESCRIPTOR_RANGE1 range_uav;
		range_uav.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 6, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		// TODO: Hardcoded
		CD3DX12_ROOT_PARAMETER1 root_parameters[3];
		root_parameters[0].InitAsDescriptorTable(1, &range_srv, D3D12_SHADER_VISIBILITY_ALL);
		root_parameters[1].InitAsDescriptorTable(1, &range_cbv, D3D12_SHADER_VISIBILITY_ALL);
		root_parameters[2].InitAsDescriptorTable(1, &range_uav, D3D12_SHADER_VISIBILITY_ALL);

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		//sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		//sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		//sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		//sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
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
	_sp._root_signature = root_signature;

	_sp._descriptor_heap_dsv_cpu = sp_descriptor_heap_create("dsv_cpu", { 16, sp_descriptor_heap_visibility::cpu_only, sp_descriptor_heap_type::dsv });
	_sp._descriptor_heap_rtv_cpu = sp_descriptor_heap_create("rtv_cpu", { 128, sp_descriptor_heap_visibility::cpu_only, sp_descriptor_heap_type::rtv });
	_sp._descriptor_heap_cbv_srv_uav_cpu = sp_descriptor_heap_create("cbv_srv_uav_cpu", { 2048, sp_descriptor_heap_visibility::cpu_only, sp_descriptor_heap_type::cbv_srv_uav });
	_sp._descriptor_heap_cbv_srv_uav_cpu_transient = sp_descriptor_heap_create("cbv_srv_uav_cpu_transient", { 2048, sp_descriptor_heap_visibility::cpu_only, sp_descriptor_heap_type::cbv_srv_uav });
	_sp._descriptor_heap_cbv_srv_uav_gpu = sp_descriptor_heap_create("cbv_srv_uav_gpu", { 1024, sp_descriptor_heap_visibility::cpu_and_gpu, sp_descriptor_heap_type::cbv_srv_uav });

	detail::sp_texture_pool_create();
	detail::sp_vertex_buffer_pool_create();
	detail::sp_graphics_pipeline_state_pool_create();
	detail::sp_compute_pipeline_state_pool_create();
	detail::sp_pixel_shader_pool_create();
	detail::sp_vertex_shader_pool_create();
	detail::sp_compute_shader_pool_create();

	detail::sp_texture_defaults_create();

	for (int back_buffer_index = 0; back_buffer_index < k_back_buffer_count; ++back_buffer_index)
	{
		// TODO: sp_texture_create_from_swap_chain?
		_sp._back_buffer_texture_handles[back_buffer_index] = detail::sp_texture_handle_alloc();
		sp_texture& texture = detail::sp_texture_pool_get(_sp._back_buffer_texture_handles[back_buffer_index]);

		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc;
		hr = swap_chain3->GetDesc1(&swap_chain_desc);
		assert(SUCCEEDED(hr));

		texture._name = "swap_chain";
		texture._width = swap_chain_desc.Width;
		texture._height = swap_chain_desc.Height;

		texture._default_state = D3D12_RESOURCE_STATE_PRESENT;

		hr = swap_chain3->GetBuffer(back_buffer_index, IID_PPV_ARGS(&texture._resource));
		assert(SUCCEEDED(hr));

		texture._render_target_view = sp_descriptor_alloc(&_sp._descriptor_heap_rtv_cpu);
		device->CreateRenderTargetView(texture._resource.Get(), nullptr, texture._render_target_view._handle_cpu_d3d12);
	}

	_sp._descriptor_heap_debug_gui_gpu = sp_descriptor_heap_create("debug_gui_gpu", { 1024, sp_descriptor_heap_visibility::cpu_and_gpu, sp_descriptor_heap_type::cbv_srv_uav });

	detail::sp_debug_gui_init(device.Get(), window, sp_descriptor_alloc(&_sp._descriptor_heap_debug_gui_gpu));
}

void sp_shutdown()
{
	for (int back_buffer_index = 0; back_buffer_index < k_back_buffer_count; ++back_buffer_index)
	{
		sp_texture_destroy(_sp._back_buffer_texture_handles[back_buffer_index]);
		_sp._back_buffer_texture_handles[back_buffer_index] = sp_texture_handle();
	}

	detail::sp_texture_defaults_destroy();

	detail::sp_texture_pool_destroy();
	detail::sp_vertex_buffer_pool_destroy();
	detail::sp_graphics_pipeline_state_pool_destroy();
	detail::sp_compute_pipeline_state_pool_destroy();
	detail::sp_pixel_shader_pool_destroy();
	detail::sp_vertex_shader_pool_destroy();
	detail::sp_compute_shader_pool_destroy();

	sp_descriptor_heap_destroy(&_sp._descriptor_heap_dsv_cpu);
	sp_descriptor_heap_destroy(&_sp._descriptor_heap_rtv_cpu);
	sp_descriptor_heap_destroy(&_sp._descriptor_heap_cbv_srv_uav_cpu);
	sp_descriptor_heap_destroy(&_sp._descriptor_heap_cbv_srv_uav_cpu_transient);
	sp_descriptor_heap_destroy(&_sp._descriptor_heap_cbv_srv_uav_gpu);
	sp_descriptor_heap_destroy(&_sp._descriptor_heap_debug_gui_gpu);

	_sp._swap_chain.Reset();
	_sp._graphics_queue.Reset();
	_sp._root_signature.Reset();

#if _DEBUG
	{
		Microsoft::WRL::ComPtr<ID3D12DebugDevice> debug_device_interface;
		if (SUCCEEDED(_sp._device->QueryInterface(IID_PPV_ARGS(&debug_device_interface))))
		{
			debug_device_interface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		}
	}
#endif

	_sp._device.Reset();
}

void sp_graphics_queue_execute(sp_graphics_command_list command_list)
{
	ID3D12CommandList* command_lists_d3d12[] = { command_list._command_list_d3d12.Get() };
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

void sp_device_wait_for_idle()
{
	sp_graphics_queue_wait_for_idle();
	// sp_compute_queue_wait_for_idle();
}

void sp_swap_chain_present()
{
	HRESULT hr = _sp._swap_chain->Present(1, 0);
	assert(SUCCEEDED(hr));
}