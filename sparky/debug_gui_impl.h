#pragma once

#include "window.h"
#include "descriptor.h"

#include <imgui.h>
#include <imgui.cpp>
#include <imgui_draw.cpp>
#include <imgui_widgets.cpp>
#include <imgui_impl_dx12.h>
#include <imgui_impl_dx12.cpp>

namespace detail
{
	void sp_debug_gui_init(ID3D12Device* device, const sp_window& window, sp_descriptor_handle font_descriptor_handle)
	{
		IMGUI_CHECKVERSION();

		ImGui::CreateContext();

		ImGui_ImplDX12_Init(device, 2, DXGI_FORMAT_R8G8B8A8_UNORM, font_descriptor_handle._handle_cpu_d3d12, font_descriptor_handle._handle_gpu_d3d12);
	}

	void sp_debug_gui_shutdown()
	{
		ImGui_ImplDX12_Shutdown();

		ImGui::DestroyContext();
	}
}