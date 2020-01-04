#pragma once

#include "descriptor.h"
#include "constant_buffer.h"
#include "texture.h"

#define NOMINMAX
#include <d3d12.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>

#include <wrl.h>

const int k_back_buffer_count = 3;
const int k_frame_latency_max = 2;

namespace detail
{
	static inline struct sp_context
	{
		Microsoft::WRL::ComPtr<ID3D12Device> _device;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> _swap_chain;
		int _back_buffer_index;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> _graphics_queue;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> _compute_queue;

		sp_descriptor_heap _descriptor_heap_rtv_cpu;
		sp_descriptor_heap _descriptor_heap_dsv_cpu;
		sp_descriptor_heap _descriptor_heap_cbv_srv_uav_cpu;
		sp_descriptor_heap _descriptor_heap_cbv_srv_uav_gpu;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> _root_signature;

		sp_texture_handle _back_buffer_texture_handles[k_back_buffer_count];

		sp_constant_buffer_heap _constant_buffer_heap;

	} _sp;
}